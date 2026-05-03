// =============================================================================
// fx.h — Lightweight, header-only DSP effects for the Midnight Melody Maker.
//
// Provides per-channel state for:
//   - fx::Chorus         : 2-tap modulated stereo delay, slight pitch shimmer
//   - fx::Delay          : feedback stereo delay with damping (low-pass)
//   - fx::Reverb         : Schroeder-style 4 combs + 2 allpasses per channel
//   - fx::CassetteNoise  : filtered random noise + slow flutter + saturation
//
// fx::FxChain combines all four and processes interleaved stereo TSF buffers.
// Designed for minimal CPU; not bit-exact with any commercial reverb. ;-)
// =============================================================================
#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace fx {

constexpr float kPi  = 3.14159265358979f;
constexpr float kTwoPi = 6.2831853071795f;

// -----------------------------------------------------------------------------
// Chorus : 2-tap modulated delay (LFO sin), L/R LFO 90° apart for stereo width.
// -----------------------------------------------------------------------------
struct Chorus {
    std::vector<float> bufL, bufR;
    int   writeIdx = 0;
    float lfoPhase = 0.0f;
    float sampleRate = 48000.0f;
    float rateHz = 0.7f;
    float depthMs = 5.0f;
    float baseMs  = 12.0f;
    float mix     = 0.5f;
    bool  enabled = false;

    void prepare(float sr) {
        sampleRate = sr;
        int n = (int)(sr * 0.060f); // 60 ms ring
        if (n < 64) n = 64;
        bufL.assign(n, 0.0f);
        bufR.assign(n, 0.0f);
        writeIdx = 0;
        lfoPhase = 0.0f;
    }
    void setRate(float hz)  { rateHz  = std::clamp(hz, 0.05f, 6.0f); }
    void setDepth(float ms) { depthMs = std::clamp(ms, 0.0f, 20.0f); }
    void setMix(float m)    { mix     = std::clamp(m, 0.0f, 1.0f); }

    static inline float tap(const std::vector<float>& buf, int writeIdx, float dly) {
        const int N = (int)buf.size();
        if (N < 2) return 0.0f;
        float r = (float)writeIdx - dly;
        while (r < 0)    r += (float)N;
        while (r >= N)   r -= (float)N;
        int i0 = (int)r; int i1 = (i0 + 1) % N;
        float frac = r - (float)i0;
        return buf[i0] * (1.0f - frac) + buf[i1] * frac;
    }

    void process(float* L, float* R, int n) {
        if (!enabled || bufL.empty()) return;
        const int N = (int)bufL.size();
        const float lfoInc = kTwoPi * rateHz / sampleRate;
        const float baseSamp = baseMs  * 0.001f * sampleRate;
        const float modSamp  = depthMs * 0.001f * sampleRate;
        for (int i = 0; i < n; ++i) {
            float s1 = std::sin(lfoPhase);
            float s2 = std::sin(lfoPhase + 1.5707963f);
            lfoPhase += lfoInc;
            if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;
            float dL = baseSamp + s1 * modSamp;
            float dR = baseSamp + s2 * modSamp;
            bufL[writeIdx] = L[i];
            bufR[writeIdx] = R[i];
            float wetL = tap(bufL, writeIdx, dL);
            float wetR = tap(bufR, writeIdx, dR);
            writeIdx = (writeIdx + 1) % N;
            L[i] = L[i] * (1.0f - mix) + wetL * mix;
            R[i] = R[i] * (1.0f - mix) + wetR * mix;
        }
    }
};

// -----------------------------------------------------------------------------
// Delay : stereo feedback delay with low-pass damping on the feedback path.
// -----------------------------------------------------------------------------
struct Delay {
    std::vector<float> bufL, bufR;
    int   writeIdx = 0;
    float sampleRate = 48000.0f;
    float timeMs = 350.0f;
    float feedback = 0.4f;
    float mix      = 0.3f;
    float dampL = 0.0f, dampR = 0.0f;
    float dampCoef = 0.4f; // 0=bright, 1=very dark
    bool  enabled = false;

    void prepare(float sr) {
        sampleRate = sr;
        int n = (int)(sr * 2.5f); // 2.5 s max
        if (n < 256) n = 256;
        bufL.assign(n, 0.0f);
        bufR.assign(n, 0.0f);
        writeIdx = 0; dampL = dampR = 0.0f;
    }
    void setTime(float ms)    { timeMs   = std::clamp(ms, 1.0f, 1500.0f); }
    void setFeedback(float f) { feedback = std::clamp(f, 0.0f, 0.92f); }
    void setMix(float m)      { mix      = std::clamp(m, 0.0f, 1.0f); }
    void setDamp(float d)     { dampCoef = std::clamp(d, 0.0f, 0.92f); }

    void process(float* L, float* R, int n) {
        if (!enabled || bufL.empty()) return;
        const int N = (int)bufL.size();
        int delaySamp = std::clamp((int)(timeMs * 0.001f * sampleRate), 1, N - 2);
        for (int i = 0; i < n; ++i) {
            int rIdx = writeIdx - delaySamp;
            if (rIdx < 0) rIdx += N;
            float dL = bufL[rIdx];
            float dR = bufR[rIdx];
            // 1-pole low-pass on feedback
            dampL = dampL + (1.0f - dampCoef) * (dL - dampL);
            dampR = dampR + (1.0f - dampCoef) * (dR - dampR);
            bufL[writeIdx] = L[i] + dampL * feedback;
            bufR[writeIdx] = R[i] + dampR * feedback;
            writeIdx = (writeIdx + 1) % N;
            L[i] = L[i] * (1.0f - mix) + dL * mix;
            R[i] = R[i] * (1.0f - mix) + dR * mix;
        }
    }
};

// -----------------------------------------------------------------------------
// Reverb : Schroeder-style 4 combs + 2 allpasses per channel.
// Lengths scaled to the running sample rate (base values @ 44.1 kHz).
// -----------------------------------------------------------------------------
struct Reverb {
    static constexpr int kCombs = 4;
    static constexpr int kAps   = 2;
    struct Comb {
        std::vector<float> buf;
        int   idx = 0;
        float store = 0.0f;
    };
    struct Ap {
        std::vector<float> buf;
        int   idx = 0;
    };
    Comb  combsL[kCombs], combsR[kCombs];
    Ap    apsL[kAps],     apsR[kAps];
    float feedback   = 0.84f;
    float damp       = 0.2f;
    float mix        = 0.25f;
    float sampleRate = 48000.0f;
    bool  enabled    = false;

    void prepare(float sr) {
        sampleRate = sr;
        const int kCombLens[kCombs] = { 1557, 1617, 1491, 1422 };
        const int kApLens[kAps]     = {  556,  441 };
        const float scale = sr / 44100.0f;
        for (int i = 0; i < kCombs; ++i) {
            int lL = (std::max)(8, (int)(kCombLens[i] * scale));
            int lR = (std::max)(8, (int)((kCombLens[i] + 23) * scale));
            combsL[i].buf.assign(lL, 0.0f); combsL[i].idx = 0; combsL[i].store = 0.0f;
            combsR[i].buf.assign(lR, 0.0f); combsR[i].idx = 0; combsR[i].store = 0.0f;
        }
        for (int i = 0; i < kAps; ++i) {
            int lL = (std::max)(8, (int)(kApLens[i] * scale));
            int lR = (std::max)(8, (int)((kApLens[i] + 23) * scale));
            apsL[i].buf.assign(lL, 0.0f); apsL[i].idx = 0;
            apsR[i].buf.assign(lR, 0.0f); apsR[i].idx = 0;
        }
    }
    // size 0..1 -> feedback 0.7..0.97
    void setSize(float s) { feedback = std::clamp(0.7f + std::clamp(s, 0.0f, 1.0f) * 0.27f, 0.7f, 0.97f); }
    void setDamp(float d) { damp     = std::clamp(d, 0.0f, 0.95f); }
    void setMix(float m)  { mix      = std::clamp(m, 0.0f, 1.0f); }

    static inline float procComb(Comb& c, float in, float fb, float d) {
        float y = c.buf[c.idx];
        c.store = y * (1.0f - d) + c.store * d;
        c.buf[c.idx] = in + c.store * fb;
        c.idx = (c.idx + 1) % (int)c.buf.size();
        return y;
    }
    static inline float procAp(Ap& a, float in) {
        const float g = 0.5f;
        float bufout = a.buf[a.idx];
        float y = -in + bufout;
        a.buf[a.idx] = in + bufout * g;
        a.idx = (a.idx + 1) % (int)a.buf.size();
        return y;
    }

    void process(float* L, float* R, int n) {
        if (!enabled || combsL[0].buf.empty()) return;
        for (int i = 0; i < n; ++i) {
            float inL = L[i], inR = R[i];
            float yL = 0.0f, yR = 0.0f;
            for (int c = 0; c < kCombs; ++c) {
                yL += procComb(combsL[c], inL, feedback, damp);
                yR += procComb(combsR[c], inR, feedback, damp);
            }
            for (int a = 0; a < kAps; ++a) {
                yL = procAp(apsL[a], yL);
                yR = procAp(apsR[a], yR);
            }
            L[i] = inL * (1.0f - mix) + yL * mix * 0.25f;
            R[i] = inR * (1.0f - mix) + yR * mix * 0.25f;
        }
    }
};

// -----------------------------------------------------------------------------
// CassetteNoise : random tape hiss + slow wow/flutter + soft saturation.
// -----------------------------------------------------------------------------
struct CassetteNoise {
    uint32_t rng    = 0xCAFEBABEu; // audio-noise RNG
    uint32_t modRng = 0xDEAD1234u; // separate RNG for amplitude wander
    float lpL = 0.0f, lpR = 0.0f;
    // Amplitude modulator: random slow wander (replaces the periodic sine LFO).
    float modCurr      = 1.0f; // current amplitude scale
    float modTarget    = 1.0f; // next interpolation target
    float modSlew      = 0.0f; // 1-pole LP coefficient (~60 ms)
    int   modCountdown = 0;    // samples until next target is drawn
    float sampleRate = 48000.0f;
    float level   = 0.05f; // 0..1 (stored as slider^2 for log feel)
    float flutter = 0.5f;  // 0..1
    float tone    = 0.4f;  // 0=dark 1=bright
    bool  enabled = false;

    void prepare(float sr) {
        sampleRate = sr;
        lpL = lpR = 0.0f;
        modCurr = modTarget = 1.0f;
        modCountdown = 0;
        // Slew: ~60 ms interpolation toward each random target.
        modSlew = std::exp(-1.0f / (0.06f * sr));
    }
    // Logarithmic feel: slider^2 so 20 % → 4 % actual, 50 % → 25 %, 100 % → 100 %.
    void setLevel(float l)   { l = std::clamp(l, 0.0f, 1.0f); level = l * l; }
    void setFlutter(float f) { flutter = std::clamp(f, 0.0f, 1.0f); }
    void setTone(float t)    { tone    = std::clamp(t, 0.0f, 1.0f); }

    static inline float frand(uint32_t& s) {
        s = s * 1664525u + 1013904223u;
        return (float)((int32_t)(s >> 8)) * (1.0f / 8388608.0f);
    }
    // Signed uniform in [-1, 1].
    static inline float frandS(uint32_t& s) {
        s = s * 1664525u + 1013904223u;
        return (float)((int32_t)s) * (1.0f / 2147483648.0f);
    }

    void process(float* L, float* R, int n) {
        if (!enabled) return;
        // Tone : LP cut-off 800..6000 Hz
        float cut = 800.0f + tone * 5200.0f;
        float a = std::exp(-kTwoPi * cut / sampleRate);
        float oneMa = 1.0f - a;
        for (int i = 0; i < n; ++i) {
            // ---- Random slow amplitude wander (aperiodic, replaces sine LFO) ----
            if (--modCountdown <= 0) {
                // Random hold interval: 80 ms .. 600 ms — completely irregular.
                float frac = (float)(modRng >> 8) * (1.0f / 16777216.0f); // 0..1
                modRng = modRng * 1664525u + 1013904223u;
                modCountdown = (int)((0.08f + frac * 0.52f) * sampleRate);
                // New random target: 1 ± (flutter * 0.30) — less extreme than before.
                modTarget = 1.0f + flutter * 0.30f * frandS(modRng);
            }
            modCurr = modCurr * modSlew + modTarget * (1.0f - modSlew);

            float wL = frand(rng) * 0.5f;
            float wR = frand(rng) * 0.5f;
            // 1-pole LP, then HP-ish via subtraction => band-pass tape hiss
            lpL = lpL * a + wL * oneMa;
            lpR = lpR * a + wR * oneMa;
            float hL = wL - lpL * 0.6f;
            float hR = wR - lpR * 0.6f;
            float nL = hL * level * modCurr;
            float nR = hR * level * modCurr;
            // gentle saturation on signal+noise
            float sL = std::tanh((L[i] + nL) * 1.0f);
            float sR = std::tanh((R[i] + nR) * 1.0f);
            L[i] = L[i] * 0.7f + sL * 0.3f + nL;
            R[i] = R[i] * 0.7f + sR * 0.3f + nR;
        }
    }
};

// -----------------------------------------------------------------------------
// FxChain : the four FX in series, plus a deinterleave/reinterleave helper.
// Order : Chorus -> Delay -> Reverb -> CassetteNoise.
// -----------------------------------------------------------------------------
struct FxChain {
    Chorus        chorus;
    Delay         del;
    Reverb        reverb;
    CassetteNoise noise;

    void prepare(float sr) {
        chorus.prepare(sr);
        del.prepare(sr);
        reverb.prepare(sr);
        noise.prepare(sr);
    }
    bool anyEnabled() const {
        return chorus.enabled || del.enabled || reverb.enabled || noise.enabled;
    }
    // sfBuf is interleaved L,R,L,R,...
    void processInterleaved(float* sfBuf, int frames) {
        if (frames <= 0 || !anyEnabled()) return;
        thread_local std::vector<float> L, R;
        if ((int)L.size() < frames) { L.resize(frames); R.resize(frames); }
        for (int i = 0; i < frames; ++i) {
            L[i] = sfBuf[2 * i];
            R[i] = sfBuf[2 * i + 1];
        }
        chorus.process(L.data(), R.data(), frames);
        del.process(L.data(), R.data(), frames);
        reverb.process(L.data(), R.data(), frames);
        noise.process(L.data(), R.data(), frames);
        for (int i = 0; i < frames; ++i) {
            sfBuf[2 * i]     = L[i];
            sfBuf[2 * i + 1] = R[i];
        }
    }
};

} // namespace fx
