// AY3-8910.c — AY-3-8910 PSG emulation core. See AY3-8910.h.
// Based on Lunar RF Labs's emu8910 (https://github.com/lunar-sh/emu8910).

#include "AY3-8910.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define AUDIO_BUF_SAMPLES 4096

typedef struct {
    psg_config_t cfg;

    uint8_t reg[16];
    uint8_t reg_live[16];

    uint16_t tone_cnt[3];
    uint8_t  tone_out[3];

    uint16_t noise_cnt;
    uint32_t lfsr;
    uint8_t  noise_out;

    uint16_t env_cnt;
    uint8_t  env_phase;
    uint8_t  env_out;
    uint8_t  env_attack;
    bool     env_holding;

    uint32_t cpu_div;
    uint32_t tone_div_ctr;
    uint32_t env_div_ctr;
    uint32_t sample_accum;

    int16_t  audio_buf[AUDIO_BUF_SAMPLES];
    int      audio_head;
    int      audio_tail;
} PSG;

static PSG psg;

static inline uint16_t period12(uint8_t lo, uint8_t hi) {
    uint16_t p = lo | (hi << 8);
    return p ? p : 1;
}

static void psg_tone_tick(void) {
    for (int ch = 0; ch < 3; ch++) {
        uint16_t p = period12(psg.reg_live[ch*2], psg.reg_live[ch*2+1]);
        if (p == 1) {
            psg.tone_out[ch] ^= 1;
        } else {
            psg.tone_cnt[ch]++;
            if (psg.tone_cnt[ch] >= p) psg.tone_cnt[ch] = 0;
            psg.tone_out[ch] = (psg.tone_cnt[ch] >= (p >> 1)) ? 1 : 0;
        }
    }
}

static void psg_noise_tick(void) {
    uint16_t np = psg.reg_live[6] & 0x1F;
    if (np == 0) np = 1;
    if (psg.noise_cnt == 0) {
        psg.noise_cnt = np;
        uint32_t fb = (psg.lfsr ^ (psg.lfsr >> 3)) & 1;
        psg.lfsr = (psg.lfsr >> 1) | (fb << 16);
    } else {
        psg.noise_cnt--;
    }
    psg.noise_out = psg.lfsr & 1;
}

static void psg_env_step(void) {
    if (psg.env_holding) return;
    uint8_t shape = psg.reg_live[13] & 0x0F;
    psg.env_phase++;
    if (psg.env_phase >= 16) {
        psg.env_phase = 0;
        if (shape & 0x08) { psg.env_holding = 1; psg.env_phase = 15; }
        else if (shape & 0x04) { if (shape & 0x02) psg.env_attack ^= 1; }
        else { psg.env_holding = 1; psg.env_phase = 15; }
    }
    psg.env_out = psg.env_attack ? psg.env_phase : (15 - psg.env_phase);
}

static void psg_envelope_tick(void) {
    uint16_t ep = psg.reg_live[11] | (psg.reg_live[12] << 8);
    if (ep == 0) ep = 1;
    if (psg.env_cnt == 0) { psg.env_cnt = ep; psg_env_step(); }
    else psg.env_cnt--;
}

static int psg_mix(void) {
    uint8_t mixer = psg.reg_live[7];
    int sum = 0;
    for (int ch = 0; ch < 3; ch++) {
        int tone_off  = (mixer >> ch)       & 1;
        int noise_off = (mixer >> (ch + 3)) & 1;
        int tone_contrib  = tone_off  ? 1 : psg.tone_out[ch];
        int noise_contrib = noise_off ? 1 : psg.noise_out;
        int mixed = tone_contrib & noise_contrib;
        uint8_t vol_reg = psg.reg_live[8 + ch];
        int volume = (vol_reg & 0x10) ? psg.env_out : (vol_reg & 0x0F);
        sum += mixed ? volume : 0;
    }
    return sum;   // 0..45
}

void psg_init(const psg_config_t *c) {
    memset(&psg, 0, sizeof(psg));
    psg.cfg = *c;
    if (psg.cfg.tone_div == 0) psg.cfg.tone_div = 16;
    if (psg.cfg.env_div  == 0) psg.cfg.env_div  = 256;
    if (psg.cfg.chip_div == 0) psg.cfg.chip_div = 1;
    if (psg.cfg.sample_rate == 0) psg.cfg.sample_rate = 48000;
    psg.lfsr = 0x1FFFF;
    psg.env_attack = 1;
}

void psg_reset(void) {
    psg_config_t saved = psg.cfg;
    memset(&psg, 0, sizeof(psg));
    psg.cfg = saved;
    psg.lfsr = 0x1FFFF;
    psg.env_attack = 1;
}

void psg_tick(void) {
    if (psg.cfg.chip_div > 1) {
        if (++psg.cpu_div < psg.cfg.chip_div) return;
        psg.cpu_div = 0;
    }

    // Φ tick
    for (int i = 0; i < 16; i++) psg.reg_live[i] = psg.reg[i];

    if (++psg.tone_div_ctr >= psg.cfg.tone_div) {
        psg.tone_div_ctr = 0;
        psg_tone_tick();
        psg_noise_tick();
    }
    if (++psg.env_div_ctr >= psg.cfg.env_div) {
        psg.env_div_ctr = 0;
        psg_envelope_tick();
    }

    // Audio decimation
    psg.sample_accum += psg.cfg.sample_rate;
    if (psg.sample_accum >= psg.cfg.phi_hz) {
        psg.sample_accum -= psg.cfg.phi_hz;
        int next = (psg.audio_tail + 1) % AUDIO_BUF_SAMPLES;
        if (next != psg.audio_head) {
            int raw = psg_mix();
            int scaled = (raw * 32767) / 45;
            psg.audio_buf[psg.audio_tail] = (int16_t)scaled;
            psg.audio_tail = next;
        }
    }
}

uint8_t psg_read(int reg) {
    if (reg < 0 || reg > 15) return 0;
    return psg.reg[reg];
}

void psg_write(int reg, uint8_t val) {
    if (reg < 0 || reg > 15) return;
    psg.reg[reg] = val;
    if (reg == 13) {
        psg.env_phase   = 0;
        psg.env_holding = false;
        psg.env_attack  = (val & 0x01) ? 1 : 0;
        psg.env_out     = psg.env_attack ? 0 : 15;
    }
}

int psg_render(int16_t *buf, int samples) {
    int n = 0;
    while (n < samples && psg.audio_head != psg.audio_tail) {
        buf[n++] = psg.audio_buf[psg.audio_head];
        psg.audio_head = (psg.audio_head + 1) % AUDIO_BUF_SAMPLES;
    }
    return n;
}

int psg_sample(void) {
    return psg_mix();
}