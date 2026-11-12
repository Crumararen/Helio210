// AY3-8910.h — AY-3-8910 Programmable Sound Generator emulation.
// Based on Lunar RF Labs's open source emu8910 project which can
// be found on github at https://github.com/lunar-sh/emu8910;
// I claim no ownership over this code or general content here.
//
// Emulates the register file, three tone channels, noise generator, and
// shared envelope controller of the AY-3-8910. Produces mixed audio
// samples at a requested rate. Does not open any audio device; that's
// the caller's job. For SDL2 output, see AY3-8910_sdl.h.
//
// Clocking:
//   The chip has a native clock Φ. Internally:
//     tone/noise tick  = Φ / tone_div   (AY-3-8910: 16)
//     envelope tick    = Φ / env_div    (AY-3-8910: 256)
//   Call psg_tick() once per CPU cycle; internally it 
//   divides by chip_div to derive Φ. Set chip_div = 0 
//   for a 1:1 ratio (chip clocked at CPU speed).
//
// Audio output:
//   The core produces int16 samples at sample_rate. Callers pull them
//   with psg_render(). Samples are internally decimated from Φ; the
//   ring buffer absorbs jitter between the CPU-rate tick and the
//   audio-rate pull. psg_render() returns the number of samples
//   actually written (which may be less than requested if the buffer
//   underruns; the caller should zero-fill the remainder).
//
// Portability:
//   Pure C, no platform dependencies. Includes only <stdint.h> in the
//   header and <stdint.h>, <stdbool.h>, <string.h> in the .c. Can be
//   dropped into any emulator or audio application. For SDL output,
//   link AY3-8910_sdl.c on top.
//
// Integration:
//   1. Call psg_init(&cfg) once at startup.
//   2. Call psg_tick() once per CPU cycle (or at the CPU's cycle rate).
//   3. Call psg_render(buf, n) at your audio callback's rate. The core
//      handles decimation internally; you don't need to know Φ.
//   4. From your memory-map handlers, call psg_read(reg) and
//      psg_write(reg, val). reg is 0..15.
//   5. Call psg_reset() to silence the chip without re-initializing
//      the config or ring buffer.
//
// Register layout (see datasheet for full semantics):
//   R0/R1   Channel A tone period (12-bit, lo/hi)
//   R2/R3   Channel B tone period
//   R4/R5   Channel C tone period
//   R6      Noise period (5-bit)
//   R7      Mixer (active-low: 0 = enabled)
//   R8/R9/R10 Channel A/B/C volume (bit 4 = envelope mode)
//   R11/R12 Envelope period (16-bit, lo/hi)
//   R13     Envelope shape (attack, alternate, continue, hold)
//   R14/R15 I/O ports (unused in this emulation)

#ifndef AY3_8910_H
#define AY3_8910_H

#include <stdint.h>

typedef struct {
    uint32_t phi_hz;        // chip native clock, e.g. 1250000
    uint32_t chip_div;      // CPU cycles per Φ tick; 0 or 1 = direct
    uint32_t sample_rate;   // audio output rate, e.g. 48000
    uint32_t tone_div;      // tone tick divisor; 0 = default 16
    uint32_t env_div;       // envelope tick divisor; 0 = default 256
} psg_config_t;

void    psg_init(const psg_config_t *cfg);
void    psg_reset(void);
void    psg_tick(void);                              // once per CPU cycle
uint8_t psg_read(int reg);
void    psg_write(int reg, uint8_t val);
int     psg_render(int16_t *buf, int samples);       // returns written
int     psg_sample(void);                            // current mixed value

#endif