// uart_stub.c — stub ACIA for builds without networking.
// From the emulator's perspective, if compiled with this, 
// the ACIA is still installed; the serial ports are simply
// broken, cut, or generally unplugged.
#include "uart.h"

void acia_init(const acia_config_t *cfg) { (void)cfg; }
void acia_tick(void)     {}
void acia_poll(void)     {}
void acia_shutdown(void) {}

uint8_t acia_read(int unit, int reg) {
    (void)unit;
    return (reg == 1) ? 0x10 : 0x00;   // status: TDRE set, all else clear
}

void acia_write(int unit, int reg, uint8_t val) {
    (void)unit; (void)reg; (void)val;  // writes go nowhere
}