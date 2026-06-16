#include "CSIFallGuard.h"
#include "rx/CsiReceiver.hpp"
#include "tx/EspNowTransmitter.hpp"
#include <string.h>

// ===== ĐỊNH NGHĨA BIẾN TOÀN CỤC (KHÔNG static) =====
uint8_t s_tx_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t s_rx_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int s_channel = 1;

static int s_mode = -1;
static EspNowTransmitter s_tx;
static CsiReceiver s_rx;

// ===== CẤU HÌNH MẠNG =====
void CSIFallGuard_set_channel(int channel) {
    s_channel = channel;
}

void CSIFallGuard_set_tx_mac(const uint8_t *mac) {
    memcpy(s_tx_mac, mac, 6);
}

void CSIFallGuard_set_rx_mac(const uint8_t *mac) {
    memcpy(s_rx_mac, mac, 6);
}

// ===== KHỞI TẠO =====
void CSIFallGuard_begin(int mode, int channel) {
    s_mode = mode;
    s_channel = channel;
    
    if (mode == MODE_TX) {
        s_tx.begin();
    } else {
        s_rx.begin();
    }
}

void CSIFallGuard_update(void) {}

// ===== TX APIs =====
void CSIFallGuard_set_tx_power(int power) {}
void CSIFallGuard_set_tx_frequency(int hz) {}
uint32_t CSIFallGuard_get_tx_packet_count(void) { return 0; }

// ===== RX APIs =====
void CSIFallGuard_set_rx_threshold(int standing, int lying) {
    s_rx.setThreshold(standing, lying);
}

void CSIFallGuard_register_fall_callback(void (*cb)(void)) {}
void CSIFallGuard_register_person_callback(void (*cb)(bool)) {}

int CSIFallGuard_get_rssi(void) { return 0; }
int CSIFallGuard_get_delta(void) { return 0; }
bool CSIFallGuard_is_person(void) { return s_rx.isPersonPresent(); }
bool CSIFallGuard_is_fall(void) { return s_rx.isFallDetected(); }

// ===== HÀM MỚI =====
void CSIFallGuard_set_print_mode(uint8_t mode) {
    s_rx.setPrintMode(mode);
}

void CSIFallGuard_set_lying_params(int delta, int confirm_ms, int exit_ms) {
    s_rx.setLyingParams(delta, confirm_ms, exit_ms);
}

bool CSIFallGuard_is_lying(void) {
    return s_rx.isLying();
}
