// uart.h — R6551 Asynchronous Communications Interface Adapter emulation.
//
// Emulates one or more R6551 ACIAs. Each ACIA exposes four registers
// (RDR/TDR, Status/Reset, Command, Control), addressed by (unit, reg)
// rather than by absolute memory address, so this module is independent
// of any particular machine's memory map.
//
// Transport:
//   Incoming/outgoing bytes are bridged over TCP. Each unit listens on
//   its own port on 127.0.0.1; exactly one client can be connected per
//   unit at a time. Connect with any terminal, e.g. `nc 127.0.0.1 9000`.
//   If no client is connected, transmits complete on schedule (TDRE
//   still sets) but the bytes go nowhere, and receives never arrive.
//
// Portability:
//   The ACIA state machine (register semantics, TDRE/RDRF timing, baud
//   generation) is portable C with no platform dependencies.
//   The TCP transport is POSIX. To use a different transport or port to
//   Windows (Winsock), replace the `net_*` functions in uart.c; the
//   ACIA-facing code does not change.
//
// Clocking:
//   The R6551's register interface is clocked by the peripheral bus,
//   which is assumed to be cpu_hz / periph_div. The baud generator is
//   driven by a crystal whose frequency is implied by the control
//   register's rate selector table (see datasheet).
//
// Integration:
//   1. Call acia_init(&cfg) once at startup. Configures num_units,
//      clock rates, and TCP ports.
//   2. Call acia_tick() once per CPU cycle. Divides internally to the
//      peripheral rate.
//   3. Call acia_poll() once per main-loop iteration (or more often).
//      This does the non-blocking socket accept/recv work. Do not call
//      it from a hot loop — it's O(1) but touches syscalls.
//   4. From your memory-map handlers, call acia_read(unit, reg) and
//      acia_write(unit, reg, val). Register layout is documented below.
//   5. Call acia_shutdown() before exit to close sockets cleanly.
//
// Register layout (per unit):
//   reg 0: RDR (read)      / TDR (write)
//   reg 1: Status (read)   / Programmed Reset (write)
//   reg 2: Command (read/write)
//   reg 3: Control (read/write)
//
// Status register bits:
//   bit 7: IRQ   (never set — no interrupt line is connected)
//   bit 6: DSR   (always 0 — data set ready)
//   bit 5: DCD   (always 0 — carrier detected)
//   bit 4: TDRE  (1 = transmit register empty)
//   bit 3: RDRF  (1 = receive register full)
//   bit 2: OVRN  (1 = overrun has occurred)
//   bit 1: FE    (always 0 — no framing errors in emulation)
//   bit 0: PE    (always 0 — no parity errors in emulation)

#ifndef UART_H
#define UART_H

#include <stdint.h>

typedef struct {
    uint32_t cpu_hz;        // CPU clock in Hz, e.g. 10000000
    uint32_t periph_div;    // peripheral bus = cpu_hz / periph_div, e.g. 8
    int      num_units;     // number of ACIAs (1 or 2)
    int      port_base;     // TCP port for unit 0; unit N uses port_base + N
} acia_config_t;

void    acia_init(const acia_config_t *cfg);
void    acia_shutdown(void);
void    acia_tick(void);                 // call once per CPU cycle
void    acia_poll(void);                 // call once per main-loop iteration
uint8_t acia_read (int unit, int reg);   // unit in [0, num_units)
void    acia_write(int unit, int reg, uint8_t val);

#endif