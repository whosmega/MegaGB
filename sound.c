#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <stdbool.h>

void audioCallback(void* userData, uint8_t* stream, int len) {
    int* samplesPlayed = (int*)userData;
    float* fstream = (float*)stream;
    int numSamples = len / (sizeof(float) * 2);         /* 2 channels */
    /* We fill the whole stream buffer in one go, sampling rate is 44.1 kHz
     * stream buffer contains len no of samples, len can vary and it represents the number of samples 
     * SDL2 needs, it does not necessary have to be the size of the full audio buffer */

    // 440 Hz square wave
    for (int i = 0; i < numSamples; i++) {
        float amp = ((*samplesPlayed + i) % 200) > 100 ? -.5 : .5;

        fstream[2*i] = amp;
        fstream[2*i+1] = fstream[2*i];
    }

    *samplesPlayed += numSamples;
}

int main() {
    int samplesPlayed = 0;

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL Audio", 
            SDL_WINDOWPOS_CENTERED, 
            SDL_WINDOWPOS_CENTERED,
            600, 600, SDL_WINDOW_SHOWN);

    if (!window) {
        SDL_Quit();
        return -3;
    }

    SDL_AudioSpec want, spec;
    memset(&want, 0, sizeof(want));

    want.freq = 44000;              // sampling rate
    want.format = AUDIO_F32SYS;     // 32 bit float
    want.channels = 2;
    want.samples = 512;             // buffer size
    want.callback = audioCallback;
    want.userdata = (void*)&samplesPlayed;

    SDL_AudioDeviceID id = SDL_OpenAudioDevice(NULL, 0, &want, &spec, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (!id) {
        SDL_Quit();
        return -2;
    }

    printf("Audio device successfully opened\n");
    SDL_PauseAudioDevice(id, 0);
    
    bool run = true;

    while (run) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) run = false;
        }
    }

    SDL_DestroyWindow(window);
    SDL_CloseAudioDevice(id);
    SDL_Quit();
    return 0;
}
