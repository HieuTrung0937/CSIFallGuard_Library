#include "CSIFallGuard.h"
#include "EspNowTransmitter.hpp"
#include "CsiReceiver.hpp"
#include <string.h>

static int s_mode = -1;
static uint8_t s_tx_mac[6];
static uint8_t s_rx_mac[6];
static bool s_tx_mac_set = false;
static bool s_rx_mac_set = false;

// ===== MAC CONFIG =====
void CSIFallGuard_set_tx_mac(const uint8_t *mac) {
    memcpy(s_tx_mac, mac, 6);
    s_tx_mac_set = true;
}

void CSIFallGuard_set_rx_mac(const uint8_t *mac) {
    memcpy(s_rx_mac, mac, 6);
    s_rx_mac_set = true;
}

// ===== MAIN =====
void CSIFallGuard_begin(int mode, int channel) {
    s_mode = mode;
    
    if (mode == MODE_TX) {
        EspNowTransmitter tx;
        tx.begin();
    } else {
        CsiReceiver rx;
        rx.begin();
    }
}

void CSIFallGuard_update(void) {
    // Tasks run in background
}

// ===== TX APIs =====
void CSIFallGuard_set_tx_power(int power) {
    // TODO: implement
}

void CSIFallGuard_set_tx_frequency(int hz) {
    // TODO: implement
}

uint32_t CSIFallGuard_get_tx_packet_count(void) {
    return 0;
}

// ===== RX APIs =====
void CSIFallGuard_set_rx_threshold(int standing, int lying) {}
void CSIFallGuard_register_fall_callback(void (*callback)(void)) {}
void CSIFallGuard_register_person_callback(void (*callback)(bool present)) {}
int CSIFallGuard_get_rssi(void) { return 0; }
int CSIFallGuard_get_delta(void) { return 0; }
bool CSIFallGuard_is_person(void) { return false; }
bool CSIFallGuard_is_fall(void) { return false; }
