#ifndef CSIFALLGUARD_H
#define CSIFALLGUARD_H

#ifdef __cplusplus
extern "C" {
#endif

#define MODE_TX   0
#define MODE_RX   1

void CSIFallGuard_begin(int mode, int channel);
void CSIFallGuard_update(void);

#ifdef __cplusplus
}
#endif

#endif