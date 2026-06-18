#include "CsiReceiver.hpp"
#include <string.h>
#include <math.h>
#include <stdlib.h>

int s_last_rssi_delta = 0;
extern int s_channel;
CsiReceiver *CsiReceiver::s_instance = nullptr;

// ========== HEADER 802.11 ==========
typedef struct {
    uint16_t frame_ctrl;
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
} __attribute__((packed)) wifi_80211_hdr_t;

static int compare_float(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

void IRAM_ATTR CsiReceiver::s_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_instance) return;

    auto *pkt = (wifi_promiscuous_pkt_t*)buf;
    auto *hdr = (wifi_80211_hdr_t*)pkt->payload;

    extern uint8_t s_tx_mac[6];
    if (memcmp(hdr->addr2, s_tx_mac, 6) != 0) return;

    uint32_t seq = s_instance->pkt_count_ + 1;
    s_instance->pkt_count_ = seq;

    #define CSI_OFFSET 34
    int16_t *csi_data = (int16_t*)(pkt->payload + CSI_OFFSET);
    
    csi_event_t ev;
    ev.seq = seq;
    ev.rssi = pkt->rx_ctrl.rssi;
    ev.sig_len = pkt->rx_ctrl.sig_len;
    memcpy(ev.csi_raw, csi_data, 104 * sizeof(int16_t));

    static uint32_t last_print = 0;
    if (seq - last_print > 500) {
        last_print = seq;
        printf("CSI[%lu]: ", seq);
        for (int i = 0; i < 5; i++) {
            printf("(%d,%d) ", ev.csi_raw[i*2], ev.csi_raw[i*2+1]);
        }
        printf("\n");
    }

    BaseType_t wake = pdFALSE;
    xQueueSendFromISR(s_instance->event_queue_, &ev, &wake);
    if (wake) portYIELD_FROM_ISR();
}

static inline void compute_amplitudes(int16_t *csi_raw, float *amplitudes) {
    for (int i = 0; i < 52; i++) {
        int16_t I = csi_raw[i * 2];
        int16_t Q = csi_raw[i * 2 + 1];
        amplitudes[i] = sqrtf((float)(I * I + Q * Q));
    }
}

static int median_filter(float *input, float *output, int size, int keep) {
    float sorted[52];
    memcpy(sorted, input, size * sizeof(float));
    qsort(sorted, size, sizeof(float), compare_float);
    
    int start = (size - keep) / 2;
    for (int i = 0; i < keep; i++) {
        output[i] = sorted[start + i];
    }
    return keep;
}

// ============================================================
// processRSSI - PHÂN BIỆT NGỒI (DAO ĐỘNG) VS NẰM (ỔN ĐỊNH)
// ============================================================
void CsiReceiver::processRSSI(int rssi, uint32_t seq) {
    static int win[8] = {0};
    static int idx = 0, filled = 0;
    static int baseline = 0, base_cnt = 0;
    static bool base_ready = false;
    
    // Lưu lịch sử delta để tính độ ổn định
    static int delta_history[40] = {0};  // Tăng từ 30 lên 40
    static int history_idx = 0;
    static int history_filled = 0;
    
    // Lưu lịch sử RSSI filtered để phát hiện xu hướng
    static int rssi_history[30] = {0};   // Tăng từ 20 lên 30
    static int rssi_idx = 0;
    static int rssi_filled = 0;

    // Lọc trung bình động
    win[idx++] = rssi;
    if (idx >= 8) { idx = 0; filled = 1; }
    int n = filled ? 8 : idx;
    int sum = 0;
    for (int i = 0; i < n; i++) sum += win[i];
    int filtered = sum / n;

    // Lưu lịch sử RSSI
    rssi_history[rssi_idx++] = filtered;
    if (rssi_idx >= 30) { rssi_idx = 0; rssi_filled = 1; }

    if (!base_ready && seq < 256) {
        if (base_cnt == 0) baseline = filtered;
        else baseline = (baseline + filtered) / 2;
        base_cnt++;
        if (seq == 255) {
            base_ready = true;
            if (print_mode_ & PRINT_MODE_STATE)
                ESP_LOGI("CSI", "Baseline RSSI: %d dBm", baseline);
        }
        return;
    }
    if (!base_ready) return;

    int delta = abs(filtered - baseline);
    uint32_t now = esp_timer_get_time() / 1000;

    // ★ GIỮ NGUYÊN ĐỊNH DẠNG RSSI CŨ
    printf("RSSI,%lu,%d,%d,%d,%d,%.1f\n", 
           (unsigned long)seq, rssi, filtered, delta, 
           is_lying_ ? 1 : 0, 0.0f);

    // Lưu lịch sử delta
    delta_history[history_idx++] = delta;
    if (history_idx >= 40) { history_idx = 0; history_filled = 1; }
    
    // Tính độ ổn định
    int hist_len = history_filled ? 40 : history_idx;
    if (hist_len > 15) {  // Tăng từ 10 lên 15
        int avg = 0;
        for (int i = 0; i < hist_len; i++) avg += delta_history[i];
        avg /= hist_len;
        
        int variance = 0;
        for (int i = 0; i < hist_len; i++) {
            int diff = delta_history[i] - avg;
            variance += diff * diff;
        }
        variance /= hist_len;
        int stability = sqrt(variance);
        
        // ★ TÍNH XU HƯỚNG RSSI (đang giảm hay tăng)
        int rssi_len = rssi_filled ? 30 : rssi_idx;
        int rssi_trend = 0;
        int rssi_variance = 0;
        if (rssi_len > 10) {
            // Tính trung bình RSSI
            int rssi_avg = 0;
            for (int i = 0; i < rssi_len; i++) {
                rssi_avg += rssi_history[i];
            }
            rssi_avg /= rssi_len;
            
            // Tính độ lệch chuẩn của RSSI (độ dao động)
            int rssi_var = 0;
            for (int i = 0; i < rssi_len; i++) {
                int diff = rssi_history[i] - rssi_avg;
                rssi_var += diff * diff;
            }
            rssi_var /= rssi_len;
            rssi_variance = sqrt(rssi_var);
            
            // Tính xu hướng (so sánh 5 mẫu đầu và 5 mẫu cuối)
            if (rssi_len > 10) {
                int first_sum = 0, last_sum = 0;
                for (int i = 0; i < 5; i++) {
                    first_sum += rssi_history[i];
                    last_sum += rssi_history[rssi_len - 5 + i];
                }
                rssi_trend = (last_sum - first_sum) / 5;  // Âm = đang giảm
            }
        }
        
        // ★ IN DEBUG để xem giá trị
        if (seq % 50 == 0) {
            printf("DEBUG: delta=%d, stability=%d, trend=%d, rssi_var=%d\n", 
                   delta, stability, rssi_trend, rssi_variance);
        }
        
        // ============================================================
        // ★ PHÁT HIỆN NẰM (LYING) - PHÂN BIỆT NGỒI
        // ============================================================
        // ★ QUAN TRỌNG: 
        //   - Ngồi: delta cao (7-10) NHƯNG stability CAO (>5) và rssi_variance CAO (>3)
        //   - Nằm: delta cao (7-10) NHƯNG stability THẤP (<3) và rssi_variance THẤP (<2)
        //   - Nằm lâu: trend gần 0 (ổn định), ngồi: trend dao động
        
        static bool still_candidate = false;
        static uint32_t still_start = 0;
        static int stable_count = 0;
        
        // ★ Điều kiện nằm: delta > 3 VÀ stability < 4 VÀ rssi_variance < 3
        //   VÀ (trend đang giảm HOẶC trend gần 0 - đã ổn định)
        bool is_lying_condition = (delta > 3) && 
                                  (stability < 4) && 
                                  (rssi_variance < 3) &&
                                  (rssi_trend <= 1);  // Không tăng
        
        if (is_lying_condition) {
            stable_count++;
            if (stable_count >= 10) {  // ~1 giây
                if (!still_candidate) {
                    still_candidate = true;
                    still_start = now;
                    printf("LYING CANDIDATE: delta=%d, stability=%d, var=%d, trend=%d\n", 
                           delta, stability, rssi_variance, rssi_trend);
                }
            }
        } else {
            stable_count = 0;
            still_candidate = false;
        }
        
        // ★ Xác nhận nằm sau 2 giây (tăng từ 1.5 lên 2)
        if (still_candidate && !is_lying_ &&
            (now - still_start) >= 2000) {
            is_lying_ = true;
            lying_start_time_ = now;
            person_present_ = true;
            if (print_mode_ & PRINT_MODE_STATE)
                ESP_LOGW("CSI", "LYING detected (delta=%d, stability=%d, var=%d)", 
                         delta, stability, rssi_variance);
        }
        
        // ★ Thoát nằm: delta < 2 HOẶC stability > 6 HOẶC rssi_variance > 4
        if (is_lying_ && (delta < 2 || stability > 6 || rssi_variance > 4) &&
            (now - lying_start_time_) >= 1000) {
            is_lying_ = false;
            person_present_ = false;
            if (print_mode_ & PRINT_MODE_STATE)
                ESP_LOGI("CSI", "Exit lying (delta=%d, stability=%d, var=%d)", 
                         delta, stability, rssi_variance);
            still_candidate = false;
            stable_count = 0;
        }
    }
    
    s_last_rssi_delta = delta;
}

// ============================================================
// processCSI - PHÁT HIỆN FALL (CẦN SHOCK MẠNH + STILL)
// ============================================================
void CsiReceiver::processCSI(int16_t *csi_raw, uint32_t seq) {
    static float baseline_amp[52] = {0};
    static bool base_ready = false;
    static int base_cnt = 0;
    static uint32_t fall_start = 0;
    static bool fall_tracking = false;
    static int shock_count = 0;

    float cur[52];
    compute_amplitudes(csi_raw, cur);

    float filtered_cur[52];
    int valid_count = 0;
    for (int i = 0; i < 52; i++) {
        if (cur[i] > 5.0f && cur[i] < 10000.0f) {
            filtered_cur[valid_count] = cur[i];
            valid_count++;
        }
    }

    if (valid_count < 15) return;

    if (!base_ready && seq < 256) {
        if (base_cnt == 0) {
            for (int i = 0; i < valid_count; i++) baseline_amp[i] = filtered_cur[i];
        } else {
            for (int i = 0; i < valid_count; i++) {
                baseline_amp[i] = (baseline_amp[i] + filtered_cur[i]) * 0.5f;
            }
        }
        base_cnt++;
        if (seq == 255) {
            ESP_LOGI("CSI", "CSI Baseline ready (valid: %d)", valid_count);
            base_ready = true;
        }
        return;
    }
    if (!base_ready) return;

    float total_change = 0;
    int change_count = 0;
    for (int i = 0; i < valid_count; i++) {
        if (baseline_amp[i] > 0.01f) {
            total_change += fabs(filtered_cur[i] - baseline_amp[i]) / baseline_amp[i];
            change_count++;
        }
    }
    if (change_count < 5) return;

    float avg_change = (total_change / change_count) * 100;
    uint32_t now = esp_timer_get_time() / 1000;

    extern int s_last_rssi_delta;
    int rssi_delta = s_last_rssi_delta;

    // ============================================================
    // ★ PHÁT HIỆN FALL: SHOCK MẠNH (delta > 10) + STILL (nằm lâu)
    // ============================================================
    // ★ TĂNG NGƯỠNG SHOCK: delta > 10 (thay vì > 5)
    bool shock = (rssi_delta > 10) && (avg_change > 10.0f);
    
    if (shock) {
        shock_count++;
        if (shock_count >= 8) {  // ~800ms
            if (!fall_tracking) {
                fall_tracking = true;
                fall_start = now;
                if (print_mode_ & PRINT_MODE_STATE)
                    printf("SHOCK detected! delta=%d, amp=%.1f%%\n", rssi_delta, avg_change);
            }
        }
    } else {
        shock_count = 0;
    }
    
    // STILL: đang nằm VÀ nằm được > 2 giây
    bool still = is_lying_ && ((now - lying_start_time_) > 2000);
    
    // FALL = SHOCK (trong 2s) + STILL (nằm lâu)
    if (fall_tracking && still && !fall_detected_) {
        if ((now - fall_start) > 2000) {
            fall_detected_ = true;
            printf("!!! FALL DETECTED !!!\n");
            fall_tracking = false;
        }
    }
    
    // Reset nếu quá lâu không có still
    if (fall_tracking && (now - fall_start) > 5000) {
        fall_tracking = false;
        shock_count = 0;
        printf("Fall tracking timeout\n");
    }

    if (fall_detected_ && (now - fall_start) > 10000) {
        fall_detected_ = false;
        printf("Fall reset\n");
    }
}

void CsiReceiver::s_process_task(void *arg) {
    auto *self = (CsiReceiver*)arg;
    csi_event_t ev;

    while (1) {
        if (uxQueueMessagesWaiting(self->event_queue_) > 800) {
            ESP_LOGW("CSI", "Queue full, flushing");
            xQueueReset(self->event_queue_);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (xQueueReceive(self->event_queue_, &ev, portMAX_DELAY) == pdTRUE) {
            self->processRSSI(ev.rssi, ev.seq);
            self->processCSI(ev.csi_raw, ev.seq);
        }
    }
}

void CsiReceiver::s_stats_task(void *arg) {
    auto *self = (CsiReceiver*)arg;
    uint32_t last = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        uint32_t now = self->pkt_count_;
        ESP_LOGI("CSI", "pkt/s: %lu, queue: %u",
                 (now - last) / 3,
                 uxQueueMessagesWaiting(self->event_queue_));
        last = now;
    }
}

void CsiReceiver::init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void CsiReceiver::init_wifi() {
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(s_promiscuous_cb);
    ESP_LOGI("CSI", "WiFi promiscuous started");
}

void CsiReceiver::begin() {
    s_instance = this;
    event_queue_ = xQueueCreate(1024, sizeof(csi_event_t));

    init_nvs();
    init_wifi();

    extern uint8_t s_tx_mac[6];
    ESP_LOGI("CSI", "Channel: %d, Offset: 34", s_channel);
    ESP_LOGI("CSI", "CsiReceiver started");

    xTaskCreatePinnedToCore(s_process_task, "csi_proc", 8192, this, 10, NULL, 0);
    xTaskCreatePinnedToCore(s_stats_task,   "csi_stat", 2048, this, 3, NULL, 1);
}

CsiReceiver::~CsiReceiver() {
    if (event_queue_) vQueueDelete(event_queue_);
}

void CsiReceiver::setPrintMode(uint8_t mode) { print_mode_ = mode; }
void CsiReceiver::setThreshold(uint8_t standing, uint8_t lying) {
    thresh_standing_ = standing;
    thresh_lying_ = lying;
}
void CsiReceiver::setLyingParams(int delta_threshold, int confirm_ms, int exit_ms) {
    lying_delta_threshold_ = delta_threshold;
    lying_confirm_ms_      = confirm_ms;
    lying_exit_ms_         = exit_ms;
}
bool CsiReceiver::isLying() { return is_lying_; }
bool CsiReceiver::isPersonPresent() { return person_present_; }
bool CsiReceiver::isFallDetected() { return fall_detected_; }
