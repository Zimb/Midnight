// =============================================================================
// sf2_maker.h â€“ Procedural SoundFont (.sf2) generator (header-only, C++17)
//
// C++ port of generate_soundfont.py: synthesizes a single-instrument SF2 from
// scratch using DSP only (no external samples). Writes a valid SF2 file via a
// minimal RIFF writer.
//
// Usage:
//     SfmConfig cfg;
//     cfg.name      = "Lofi Piano";
//     cfg.synthType = SfmSynth::FM;
//     cfg.attack    = 0.005f;
//     cfg.decay     = 0.40f;
//     cfg.sustain   = 0.45f;
//     cfg.release   = 0.60f;
//     cfg.modIndex  = 4.0f;
//     cfg.modDecay  = 6.0f;
//     SfmGenerator::generate(cfg, "C:/.../Lofi Piano.sf2");
//
// Phase 1 supports SfmSynth::FM only. SfmSynth::Additive and KS are stubbed
// to FM for forward compatibility â€“ the wiring is in place.
// =============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

namespace sfm {

// ----------------------------------------------------------------------------
// Public configuration
// ----------------------------------------------------------------------------

enum class SfmSynth : int {
    FM       = 0,  // 2-operator FM (piano / EP / bell)
    Additive = 1,  // detuned harmonic partials (pad)  â€“ Phase 2
    KS       = 2,  // Karplus-Strong (plucked string) â€“ Phase 2
};

struct SfmConfig {
    std::string name        = "Custom";
    SfmSynth    synthType   = SfmSynth::FM;

    // Common amplitude envelope
    float       attack      = 0.005f;
    float       decay       = 0.40f;
    float       sustain     = 0.45f;
    float       release     = 0.60f;

    // Timbre params (interpretation depends on synthType)
    // FM:       modIndex = mod depth init, modDecay = decay rate (1/s)
    // Additive: modIndex = high-partial mix, modDecay = detune cents
    // KS:       modIndex = damping (0.99..0.999), modDecay = excitation LP cutoff
    float       modIndex    = 4.0f;
    float       modDecay    = 6.0f;

    // Multi-sampling plan
    int         lowNote     = 36;
    int         highNote    = 96;
    int         step        = 4;     // semitones between sample roots

    // Output amplitude (0..1)
    float       gain        = 0.9f;
};

// ----------------------------------------------------------------------------
// Generator
// ----------------------------------------------------------------------------

class SfmGenerator {
public:
    static constexpr int   SR             = 44100;
    static constexpr float SAMPLE_LEN_S   = 1.6f;
    static constexpr float LOOP_START_S   = 0.9f;
    static constexpr float LOOP_END_S     = 1.5f;
    static constexpr int   CROSSFADE_MS   = 30;

    // Generates an .sf2 in memory (no disk I/O). Returns raw bytes; empty on failure.
    static std::vector<uint8_t> generateToMemory(const SfmConfig& cfg) {
        std::vector<SampleEntry> samples;
        samples.reserve(64);
        const int loopStart = (int)(LOOP_START_S * SR);
        const int loopEnd   = (int)(LOOP_END_S   * SR);
        const int durSamps  = (int)(SAMPLE_LEN_S * SR);
        std::vector<std::tuple<int,int,int>> zones;
        {
            std::vector<int> roots;
            for (int n = cfg.lowNote; n <= cfg.highNote; n += cfg.step) roots.push_back(n);
            for (size_t i = 0; i < roots.size(); ++i) {
                int r  = roots[i];
                int lo = (i == 0)               ? 0   : r - cfg.step / 2;
                int hi = (i == roots.size() - 1) ? 127 : r + (cfg.step - cfg.step / 2 - 1);
                zones.emplace_back(r, lo, hi);
            }
        }
        for (auto [root, keyLo, keyHi] : zones) {
            std::vector<float> audio = synth(cfg, root, durSamps);
            prepareLoop(audio, loopStart, loopEnd, CROSSFADE_MS);
            SampleEntry e;
            e.name = cfg.name + "_" + std::to_string(root);
            if (e.name.size() > 19) e.name.resize(19);
            e.pcm.resize(audio.size());
            for (size_t i = 0; i < audio.size(); ++i) {
                float v = audio[i];
                if (v >  1.f) v =  1.f;
                if (v < -1.f) v = -1.f;
                e.pcm[i] = (int16_t)std::lrint(v * 32767.0f);
            }
            e.localLoopStart = loopStart;
            e.localLoopEnd   = loopEnd;
            e.originalPitch  = (uint8_t)root;
            e.keyLo = (uint8_t)keyLo;
            e.keyHi = (uint8_t)keyHi;
            samples.push_back(std::move(e));
        }
        return buildSf2Bytes(cfg.name, samples);
    }

    // Generates an .sf2 file at outPath. Returns true on success.
    static bool generate(const SfmConfig& cfg, const std::string& outPath) {
        std::vector<SampleEntry> samples;
        samples.reserve(64);

        const int loopStart = (int)(LOOP_START_S * SR);
        const int loopEnd   = (int)(LOOP_END_S   * SR);
        const int durSamps  = (int)(SAMPLE_LEN_S * SR);

        // Multi-sample plan: roots every `step` semitones across [low, high].
        std::vector<std::tuple<int,int,int>> zones; // root, keyLo, keyHi
        {
            std::vector<int> roots;
            for (int n = cfg.lowNote; n <= cfg.highNote; n += cfg.step) roots.push_back(n);
            for (size_t i = 0; i < roots.size(); ++i) {
                int r  = roots[i];
                int lo = (i == 0)               ? 0   : r - cfg.step / 2;
                int hi = (i == roots.size() - 1) ? 127 : r + (cfg.step - cfg.step / 2 - 1);
                zones.emplace_back(r, lo, hi);
            }
        }

        for (auto [root, keyLo, keyHi] : zones) {
            std::vector<float> audio = synth(cfg, root, durSamps);
            prepareLoop(audio, loopStart, loopEnd, CROSSFADE_MS);
            SampleEntry e;
            e.name = cfg.name + "_" + std::to_string(root);
            if (e.name.size() > 19) e.name.resize(19);
            e.pcm.resize(audio.size());
            for (size_t i = 0; i < audio.size(); ++i) {
                float v = audio[i];
                if (v >  1.f) v =  1.f;
                if (v < -1.f) v = -1.f;
                e.pcm[i] = (int16_t)std::lrint(v * 32767.0f);
            }
            e.localLoopStart = loopStart;
            e.localLoopEnd   = loopEnd;
            e.originalPitch  = (uint8_t)root;
            e.keyLo = (uint8_t)keyLo;
            e.keyHi = (uint8_t)keyHi;
            samples.push_back(std::move(e));
        }

        return writeSf2(outPath, cfg.name, samples);
    }

private:
    // ------------------------------------------------------------------------
    // Internal types
    // ------------------------------------------------------------------------
    struct SampleEntry {
        std::string          name;
        std::vector<int16_t> pcm;
        int                  localLoopStart = 0;
        int                  localLoopEnd   = 0;
        uint8_t              originalPitch  = 60;
        uint8_t              keyLo          = 0;
        uint8_t              keyHi          = 127;

        // Filled by writer (global offsets in concatenated sample data).
        uint32_t             startGlobal = 0;
        uint32_t             endGlobal   = 0;
        uint32_t             loopStartGlobal = 0;
        uint32_t             loopEndGlobal   = 0;
    };

    // ------------------------------------------------------------------------
    // DSP helpers
    // ------------------------------------------------------------------------
    static float midiToHz(int n) {
        return 440.0f * std::pow(2.0f, (n - 69) / 12.0f);
    }

    static void adsr(std::vector<float>& env,
                     int n, float a, float d, float s, float r,
                     int sustainTo) {
        env.assign(n, 0.0f);
        int aN = std::max(1, (int)(a * SR));
        int dN = std::max(1, (int)(d * SR));
        int rN = std::max(1, (int)(r * SR));
        sustainTo = std::min(sustainTo, n);

        int endA = std::min(aN, sustainTo);
        for (int i = 0; i < endA; ++i)
            env[i] = (float)i / (float)endA;

        int endD = std::min(endA + dN, sustainTo);
        for (int i = endA; i < endD; ++i) {
            float t = (float)(i - endA) / (float)(endD - endA);
            env[i] = 1.0f + t * (s - 1.0f);
        }
        for (int i = endD; i < sustainTo; ++i) env[i] = s;

        int endR = std::min(sustainTo + rN, n);
        for (int i = sustainTo; i < endR; ++i) {
            float t = (float)(i - sustainTo) / (float)(endR - sustainTo);
            env[i] = s * (1.0f - t);
        }
    }

    static void normalize(std::vector<float>& x, float peak) {
        float m = 0.0f;
        for (float v : x) { float a = std::fabs(v); if (a > m) m = a; }
        if (m <= 0.0f) return;
        float g = peak / m;
        for (float& v : x) v *= g;
    }

    // ------------------------------------------------------------------------
    // Synthesizers
    // ------------------------------------------------------------------------
    static std::vector<float> synth(const SfmConfig& cfg, int midiNote, int n) {
        switch (cfg.synthType) {
        case SfmSynth::FM:       return synthFM(cfg, midiNote, n);
        case SfmSynth::Additive: return synthAdditive(cfg, midiNote, n);
        case SfmSynth::KS:       return synthKS(cfg, midiNote, n);
        }
        return synthFM(cfg, midiNote, n);
    }

    // ---- Karplus-Strong (plucked string): harp, piano ----
    // cfg.modIndex (0..20) : brightness  0=pure noise pluck, 20=tonal excitation
    // cfg.modDecay (0..30) : ring time   0=very short, 30=very long sustain
    static std::vector<float> synthKS(const SfmConfig& cfg, int midiNote, int n) {
        float f = midiToHz(midiNote);
        int P = std::max(2, (int)std::round((float)SR / f));

        // brightness: how tonal the initial pluck is
        float brightness = std::clamp(cfg.modIndex / 20.0f, 0.0f, 1.0f);
        // damping → ring time: 0→≈0.3s, 15→≈1.5s, 30→≈4s (at A4)
        float damping = 0.9920f + std::clamp(cfg.modDecay / 30.0f, 0.0f, 1.0f) * 0.0075f;

        // Excitation: noise + optional tonal partial
        uint32_t rng = 0xA3C9E000u ^ ((uint32_t)(midiNote * 1234567u));
        std::vector<float> delay(P);
        for (int i = 0; i < P; ++i) {
            rng = rng * 1664525u + 1013904223u;
            float noise = (float)((int32_t)rng) * (1.0f / 2147483648.0f);
            float tonal = std::sin(2.0f * 3.14159265f * (float)i / (float)P);
            delay[i] = noise * (1.0f - brightness) + tonal * brightness;
        }

        // KS loop: each step averages two adjacent delay-line samples
        std::vector<float> out(n);
        int rd = 0;
        for (int i = 0; i < n; ++i) {
            int rd1 = (rd + 1) % P;
            out[i]    = delay[rd];
            delay[rd] = damping * 0.5f * (delay[rd] + delay[rd1]);
            rd = rd1;
        }

        // Short attack fade-in (cfg.attack ≤ 0.03s for plucked strings)
        int attSamps = std::max(1, (int)(cfg.attack * SR));
        for (int i = 0; i < std::min(attSamps, n); ++i)
            out[i] *= (float)i / (float)attSamps;

        normalize(out, cfg.gain);
        return out;
    }

    // ---- Additive (detuned harmonic partials): pads, voices ----
    // cfg.modIndex (0..20) : number of harmonics (1..16)
    // cfg.modDecay (0..30) : detune per harmonic in cents (0..15 cents)
    static std::vector<float> synthAdditive(const SfmConfig& cfg, int midiNote, int n) {
        float f = midiToHz(midiNote);
        const float twoPi = 2.0f * 3.14159265358979f;

        int numPartials = std::max(1, std::min(16, (int)std::round(cfg.modIndex)));
        float detuneCents = cfg.modDecay * 0.5f; // 0..15 cents

        int sustainTo = (int)(LOOP_END_S * SR);
        std::vector<float> env;
        adsr(env, n, cfg.attack, cfg.decay, cfg.sustain, cfg.release, sustainTo);

        std::vector<float> out(n, 0.0f);
        float totalAmp = 0.0f;
        for (int k = 1; k <= numPartials; ++k) {
            float amp = 1.0f / (float)k; // natural harmonic rolloff
            totalAmp += amp;
            // Slight up-detune that reduces for higher partials (chorus feel)
            float detune_ratio = std::pow(2.0f, (detuneCents / (float)k) / 1200.0f);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / (float)SR;
                out[i] += amp * std::sin(twoPi * f * (float)k * detune_ratio * t) * env[i];
            }
        }
        if (totalAmp > 0.0f) {
            float inv = 1.0f / totalAmp;
            for (float& v : out) v *= inv;
        }
        normalize(out, cfg.gain);
        return out;
    }

    static std::vector<float> synthFM(const SfmConfig& cfg, int midiNote, int n) {
        std::vector<float> out(n, 0.0f);
        float f = midiToHz(midiNote);
        float twoPiF = 2.0f * 3.14159265358979323846f * f;
        int sustainTo = (int)(LOOP_END_S * SR);

        std::vector<float> env;
        adsr(env, n, cfg.attack, cfg.decay, cfg.sustain, cfg.release, sustainTo);

        for (int i = 0; i < n; ++i) {
            float t        = (float)i / (float)SR;
            float modIdx   = cfg.modIndex * std::exp(-t * cfg.modDecay);
            float mod      = modIdx * std::sin(twoPiF * t);
            float carrier  = std::sin(twoPiF * t + mod);
            float partial2 = 0.15f * std::sin(2.0f * twoPiF * t);
            out[i] = (carrier + partial2) * env[i];
        }
        normalize(out, cfg.gain);
        return out;
    }

    // ------------------------------------------------------------------------
    // Loop preparation: crossfade end of loop into its start.
    // ------------------------------------------------------------------------
    static int nearestZeroCrossing(const std::vector<float>& a, int idx, int search) {
        int lo = std::max(1, idx - search);
        int hi = std::min((int)a.size() - 1, idx + search);
        int best = idx;
        float bestAbs = std::fabs(a[idx]);
        for (int i = lo; i < hi; ++i) {
            if (a[i - 1] * a[i] <= 0.0f) {
                float v = std::fabs(a[i]);
                if (v < bestAbs) { bestAbs = v; best = i; }
            }
        }
        return best;
    }

    static void prepareLoop(std::vector<float>& a, int loopStart, int loopEnd, int xfadeMs) {
        int xfade = std::min((int)(xfadeMs * SR / 1000), (loopEnd - loopStart) / 2);
        if (xfade < 8) return;
        loopStart = nearestZeroCrossing(a, loopStart, 400);
        loopEnd   = nearestZeroCrossing(a, loopEnd,   400);
        for (int i = 0; i < xfade; ++i) {
            float fadeIn  = (float)i / (float)xfade;
            float fadeOut = 1.0f - fadeIn;
            float pre  = a[loopEnd - xfade + i];
            float post = a[loopStart + i];
            a[loopEnd - xfade + i] = pre * fadeOut + post * fadeIn;
        }
    }

    // ------------------------------------------------------------------------
    // SF2 / RIFF writer
    // ------------------------------------------------------------------------
    static constexpr uint16_t GEN_KEY_RANGE       = 43;
    static constexpr uint16_t GEN_SAMPLE_MODES    = 54;
    static constexpr uint16_t GEN_OVERRIDING_ROOT = 58;
    static constexpr uint16_t GEN_INSTRUMENT      = 41;
    static constexpr uint16_t GEN_SAMPLE_ID       = 53;

    // Append a RIFF chunk: tag (4) + size (4 LE) + data + pad to even.
    static void appendChunk(std::vector<uint8_t>& dst, const char tag[4],
                            const std::vector<uint8_t>& data) {
        dst.insert(dst.end(), tag, tag + 4);
        uint32_t sz = (uint32_t)data.size();
        for (int i = 0; i < 4; ++i) dst.push_back((uint8_t)(sz >> (8 * i)));
        dst.insert(dst.end(), data.begin(), data.end());
        if (sz & 1) dst.push_back(0);
    }

    static void appendListChunk(std::vector<uint8_t>& dst, const char form[4],
                                const std::vector<uint8_t>& data) {
        std::vector<uint8_t> inner;
        inner.insert(inner.end(), form, form + 4);
        inner.insert(inner.end(), data.begin(), data.end());
        appendChunk(dst, "LIST", inner);
    }

    static void putU16(std::vector<uint8_t>& v, uint16_t x) {
        v.push_back((uint8_t)(x & 0xFF));
        v.push_back((uint8_t)(x >> 8));
    }
    static void putU32(std::vector<uint8_t>& v, uint32_t x) {
        for (int i = 0; i < 4; ++i) v.push_back((uint8_t)(x >> (8 * i)));
    }
    static void putI16(std::vector<uint8_t>& v, int16_t x) {
        putU16(v, (uint16_t)x);
    }
    static void putPadStr(std::vector<uint8_t>& v, const std::string& s, int n) {
        std::string c = s;
        if ((int)c.size() > n - 1) c.resize(n - 1);
        for (char ch : c) v.push_back((uint8_t)ch);
        for (int i = (int)c.size(); i < n; ++i) v.push_back(0);
    }

    static bool writeSf2(const std::string& path, const std::string& presetName,
                         std::vector<SampleEntry>& samples)
    {
        std::vector<uint8_t> bytes = buildSf2Bytes(presetName, samples);
        if (bytes.empty()) return false;
        FILE* f = nullptr;
#ifdef _WIN32
        if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;
#else
        f = std::fopen(path.c_str(), "wb");
        if (!f) return false;
#endif
        std::fwrite(bytes.data(), 1, bytes.size(), f);
        std::fclose(f);
        return true;
    }

    // Build raw SF2 bytes in memory (called by both writeSf2 and generateToMemory).
    static std::vector<uint8_t> buildSf2Bytes(const std::string& presetName,
                                              std::vector<SampleEntry>& samples)
    {
        // ---- INFO chunk ----------------------------------------------------
        std::vector<uint8_t> info;
        {
            std::vector<uint8_t> ifil; putU16(ifil, 2); putU16(ifil, 1);
            appendChunk(info, "ifil", ifil);

            std::vector<uint8_t> isng; putPadStr(isng, "EMU8000", 8);
            appendChunk(info, "isng", isng);

            std::vector<uint8_t> inam; putPadStr(inam, presetName, 32);
            appendChunk(info, "INAM", inam);

            std::vector<uint8_t> isft; putPadStr(isft, "sf2_maker.h", 32);
            appendChunk(info, "ISFT", isft);
        }

        // ---- sdta (concatenated samples + 46-sample guard) -----------------
        const int GUARD = 46;
        std::vector<uint8_t> sdata;
        {
            uint32_t cursor = 0;
            size_t total = 0;
            for (auto& s : samples) total += s.pcm.size() * 2 + GUARD * 2;
            sdata.reserve(total + 8);
            for (auto& s : samples) {
                s.startGlobal     = cursor;
                s.endGlobal       = cursor + (uint32_t)s.pcm.size();
                s.loopStartGlobal = cursor + (uint32_t)s.localLoopStart;
                s.loopEndGlobal   = cursor + (uint32_t)s.localLoopEnd;
                for (int16_t v : s.pcm) putI16(sdata, v);
                for (int i = 0; i < GUARD; ++i) putI16(sdata, 0);
                cursor += (uint32_t)s.pcm.size() + (uint32_t)GUARD;
            }
        }
        std::vector<uint8_t> sdtaInner;
        appendChunk(sdtaInner, "smpl", sdata);

        // ---- pdta ----------------------------------------------------------
        std::vector<uint8_t> phdr;
        putPadStr(phdr, presetName, 20);
        putU16(phdr, 0); putU16(phdr, 0); putU16(phdr, 0);
        putU32(phdr, 0); putU32(phdr, 0); putU32(phdr, 0);
        putPadStr(phdr, "EOP", 20);
        putU16(phdr, 0); putU16(phdr, 0); putU16(phdr, 1);
        putU32(phdr, 0); putU32(phdr, 0); putU32(phdr, 0);

        std::vector<uint8_t> pbag;
        putU16(pbag, 0); putU16(pbag, 0);
        putU16(pbag, 1); putU16(pbag, 0);

        std::vector<uint8_t> pmod;
        putU16(pmod, 0); putU16(pmod, 0); putI16(pmod, 0);
        putU16(pmod, 0); putU16(pmod, 0);

        std::vector<uint8_t> pgen;
        putU16(pgen, GEN_INSTRUMENT); putU16(pgen, 0);
        putU16(pgen, 0); putU16(pgen, 0);

        std::vector<uint8_t> inst;
        putPadStr(inst, presetName, 20); putU16(inst, 0);
        putPadStr(inst, "EOI", 20);
        putU16(inst, (uint16_t)samples.size());

        std::vector<uint8_t> ibag;
        {
            uint16_t igenIdx = 0;
            for (size_t i = 0; i < samples.size(); ++i) {
                putU16(ibag, igenIdx); putU16(ibag, 0);
                igenIdx += 4;
            }
            putU16(ibag, igenIdx); putU16(ibag, 0);
        }

        std::vector<uint8_t> imod;
        putU16(imod, 0); putU16(imod, 0); putI16(imod, 0);
        putU16(imod, 0); putU16(imod, 0);

        std::vector<uint8_t> igen;
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto& s = samples[i];
            uint16_t kr = (uint16_t)((s.keyHi << 8) | (s.keyLo & 0xFF));
            putU16(igen, GEN_KEY_RANGE);       putU16(igen, kr);
            putU16(igen, GEN_OVERRIDING_ROOT); putU16(igen, s.originalPitch);
            putU16(igen, GEN_SAMPLE_MODES);    putU16(igen, 1);
            putU16(igen, GEN_SAMPLE_ID);       putU16(igen, (uint16_t)i);
        }
        putU16(igen, 0); putU16(igen, 0);

        std::vector<uint8_t> shdr;
        for (const auto& s : samples) {
            putPadStr(shdr, s.name, 20);
            putU32(shdr, s.startGlobal);
            putU32(shdr, s.endGlobal);
            putU32(shdr, s.loopStartGlobal);
            putU32(shdr, s.loopEndGlobal);
            putU32(shdr, (uint32_t)SR);
            shdr.push_back(s.originalPitch);
            shdr.push_back(0);
            putU16(shdr, 0); putU16(shdr, 1);
        }
        putPadStr(shdr, "EOS", 20);
        for (int i = 0; i < 4; ++i) putU32(shdr, 0);
        putU32(shdr, 0);
        shdr.push_back(0); shdr.push_back(0);
        putU16(shdr, 0); putU16(shdr, 0);

        std::vector<uint8_t> pdtaInner;
        appendChunk(pdtaInner, "phdr", phdr);
        appendChunk(pdtaInner, "pbag", pbag);
        appendChunk(pdtaInner, "pmod", pmod);
        appendChunk(pdtaInner, "pgen", pgen);
        appendChunk(pdtaInner, "inst", inst);
        appendChunk(pdtaInner, "ibag", ibag);
        appendChunk(pdtaInner, "imod", imod);
        appendChunk(pdtaInner, "igen", igen);
        appendChunk(pdtaInner, "shdr", shdr);

        // ---- Top-level RIFF ------------------------------------------------
        std::vector<uint8_t> body;
        body.insert(body.end(), {'s','f','b','k'});
        appendListChunk(body, "INFO", info);
        appendListChunk(body, "sdta", sdtaInner);
        appendListChunk(body, "pdta", pdtaInner);

        // Prepend "RIFF" + body size
        uint32_t bodySz = (uint32_t)body.size();
        std::vector<uint8_t> result;
        result.reserve(8 + body.size());
        result.insert(result.end(), {'R','I','F','F'});
        result.push_back((uint8_t)(bodySz));
        result.push_back((uint8_t)(bodySz >> 8));
        result.push_back((uint8_t)(bodySz >> 16));
        result.push_back((uint8_t)(bodySz >> 24));
        result.insert(result.end(), body.begin(), body.end());
        return result;
    }
}; // class SfmGenerator

// ----------------------------------------------------------------------------
// Default factory presets (the seeds we wrote in Python).
// ----------------------------------------------------------------------------

inline SfmConfig makeDefaultLofiPiano() {
    SfmConfig c;
    c.name      = "Lofi Piano";
    c.synthType = SfmSynth::FM;
    c.attack    = 0.005f;
    c.decay     = 0.40f;
    c.sustain   = 0.45f;
    c.release   = 0.60f;
    c.modIndex  = 4.0f;
    c.modDecay  = 6.0f;
    c.lowNote   = 36;
    c.highNote  = 96;
    c.step      = 4;
    c.gain      = 0.9f;
    return c;
}

} // namespace sfm
