// AY3-8910_sdl.c — SDL2 audio wrapper. See AY3-8910_sdl.h.

#include "AY3-8910_sdl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static SDL_AudioDeviceID audio_dev = 0;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int samples = len / (int)sizeof(int16_t);
    int n = psg_render((int16_t *)stream, samples);
    if (n < samples) {
        memset(stream + n * sizeof(int16_t), 0,
               (samples - n) * sizeof(int16_t));
    }
}

int psg_sdl_open(const psg_config_t *cfg) {
    psg_init(cfg);

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_InitSubSystem(AUDIO): %s\n", SDL_GetError());
        return -1;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq     = (int)cfg->sample_rate;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 1024;
    want.callback = audio_callback;

    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return -1;
    }
    SDL_PauseAudioDevice(audio_dev, 0);
    return 0;
}

void psg_sdl_close(void) {
    if (audio_dev) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
    }
}