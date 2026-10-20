#pragma once

#include <SDL.h>

class SoundEffects {
public:
    SoundEffects() = default;
    ~SoundEffects();

    SoundEffects(const SoundEffects &) = delete;
    SoundEffects &operator=(const SoundEffects &) = delete;

    bool open();
    void close();
    void setEnabled(bool enabled);
    void setVolume(int percent);
    void petalChanged(int petal);
    void buttonPressed();

private:
    void play(int frequency, int durationMs, float strength);

    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec format{};
    int volume = 35;
    bool enabled = true;
    bool audioSubsystemInitialized = false;
};
