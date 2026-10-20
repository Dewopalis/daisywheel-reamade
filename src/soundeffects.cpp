#include "soundeffects.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;

double midiNote(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

double envelope(double time, double decaySeconds, double attackSeconds = 0.004) {
    const double attack = std::min(1.0, time / attackSeconds);
    return attack * std::exp(-time / decaySeconds);
}
}

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

void SoundEffects::petalChanged(int) {
    if (!device || !enabled || volume == 0)
        return;

    // Use one shared MIDI-pitched cue for petal movement: D4 and Ab4 form the
    // bright ring, with a quiet A2 body under the long tail.
    const int sampleRate = format.freq > 0 ? format.freq : 48000;
    constexpr double duration = 0.62;
    const int count = static_cast<int>(sampleRate * duration);
    std::vector<float> samples(static_cast<size_t>(count), 0.0f);
    const double d4 = midiNote(62);
    const double ab4 = midiNote(68);
    const double a2 = midiNote(45);
    // Keep these cues in the background: the original synth mix was much
    // louder than a subtle navigation hint at mid-slider settings.
    const double scale = 0.045 * volume / 100.0;

    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        // The upper modes decay quickly, leaving a soft low ringing tail.
        double value = 0.0;
        value += 0.52 * envelope(t, 0.18) * std::sin(2.0 * pi * d4 * t);
        value += 0.20 * envelope(t, 0.12) * std::sin(2.0 * pi * (d4 * 2.01) * t);
        value += 0.32 * envelope(t, 0.16) * std::sin(2.0 * pi * ab4 * t + 0.3);
        value += 0.12 * envelope(t, 0.10) * std::sin(2.0 * pi * (ab4 * 2.0) * t);
        value += 0.17 * envelope(t, 0.42, 0.012) * std::sin(2.0 * pi * a2 * t);
        value += 0.08 * envelope(t, 0.36, 0.012) * std::sin(2.0 * pi * (a2 * 1.5) * t + 0.5);
        samples[static_cast<size_t>(i)] = static_cast<float>(value * scale);
    }
    queue(samples.data(), count);
}

void SoundEffects::buttonPressed() {
    if (!device || !enabled || volume == 0)
        return;

    // A short, rounded MIDI-style pluck keeps button actions distinct from the
    // longer petal ring without introducing a sharp electronic beep.
    const int sampleRate = format.freq > 0 ? format.freq : 48000;
    constexpr double duration = 0.085;
    const int count = static_cast<int>(sampleRate * duration);
    std::vector<float> samples(static_cast<size_t>(count));
    const double note = midiNote(74); // D5
    const double scale = 0.052 * volume / 100.0;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const double tone = 0.75 * std::sin(2.0 * pi * note * t)
                          + 0.25 * std::sin(2.0 * pi * note * 2.4 * t);
        samples[static_cast<size_t>(i)] = static_cast<float>(tone * envelope(t, 0.035, 0.001) * scale);
    }
    queue(samples.data(), count);
}

void SoundEffects::queue(const float *samples, int sampleCount) {
    if (!device || !samples || sampleCount <= 0)
        return;

    std::vector<Sint16> pcm(static_cast<size_t>(sampleCount));
    for (int i = 0; i < sampleCount; ++i) {
        const double scaled = std::clamp(static_cast<double>(samples[i]), -1.0, 1.0) * 32767.0;
        pcm[static_cast<size_t>(i)] = static_cast<Sint16>(scaled);
    }

    // Controller movement can cross petals faster than the cue can finish.
    // Replace stale audio instead of building a queue that plays out of sync.
    constexpr Uint32 maximumQueuedMs = 90;
    const int sampleRate = format.freq > 0 ? format.freq : 48000;
    const Uint32 queuedBytes = SDL_GetQueuedAudioSize(device);
    const Uint32 maximumQueuedBytes = static_cast<Uint32>(sampleRate * sizeof(Sint16) * maximumQueuedMs / 1000);
    if (queuedBytes > maximumQueuedBytes)
        SDL_ClearQueuedAudio(device);
    SDL_QueueAudio(device, pcm.data(), static_cast<Uint32>(pcm.size() * sizeof(Sint16)));
}
