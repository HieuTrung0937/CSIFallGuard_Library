#include "CsiReceiver.hpp"
#include <string.h>
#include <math.h>
#include <stdlib.h>

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

// ============================================================
// SO SÁNH FLOAT CHO QSORT
// ============================================================
static int compare_float(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

// ============================================================
// PROMISCUOUS CALLBACK (ISR) - GIẢI MÃ CSI
// ============================================================
void IRAM_ATTR CsiReceiver::s_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_instance) return;

    auto *pkt = (wifi_promiscuous_pkt_t*)buf;
    auto *hdr = (wifi_80211_hdr_t*)pkt->payload;

    // Lọc MAC TX
    extern uint8_t s_tx_mac[6];
    if (memcmp(hdr->addr2, s_tx_mac, 6) != 0) return;

    uint32_t seq = s_instance->pkt_count_ + 1;
    s_instance->pkt_count_ = seq;

    // ===== LẤY CSI RAW - CÓ THỂ ĐIỀU CHỈNH OFFSET =====
    #define CSI_OFFSET 34  // Thử 32, 34, 36
    int16_t *csi_data = (int16_t*)(pkt->payload + CSI_OFFSET);
    
    // Lưu vào event
    csi_event_t ev;
    ev.seq = seq;
    ev.rssi = pkt->rx_ctrl.rssi;
    ev.sig_len = pkt->rx_ctrl.sig_len;
    memcpy(ev.csi_raw, csi_data, 104 * sizeof(int16_t));

    // Debug: in 10 subcarrier đầu mỗi 100 gói
    static uint32_t last_print = 0;
    if (seq - last_print > 100) {
        last_print = seq;
        printf("CSI[%lu]: ", seq);
        for (int i = 0; i < 10; i++) {
            printf("(%d,%d) ", ev.csi_raw[i*2], ev.csi_raw[i*2+1]);
        }
        printf("\n");
    }

    BaseType_t wake = pdFALSE;
    xQueueSendFromISR(s_instance->event_queue_, &ev, &wake);
    if (wake) portYIELD_FROM_ISR();
}

// ============================================================
// TÍNH AMPLITUDE 52 SUBCARRIER
// ============================================================
static inline void compute_amplitudes(int16_t *csi_raw, float *amplitudes) {
    for (int i = 0; i < 52; i++) {
        int16_t I = csi_raw[i * 2];
        int16_t Q = csi_raw[i * 2 + 1];
        amplitudes[i] = sqrtf((float)(I * I + Q * Q));
    }
}

// ============================================================
// LỌC TRUNG VỊ: CHỌN 30 SUBCARRIER Ở GIỮA
// ============================================================
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
// RSSI PROCESSING
// ============================================================
void CsiReceiver::processRSSI(int rssi, uint32_t seq) {
    static int win[8] = {0};
    static int idx = 0, filled = 0;
    static int baseline = 0, base_cnt = 0;
    static bool base_ready = false;

    win[idx++] = rssi;
    if (idx >= 8) { idx = 0; filled = 1; }
    int n = filled ? 8 : idx;
    int sum = 0;
    for (int i = 0; i < n; i++) sum += win[i];
    int filtered = sum / n;

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

    printf("RSSI,%lu,%d,%d,%d,%d,%.1f\n", 
           (unsigned long)seq, rssi, filtered, delta, 
           person_present_ ? 1 : 0, 0.0f);

    static bool lying_candidate = false;
    static uint32_t candidate_start = 0;

    if (delta >= lying_delta_threshold_) {
        if (!lying_candidate) {
            lying_candidate = true;
            candidate_start = now;
        }
    } else {
        lying_candidate = false;
    }

    if (lying_candidate && !is_lying_ &&
        (now - candidate_start) >= (uint32_t)lying_confirm_ms_) {
        is_lying_ = true;
        lying_start_time_ = now;
        person_present_ = true;
        if (print_mode_ & PRINT_MODE_STATE)
            ESP_LOGW("CSI", "Lying confirmed (delta=%d dB)", delta);
    }

    if (is_lying_ && delta < (lying_delta_threshold_ / 2) &&
        (now - lying_start_time_) >= (uint32_t)lying_exit_ms_) {
        is_lying_ = false;
        person_present_ = false;
        if (print_mode_ & PRINT_MODE_STATE)
            ESP_LOGI("CSI", "Exit lying state");
    }
}

// ============================================================
// CSI PROCESSING - PHÁT HIỆN TÉ NGÃ (CÓ LỌC TRUNG VỊ)
// ============================================================
void CsiReceiver::processCSI(int16_t *csi_raw, uint32_t seq) {
    static float baseline_amp[30] = {0};  // 30 subcarrier tốt nhất
    static bool base_ready = false;
    static int base_cnt = 0;
    static uint32_t shock_time = 0;
    static bool shock_detected = false;

    // Bước 1: Tính amplitude
    float cur[52];
    compute_amplitudes(csi_raw, cur);

    // Bước 2: Lọc trung vị - giữ 30 subcarrier ở giữa
    float filtered_cur[30];
    int valid_count = median_filter(cur, filtered_cur, 52, 30);

    // Debug valid_count
    if (seq % 100 == 0) {
        printf("valid_count: %d\n", valid_count);
    }

    // Bước 3: Capture baseline (chỉ dùng 30 subcarrier tốt nhất)
    if (!base_ready && seq < 256) {
        if (base_cnt == 0) {
            for (int i = 0; i < valid_count; i++) {
                baseline_amp[i] = filtered_cur[i];
            }
        } else {
            for (int i = 0; i < valid_count; i++) {
                baseline_amp[i] = (baseline_amp[i] + filtered_cur[i]) * 0.5f;
            }
        }
        base_cnt++;
        if (seq == 255 && (print_mode_ & PRINT_MODE_STATE)) {
            ESP_LOGI("CSI", "CSI baseline ready (valid: %d)", valid_count);
            base_ready = true;
        }
        return;
    }
    if (!base_ready) return;

    // Bước 4: Tính % thay đổi
    float total_change = 0;
    int change_count = 0;
    for (int i = 0; i < valid_count; i++) {
        if (baseline_amp[i] > 0.01f) {
            float diff = fabs(filtered_cur[i] - baseline_amp[i]);
            total_change += diff / baseline_amp[i];
            change_count++;
        }
    }

    if (change_count < 10) return;

    float avg_change = (total_change / change_count) * 100;
    uint32_t now = esp_timer_get_time() / 1000;

    // In thông tin định kỳ
    if (seq % 20 == 0) {
        printf("Amp change: %.1f%% (valid: %d)\n", avg_change, change_count);
    }

    // Bước 5: SHOCK (ngưỡng 35%)
    if (avg_change > 35.0f && !shock_detected) {
        shock_detected = true;
        shock_time = now;
        if (print_mode_ & PRINT_MODE_STATE)
            ESP_LOGW("CSI", "SHOCK! change=%.1f%%", avg_change);
    }

    // Bước 6: STILL (sau shock, ổn định < 12%)
    if (shock_detected && (now - shock_time) > 2000) {
        if (avg_change < 12.0f) {
            fall_detected_ = true;
            if (print_mode_ & PRINT_MODE_STATE)
                ESP_LOGW("CSI", "!!! FALL DETECTED !!!");
        }
        shock_detected = false;
    }
}

// ============================================================
// MAIN TASK
// ============================================================
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

// ============================================================
// STATS TASK
// ============================================================
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

// ============================================================
// INIT
// ============================================================
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
    ESP_LOGI("CSI", "TX MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             s_tx_mac[0], s_tx_mac[1], s_tx_mac[2],
             s_tx_mac[3], s_tx_mac[4], s_tx_mac[5]);
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
