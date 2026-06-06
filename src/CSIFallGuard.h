#ifndef CSIFALLGUARD_H
#define CSIFALLGUARD_H

#include <stdint.h>
#include <stdbool.h>

// ===== CHẾ ĐỘ IN (bitmask) =====
#define PRINT_MODE_NONE      0
#define PRINT_MODE_RSSI      1
#define PRINT_MODE_STATE     2
#define PRINT_MODE_RAW       4
#define PRINT_MODE_AMPLITUDE 8

#ifdef __cplusplus
extern "C" {
#endif

#define MODE_TX   0
#define MODE_RX   1

// ===== CẤU HÌNH MẠNG (gọi TRƯỚC begin) =====
void CSIFallGuard_set_channel(int channel);
void CSIFallGuard_set_tx_mac(const uint8_t *mac);
void CSIFallGuard_set_rx_mac(const uint8_t *mac);

// ===== KHỞI TẠO =====
void CSIFallGuard_begin(int mode, int channel);
void CSIFallGuard_update(void);

// ===== TX APIs =====
void CSIFallGuard_set_tx_power(int power);
void CSIFallGuard_set_tx_frequency(int hz);
uint32_t CSIFallGuard_get_tx_packet_count(void);

// ===== RX APIs (cơ bản) =====
void CSIFallGuard_set_rx_threshold(int standing, int lying);
void CSIFallGuard_register_fall_callback(void (*callback)(void));
void CSIFallGuard_register_person_callback(void (*callback)(bool present));
int  CSIFallGuard_get_rssi(void);
int  CSIFallGuard_get_delta(void);
bool CSIFallGuard_is_person(void);
bool CSIFallGuard_is_fall(void);

// ===== RX APIs (nâng cao) =====
void CSIFallGuard_set_print_mode(uint8_t mode);
void CSIFallGuard_set_lying_params(int delta_threshold, int confirm_ms, int exit_ms);
bool CSIFallGuard_is_lying(void);

#ifdef __cplusplus
}
#endif

#endif
