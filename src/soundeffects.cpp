#include "soundeffects.h"

#include <algorithm>
#include <cmath>
#include <vector>

SoundEffects::~SoundEffects() {
    close();
}

bool SoundEffects::open() {
    if (device)
        return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        return false;
    audioSubsystemInitialized = true;

    SDL_AudioSpec requested{};
    requested.freq = 48000;
    requested.format = AUDIO_S16SYS;
    requested.channels = 1;
    requested.samples = 512;
    device = SDL_OpenAudioDevice(nullptr, 0, &requested, &format, 0);
    if (!device) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioSubsystemInitialized = false;
        return false;
    }

    SDL_PauseAudioDevice(device, 0);
    return true;
}

void SoundEffects::close() {
    if (device) {
        SDL_ClearQueuedAudio(device);
        SDL_CloseAudioDevice(device);
        device = 0;
    }
    if (audioSubsystemInitialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioSubsystemInitialized = false;
    }
}

void SoundEffects::setEnabled(bool value) {
    enabled = value;
    if (!enabled && device)
        SDL_ClearQueuedAudio(device);
}

void SoundEffects::setVolume(int percent) {
    volume = std::clamp(percent, 0, 100);
}

void SoundEffects::petalChanged(int petal) {
    // A gentle scale gives each petal a slightly different pitch while moving.
    constexpr int notes[] = {523, 587, 659, 698, 784, 880, 988, 1047};
    if (petal >= 0 && petal < 8)
        play(notes[petal], 22, 0.28f);
}

void SoundEffects::buttonPressed() {
    play(880, 38, 0.62f);
}

void SoundEffects::play(int frequency, int durationMs, float strength) {
    if (!device || !enabled || volume == 0)
        return;

    const int sampleRate = format.freq > 0 ? format.freq : 48000;
    const int sampleCount = sampleRate * durationMs / 1000;
    std::vector<Sint16> samples(static_cast<size_t>(sampleCount));
    constexpr double pi = 3.14159265358979323846;
    const double peak = 22000.0 * volume / 100.0 * strength;
    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const double fadeIn = std::min(1.0, static_cast<double>(i) / (sampleRate * 0.003));
        const double fadeOut = std::min(1.0, static_cast<double>(sampleCount - i) / (sampleRate * 0.009));
        const double tone = std::sin(2.0 * pi * frequency * t)
                          + 0.12 * std::sin(4.0 * pi * frequency * t);
        samples[static_cast<size_t>(i)] = static_cast<Sint16>(peak * fadeIn * fadeOut * tone);
    }

    // Replace stale movement cues if the stick crosses several petals quickly.
    constexpr Uint32 maximumQueuedMs = 90;
    const Uint32 queuedBytes = SDL_GetQueuedAudioSize(device);
    const Uint32 maximumQueuedBytes = static_cast<Uint32>(sampleRate * sizeof(Sint16) * maximumQueuedMs / 1000);
    if (queuedBytes > maximumQueuedBytes)
        SDL_ClearQueuedAudio(device);
    SDL_QueueAudio(device, samples.data(), static_cast<Uint32>(samples.size() * sizeof(Sint16)));
}
