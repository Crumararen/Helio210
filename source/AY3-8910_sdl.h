// AY3-8910_sdl.h — SDL2 audio wrapper for AY3-8910.h.
//
// Opens an SDL audio device at the sample_rate specified in the
// psg_config_t passed to psg_sdl_open(), starts a callback that pulls
// samples via psg_render(), and closes cleanly on psg_sdl_close().
//
// The PSG core (psg_tick, psg_render, psg_read/write) is unchanged;
// this file only owns the audio device lifecycle. Emulators that don't
// use SDL should not link this file.

#ifndef AY3_8910_SDL_H
#define AY3_8910_SDL_H

#include "AY3-8910.h"

int  psg_sdl_open(const psg_config_t *cfg);   // returns 0 on success
void psg_sdl_close(void);

#endif