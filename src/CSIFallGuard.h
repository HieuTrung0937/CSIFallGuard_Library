#ifndef CSIFALLGUARD_H
#define CSIFALLGUARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODE_TX   0
#define MODE_RX   1

// Khởi tạo
void CSIFallGuard_begin(int mode, int channel);
void CSIFallGuard_update(void);

// Cấu hình MAC (bắt buộc gọi trước begin)
void CSIFallGuard_set_tx_mac(const uint8_t *mac);
void CSIFallGuard_set_rx_mac(const uint8_t *mac);

// TX APIs
void CSIFallGuard_set_tx_power(int power);
void CSIFallGuard_set_tx_frequency(int hz);
uint32_t CSIFallGuard_get_tx_packet_count(void);

// RX APIs
void CSIFallGuard_set_rx_threshold(int standing, int lying);
void CSIFallGuard_register_fall_callback(void (*callback)(void));
void CSIFallGuard_register_person_callback(void (*callback)(bool present));
int CSIFallGuard_get_rssi(void);
int CSIFallGuard_get_delta(void);
bool CSIFallGuard_is_person(void);
bool CSIFallGuard_is_fall(void);

#ifdef __cplusplus
}
#endif

#endif
