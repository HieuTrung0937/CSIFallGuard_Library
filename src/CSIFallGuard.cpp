#include "CSIFallGuard.h"
#include "EspNowTransmitter.hpp"
#include "CsiReceiver.hpp"

static int s_mode = -1;

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
    // Các task chạy ngầm
}