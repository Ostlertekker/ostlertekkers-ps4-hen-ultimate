// OSTLERTEKKERS HEN - audio/audio.h
// Boot sound: an own, synthesized startup sound (no third-party recordings)
// and a loader for a WAV file the user supplies himself.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/result.h"

namespace henu {

constexpr int kAudioRate = 48000;   // all PCM here: interleaved stereo int16 @ 48 kHz

struct Pcm {
    std::vector<int16_t> samples;   // L R L R ...
    size_t frames() const { return samples.size() / 2; }
    double seconds() const { return static_cast<double>(frames()) / kAudioRate; }
};

// RIFF/WAVE, PCM 8/16/24-bit or float32, mono or stereo, any sample rate
// (resampled linearly to 48 kHz). maxSeconds caps memory use.
Result<Pcm> decodeWav(const std::string& file, double maxSeconds = 30);

// Own startup sound. variant 0..5 changes key and colour (one per boot
// animation); deterministic, ~6.5 s.
Pcm synthBootSound(int variant);

// Short own sound for the game-start animation: riser into an impact at
// kLaunchHitSeconds plus a bell; variant 0..5 changes the key. ~2.6 s.
constexpr double kLaunchHitSeconds = 1.55;
Pcm synthLaunchSound(int variant);

// RIFF/WAVE 16-bit stereo 48 kHz (preview tool, tests).
std::string encodeWav(const Pcm& pcm);

void applyVolume(Pcm& pcm, float volume);   // 0..1

}  // namespace henu
