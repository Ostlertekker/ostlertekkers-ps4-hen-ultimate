// OSTLERTEKKERS HEN - audio/audio.cpp
#include "audio/audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace henu {

namespace {
constexpr double kTwoPi = 6.283185307179586;

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

double noteHz(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

// Small deterministic noise source.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1) {}
    double next() {   // -1..1
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (s / 4294967295.0) * 2 - 1;
    }
};
}  // namespace

Result<Pcm> decodeWav(const std::string& file, double maxSeconds) {
    using R = Result<Pcm>;
    const uint8_t* d = reinterpret_cast<const uint8_t*>(file.data());
    if (file.size() < 44 || std::memcmp(d, "RIFF", 4) != 0 || std::memcmp(d + 8, "WAVE", 4) != 0)
        return R::fail(ErrorCode::ParseError, "wav: not a RIFF/WAVE file");
    size_t pos = 12;
    int fmtTag = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;
    while (pos + 8 <= file.size()) {
        const uint32_t len = le32(d + pos + 4);
        const size_t avail = file.size() - pos - 8;
        const size_t n = std::min<size_t>(len, avail);
        if (std::memcmp(d + pos, "fmt ", 4) == 0 && n >= 16) {
            fmtTag = le16(d + pos + 8);
            channels = le16(d + pos + 10);
            rate = le32(d + pos + 12);
            bits = le16(d + pos + 22);
            if (fmtTag == 0xFFFE && n >= 26) fmtTag = le16(d + pos + 32);   // WAVE_FORMAT_EXTENSIBLE
        } else if (std::memcmp(d + pos, "data", 4) == 0) {
            data = d + pos + 8;
            dataLen = n;
        }
        pos += 8 + n + (n & 1);
    }
    if (!data || channels < 1 || channels > 8 || rate < 8000 || rate > 192000)
        return R::fail(ErrorCode::ParseError, "wav: missing or invalid fmt/data chunk");
    const bool isFloat = fmtTag == 3;
    if (!((fmtTag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (isFloat && bits == 32)))
        return R::fail(ErrorCode::Unsupported, "wav: only PCM 8/16/24/32-bit or float32 supported");
    const size_t bytesPerFrame = static_cast<size_t>(channels) * (bits / 8);
    size_t inFrames = dataLen / bytesPerFrame;
    inFrames = std::min(inFrames, static_cast<size_t>(maxSeconds * rate));
    auto sampleAt = [&](size_t frame, int ch) -> float {
        const uint8_t* p = data + frame * bytesPerFrame + static_cast<size_t>(ch) * (bits / 8);
        if (isFloat) { float f; std::memcpy(&f, p, 4); return std::max(-1.f, std::min(1.f, f)); }
        switch (bits) {
            case 8: return (p[0] - 128) / 128.f;
            case 16: return static_cast<int16_t>(le16(p)) / 32768.f;
            case 24: { int32_t v = (p[0] << 8) | (p[1] << 16) | (static_cast<int32_t>(p[2]) << 24); return (v >> 8) / 8388608.f; }
            default: return static_cast<int32_t>(le32(p)) / 2147483648.f;
        }
    };
    Pcm out;
    const double step = static_cast<double>(rate) / kAudioRate;
    const size_t outFrames = inFrames ? static_cast<size_t>((inFrames - 1) / step) + 1 : 0;
    out.samples.resize(outFrames * 2);
    for (size_t i = 0; i < outFrames; ++i) {
        const double src = i * step;
        const size_t f0 = static_cast<size_t>(src);
        const size_t f1 = std::min(inFrames - 1, f0 + 1);
        const float t = static_cast<float>(src - f0);
        for (int ch = 0; ch < 2; ++ch) {
            const int c = std::min(ch, channels - 1);
            float v = sampleAt(f0, c) * (1 - t) + sampleAt(f1, c) * t;
            out.samples[i * 2 + ch] = static_cast<int16_t>(std::lround(std::max(-1.f, std::min(1.f, v)) * 32767));
        }
    }
    return R::ok(std::move(out));
}

Pcm synthBootSound(int variant) {
    // Key per variant (MIDI root) and timbre brightness.
    static const int roots[6] = {45, 43, 41, 40, 47, 38};   // A, G, F, E, B, D
    static const double bright[6] = {0.55, 0.35, 0.7, 0.5, 0.85, 0.45};
    const int v = ((variant % 6) + 6) % 6;
    const int root = roots[v];
    const double br = bright[v];
    const double dur = 6.5;
    const size_t n = static_cast<size_t>(dur * kAudioRate);
    std::vector<double> L(n, 0.0), Rch(n, 0.0);
    Rng rng(0x5EED + static_cast<uint32_t>(v));

    // 1) Sub drone: root-12 sine, slow swell 0 -> 3.4 s, then fades.
    {
        const double f = noteHz(root - 12);
        double ph = 0;
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / kAudioRate;
            const double env = t < 3.4 ? std::pow(t / 3.4, 2.0) : std::exp(-(t - 3.4) * 1.1);
            ph += kTwoPi * f / kAudioRate;
            const double s = std::sin(ph) * 0.32 * env;
            L[i] += s; Rch[i] += s;
        }
    }
    // 2) Pad: minor add9 chord of detuned saws through an opening low-pass.
    {
        const int chord[4] = {0, 7, 12 + 3, 12 + 14};   // root, fifth, minor third, ninth
        for (int k = 0; k < 4; ++k) {
            for (int det = -1; det <= 1; det += 2) {
                const double f = noteHz(root + chord[k]) * (1 + det * 0.0035);
                double ph = rng.next() * 0.5 + 0.5, lp = 0, a = 0;
                const double pan = 0.5 + 0.35 * det * (k % 2 ? 1 : -1) * 0.5;
                for (size_t i = 0; i < n; ++i) {
                    const double t = static_cast<double>(i) / kAudioRate;
                    ph += f / kAudioRate;
                    if (ph >= 1) ph -= 1;
                    const double saw = 2 * ph - 1;
                    if ((i & 63) == 0) {
                        const double cutoff = 150 + 2600 * br * std::min(1.0, t / 3.0);
                        a = 1 - std::exp(-kTwoPi * cutoff / kAudioRate);
                    }
                    lp += a * (saw - lp);
                    const double env = t < 0.8 ? t / 0.8 : (t < 3.6 ? 1.0 : std::exp(-(t - 3.6) * 0.9));
                    const double s = lp * env * 0.045;
                    L[i] += s * (1 - pan); Rch[i] += s * pan;
                }
            }
        }
    }
    // 3) Bell pings (inharmonic partials), ascending.
    {
        const double when[4] = {0.9, 1.6, 2.3, 3.45};
        const int notes[4] = {24 + 7, 24 + 12, 24 + 15, 36};
        for (int k = 0; k < 4; ++k) {
            const size_t start = static_cast<size_t>(when[k] * kAudioRate);
            const double f = noteHz(root + notes[k]);
            const double pan = 0.25 + 0.5 * (k % 2);
            const double ratios[3] = {1.0, 2.76, 5.4};
            const double amps[3] = {1.0, 0.45 * br, 0.2 * br};
            for (size_t i = start; i < n; ++i) {
                const double t = static_cast<double>(i - start) / kAudioRate;
                const double env = std::exp(-t * 2.2) * std::min(1.0, t * 400);
                if (env < 1e-4) break;
                double s = 0;
                for (int p = 0; p < 3; ++p) s += std::sin(kTwoPi * f * ratios[p] * t) * amps[p] * std::exp(-t * p * 1.5);
                s *= env * 0.11;
                L[i] += s * (1 - pan); Rch[i] += s * pan;
            }
        }
    }
    // 4) Riser noise into a hard kick-like impact at 3.45 s (the "logo hit").
    {
        const double hit = 3.45;
        double lp = 0;
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / kAudioRate;
            if (t > 1.8 && t < hit) {
                const double e = std::pow((t - 1.8) / (hit - 1.8), 3.0);
                const double a = 1 - std::exp(-kTwoPi * (300 + 5000 * e) / kAudioRate);
                lp += a * (rng.next() - lp);
                L[i] += lp * e * 0.09; Rch[i] += lp * e * 0.09;
            }
            if (t >= hit) {
                const double tt = t - hit;
                const double phase = kTwoPi * (45 * tt + 120 * (1 - std::exp(-tt * 28)) / 28);
                const double env = std::exp(-tt * 3.2);
                double s = std::tanh(std::sin(phase) * 2.2) * env * 0.42;
                L[i] += s; Rch[i] += s;
                if (tt > 1.6) break;
            }
        }
    }
    // 5) Stereo feedback delay as a cheap room.
    {
        const size_t dl = static_cast<size_t>(0.31 * kAudioRate), dr = static_cast<size_t>(0.37 * kAudioRate);
        for (size_t i = 0; i < n; ++i) {
            if (i >= dl) L[i] += Rch[i - dl] * 0.38;
            if (i >= dr) Rch[i] += L[i - dr] * 0.38;
        }
    }
    // Fade out the last 0.8 s, normalise to -1 dBFS.
    double peak = 1e-9;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kAudioRate;
        const double fade = t > dur - 0.8 ? (dur - t) / 0.8 : 1.0;
        L[i] *= fade; Rch[i] *= fade;
        peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(Rch[i])));
    }
    const double gain = 0.89 / peak;
    Pcm out;
    out.samples.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out.samples[i * 2] = static_cast<int16_t>(std::lround(L[i] * gain * 32767));
        out.samples[i * 2 + 1] = static_cast<int16_t>(std::lround(Rch[i] * gain * 32767));
    }
    return out;
}

Pcm synthLaunchSound(int variant) {
    static const int roots[6] = {45, 43, 41, 40, 47, 38};
    const int v = ((variant % 6) + 6) % 6;
    const int root = roots[v];
    const double dur = 2.6, hit = kLaunchHitSeconds;
    const size_t n = static_cast<size_t>(dur * kAudioRate);
    std::vector<double> L(n, 0.0), Rch(n, 0.0);
    Rng rng(0x1A0C + static_cast<uint32_t>(v));
    double lp = 0, ph = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kAudioRate;
        // rising filtered noise with a pitched sweep underneath
        if (t < hit) {
            const double e = std::pow(t / hit, 2.4);
            const double a = 1 - std::exp(-kTwoPi * (200 + 7000 * e) / kAudioRate);
            lp += a * (rng.next() - lp);
            ph += kTwoPi * noteHz(root + 12 + 24 * e) / kAudioRate;
            const double s = lp * e * 0.16 + std::sin(ph) * e * 0.05;
            const double pan = 0.5 + 0.4 * std::sin(t * 9);
            L[i] += s * (1 - pan) * 2; Rch[i] += s * pan * 2;
        } else {
            const double tt = t - hit;
            // impact: pitched-down sine kick + noise burst
            const double kick = std::tanh(std::sin(kTwoPi * (50 * tt + 140 * (1 - std::exp(-tt * 30)) / 30)) * 2.5) *
                                std::exp(-tt * 4.0) * 0.5;
            const double burst = rng.next() * std::exp(-tt * 18) * 0.18;
            // bell, root two octaves up (inharmonic partials)
            double bell = 0;
            const double ratios[3] = {1.0, 2.76, 5.4}, amps[3] = {1.0, 0.4, 0.18};
            for (int k = 0; k < 3; ++k)
                bell += std::sin(kTwoPi * noteHz(root + 24) * ratios[k] * tt) * amps[k] * std::exp(-tt * (2.0 + k * 1.5));
            bell *= 0.16 * std::min(1.0, tt * 300);
            L[i] += kick + burst + bell * 0.8; Rch[i] += kick + burst * 0.9 + bell;
        }
    }
    const size_t dl = static_cast<size_t>(0.23 * kAudioRate), dr = static_cast<size_t>(0.29 * kAudioRate);
    for (size_t i = 0; i < n; ++i) {
        if (i >= dl) L[i] += Rch[i - dl] * 0.3;
        if (i >= dr) Rch[i] += L[i - dr] * 0.3;
    }
    double peak = 1e-9;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / kAudioRate;
        const double fade = t > dur - 0.5 ? (dur - t) / 0.5 : 1.0;
        L[i] *= fade; Rch[i] *= fade;
        peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(Rch[i])));
    }
    const double gain = 0.89 / peak;
    Pcm out;
    out.samples.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out.samples[i * 2] = static_cast<int16_t>(std::lround(L[i] * gain * 32767));
        out.samples[i * 2 + 1] = static_cast<int16_t>(std::lround(Rch[i] * gain * 32767));
    }
    return out;
}

std::string encodeWav(const Pcm& pcm) {
    std::string s;
    auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) s += static_cast<char>((v >> (8 * i)) & 0xFF); };
    auto put16 = [&](uint16_t v) { s += static_cast<char>(v & 0xFF); s += static_cast<char>(v >> 8); };
    const uint32_t dataBytes = static_cast<uint32_t>(pcm.samples.size() * 2);
    s += "RIFF"; put32(36 + dataBytes); s += "WAVE";
    s += "fmt "; put32(16); put16(1); put16(2); put32(kAudioRate); put32(kAudioRate * 4); put16(4); put16(16);
    s += "data"; put32(dataBytes);
    for (int16_t v : pcm.samples) put16(static_cast<uint16_t>(v));
    return s;
}

void applyVolume(Pcm& pcm, float volume) {
    const float v = std::max(0.f, std::min(1.f, volume));
    if (v >= 0.999f) return;
    for (auto& x : pcm.samples) x = static_cast<int16_t>(std::lround(x * v));
}

}  // namespace henu
