#include "CsiReceiver.hpp"
#include <string.h>

CsiReceiver *CsiReceiver::s_instance = nullptr;

// ========== ĐỊNH NGHĨA HEADER 802.11 (bắt buộc) ==========
typedef struct {
    uint16_t frame_ctrl;
    uint16_t duration;
    uint8_t  addr1[6];
    uint8_t  addr2[6];
    uint8_t  addr3[6];
} __attribute__((packed)) wifi_80211_hdr_t;
// ========================================================

static inline float amplitude(int16_t I, int16_t Q) {
    return sqrtf((float)(I * I + Q * Q));
}

// ============================================================
// PROMISCUOUS CALLBACK (ISR)
// ============================================================
void IRAM_ATTR CsiReceiver::s_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_instance) return;

    auto *pkt = (wifi_promiscuous_pkt_t*)buf;
    auto *hdr = (wifi_80211_hdr_t*)pkt->payload;

    // IN MAC của tất cả gói nhận được (để debug)
    ESP_EARLY_LOGI("CSI", "RXed MAC: %02X:%02X:%02X:%02X:%02X:%02X | RSSI=%d",
                   hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
                   hdr->addr2[3], hdr->addr2[4], hdr->addr2[5],
                   pkt->rx_ctrl.rssi);

    extern uint8_t s_tx_mac[6];
    
    // So sánh với MAC của TX
    if (memcmp(hdr->addr2, s_tx_mac, 6) != 0) return;

    csi_event_t ev;
    ev.seq = s_instance->pkt_count_ + 1;
    ev.rssi = pkt->rx_ctrl.rssi;
    ev.sig_len = pkt->rx_ctrl.sig_len;
    memset(ev.csi_raw, 0, sizeof(ev.csi_raw));

    uint32_t tmp = s_instance->pkt_count_;
    tmp++;
    s_instance->pkt_count_ = tmp;

    BaseType_t wake = pdFALSE;
    xQueueSendFromISR(s_instance->event_queue_, &ev, &wake);
    if (wake) portYIELD_FROM_ISR();
}

// ============================================================
// RSSI PROCESSING (lying detection)
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

    // ===== IN DỮ LIỆU CSV CHO PYTHON (KHÔNG ĐIỀU KIỆN) =====
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
// CSI RAW PROCESSING (fall detection)
// ============================================================
void CsiReceiver::processCSI(int16_t *csi_raw, uint32_t seq) {
    static float baseline[52] = {0};
    static bool base_ready = false;
    static int base_cnt = 0;
    static uint32_t fall_printed = 0;

    if (!base_ready && seq < 256) {
        float cur[52];
        for (int i = 0; i < 52; i++)
            cur[i] = amplitude(csi_raw[i*2], csi_raw[i*2+1]);
        if (base_cnt == 0) memcpy(baseline, cur, sizeof(baseline));
        else {
            for (int i = 0; i < 52; i++)
                baseline[i] = (baseline[i] + cur[i]) * 0.5f;
        }
        base_cnt++;
        if (seq == 255 && (print_mode_ & PRINT_MODE_STATE))
            ESP_LOGI("CSI", "Amplitude baseline ready");
        return;
    }
    if (!base_ready) return;

    float cur[52];
    float change = 0;
    for (int i = 0; i < 52; i++) {
        cur[i] = amplitude(csi_raw[i*2], csi_raw[i*2+1]);
        change += fabs(cur[i] - baseline[i]);
    }
    change = change / 52;

    if ((print_mode_ & PRINT_MODE_AMPLITUDE) && (seq % 20 == 0)) {
        printf("Amplitude change: %.1f%%\n", change);
    }

    if (!fall_detected_ && change > thresh_lying_ * 2.0f) {
        fall_detected_ = true;
        fall_printed = seq;
        if (print_mode_ & PRINT_MODE_STATE)
            ESP_LOGW("CSI", "Fall detected (amp change %.1f%%)", change);
    }

    if (fall_detected_ && (seq - fall_printed) > 200) {
        fall_detected_ = false;
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
            ESP_LOGW("CSI", "Queue nearly full, flushing old data");
            xQueueReset(self->event_queue_);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (xQueueReceive(self->event_queue_, &ev, portMAX_DELAY) == pdTRUE) {
            self->processRSSI(ev.rssi, ev.seq);
            self->processCSI(ev.csi_raw, ev.seq);

            if ((self->print_mode_ & PRINT_MODE_RAW) && (ev.seq % 100 == 0)) {
                printf("Raw: seq=%lu, rssi=%d\n", ev.seq, ev.rssi);
            }
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
// INITIALISATION
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

// ============================================================
// PUBLIC API
// ============================================================
void CsiReceiver::begin() {
    s_instance = this;
    event_queue_ = xQueueCreate(1024, sizeof(csi_event_t));

    init_nvs();
    init_wifi();

    ESP_LOGI("CSI", "CsiReceiver started");
    xTaskCreatePinnedToCore(s_process_task, "csi_proc", 8192, this, 10, NULL, 0);
    xTaskCreatePinnedToCore(s_stats_task,   "csi_stat", 2048, this, 3, NULL, 1);
}

CsiReceiver::~CsiReceiver() {
    if (event_queue_) vQueueDelete(event_queue_);
}

void CsiReceiver::setPrintMode(uint8_t mode) {
    print_mode_ = mode;
}

void CsiReceiver::setThreshold(uint8_t standing, uint8_t lying) {
    thresh_standing_ = standing;
    thresh_lying_ = lying;
}

void CsiReceiver::setLyingParams(int delta_threshold, int confirm_ms, int exit_ms) {
    lying_delta_threshold_ = delta_threshold;
    lying_confirm_ms_      = confirm_ms;
    lying_exit_ms_         = exit_ms;
    ESP_LOGI("CSI", "Lying params: delta=%d, confirm=%d, exit=%d",
             delta_threshold, confirm_ms, exit_ms);
}

bool CsiReceiver::isLying() {
    return is_lying_;
}

bool CsiReceiver::isPersonPresent() {
    return person_present_;
}

bool CsiReceiver::isFallDetected() {
    return fall_detected_;
}
