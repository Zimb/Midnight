// =============================================================================
// plugin_vst3.cpp â€“ VST3 implementation of Midnight Melody Maker
//
// Type  : Instrument / Generator (stereo audio out silent + note event out)
// Format: VST3  (.vst3)
// Host  : FL Studio 20+, Cubase, Reaper, Bitwig, â€¦
//
// Parameters (identical to the CLAP version):
//   Key, Mode, Octave, Subdiv, Density, Seed, NoteLen
//
// Build: cmake --build build --config Release --target midnight_melody_vst3
// =============================================================================

#include "../../common/algo.h"

// TinySoundFont â€“ single-header SoundFont2 synthesizer (MIT)
// Provides high-quality sampled instrument sounds from any .sf2 file.
#define TSF_IMPLEMENTATION
#include "../../common/tsf.h"

// VST3 SDK headers â€“ supplied via the sdk target from FetchContent
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "base/source/fstring.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "msimg32.lib")
#include <commdlg.h>

#include <string>

// Plugin DLL instance â€“ captured in DllMain (must NOT use GetModuleHandleW(nullptr)
// inside a VST3 DLL because that returns the host exe handle, not the plugin).
static HINSTANCE g_hInst = nullptr;

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInst = hDll;
        DisableThreadLibraryCalls(hDll);
    }
    return TRUE;
}

using namespace Steinberg;
using namespace Steinberg::Vst;

// -----------------------------------------------------------------------------
// Plugin UID â€“ must never change (saved presets would break).
// {A3F7E291-5CB2-4D18-87A0-4E2C9B8D1F05}
// -----------------------------------------------------------------------------
static const FUID kMelodyMakerUID(0xA3F7E291, 0x5CB24D18, 0x87A04E2C, 0x9B8D1F05);

// -----------------------------------------------------------------------------
// SoundFont preset catalog â€“ per style, only patches that "fit" the role.
// `bank` is 0 for melodic instruments and 128 for drum kits (mididrums flag).
// -----------------------------------------------------------------------------
struct SfPreset { const wchar_t* name; int program; int bank; };

static const SfPreset kSfPresetsByStyle[6][16] = {
    // 0 â€” Voix (vents : bois & cuivres, registre lead/expressif)
    { {L"Fl\u00fbte",          73,  0}, {L"Clarinette",      71,  0},
      {L"Hautbois",         68,  0}, {L"Cor anglais",      69,  0},
      {L"Basson",           70,  0}, {L"Piccolo",          72,  0},
      {L"Pan Flute",        75,  0}, {L"Ocarina",          79,  0},
      {L"Shakuhachi",       77,  0}, {L"Sax Soprano",      64,  0},
      {L"Sax Alto",         65,  0}, {L"Sax T\u00e9nor",   66,  0},
      {L"Trompette",        56,  0}, {L"Trombone",         57,  0},
      {L"Cor Fran\u00e7ais",60,  0}, {L"Brass Section",    61,  0} },
    // 1 â€” Harpe (cordes pincÃ©es, instruments Ã  cordes)
    { {L"Harpe",           46,  0}, {L"Clavecin",         6,  0},
      {L"Clavinet",         7,  0}, {L"Guitare nylon",   24,  0},
      {L"Guitare acier",   25,  0}, {L"Guitare jazz",    26,  0},
      {L"Music Box",       10,  0}, {L"Dulcimer",        15,  0},
      {L"Sitar",          104,  0}, {L"Banjo",          105,  0},
      {L"Shamisen",       106,  0}, {L"Koto",           107,  0},
      {L"Kalimba",        108,  0}, {L"Pizzicato",       45,  0},
      {L"",                 0,  0}, {L"",                 0,  0} },
    // 2 â€” Basse
    { {L"Bass acoustique", 32,  0}, {L"E-Bass doigts",   33,  0},
      {L"E-Bass mÃ©diator", 34,  0}, {L"Fretless",        35,  0},
      {L"Slap 1",          36,  0}, {L"Slap 2",          37,  0},
      {L"Synth Bass 1",    38,  0}, {L"Synth Bass 2",    39,  0},
      {L"Contrebasse arco",43,  0}, {L"Tuba",            58,  0},
      {L"Cor FranÃ§ais",    60,  0}, {L"Trombone",        57,  0},
      {L"",                 0,  0}, {L"",                 0,  0},
      {L"",                 0,  0}, {L"",                 0,  0} },
    // 3 â€” Percu (drum kits, bank 128)
    { {L"Standard",         0,128}, {L"Room",             8,128},
      {L"Power",           16,128}, {L"Electronic",      24,128},
      {L"TR-808",          25,128}, {L"Jazz",            32,128},
      {L"Brush",           40,128}, {L"Orchestra",       48,128},
      {L"",                 0,  0}, {L"",                 0,  0},
      {L"",                 0,  0}, {L"",                 0,  0},
      {L"",                 0,  0}, {L"",                 0,  0},
      {L"",                 0,  0}, {L"",                 0,  0} },
    // 4 â€” Marimba (mailloches, percussions accordÃ©es)
    { {L"Marimba",         12,  0}, {L"Xylophone",       13,  0},
      {L"Vibraphone",      11,  0}, {L"Tubular Bells",   14,  0},
      {L"Glockenspiel",     9,  0}, {L"Celesta",          8,  0},
      {L"Music Box",       10,  0}, {L"Dulcimer",        15,  0},
      {L"Steel Drums",    114,  0}, {L"Kalimba",        108,  0},
      {L"Wood Block",     115,  0}, {L"Timpani",         47,  0},
      {L"",                 0,  0}, {L"",                 0,  0},
      {L"",                 0,  0}, {L"",                 0,  0} },
    // 5 â€” Piano (claviers, accords)
    { {L"Piano",            0,  0}, {L"Piano brillant",   1,  0},
      {L"E-Grand",          2,  0}, {L"Honky-Tonk",       3,  0},
      {L"Rhodes",           4,  0}, {L"FM Piano",         5,  0},
      {L"Clavecin",         6,  0}, {L"Clavinet",         7,  0},
      {L"Cordes",          48,  0}, {L"Synth Strings",   50,  0},
      {L"Choeur Aahs",     52,  0}, {L"Voix Oohs",       53,  0},
      {L"Pad chaud",       89,  0}, {L"Pad polysynth",   90,  0},
      {L"Orgue Hammond",   16,  0}, {L"",                 0,  0} },
};
static constexpr int kSfPresetCount[6] = { 16, 14, 12, 8, 12, 15 };

// =============================================================================
// MelodyMakerVST3
// Arrangement section types (visible to both MelodyMakerVST3 and MelodyMakerView).
enum SectionType { kSecIntro=0, kSecVerse, kSecChorus, kSecBridge, kSecOutro, kSecCount };

// Combines IComponent + IAudioProcessor + IEditController in a single object
// using Steinberg's SingleComponentEffect base class.
// =============================================================================
class MelodyMakerView; // forward

class MelodyMakerVST3 : public SingleComponentEffect
{
public:
    static FUnknown* createInstance(void*) {
        return static_cast<IAudioProcessor*>(new MelodyMakerVST3());
    }

    MelodyMakerVST3() {
        // Random seed per style (different feel for each instrument by default).
        uint32_t s = static_cast<uint32_t>(GetTickCount()) ^ 0x9E3779B9u;
        for (int st = 0; st < kStyleCount; ++st) {
            for (int i = 0; i < (int)mm::kParamCount; ++i)
                paramValuesPerStyle[st][i] = mm::kParamDefs[i].default_value;
            mm::xorshift32(s);
            paramValuesPerStyle[st][mm::kParamSeed] = (double)(s % 1000u);
            wavePerStyle[st] = 0;
            progPerStyle[st] = 0;
        }
        // Style 0 is initially active â†’ mirror its params.
        for (int i = 0; i < (int)mm::kParamCount; ++i)
            paramValues[i] = paramValuesPerStyle[0][i];
        for (auto& n : noteHist) n = -1;
        for (int b = 0; b < 2; ++b)
            for (int p = 0; p < 128; ++p) {
                ctxNoteOnBeat[b][p] = -1.0;
                ctxNoteOnVel [b][p] = 0.0f;
            }
        for (int i = 0; i < 12; ++i)
            pcHist[i].store(0.0, std::memory_order_relaxed);
    }

    // ParamID for the global section (not in mm::kParamDefs — registered separately).
    static constexpr ParamID kParamSection = 50;

    // Friend access for the editor view to call beginEdit / performEdit / endEdit.
    friend class MelodyMakerView;

    // Read the current actual (non-normalised) value for param `id`.
    double getActualParamValue(int id) const {
        if (id < 0 || id >= (int)mm::kParamCount) return 0.0;
        return paramValues[id];
    }

    // Convert actual â†’ normalised [0,1] using the param definition.
    static double actualToNorm(int id, double v) {
        const mm::ParamDef& d = mm::kParamDefs[id];
        double range = d.max_value - d.min_value;
        return (range > 0.0) ? (v - d.min_value) / range : 0.0;
    }

    // VST3: spawn the editor view
    IPlugView* PLUGIN_API createView(FIDString name) override;

    // -------------------------------------------------------------------------
    // IComponent::initialize â€“ add buses + register parameters
    // -------------------------------------------------------------------------
    tresult PLUGIN_API initialize(FUnknown* context) override
    {
        tresult res = SingleComponentEffect::initialize(context);
        if (res != kResultOk) return res;

        // Silent stereo audio output keeps FL Studio happy (it treats the
        // plugin as an instrument/generator rather than a MIDI effect).
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

        // Event input bus â€“ receives MIDI from FL Studio piano keyboard / piano roll.
        addEventInput(STR16("MIDI In"), 1);

        // Second MIDI input â€“ "Context In". Notes routed here are observed
        // (used for auto key/mode detection and voice-leading continuation)
        // but do NOT trigger generation. Connect your existing tracks here.
        addEventInput(STR16("Context In"), 1);

        // One event (note) output bus â€“ this is where the generated notes go.
        addEventOutput(STR16("Notes Out"), 16);

        // Register parameters with the EditController half.
        for (int32 i = 0; i < (int32)mm::kParamCount; ++i) {
            const mm::ParamDef& d = mm::kParamDefs[i];
            String name(d.name);
            auto* p = new RangeParameter(
                name.text16(),
                static_cast<ParamID>(i),
                nullptr,            // unit label
                d.min_value, d.max_value, d.default_value,
                d.is_stepped ? (int32)(d.max_value - d.min_value) : 0,
                ParameterInfo::kCanAutomate
            );
            parameters.addParameter(p);
        }
        // Section parameter (Intro=0 .. Outro=4, default=Verse=1).
        // Registered as a discrete 5-step parameter so the host tracks it live.
        parameters.addParameter(new RangeParameter(
            STR16("Section"), kParamSection, nullptr,
            0.0, 4.0, 1.0, 4, ParameterInfo::kCanAutomate));

        return kResultOk;
    }

    // -------------------------------------------------------------------------
    // IAudioProcessor helpers
    // -------------------------------------------------------------------------
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override {
        sampleRate = setup.sampleRate;
        loadSoundFont();
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API setBusArrangements(
        SpeakerArrangement* inputs,  int32 numIns,
        SpeakerArrangement* outputs, int32 numOuts) override
    {
        // Accept stereo out, no audio in.
        if (numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo)
            return kResultTrue;
        if (numIns == 0 && numOuts == 0)
            return kResultTrue;
        return kResultFalse;
    }

    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) override {
        return (symbolicSampleSize == kSample32) ? kResultTrue : kResultFalse;
    }

    // -------------------------------------------------------------------------
    // IAudioProcessor::process â€“ core callback, called every audio block
    // -------------------------------------------------------------------------
    tresult PLUGIN_API process(ProcessData& data) override
    {
        // -- Apply parameter automation changes --
        if (data.inputParameterChanges) {
            int32 numQ = data.inputParameterChanges->getParameterCount();
            for (int32 q = 0; q < numQ; ++q) {
                IParamValueQueue* queue = data.inputParameterChanges->getParameterData(q);
                if (!queue) continue;
                ParamID pid = queue->getParameterId();
                // Section parameter — update currentSection from automation/host restore.
                if (pid == kParamSection) {
                    int32 nPts = queue->getPointCount();
                    if (nPts > 0) {
                        int32 off; ParamValue nv;
                        queue->getPoint(nPts - 1, off, nv);
                        int sec = std::clamp((int)std::round(nv * 4.0), 0, (int)kSecCount - 1);
                        currentSection.store(sec, std::memory_order_relaxed);
                    }
                    continue;
                }
                if (pid >= mm::kParamCount) continue;
                int32 nPoints = queue->getPointCount();
                if (nPoints > 0) {
                    int32 offset; ParamValue norm;
                    queue->getPoint(nPoints - 1, offset, norm);
                    // norm âˆˆ [0, 1]  â†’  actual param range
                    const mm::ParamDef& d = mm::kParamDefs[pid];
                    paramValues[pid] = d.min_value + norm * (d.max_value - d.min_value);
                    // Mirror into the active style's slot only.
                    // NOTE: we do NOT broadcastParamLocked here because
                    // inputParameterChanges can be a roundtrip from our own
                    // setParamNormalized() call (triggered during tab switch).
                    // Broadcasting here would overwrite all locked tabs with
                    // the newly-loaded style's values, which is the bug.
                    // Locking / broadcasting is already done in sendParam()
                    // (the GUI code path) at the moment the user edits a knob.
                    int activeStyle = getStyleType();
                    paramValuesPerStyle[activeStyle][pid] = paramValues[pid];
                }
            }
        }

        // -- Handle incoming MIDI (piano roll trigger notes) --
        // A note placed in the piano roll = trigger: its pitch defines the
        // root key of the generated melody, and its duration = melody length.
        if (data.inputEvents) {
            int32 numEvIn = data.inputEvents->getEventCount();
            for (int32 e = 0; e < numEvIn; ++e) {
                Event ev{};
                if (data.inputEvents->getEvent(e, ev) == kResultOk) {
                    // bus 0 = main trigger input ; bus 1 = "Context In"
                    // (listen-only, used for auto key/mode detection).
                    int busIdx = ev.busIndex;
                    double evBeat = (data.processContext &&
                                     (data.processContext->state &
                                      ProcessContext::kProjectTimeMusicValid))
                        ? data.processContext->projectTimeMusic
                        : 0.0;
                    if (ev.type == Event::kNoteOnEvent && ev.noteOn.velocity > 0.0f) {
                        ctxNoteOn(busIdx, ev.noteOn.pitch,
                                  ev.noteOn.velocity, evBeat);
                        if (busIdx == 0) {
                            // Add to trigger stack if not already present and not full
                            bool already = false;
                            for (int ti = 0; ti < triggerCount; ++ti)
                                if (triggerStack[ti].pitch == ev.noteOn.pitch) { already = true; break; }
                            if (!already && triggerCount < kMaxTriggers) {
                                // Do NOT reset lastSlot here. When notes are placed
                                // consecutively in the piano roll (C5→E5→C#5…), the
                                // slot grid must continue seamlessly from where the
                                // previous note left off. Resetting to -1 would restart
                                // the rhythmic phase mid-bar, causing the "off-the-measure"
                                // artefact. Legitimate resets (loop restart, transport stop,
                                // backward seek) are handled further below.
                                triggerStack[triggerCount++] = { ev.noteOn.pitch, ev.noteOn.noteId, evBeat };
                            }
                        }
                    } else if (ev.type == Event::kNoteOffEvent) {
                        ctxNoteOff(busIdx, ev.noteOff.pitch, evBeat);
                        if (busIdx == 0) {
                            // Remove matching pitch; long-note triggers section change.
                            for (int ti = 0; ti < triggerCount; ++ti) {
                                if (triggerStack[ti].pitch == ev.noteOff.pitch) {
                                    double holdBeats = evBeat - triggerStack[ti].beatTimeOn;
                                    if (holdBeats >= 1.5 && triggerCount == 1) {
                                        int p = ev.noteOff.pitch % 12;
                                        int sec = (p <= 1) ? kSecIntro
                                                : (p <= 3) ? kSecVerse
                                                : (p <= 5) ? kSecChorus
                                                : (p <= 7) ? kSecBridge
                                                :             kSecOutro;
                                        currentSection.store(sec, std::memory_order_relaxed);
                                        if (sec == kSecIntro && !introFadeDone)  sectionVolumeRamp = 0.0f;
                                        if (sec == kSecOutro)  sectionVolumeRamp = 1.0f;
                                    }
                                    triggerStack[ti] = triggerStack[--triggerCount];
                                    break;
                                }
                            }
                            // Release voices when last trigger key is up.
                            // Use sfNoteOff per active note (graceful release
                            // envelope) rather than killing the synth state,
                            // which would create an audible click/dropout.
                            if (triggerCount == 0) {
                                for (auto& v : voicePool) v.noteOff = true;
                                for (const auto& n : activeNotes)
                                    sfNoteOff(n.noteId, n.key);
                                activeNotes.clear();
                            }
                        }
                    }
                }
            }
        }

        // -- Clear audio output (voices will be mixed in at end of process) --
        if (data.outputs && data.numOutputs > 0) {
            AudioBusBuffers& bus = data.outputs[0];
            bus.silenceFlags = 0;
            for (int32 ch = 0; ch < bus.numChannels; ++ch)
                if (bus.channelBuffers32 && bus.channelBuffers32[ch])
                    std::memset(bus.channelBuffers32[ch], 0,
                                sizeof(float) * static_cast<size_t>(data.numSamples));
        }

        // -- Read transport --
        bool   rolling = false;
        double beatPos = 0.0;
        double bpm     = 120.0;
        if (data.processContext) {
            const ProcessContext& ctx = *data.processContext;
            if (ctx.state & ProcessContext::kTempoValid)
                bpm = ctx.tempo;
            if (ctx.state & ProcessContext::kPlaying)
                rolling = true;
            if (ctx.state & ProcessContext::kProjectTimeMusicValid)
                beatPos = ctx.projectTimeMusic;
        }

        lastBpm.store((float)bpm, std::memory_order_relaxed);
        if (rolling) lastBeatPos.store(beatPos, std::memory_order_relaxed);

        // Detect loop restart or backward seek: reset note-trigger state.
        if (rolling && prevBeatPos >= 0.0 && beatPos < prevBeatPos - 0.25) {
            lastSlot = -1;
            // Gracefully release only the notes that were still pending
            // (so the SoundFont's natural release envelope avoids a click)
            // rather than nuking the whole synth state.
            IEventList* loopOutEvents = data.outputEvents;
            for (const auto& n : activeNotes) {
                if (loopOutEvents) {
                    Event off{};
                    off.type             = Event::kNoteOffEvent;
                    off.sampleOffset     = 0;
                    off.noteOff.channel  = n.channel;
                    off.noteOff.pitch    = n.key;
                    off.noteOff.noteId   = n.noteId;
                    off.noteOff.velocity = 0.f;
                    loopOutEvents->addEvent(off);
                }
                releaseVoice(n.noteId, n.key); // calls sfNoteOff (smooth release)
            }
            activeNotes.clear();
            if (recMtx.try_lock()) { recNotes.clear(); recMtx.unlock(); }
            recStartBeat = beatPos;
        }
        // Detect transport start: begin fresh recording
        if (rolling && prevBeatPos < 0.0) {
            if (recMtx.try_lock()) { recNotes.clear(); recMtx.unlock(); }
            recStartBeat = beatPos;
        }
        prevBeatPos = rolling ? beatPos : -1.0;

        IEventList* outEvents = data.outputEvents;
        const uint32 nframes  = static_cast<uint32>(data.numSamples);

        // -- Send MIDI CCs to downstream synth (e.g. Sytrus) on style change --
        // CCs shape the timbre to match the current style automatically.
        {
            int curStyle = getStyleType();
            if (curStyle != lastSentStyle && outEvents) {
                // {CC number, value per style[0..4]}
                // CC73=Attack  CC72=Release  CC74=Brightness  CC71=Resonance  CC91=Reverb
                struct CcPreset { uint8 cc; int8 vals[5]; };
                static const CcPreset kPresets[] = {
                    //          Mel  Harp Basse Perc  Mari
                    { 73, { 40,   5,  25,    0,   8 } }, // Attack
                    { 72, { 60,  90, 110,   10,  30 } }, // Release
                    { 74, { 64, 100,  30,  110,  90 } }, // Brightness
                    { 71, { 20,  50,  80,   15,  35 } }, // Resonance
                    { 91, { 30,  60,  15,    5,  20 } }, // Reverb
                };
                for (auto& p : kPresets) {
                    Event cc{};
                    cc.type                   = Event::kLegacyMIDICCOutEvent;
                    cc.sampleOffset           = 0;
                    cc.midiCCOut.channel      = 0;
                    cc.midiCCOut.controlNumber = p.cc;
                    cc.midiCCOut.value        = p.vals[curStyle];
                    cc.midiCCOut.value2       = 0;
                    outEvents->addEvent(cc);
                }
                lastSentStyle = curStyle;
            }
        }

        // -- Flush pending note-offs that fall within this block --
        for (auto it = activeNotes.begin(); it != activeNotes.end();) {
            if (it->offSample < static_cast<int64_t>(nframes)) {
                if (outEvents) {
                    Event off{};
                    off.type          = Event::kNoteOffEvent;
                    off.sampleOffset  = static_cast<int32>(std::max(0LL, it->offSample));
                    off.noteOff.channel  = it->channel;
                    off.noteOff.pitch    = it->key;
                    off.noteOff.noteId   = it->noteId;
                    off.noteOff.velocity = 0.f;
                    outEvents->addEvent(off);
                }
                releaseVoice(it->noteId, it->key);
                it = activeNotes.erase(it);
            } else {
                it->offSample -= static_cast<int64_t>(nframes);
                ++it;
            }
        }

        if (!rolling) {
            triggerCount = 0;
            // Kill any still-sounding notes when transport stops.
            if (outEvents && !activeNotes.empty()) {
                for (auto& n : activeNotes) {
                    Event off{};
                    off.type             = Event::kNoteOffEvent;
                    off.sampleOffset     = 0;
                    off.noteOff.channel  = n.channel;
                    off.noteOff.pitch    = n.key;
                    off.noteOff.noteId   = n.noteId;
                    off.noteOff.velocity = 0.f;
                    outEvents->addEvent(off);
                    releaseVoice(n.noteId, n.key);
                }
                activeNotes.clear();
                sfAllNotesOff();
            }
            lastSlot = -1;
            renderVoices(data.outputs, data.numOutputs, nframes);
            return kResultOk;
        }

        // Only generate melody while at least one trigger note is held.
        if (!hasTrigger()) {
            renderVoices(data.outputs, data.numOutputs, nframes);
            return kResultOk;
        }

        // -- Generate notes on beat subdivisions --
        double bps           = std::max(0.05, bpm / 60.0);
        double beatsPerSample = bps / sampleRate;
        double blockEndBeat  = beatPos + static_cast<double>(nframes) * beatsPerSample;

        // Master grid uses the active tab's subdiv (one shared rhythmic
        // grid keeps instruments in sync). Density / NoteLen / Seed remain
        // independent per instrument and are applied inside the style loop.
        int    subdiv      = mm::normalized_subdiv(paramValues[mm::kParamSubdiv]);
        double slotBeats   = 1.0 / static_cast<double>(subdiv);

        // Update global section volume ramp once per process block.
        {
            static constexpr float kSecVol[kSecCount] = { 1.0f, 1.0f, 1.15f, 0.75f, 0.0f };
            int   sec    = currentSection.load(std::memory_order_relaxed);
            float target = kSecVol[std::clamp(sec, 0, (int)kSecCount - 1)];
            float bblock = (float)((double)nframes * beatsPerSample);
            float rate   = bblock / ((sec==kSecIntro || sec==kSecOutro) ? 16.0f : 4.0f);
            if (sectionVolumeRamp < target) sectionVolumeRamp = std::min(target, sectionVolumeRamp + rate);
            else                            sectionVolumeRamp = std::max(target, sectionVolumeRamp - rate);
            // Mark the Intro fade as done once volume reaches 1 for the first time.
            if (!introFadeDone && sectionVolumeRamp >= 1.0f && currentSection.load(std::memory_order_relaxed) == kSecIntro)
                introFadeDone = true;
        }

        int64_t firstSlot = static_cast<int64_t>(
            std::ceil(beatPos / slotBeats - 1e-9));
        if (firstSlot <= lastSlot) firstSlot = lastSlot + 1;

        for (int64_t slot = firstSlot; ; ++slot) {
            double slotBeat = slot * slotBeats;
            if (slotBeat >= blockEndBeat) break;

            double offsetBeats = slotBeat - beatPos;
            int32  sampleOffset = static_cast<int32>(
                std::max(0.0, std::floor(offsetBeats / beatsPerSample)));
            if (sampleOffset >= static_cast<int32>(nframes)) break;

            // Compute slot position weight (shared across instruments).
            int barLen     = subdiv * 4;
            int posInBar   = (int)(((slot % barLen) + barLen) % barLen);
            int beatInBar  = posInBar / subdiv;          // 0..3
            int subInBeat  = posInBar % subdiv;
            bool isDown    = (subInBeat == 0);
            bool isStrong  = isDown && (beatInBar % 2 == 0);
            double posWeight = isStrong ? 1.20 : isDown ? 1.00 : 0.55;
            lastSlot = slot;

            const uint32_t enabledMask = getEnabledMask();
            // Per-style velocity is computed inside the loop now.

            // ---- Auto key/mode from listened context ----------------------
            // Run Krumhansl-Schmuckler on the rolling pitch-class histogram
            // collected from incoming MIDI on both buses. Only override the
            // user params when we have enough material to be confident.
            int detectedKey = -1, detectedMode = -1;
            if (getAutoKey()) {
                double hist[12];
                ctxSnapshot(hist);
                double total = 0.0;
                for (int i = 0; i < 12; ++i) total += hist[i];
                if (total > 2.0) {
                    int r, m;
                    mm::detect_key(hist, r, m);
                    detectedKey  = r;
                    detectedMode = m; // 0=Major, 1=Minor (matches our enum)
                }
                // Slow decay so the analyser tracks modulations over time
                // (â‰ˆ 0.5% per generation slot).
                ctxDecay(0.995);
            }

            // Iterate over every enabled instrument tab. Each style now has
            // its own Density / NoteLen / Seed / Mode / Progression /
            // Wave â€” only the master rhythmic grid (Subdiv) is shared.
            for (int styleIdx = 0; styleIdx < kStyleCount; ++styleIdx) {
                if (!((enabledMask >> styleIdx) & 1u)) continue;
                // Per-style start-bar: suppress notes until N bars have elapsed.
                if (startBarPerStyle[styleIdx] > 0) {
                    int64_t currentBar = slot / (int64_t)barLen;
                    if (currentBar < (int64_t)startBarPerStyle[styleIdx]) continue;
                }
                const int style    = styleIdx;
                const double* sp   = paramValuesPerStyle[styleIdx];
                const int    sSeed = (int)std::round(sp[mm::kParamSeed]);
                const double sDens = std::clamp(sp[mm::kParamDensity], 0.0, 1.0);
                const double sNL   = std::clamp(sp[mm::kParamNoteLen], 0.05, 1.0);
                const int    sProg = std::clamp(progPerStyle[styleIdx],
                                                0, mm::kProgressionCount - 1);

                // Per-style density gate (independent RNG per instrument).
                // Mix EVERY style param into the seed so any GUI tweak
                // instantly produces a different roll â€” Density, NoteLen,
                // Mode, Wave, Progression, percRhythm, pianoMelody, even
                // Subdiv all participate. This guarantees real-time
                // refresh of the audio output on every parameter change.
                uint32_t paramSalt = 0u;
                for (int pi = 0; pi < (int)mm::kParamCount; ++pi) {
                    // Quantize to int to ignore tiny float jitter from host.
                    int32_t q = (int32_t)std::round(sp[pi] * 1000.0);
                    paramSalt = paramSalt * 2654435761u
                              + static_cast<uint32_t>(q) + 0x9E3779B9u;
                }
                paramSalt ^= static_cast<uint32_t>(wavePerStyle[styleIdx])    * 0x85EBCA6Bu;
                paramSalt ^= static_cast<uint32_t>(progPerStyle[styleIdx])    * 0xC2B2AE35u;
                paramSalt ^= static_cast<uint32_t>(percRhythmPerStyle[styleIdx]) * 0x27D4EB2Fu;
                paramSalt ^= static_cast<uint32_t>(pianoMelodyPerStyle[styleIdx] ? 1u : 0u) * 0x165667B1u;

                uint32_t rng = static_cast<uint32_t>(
                    slot * 2246822519u
                    + static_cast<uint32_t>(sSeed)    * 374761393u
                    + static_cast<uint32_t>(styleIdx) * 2654435761u
                    + paramSalt + 1u);
                if (!rng) rng = 1;

                // Per-style timing offset: Humanize (random jitter) + Retard (groove delay).
                float humanize = humanizePerStyle[styleIdx];
                float retard   = retardPerStyle[styleIdx];
                int32 retardSamples = (int32)std::round(
                    retard * slotBeats * 0.5 / beatsPerSample);
                int32 humanizeSamples = 0;
                if (humanize > 0.0f) {
                    uint32_t hr = rng ^ 0xFACE7777u ^ (uint32_t)(styleIdx * 99991u);
                    float ratio = ((float)(mm::xorshift32(hr) % 2001) / 2000.f - 0.5f) * 2.0f;
                    humanizeSamples = (int32)std::round(
                        ratio * humanize * slotBeats * 0.25 / beatsPerSample);
                }
                int32 styleOffset = std::max((int32)0,
                    std::min((int32)nframes - 1,
                             sampleOffset + retardSamples + humanizeSamples));


                // own density via the pattern / chord layout. Gating them
                // with the global Density knob would punch random holes in
                // the rhythm, which is exactly the "pauses gÃªnantes" the
                // user reported. Density is therefore only applied to the
                // melodic styles (MÃ©lodie / Harpe / Basse / Marimba).
                if (style != 3 && style != 5) {
                    double roll = static_cast<double>(mm::xorshift32(rng))
                                / static_cast<double>(0xFFFFFFFFu);
                    // Strong beats (beat 1/3): floor at 65%, reaches 100% at max density.
                    // Downbeats (all beat onsets): start firing at Density >= 25%.
                    // Off-beats (subdivisions): start firing at Density >= 55%.
                    double effDensity;
                    if (isStrong) {
                        effDensity = 0.65 + 0.35 * sDens;
                    } else if (isDown) {
                        effDensity = (sDens > 0.25) ? (sDens - 0.25) / 0.75 : 0.0;
                    } else {
                        effDensity = (sDens > 0.55) ? (sDens - 0.55) / 0.45 : 0.0;
                    }
                    if (roll > effDensity) continue;
                } else {
                    // Still consume one RNG step so the velocity humanizer
                    // below stays seeded the same way for all styles.
                    mm::xorshift32(rng);
                }

                // Velocity roll, also per style.
                uint32_t velRng = rng;
                float baseVelShared = 0.7f + 0.25f * (
                    static_cast<float>(mm::xorshift32(velRng))
                    / static_cast<float>(0xFFFFFFFFu));
                if (baseVelShared > 1.f) baseVelShared = 1.f;

                // Generate one note per held trigger key.
                // CHORD MODE: when 2+ trigger notes are held simultaneously,
                // analyse them as a chord and use the detected harmonic context
                // (root, mode, progression) for ALL styles — instead of naively
                // transposing the same melody N times (which sounds chaotic).
                // Single-note trigger keeps the original behaviour (key = trigger pitch).
                mm::ChordContext chordCtx{-1, -1, -1};
                if (triggerCount >= 2) {
                    int pitches[8];
                    for (int ti = 0; ti < triggerCount; ++ti)
                        pitches[ti] = triggerStack[ti].pitch;
                    chordCtx = mm::detect_chord(pitches, triggerCount);
                }

                // When in chord mode, generate one voice (not N duplicates).
                int effectiveTriggers = (chordCtx.root >= 0) ? 1 : triggerCount;

                for (int ti = 0; ti < effectiveTriggers; ++ti) {
                    // Drums and Piano-chords don't follow trigger pitch â€“
                    // they only need to fire once per slot regardless of
                    // how many MIDI keys are held.
                    if ((style == 3 || style == 5) && ti > 0) continue;
                    // Build per-trigger params: take this style's params, then
                    // overwrite Key/Octave from the trigger pitch (and from
                    // chord detection or auto-key detection if active).
                    double trigParams[mm::kParamCount];
                    std::copy(sp, sp + mm::kParamCount, trigParams);
                    // GUI Octave knob = offset relative to the trigger note's octave.
                    // Param stores 3-6, default=4 (→ +0). So offset = guiOct - 4.
                    int guiOctOffset = (int)std::round(sp[mm::kParamOctave]) - 4;
                    if (chordCtx.root >= 0) {
                        // Chord mode: the detected root sets the KEY (tonal centre),
                        // but the MODE knob in the GUI stays as the user's colour choice.
                        // Example: C+E+G (major triad) + Mode=Lydien → C Lydien,
                        // the #4 (F#) is the added colour note. Only the progression
                        // is suggested by the chord quality (it fits that harmony best).
                        int lowestPitch = triggerStack[0].pitch;
                        for (int ti2 = 1; ti2 < triggerCount; ++ti2)
                            if (triggerStack[ti2].pitch < lowestPitch)
                                lowestPitch = triggerStack[ti2].pitch;
                        trigParams[mm::kParamKey]  = (double)chordCtx.root;
                        // kParamMode is NOT overridden — GUI value (sp[kParamMode]) is kept.
                        int oct = (lowestPitch / 12) - 1 + guiOctOffset;
                        trigParams[mm::kParamOctave] = (double)std::clamp(oct, 3, 6);
                    } else {
                        // Single-note mode: trigger pitch + GUI octave offset.
                        trigParams[mm::kParamKey] = (double)(triggerStack[ti].pitch % 12);
                        int oct = (triggerStack[ti].pitch / 12) - 1 + guiOctOffset;
                        trigParams[mm::kParamOctave] = (double)std::clamp(oct, 3, 6);
                    }
                    if (detectedKey >= 0 && chordCtx.root < 0) {
                        // Auto-key only applies in single-note mode.
                        trigParams[mm::kParamKey]  = (double)detectedKey;
                        trigParams[mm::kParamMode] = (double)detectedMode;
                    }
                    // Effective progression: chord-detected overrides style setting.
                    int effectiveProg = (chordCtx.prog >= 0)
                        ? chordCtx.prog
                        : sProg;
                    // Vary pitch rng per trigger so voices aren't identical
                    uint32_t trigRng = rng ^ (uint32_t)(ti * 1234567u + styleIdx * 2654435761u + 1u);
                    if (!trigRng) trigRng = 1;
                    mm::xorshift32(trigRng);

                    // Build the list of voices to emit for this slot. Most
                    // styles produce a single pitch; Piano emits a 3-note
                    // chord (+optional melody) and Percussion can emit up
                    // to 3 simultaneous drums from the active rhythm.
                    StyleVoices voices;
                    voicesForStyle(style, trigParams,
                        slot + (int64_t)(ti * 7919) + (int64_t)(styleIdx * 104729),
                        effectiveProg,
                        pianoMelodyPerStyle[styleIdx],
                        pianoChordPerStyle[styleIdx],
                        percRhythmPerStyle[styleIdx],
                        voices);
                    if (voices.count == 0) continue;

                    for (int vi = 0; vi < voices.count; ++vi) {
                        int16_t pitch = voices.pitches[vi];
                        float velocity;
                        if (style == 3) {
                            // Drum patterns already encode their own accents
                            // (kick = 110, ghost snare = 40 â€¦). Re-applying
                            // velForStyle's strong/weak-beat accent would
                            // squash that character and make every rhythm
                            // sound the same. Use the pattern's velocity
                            // directly + a tiny seed-driven humanization.
                            uint32_t hRng = rng ^ (uint32_t)(vi * 2654435761u + 0x9E37u);
                            if (!hRng) hRng = 1;
                            float jitter = ((float)(mm::xorshift32(hRng) % 1000) / 1000.f - 0.5f) * 0.12f;
                            velocity = voices.vels[vi] * (1.0f + jitter);
                        } else {
                            velocity = velForStyle(style, slot, sp, baseVelShared)
                                     * voices.vels[vi];
                        }
                        if (velocity > 1.f) velocity = 1.f;
                        if (velocity < 0.05f) velocity = 0.05f;
                        // Apply section envelope.
                        velocity *= sectionVolumeRamp;
                        if (velocity > 1.f) velocity = 1.f;
                        if (velocity < 0.0f) velocity = 0.0f;
                        int32 noteId  = nextNoteId++;
                    // Fixed MIDI channel per style for multi-timbral hosts (e.g. Omnisphere).
                    int16 channel = (int16)styleIdx;  // one fixed channel per style (0=Voix..5=Piano)

                    if (outEvents) {
                        Event on{};
                        on.type              = Event::kNoteOnEvent;
                        on.sampleOffset      = styleOffset;
                        on.noteOn.channel    = channel;
                        on.noteOn.pitch      = static_cast<int16>(pitch);
                        on.noteOn.velocity   = velocity;
                        on.noteOn.noteId     = noteId;
                        outEvents->addEvent(on);
                    }
                    if (ti == 0 && style == getStyleType()) {
                        uint32_t h = noteHistHead.fetch_add(1, std::memory_order_relaxed);
                        noteHist[h % kNoteHistSize] = pitch;
                    }

                    // Schedule the corresponding note-off.
                    double  noteBeats = noteLenForStyle(style, slotBeats, sNL);
                    if (style != 3) {
                        // Small random variation around the base length (±30% max).
                        // Keep it relative so NoteLen=100% stays long.
                        uint32_t lenRng = rng ^ 0xA5A5u ^ (uint32_t)(styleIdx * 13);
                        uint32_t r = mm::xorshift32(lenRng);
                        if      (isStrong && (r % 5) == 0) noteBeats *= 1.40;
                        else if (isStrong && (r % 7) == 0) noteBeats *= 1.80;
                        else if (!isDown  && (r % 6) == 0) noteBeats *= 0.65;
                    }
                    int64_t offSample = static_cast<int64_t>(styleOffset)
                                      + static_cast<int64_t>(
                                            std::round(noteBeats / beatsPerSample));
                    if (offSample <= styleOffset) offSample = styleOffset + 1;

                    const bool isPerc = (style == 3);
                    triggerVoice(pitch, velocity,
                        offSample < (int64_t)nframes ? offSample : -1, isPerc, noteId,
                        styleIdx);

                    if (recMtx.try_lock()) {
                        if (recNotes.size() < 65536)
                            recNotes.push_back({slotBeat - recStartBeat, noteBeats, pitch, velocity, (uint8_t)style});
                        recMtx.unlock();
                    }

                    if (offSample < static_cast<int64_t>(nframes)) {
                        if (outEvents) {
                            Event off{};
                            off.type             = Event::kNoteOffEvent;
                            off.sampleOffset     = static_cast<int32>(offSample);
                            off.noteOff.channel  = channel;
                            off.noteOff.pitch    = static_cast<int16>(pitch);
                            off.noteOff.noteId   = noteId;
                            off.noteOff.velocity = 0.f;
                            outEvents->addEvent(off);
                        }
                    } else {
                        activeNotes.push_back({
                            static_cast<int16_t>(pitch), channel, noteId,
                            offSample - static_cast<int64_t>(nframes)
                        });
                    }
                    } // end voice loop
                } // end trigger loop
            } // end style loop
        }

        // -- Render internal oscillator voices to audio output --
        renderVoices(data.outputs, data.numOutputs, nframes);

        return kResultOk;
    }

    // -------------------------------------------------------------------------
    // State persistence
    // -------------------------------------------------------------------------
    tresult PLUGIN_API getState(IBStream* stream) override {
        IBStreamer s(stream, kLittleEndian);
        // Make sure the live mirror is committed to its style slot first.
        const_cast<MelodyMakerVST3*>(this)->cacheCurrentStyleParams(getStyleType());
        // Magic + version for forward compat (v2 = per-style params).
        s.writeInt32(0x4D4D4D32);   // 'MMM2'
        s.writeInt32(2);
        s.writeInt32(getStyleType());
        s.writeInt32(static_cast<int32>(getEnabledMask()));
        s.writeInt32(getAutoKey() ? 1 : 0);
        for (int st = 0; st < kStyleCount; ++st) {
            for (int i = 0; i < (int)mm::kParamCount; ++i)
                s.writeDouble(paramValuesPerStyle[st][i]);
            s.writeInt32(wavePerStyle[st]);
            s.writeInt32(progPerStyle[st]);
        }
        // v2.1 trailing fields (per-style piano/perc options).
        for (int st = 0; st < kStyleCount; ++st) {
            s.writeInt32(pianoMelodyPerStyle[st] ? 1 : 0);
            s.writeInt32(percRhythmPerStyle[st]);
        }
        // v2.2 trailing fields (per-style SoundFont preset index + volume).
        for (int st = 0; st < kStyleCount; ++st) {
            s.writeInt32(sfPresetIdxPerStyle[st]);
            s.writeFloat(sfVolumePerStyle[st]);
        }
        // v2.4 trailing fields (per-tab lock toggles for Mode/Prog/Subdiv).
        for (int st = 0; st < kStyleCount; ++st) s.writeInt32(lockModePerStyle[st]   ? 1 : 0);
        for (int st = 0; st < kStyleCount; ++st) s.writeInt32(lockProgPerStyle[st]   ? 1 : 0);
        for (int st = 0; st < kStyleCount; ++st) s.writeInt32(lockSubdivPerStyle[st] ? 1 : 0);
        // v2.5 trailing fields (piano chord toggle).
        for (int st = 0; st < kStyleCount; ++st) s.writeInt32(pianoChordPerStyle[st] ? 1 : 0);
        // v2.6 trailing field (global time signature override: 0=auto, 2-7=manual).
        s.writeInt32(beatsPerBarOverride);
        // v2.7 trailing fields (per-style humanize + retard).
        for (int st = 0; st < kStyleCount; ++st) s.writeFloat(humanizePerStyle[st]);
        for (int st = 0; st < kStyleCount; ++st) s.writeFloat(retardPerStyle[st]);
        // v2.8: section + per-style start bar.
        s.writeInt32(currentSection.load(std::memory_order_relaxed));
        for (int st = 0; st < kStyleCount; ++st) s.writeInt32(startBarPerStyle[st]);
        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream* stream) override {
        IBStreamer s(stream, kLittleEndian);
        int32 magic = 0;
        // Peek the first int. v2 starts with 'MMM2', v1 starts with the
        // first param double (so the first 4 bytes won't match the magic).
        if (!s.readInt32(magic)) return kResultOk;
        if (magic == 0x4D4D4D32) {
            int32 version = 0; s.readInt32(version);
            int32 st = 0; s.readInt32(st);
            int32 em = 0; s.readInt32(em);
            int32 ak = 0; s.readInt32(ak);
            for (int sti = 0; sti < kStyleCount; ++sti) {
                for (int i = 0; i < (int)mm::kParamCount; ++i) {
                    double v = mm::kParamDefs[i].default_value;
                    s.readDouble(v);
                    paramValuesPerStyle[sti][i] = v;
                }
                int32 w = 0; s.readInt32(w); wavePerStyle[sti] = std::clamp((int)w, 0, 3);
                int32 p = 0; s.readInt32(p); progPerStyle[sti] = std::clamp((int)p, 0, mm::kProgressionCount - 1);
            }
            // v2.1 trailing fields (best-effort, ignore if missing).
            for (int sti = 0; sti < kStyleCount; ++sti) {
                int32 pm = 0;
                if (s.readInt32(pm)) pianoMelodyPerStyle[sti] = (pm != 0);
                int32 pr = 0;
                if (s.readInt32(pr))
                    percRhythmPerStyle[sti] = std::clamp((int)pr, 0, mm::kDrumPatternCount - 1);
            }
            // v2.2 trailing fields (per-style SF preset/volume).
            for (int sti = 0; sti < kStyleCount; ++sti) {
                int32 sp = 0;
                if (s.readInt32(sp))
                    sfPresetIdxPerStyle[sti] = std::clamp((int)sp, 0, kSfPresetCount[sti] - 1);
                float sv = 0.85f;
                if (s.readFloat(sv))
                    sfVolumePerStyle[sti] = std::clamp(sv, 0.0f, 1.5f);
            }
            // v2.4 trailing fields (per-tab lock toggles). Best-effort:
            // if absent (older state) all tabs stay at their default = locked.
            for (int sti = 0; sti < kStyleCount; ++sti) {
                int32 lm = 1; if (s.readInt32(lm)) lockModePerStyle[sti]   = (lm != 0);
            }
            for (int sti = 0; sti < kStyleCount; ++sti) {
                int32 lp = 1; if (s.readInt32(lp)) lockProgPerStyle[sti]   = (lp != 0);
            }
            for (int sti = 0; sti < kStyleCount; ++sti) {
                int32 ls = 1; if (s.readInt32(ls)) lockSubdivPerStyle[sti] = (ls != 0);
            }
            // v2.5 trailing fields (piano chord toggle). Best-effort.
            for (int sti = 0; sti < kStyleCount; ++sti) {
                int32 pc = 1; if (s.readInt32(pc)) pianoChordPerStyle[sti] = (pc != 0);
            }
            // v2.6 trailing field (global time signature override). Best-effort.
            {
                int32 bpo = 0;
                if (s.readInt32(bpo)) {
                    beatsPerBarOverride = std::clamp((int)bpo, 0, 7);
                    if (beatsPerBarOverride >= 2)
                        beatsPerBarAtomic.store(beatsPerBarOverride, std::memory_order_relaxed);
                }
            }
            // v2.7 trailing fields (per-style humanize + retard). Best-effort.
            for (int sti = 0; sti < kStyleCount; ++sti) {
                float hv = 0.0f;
                if (s.readFloat(hv)) humanizePerStyle[sti] = std::clamp(hv, 0.0f, 1.0f);
            }
            for (int sti = 0; sti < kStyleCount; ++sti) {
                float rv = 0.0f;
                if (s.readFloat(rv)) retardPerStyle[sti] = std::clamp(rv, 0.0f, 1.0f);
            }
            // v2.8: section + per-style start bar. Best-effort.
            { int32 sec = (int32)kSecVerse;
              if (s.readInt32(sec)) currentSection.store(
                  std::clamp((int)sec, 0, (int)kSecCount - 1), std::memory_order_relaxed); }
            { static constexpr int kSBV[] = { 0,1,2,4,8 };
              for (int sti2 = 0; sti2 < kStyleCount; ++sti2) {
                  int32 sb = 0;
                  if (s.readInt32(sb)) {
                      int best=0, bestD=abs((int)sb);
                      for (int k=1;k<5;k++) { int d=abs((int)sb-kSBV[k]); if(d<bestD){bestD=d;best=k;} }
                      startBarPerStyle[sti2] = kSBV[best];
                  }
              }
            }
            int active = std::clamp((int)st, 0, kStyleCount - 1);
            styleType.store(active, std::memory_order_relaxed);
            loadStyleParams(active);
            uint32_t mask = static_cast<uint32_t>(em) & ((1u << kStyleCount) - 1u);
            if (mask == 0) mask = 1u << active;
            setEnabledMask(mask);
            setAutoKey(ak != 0);
            // Push live params to the host EditController.
            for (int i = 0; i < (int)mm::kParamCount; ++i) {
                const mm::ParamDef& d = mm::kParamDefs[i];
                double range = d.max_value - d.min_value;
                double norm  = (range > 0.0) ? (paramValues[i] - d.min_value) / range : 0.0;
                setParamNormalized((ParamID)i, std::clamp(norm, 0.0, 1.0));
            }
            return kResultOk;
        }
        // ---- Legacy v1 path: rewind and read old-style flat layout. ----
        // The stream interface has no real seek-back, so reinterpret the
        // already-consumed magic as the first parameter's first 4 bytes.
        // Simpler: build paramValues from defaults if the stream layout is
        // unrecognised. We accept losing legacy-state for the dev iterations.
        for (int i = 0; i < (int)mm::kParamCount; ++i) {
            double v = mm::kParamDefs[i].default_value;
            paramValues[i] = v;
            paramValuesPerStyle[0][i] = v;
            const mm::ParamDef& d = mm::kParamDefs[i];
            double range = d.max_value - d.min_value;
            double norm  = (range > 0.0) ? (v - d.min_value) / range : 0.0;
            setParamNormalized(static_cast<ParamID>(i), norm);
        }
        return kResultOk;
    }

    tresult PLUGIN_API setComponentState(IBStream* stream) override {
        // Combined component + controller â†’ delegate to setState.
        return setState(stream);
    }

private:
    double   sampleRate   = 48000.0;
    double   paramValues[mm::kParamCount];
    // Per-style backup of every parameter, including wave & progression.
    // `paramValues`, `waveType`, `progType` always mirror the *currently
    // selected* style, so existing code paths keep working unchanged.
    double   paramValuesPerStyle[6][mm::kParamCount];
    int      wavePerStyle[6];
    int      progPerStyle[6];
    // Per-style SoundFont preset (index into kSfPresetsByStyle[s]) and volume.
    int    sfPresetIdxPerStyle[6] = { 0, 0, 0, 0, 0, 0 };
    float  sfVolumePerStyle[6]    = { 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f };
    // Piano-only: when true, the Piano tab also emits a right-hand
    // melody on top of its left-hand chords.
    bool     pianoMelodyPerStyle[6] = { false, false, false, false, false, false };
    // Piano-only: when true, play full 3-note chords; when false, single root notes.
    bool     pianoChordPerStyle[6]  = { true,  true,  true,  true,  true,  true  };
    // Percussion-only: index into `mm::kDrumPatterns`.
    int      percRhythmPerStyle[6]  = { 0, 0, 0, 0, 0, 0 };
    // Global time-signature override: 0=auto (from DAW), 2-7=manual.
    int      beatsPerBarOverride    = 0;
    std::atomic<int> beatsPerBarAtomic{4};
    // Per-style timing feel: 0=tight, 1=max humanization (random timing jitter).
    float    humanizePerStyle[6]    = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    // Per-style groove retard: 0=on-grid, 1=max retard (fraction of one slot).
    float    retardPerStyle[6]      = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    // Global arrangement section.
    std::atomic<int> currentSection{kSecVerse};
    float sectionVolumeRamp = 1.0f;  // audio-thread-only fader
    bool  introFadeDone = false;     // true once the Intro fade has completed once
    // Per-style note delay in bars before instrument begins.
    int  startBarPerStyle[6] = { 0, 0, 0, 0, 0, 0 };

public:
    // -------- Per-tab lock toggles --------------------------------------
    // Each tab has its own padlock for Mode / Progression / Subdiv.
    // Default = all locked. Behaviour:
    //   - Editing param P on tab S writes S's slot, then broadcasts the
    //     new value to every OTHER tab T whose lock for P is also closed.
    //     A tab whose padlock is open keeps its independent value.
    //   - Re-locking tab S (false -> true) immediately propagates S's
    //     current value to every other locked tab.
    bool lockModePerStyle  [6] = { true, true, true, true, true, true };
    bool lockProgPerStyle  [6] = { true, true, true, true, true, true };
    bool lockSubdivPerStyle[6] = { true, true, true, true, true, true };

    // Broadcast `param` value from style `src` to every other tab whose
    // lock for that param is closed. `src` must already be up to date.
    void broadcastParamLocked(int param, int src) {
        if (src < 0 || src >= kStyleCount) return;
        const bool* mask = nullptr;
        switch (param) {
            case mm::kParamMode:   mask = lockModePerStyle;   break;
            case mm::kParamSubdiv: mask = lockSubdivPerStyle; break;
            default: return;
        }
        if (!mask[src]) return; // source itself is unlocked: no broadcast
        double v = paramValuesPerStyle[src][param];
        for (int st = 0; st < kStyleCount; ++st) {
            if (st == src) continue;
            if (!mask[st]) continue;
            paramValuesPerStyle[st][param] = v;
        }
    }
    // Same logic for progression (stored separately as int).
    void broadcastProgLocked(int src) {
        if (src < 0 || src >= kStyleCount) return;
        if (!lockProgPerStyle[src]) return;
        int v = progPerStyle[src];
        for (int st = 0; st < kStyleCount; ++st) {
            if (st == src) continue;
            if (!lockProgPerStyle[st]) continue;
            progPerStyle[st] = v;
        }
    }

    void setLockMode(int s, bool b) {
        if (s < 0 || s >= kStyleCount) return;
        lockModePerStyle[s] = b;
        if (b) broadcastParamLocked(mm::kParamMode, s);
    }
    void setLockProg(int s, bool b) {
        if (s < 0 || s >= kStyleCount) return;
        lockProgPerStyle[s] = b;
        if (b) broadcastProgLocked(s);
    }
    void setLockSubdiv(int s, bool b) {
        if (s < 0 || s >= kStyleCount) return;
        lockSubdivPerStyle[s] = b;
        if (b) broadcastParamLocked(mm::kParamSubdiv, s);
    }

private:
    void cacheCurrentStyleParams(int s) {
        if (s < 0 || s >= kStyleCount) return;
        for (int i = 0; i < (int)mm::kParamCount; ++i)
            paramValuesPerStyle[s][i] = paramValues[i];
        wavePerStyle[s] = waveType.load(std::memory_order_relaxed);
        progPerStyle[s] = progType.load(std::memory_order_relaxed);
    }
    void loadStyleParams(int s) {
        if (s < 0 || s >= kStyleCount) return;
        for (int i = 0; i < (int)mm::kParamCount; ++i)
            paramValues[i] = paramValuesPerStyle[s][i];
        waveType.store(wavePerStyle[s], std::memory_order_relaxed);
        progType.store(progPerStyle[s], std::memory_order_relaxed);
    }
public:
    // GUI helper: randomize the seed of the active style.
    void randomizeSeed() {
        uint32_t s = (uint32_t)GetTickCount() ^ 0x12345678u;
        mm::xorshift32(s);
        double v = (double)(s % 1000u);
        paramValues[mm::kParamSeed] = v;
        paramValuesPerStyle[getStyleType()][mm::kParamSeed] = v;
        // Sync to the EditController.
        const mm::ParamDef& d = mm::kParamDefs[mm::kParamSeed];
        double range = d.max_value - d.min_value;
        double norm  = (range > 0.0) ? (v - d.min_value) / range : 0.0;
        setParamNormalized((ParamID)mm::kParamSeed, norm);
    }
private:

    int64_t  lastSlot            = -1;
    int32    nextNoteId           = 1;
    double   prevBeatPos          = -1.0;
    std::atomic<float> lastBpm{120.0f};
    std::atomic<double> lastBeatPos{0.0}; // updated every process() block for piano roll

    // Trigger note stack (from piano roll) â€“ up to 8 simultaneous held keys
    struct TriggerNote { int16_t pitch; int32 noteId; double beatTimeOn; };
    static constexpr int kMaxTriggers = 8;
    TriggerNote  triggerStack[kMaxTriggers]{};
    int          triggerCount = 0;
    int          lastSentStyle = -1;  // detect style change to send CCs
    bool hasTrigger() const { return triggerCount > 0; }

    // Real-time recording buffer (audio thread writes, GUI reads on export)
    struct RecordedNote { double beatOn; double beatLen; int16_t pitch; float vel; uint8_t style; };
    std::mutex             recMtx;
    std::vector<RecordedNote> recNotes;
    double                 recStartBeat = 0.0;

    // Waveform: 0=Sine 1=Triangle 2=Saw 3=Square
    std::atomic<int> waveType{0};
    int getWaveType() const { return waveType.load(std::memory_order_relaxed); }
    void setWaveType(int w) {
        waveType.store(w, std::memory_order_relaxed);
        wavePerStyle[getStyleType()] = w;
    }

    // Style: 0=Melodie 1=Harpe 2=Basse 3=Percussions 4=Marimba 5=Piano
    std::atomic<int> styleType{0};
    int getStyleType() const { return styleType.load(std::memory_order_relaxed); }
    void setStyleType(int s) {
        int prev = styleType.load(std::memory_order_relaxed);
        if (prev == s) {
            // Even if the tab didn't change, ensure it is enabled.
            setStyleEnabled(s, true);
            return;
        }
        // Save current edits to the previous style's slot, then load the
        // new style's parameters into the live mirror.
        cacheCurrentStyleParams(prev);
        loadStyleParams(s);
        styleType.store(s, std::memory_order_relaxed);
        // Selecting a tab also enables it (so the user always hears what
        // they just clicked on).
        setStyleEnabled(s, true);
        // Notify the EditController of the new on-screen values so the host
        // automation lanes follow the active instrument.
        for (int i = 0; i < (int)mm::kParamCount; ++i) {
            const mm::ParamDef& d = mm::kParamDefs[i];
            double range = d.max_value - d.min_value;
            double norm  = (range > 0.0) ? (paramValues[i] - d.min_value) / range : 0.0;
            setParamNormalized((ParamID)i, std::clamp(norm, 0.0, 1.0));
        }
    }

    // Bitmask of currently active styles. Default: only the initial style.
    static constexpr int kStyleCount = 6;
    std::atomic<uint32_t> enabledStyles{0x01u};
    bool isStyleEnabled(int s) const {
        return (enabledStyles.load(std::memory_order_relaxed) >> (s & 31)) & 1u;
    }
    void setStyleEnabled(int s, bool on) {
        uint32_t cur = enabledStyles.load(std::memory_order_relaxed);
        uint32_t bit = 1u << (s & 31);
        uint32_t next = on ? (cur | bit) : (cur & ~bit);
        enabledStyles.store(next, std::memory_order_relaxed);
    }
    void toggleStyleEnabled(int s) { setStyleEnabled(s, !isStyleEnabled(s)); }
    uint32_t getEnabledMask() const { return enabledStyles.load(std::memory_order_relaxed); }
    void setEnabledMask(uint32_t m) { enabledStyles.store(m, std::memory_order_relaxed); }

    // ---- Context-listening (Continue mode) -----------------------------
    // When ON, MMM analyses notes received on the "Context In" bus (and
    // on the main bus too) to auto-detect the song's key/mode and to
    // anchor the generated melody on the last pitch heard.
    std::atomic<bool> autoKey{false};
    bool getAutoKey() const { return autoKey.load(std::memory_order_relaxed); }
    void setAutoKey(bool v) { autoKey.store(v, std::memory_order_relaxed); }

    // Pitch-class histogram (weight = duration * velocity). Decays slowly
    // so the detection follows modulations rather than averaging the whole
    // session. Updated on note-off events.
    std::atomic<double> pcHist[12]{};

    // Last pitch observed on any input bus (â‰¥0 if any). Used as the voice-
    // leading anchor for the first note of a freshly generated phrase.
    std::atomic<int>    lastInputPitch{-1};
    std::atomic<double> lastInputBeat{-1.0};

    // Per-bus active note-on times (for computing duration on note-off).
    // Indexed by [bus][pitch]; -1.0 if no note-on currently held.
    double  ctxNoteOnBeat[2][128];
    float   ctxNoteOnVel [2][128];

    // Add a note-on observation to the context.
    void ctxNoteOn(int bus, int pitch, float vel, double beat) {
        if (bus < 0 || bus > 1 || pitch < 0 || pitch > 127) return;
        ctxNoteOnBeat[bus][pitch] = beat;
        ctxNoteOnVel [bus][pitch] = vel;
        lastInputPitch.store(pitch, std::memory_order_relaxed);
        lastInputBeat .store(beat,  std::memory_order_relaxed);
    }
    // Commit a note's contribution to the histogram on note-off.
    void ctxNoteOff(int bus, int pitch, double beat) {
        if (bus < 0 || bus > 1 || pitch < 0 || pitch > 127) return;
        double on = ctxNoteOnBeat[bus][pitch];
        if (on < 0.0) return;
        double dur = std::max(0.05, beat - on);
        float  vel = ctxNoteOnVel[bus][pitch];
        ctxNoteOnBeat[bus][pitch] = -1.0;
        int pc = pitch % 12;
        // Atomic accumulate via compare-exchange (single audio thread â†’
        // load+store is fine in practice, but keep it correct).
        double cur = pcHist[pc].load(std::memory_order_relaxed);
        double add = dur * (0.3 + 0.7 * vel);
        pcHist[pc].store(cur + add, std::memory_order_relaxed);
    }
    // Apply a slow exponential decay to the histogram so old material fades.
    void ctxDecay(double factor) {
        for (int i = 0; i < 12; ++i) {
            double v = pcHist[i].load(std::memory_order_relaxed);
            pcHist[i].store(v * factor, std::memory_order_relaxed);
        }
    }
    // Snapshot the histogram into a plain array.
    void ctxSnapshot(double out[12]) const {
        for (int i = 0; i < 12; ++i)
            out[i] = pcHist[i].load(std::memory_order_relaxed);
    }
    void ctxClear() {
        for (int i = 0; i < 12; ++i) pcHist[i].store(0.0, std::memory_order_relaxed);
        for (int b = 0; b < 2; ++b)
            for (int p = 0; p < 128; ++p) {
                ctxNoteOnBeat[b][p] = -1.0;
                ctxNoteOnVel [b][p] = 0.0f;
            }
        lastInputPitch.store(-1, std::memory_order_relaxed);
    }

    // Chord progression index (see mm::kProgressions)
    std::atomic<int> progType{0};
    int  getProgType() const { return progType.load(std::memory_order_relaxed); }
    void setProgType(int p) {
        progType.store(p, std::memory_order_relaxed);
        int active = getStyleType();
        progPerStyle[active] = p;
        broadcastProgLocked(active);
    }

    struct ActiveNote {
        int16_t key;
        int16_t channel;
        int32   noteId;
        int64_t offSample;  // samples remaining until note-off
    };
    std::vector<ActiveNote> activeNotes;

    // -------------------------------------------------------------------------
    // SoundFont synth (TinySoundFont) â€“ loaded at setupProcessing.
    // -------------------------------------------------------------------------
    tsf*       sfSynth      = nullptr;
    bool       sfReady      = false;
    std::mutex sfMutex;          // serialize tsf_note_on/off vs render
    // Track per-noteId TSF channel so we can route note-off correctly.
    struct SfActive { int32 noteId; int16_t pitch; int8_t channel; bool used; };
    static constexpr int kSfMaxActive = 64;
    SfActive sfActive[kSfMaxActive] = {};

    // GM program per style (channel 9 reserved for drums).
    // 0 Piano (MÃ©lodie), 46 Harp, 33 E-Bass, drums, 12 Marimba, 0 Piano (chord)
    static constexpr int kSfChannelOf(int style) {
        return style == 3 ? 9 : style; // drums on ch 9, others on their style index
    }

    void loadSoundFont() {
        if (sfReady && sfSynth) {
            // Just update sample rate.
            tsf_set_output(sfSynth, TSF_STEREO_INTERLEAVED, (int)sampleRate, 0.0f);
            return;
        }
        // Locate the bundled SF2: <bundle>/Contents/Resources/GeneralUser.sf2
        wchar_t modPath[MAX_PATH] = {};
        if (!GetModuleFileNameW(g_hInst, modPath, MAX_PATH)) return;
        // Strip filename â†’ <bundle>/Contents/x86_64-win
        for (int i = (int)wcslen(modPath) - 1; i >= 0; --i) {
            if (modPath[i] == L'\\' || modPath[i] == L'/') { modPath[i] = 0; break; }
        }
        std::wstring sfPath = std::wstring(modPath) + L"\\..\\Resources\\GeneralUser.sf2";
        // Convert to UTF-8 for tsf_load_filename (which uses fopen).
        char sfPath8[MAX_PATH * 4] = {};
        WideCharToMultiByte(CP_ACP, 0, sfPath.c_str(), -1,
                            sfPath8, sizeof(sfPath8), nullptr, nullptr);
        sfSynth = tsf_load_filename(sfPath8);
        if (!sfSynth) return;
        tsf_set_output(sfSynth, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
        tsf_set_max_voices(sfSynth, 64);
        // Apply each style's selected preset and per-style volume.
        for (int s = 0; s < 6; ++s) {
            int ch = kSfChannelOf(s);
            int idx = std::clamp(sfPresetIdxPerStyle[s], 0, kSfPresetCount[s] - 1);
            const SfPreset& p = kSfPresetsByStyle[s][idx];
            tsf_channel_set_presetnumber(sfSynth, ch, p.program, p.bank == 128 ? 1 : 0);
            tsf_channel_set_volume(sfSynth, ch, sfVolumePerStyle[s]);
        }
        sfReady = true;
    }

    void closeSoundFont() {
        if (sfSynth) { tsf_close(sfSynth); sfSynth = nullptr; }
        sfReady = false;
    }

    void sfNoteOn(int style, int16_t pitch, float vel, int32 noteId) {
        if (!sfReady) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        int ch = kSfChannelOf(style);
        tsf_channel_note_on(sfSynth, ch, pitch, vel);
        // Remember for matching note-off.
        for (auto& a : sfActive) {
            if (!a.used) {
                a.used = true; a.noteId = noteId;
                a.pitch = pitch; a.channel = (int8_t)ch;
                return;
            }
        }
    }

    void sfNoteOff(int32 noteId, int16_t pitchFallback) {
        if (!sfReady) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        for (auto& a : sfActive) {
            if (!a.used) continue;
            if (noteId >= 0 ? (a.noteId == noteId)
                            : (a.pitch == pitchFallback)) {
                tsf_channel_note_off(sfSynth, a.channel, a.pitch);
                a.used = false;
                return;
            }
        }
    }

    void sfAllNotesOff() {
        if (!sfReady) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        for (int ch = 0; ch < 10; ++ch)
            tsf_channel_sounds_off_all(sfSynth, ch);
        for (auto& a : sfActive) a.used = false;
    }

    // Public-ish helpers used by the GUI to change preset / volume per style.
    void setSfPreset(int style, int idx) {
        if (style < 0 || style >= 6) return;
        idx = std::clamp(idx, 0, kSfPresetCount[style] - 1);
        sfPresetIdxPerStyle[style] = idx;
        if (!sfReady) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        int ch = kSfChannelOf(style);
        const SfPreset& p = kSfPresetsByStyle[style][idx];
        tsf_channel_sounds_off_all(sfSynth, ch);
        tsf_channel_set_presetnumber(sfSynth, ch, p.program, p.bank == 128 ? 1 : 0);
    }

    void setSfVolume(int style, float v) {
        if (style < 0 || style >= 6) return;
        sfVolumePerStyle[style] = std::clamp(v, 0.0f, 1.5f);
        if (!sfReady) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        tsf_channel_set_volume(sfSynth, kSfChannelOf(style),
                               sfVolumePerStyle[style]);
    }

    // -------------------------------------------------------------------------
    // Built-in polyphonic sine oscillator (no Patcher needed)
    // -------------------------------------------------------------------------
    static constexpr double kPi2      = 6.283185307179586;
    static constexpr int    kMaxVoices = 16;

    struct Voice {
        double   phase       = 0.0;
        double   freq        = 440.0;
        float    vel         = 1.0f;
        double   env         = 0.0;
        bool     noteOff     = true;    // true = free or in release
        int32    offSample   = -1;      // sample within block for release; -1 = held
        int16_t  pitch       = 69;
        int32    noteId      = -1;      // matches VST3 noteId for exact release
        bool     isPerc      = false;   // percussive: noise + fast envelope
        double   percRelRate = 0.0;     // per-voice release rate (perc only)
        uint32_t noiseState  = 12345u;  // LCG state for noise generation
        bool     sfPendingOff = false;  // fire sfNoteOff after this block
    };
    Voice voicePool[kMaxVoices]{};

    void triggerVoice(int16_t pitch, float vel, int64_t offSampleInBlock,
                      bool perc = false, int32 nid = -1, int style = 0) {
        // SoundFont path: also start the sampled instrument note.
        if (sfReady) sfNoteOn(style, pitch, vel, nid);
        // Find a free voice, or steal the quietest one.
        Voice* target = nullptr;
        double quietest = 2.0;
        for (auto& v : voicePool) {
            if (v.env <= 0.0 && v.noteOff) { target = &v; break; }
            if (v.env < quietest) { quietest = v.env; target = &v; }
        }
        if (!target) return;
        target->phase      = 0.0;
        target->freq       = 440.0 * std::pow(2.0, (pitch - 69.0) / 12.0);
        target->vel        = std::min(1.0f, vel);
        target->env        = 0.0;
        target->noteOff    = false;
        target->offSample  = (offSampleInBlock < 0) ? -1 : (int32)offSampleInBlock;
        target->pitch      = pitch;
        target->noteId     = nid;
        target->isPerc     = perc;
        target->noiseState = (uint32_t)(pitch * 997u + 1u);
        // SF2 in-block release flag: fire sfNoteOff after this block's render.
        target->sfPendingOff = (sfReady && offSampleInBlock >= 0);
        if (perc) {
            const double invSR = 1.0 / sampleRate;
            if      (pitch <= 37)                  target->percRelRate = invSR / 0.18;  // Kick 180ms
            else if (pitch <= 41)                  target->percRelRate = invSR / 0.10;  // Snare 100ms
            else if (pitch == 42 || pitch == 44)   target->percRelRate = invSR / 0.04;  // Closed/Pedal hat 40ms
            else if (pitch == 46)                  target->percRelRate = invSR / 0.18;  // Open hat 180ms
            else if (pitch == 49 || pitch == 57)   target->percRelRate = invSR / 1.20;  // Crash 1.2s
            else if (pitch == 51 || pitch == 59)   target->percRelRate = invSR / 0.45;  // Ride 450ms
            else if (pitch == 54 || pitch == 70)   target->percRelRate = invSR / 0.08;  // Tamb / maracas 80ms
            else if (pitch == 56)                  target->percRelRate = invSR / 0.20;  // Cowbell 200ms
            else if (pitch >= 60 && pitch <= 65)   target->percRelRate = invSR / 0.20;  // Bongo / conga 200ms
            else if (pitch >= 66 && pitch <= 69)   target->percRelRate = invSR / 0.25;  // Timbale 250ms
            else if (pitch >= 75 && pitch <= 78)   target->percRelRate = invSR / 0.04;  // Clave / wood 40ms
            else                                   target->percRelRate = invSR / 0.06;  // Default 60ms
        } else {
            target->percRelRate = 0.0;
        }
    }

    void releaseVoice(int32 nid, int16_t pitch) {
        // SoundFont release first so the sampled tail decays naturally.
        if (sfReady) sfNoteOff(nid, pitch);
        // Match by noteId first; fall back to pitch if noteId is unknown (-1).
        for (auto& v : voicePool) {
            if (v.noteOff || v.offSample >= 0) continue;
            if (nid >= 0 ? (v.noteId == nid) : (v.pitch == pitch))
                { v.noteOff = true; break; }
        }
    }

    void renderVoices(AudioBusBuffers* outBuses, int32 numOutputs, uint32_t nframes) {
        if (!outBuses || numOutputs < 1) return;
        AudioBusBuffers& bus = outBuses[0];
        if (!bus.channelBuffers32) return;
        float* L = bus.numChannels > 0 ? bus.channelBuffers32[0] : nullptr;
        float* R = bus.numChannels > 1 ? bus.channelBuffers32[1] : L;
        if (!L) return;

        // SoundFont path: render TSF and bypass the internal oscillator synth.
        if (sfReady) {
            {
                std::lock_guard<std::mutex> lk(sfMutex);
                thread_local std::vector<float> sfBuf;
                if (sfBuf.size() < nframes * 2) sfBuf.resize(nframes * 2);
                tsf_render_float(sfSynth, sfBuf.data(), (int)nframes, 0);
                for (uint32_t s = 0; s < nframes; ++s) {
                    L[s] += sfBuf[2 * s];
                    if (R != L) R[s] += sfBuf[2 * s + 1];
                }
            }
            // Fire pending SoundFont note-offs (in-block releases, approximated to block end).
            for (auto& vo : voicePool) {
                if (vo.sfPendingOff) {
                    vo.sfPendingOff = false;
                    sfNoteOff(vo.noteId, vo.pitch);
                    vo.env = 0.0; vo.noteOff = true; vo.offSample = -1;
                }
            }
            return;
        }

        const double invSR   = 1.0 / sampleRate;
        const double atkRate = invSR / 0.008;  // 8 ms attack
        const double relRate = invSR / 0.18;   // 180 ms release

        for (auto& vo : voicePool) {
            if (vo.env <= 0.0 && vo.noteOff) continue;
            const double effAtk = vo.isPerc ? (invSR / 0.001) : atkRate; // 1ms perc
            const double effRel = (vo.isPerc && vo.percRelRate > 0.0)
                                      ? vo.percRelRate : relRate;
            for (uint32_t s = 0; s < nframes; ++s) {
                if (vo.offSample >= 0 && (int32)s >= vo.offSample)
                    { vo.noteOff = true; vo.offSample = -1; }
                if (!vo.noteOff) {
                    vo.env = std::min(1.0, vo.env + effAtk);
                } else {
                    vo.env -= effRel;
                    if (vo.env <= 0.0) { vo.env = 0.0; break; }
                }
                float samp;
                double p = vo.phase; // 0..1
                if (vo.isPerc) {
                    float raw;
                    int p7 = vo.pitch;
                    if (p7 <= 37) {
                        // Kick: pitch-swept sine (60Hz â†’ ~40Hz over the env)
                        double pitchEnv = 1.0 + (1.0 - vo.env) * 1.5;
                        raw = (float)std::sin(p * kPi2 * pitchEnv);
                    } else if (p7 <= 41) {
                        // Snare / E-snare: mid sine + noise
                        vo.noiseState = vo.noiseState * 1664525u + 1013904223u;
                        float noise = (float)((int32_t)vo.noiseState) / (float)0x80000000u;
                        raw = (float)std::sin(p * kPi2) * 0.35f + noise * 0.65f;
                    } else if (p7 == 42 || p7 == 44) {
                        // Closed / pedal hi-hat: bright filtered noise
                        vo.noiseState = vo.noiseState * 1664525u + 1013904223u;
                        float n = (float)((int32_t)vo.noiseState) / (float)0x80000000u;
                        // High-pass-ish: keep just the difference
                        static thread_local float prev = 0.f;
                        float hp = n - prev * 0.85f; prev = n;
                        raw = hp;
                    } else if (p7 == 46) {
                        // Open hi-hat: noisy, slightly metallic
                        vo.noiseState = vo.noiseState * 1664525u + 1013904223u;
                        float n = (float)((int32_t)vo.noiseState) / (float)0x80000000u;
                        // Mix three sines around 8/12/16 kHz harmonics for ring
                        double ph = p * kPi2;
                        float ring = (float)(std::sin(ph * 8.0) + std::sin(ph * 11.0)
                                            + std::sin(ph * 16.0)) * 0.10f;
                        raw = n * 0.85f + ring;
                    } else if (p7 == 49 || p7 == 57) {
                        // Crash: dense bright noise + ringing harmonics
                        vo.noiseState = vo.noiseState * 1664525u + 1013904223u;
                        float n = (float)((int32_t)vo.noiseState) / (float)0x80000000u;
                        double ph = p * kPi2;
                        float ring = (float)(std::sin(ph * 5.3) + std::sin(ph * 9.7)
                                            + std::sin(ph * 14.1)) * 0.12f;
                        raw = n * 0.7f + ring;
                    } else if (p7 == 51 || p7 == 59) {
                        // Ride: tight noise burst + bell sine (clean tonality)
                        vo.noiseState = vo.noiseState * 1664525u + 1013904223u;
                        float n = (float)((int32_t)vo.noiseState) / (float)0x80000000u;
                        float bell = (float)std::sin(p * kPi2);
                        raw = bell * 0.55f + n * 0.30f;
                    } else if (p7 == 54 || p7 == 70) {
                        // Tambourine / maracas: grainy rapid noise
                        vo.noiseState = vo.noiseState * 1664525u + 1013904223u;
                        float n = (float)((int32_t)vo.noiseState) / (float)0x80000000u;
                        raw = n * 0.95f;
                    } else if (p7 == 56) {
                        // Cowbell: bright square-ish (two stacked sines)
                        double ph = p * kPi2;
                        raw = (float)(std::sin(ph) * 0.6 + std::sin(ph * 1.5) * 0.4);
                    } else if (p7 >= 60 && p7 <= 65) {
                        // Bongo / Conga: tuned tom-like sine with quick decay
                        // (frequency already from MIDI pitch)
                        raw = (float)std::sin(p * kPi2);
                    } else if (p7 >= 75 && p7 <= 78) {
                        // Claves / wood block: short bright sine click
                        raw = (float)std::sin(p * kPi2);
                        // Attenuate if env < 0.6 so it sounds clicky-short
                    } else if (p7 >= 66 && p7 <= 69) {
                        // Timbales / agogo: bright sine
                        raw = (float)std::sin(p * kPi2);
                    } else {
                        // Default: pitched sine
                        raw = (float)std::sin(p * kPi2);
                    }
                    samp = raw * (float)vo.env * vo.vel * 0.18f;
                } else {
                    switch (getWaveType()) {
                        case 1: // Triangle
                            samp = (float)((p < 0.5 ? 4.0*p - 1.0 : 3.0 - 4.0*p)
                                           * vo.env * (double)vo.vel * 0.18);
                            break;
                        case 2: // Sawtooth
                            samp = (float)((2.0*p - 1.0) * vo.env * (double)vo.vel * 0.14);
                            break;
                        case 3: // Square
                            samp = (float)((p < 0.5 ? 1.0 : -1.0) * vo.env * (double)vo.vel * 0.14);
                            break;
                        default: // Sine
                            samp = (float)(std::sin(p * kPi2) * vo.env * (double)vo.vel * 0.18);
                            break;
                    }
                }
                vo.phase += vo.freq * invSR;
                if (vo.phase >= 1.0) vo.phase -= 1.0;
                L[s] += samp;
                if (R != L) R[s] += samp;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Style-specific note selection, velocity and length
    // -------------------------------------------------------------------------
    struct StyleVoices {
        int16_t pitches[6];
        float   vels[6];   // velocity multipliers, applied on top of velForStyle
        int     count;
    };

    static int16_t choosePitchStyled(int style, const double* params, int64_t slot, int progression = 0) {
        if (style == 0) return mm::choose_pitch(params, slot, progression);
        int key  = (int)std::round(params[mm::kParamKey]) % 12;
        int mode = std::clamp((int)std::round(params[mm::kParamMode]), 0, mm::kScaleCount - 1);
        int oct  = std::clamp((int)std::round(params[mm::kParamOctave]), 3, 6);
        int seed = (int)std::round(params[mm::kParamSeed]);
        const int* ivl = mm::kScaleIntervals[mode];
        const int  len  = mm::kScaleLengths[mode];
        switch (style) {
        case 1: { // Harpe: arpÃ¨ge chord-tones sur 2 octaves, montÃ©e puis descente
            // Suit la progression d'accords choisie (4 mesures).
            int subdiv      = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar = subdiv * 4;
            int64_t bar     = slot / slotsPerBar;
            int prog = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int chordRoot = mm::kProgressions[prog][((bar % mm::kProgressionLength) + mm::kProgressionLength) % mm::kProgressionLength];

            // Notes de l'accord (degres 1,3,5 du chord root) dans la gamme.
            int t0 = ivl[(chordRoot + 0) % len];
            int t1 = ivl[(chordRoot + 2) % len];
            int t2 = ivl[(chordRoot + 4) % len];

            // Cycle de 14 pas : 7 montants + 7 descendants, type "dents de scie".
            // MontÃ©e  : R, 3, 5, R+12, 3+12, 5+12, R+24
            // Descente: 5+12, 3+12, R+12, 5, 3, R, R-12 (puis on recycle)
            static constexpr int kSteps = 14;
            int pos = (int)(((slot % kSteps) + kSteps) % kSteps);
            int tone, octShift;
            switch (pos) {
                case 0:  tone = t0; octShift = 0; break;
                case 1:  tone = t1; octShift = 0; break;
                case 2:  tone = t2; octShift = 0; break;
                case 3:  tone = t0; octShift = 1; break;
                case 4:  tone = t1; octShift = 1; break;
                case 5:  tone = t2; octShift = 1; break;
                case 6:  tone = t0; octShift = 2; break;
                case 7:  tone = t2; octShift = 1; break;
                case 8:  tone = t1; octShift = 1; break;
                case 9:  tone = t0; octShift = 1; break;
                case 10: tone = t2; octShift = 0; break;
                case 11: tone = t1; octShift = 0; break;
                case 12: tone = t0; octShift = 0; break;
                default: tone = t2; octShift = -1; break; // case 13: bas du cycle
            }
            int semitone = key + tone + (oct + octShift) * 12;
            return (int16_t)std::clamp(semitone, 0, 127);
        }
        case 2: { // Basse: root + fifth, one octave lower
            int bassOct = std::max(2, oct - 1);
            int degree  = ((slot % 4) == 2) ? 4 : 0;
            return (int16_t)std::clamp(key + ivl[degree] + bassOct * 12, 0, 127);
        }
        case 3: { // Percussions: GM drum map
            int subdiv = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int barLen = subdiv * 4;
            int pos    = (int)(((slot % barLen) + barLen) % barLen);
            int beat   = pos / subdiv;
            int sub    = pos % subdiv;
            if (sub == 0) return (beat == 0 || beat == 2) ? 36 : 38; // Kick/Snare
            uint32_t r = (uint32_t)(slot * 2246822519u + (uint32_t)seed * 374761393u + 1u);
            if (!r) r = 1;
            mm::xorshift32(r);
            return (mm::xorshift32(r) % 8 == 0) ? 46 : 42; // Open/Closed hi-hat
        }
        case 4: { // Marimba: major pentatonic, random octave offset
            static constexpr int penta[] = {0, 2, 4, 7, 9};
            uint32_t s = (uint32_t)(slot * 2654435761u + (uint32_t)seed * 97u + 1u);
            if (!s) s = 1;
            mm::xorshift32(s);
            int idx    = (int)(mm::xorshift32(s) % 5);
            int octOff = (int)(mm::xorshift32(s) % 3) - 1;
            return (int16_t)std::clamp(key + penta[idx] + (oct + octOff) * 12, 0, 127);
        }
        case 5: { // Piano: chord root in left hand, in low octave
            int subdiv      = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar = subdiv * 4;
            int64_t bar     = slot / slotsPerBar;
            int prog = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int chordRoot = mm::kProgressions[prog][((bar % mm::kProgressionLength) + mm::kProgressionLength) % mm::kProgressionLength];
            int leftOct   = std::max(2, oct - 2);
            return (int16_t)std::clamp(key + ivl[chordRoot] + leftOct * 12, 0, 127);
        }
        default: return mm::choose_pitch(params, slot, progression);
        }
    }

    // Compute every pitch (and per-voice velocity multiplier) that should be
    // emitted for the given style at this slot. Most styles output a single
    // voice; Piano emits a 3-note left-hand chord (+ optional melody) and
    // Percussion can output up to 3 simultaneous drums depending on the
    // active rhythm pattern.
    static void voicesForStyle(int style, const double* params, int64_t slot,
                               int progression, bool pianoMelody, bool pianoChord, int percRhythm,
                               StyleVoices& out)
    {
        out.count = 0;

        // ---- Piano: arpeggio on every subdiv slot (like Harp) --------------
        if (style == 5) {
            int key  = (int)std::round(params[mm::kParamKey]) % 12;
            int mode = std::clamp((int)std::round(params[mm::kParamMode]), 0, mm::kScaleCount - 1);
            int oct  = std::clamp((int)std::round(params[mm::kParamOctave]), 3, 6);
            int subdiv      = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar = subdiv * 4;
            int64_t bar     = slot / slotsPerBar;
            int prog = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int chordRoot = mm::kProgressions[prog][((bar % mm::kProgressionLength) + mm::kProgressionLength) % mm::kProgressionLength];
            const int* ivl = mm::kScaleIntervals[mode];
            const int  len  = mm::kScaleLengths[mode];
            int leftOct = std::max(2, oct - 1);
            int t0 = ivl[(chordRoot + 0) % len];
            int t1 = ivl[(chordRoot + 2) % len];
            int t2 = ivl[(chordRoot + 4) % len];
            // Arpeggiate on every slot like the Harp: cycle R/3/5 with
            // an octave lift on the upper half of the bar.
            int slotInBar = (int)(((slot % slotsPerBar) + slotsPerBar) % slotsPerBar);
            int pos3 = slotInBar % 3;  // 0=root, 1=3rd, 2=5th
            int octShift = (slotInBar >= slotsPerBar / 2) ? 1 : 0;
            int tone = (pos3 == 0) ? t0 : (pos3 == 1) ? t1 : t2;
            int slotPerBeat = std::max(1, slotsPerBar / 4);
            int beatInBar = slotInBar / slotPerBeat;
            if (pianoChord) {
                out.pitches[0] = (int16_t)std::clamp(
                    key + tone + (leftOct + octShift) * 12, 0, 127);
                out.vels[0] = (beatInBar == 0 && pos3 == 0) ? 1.00f
                            : (beatInBar == 2 && pos3 == 0) ? 0.90f
                            : (pos3 == 0)                   ? 0.80f : 0.65f;
                out.count = 1;
            } else {
                // Single-note mode: root on every slot.
                out.pitches[0] = (int16_t)std::clamp(
                    key + t0 + (leftOct + octShift) * 12, 0, 127);
                out.vels[0] = (beatInBar == 0) ? 1.00f
                            : (beatInBar == 2) ? 0.88f : 0.70f;
                out.count = 1;
            }
            // Right-hand melody (if enabled).
            if (pianoMelody) {
                double mp[mm::kParamCount];
                std::copy(params, params + mm::kParamCount, mp);
                mp[mm::kParamOctave] = (double)std::clamp(oct + 1, 3, 7);
                int16_t mel = mm::choose_pitch(mp, slot, progression);
                if (out.count < 6) {
                    out.pitches[out.count] = mel;
                    out.vels[out.count]    = 0.95f;
                    ++out.count;
                }
            }
            return;
        }

        // ---- Percussion: ethnic rhythm pattern -----------------------------
        if (style == 3) {
            int subdiv      = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar = subdiv * 4;
            // Map our subdiv-resolution slot to the 16-step pattern grid.
            // If the host runs at a finer subdiv (e.g. 8 -> 32 slots/bar),
            // only emit on slots that align with a pattern step.
            int slotInBar = (int)(((slot % slotsPerBar) + slotsPerBar) % slotsPerBar);
            int slotsPerStep = slotsPerBar / 16;
            if (slotsPerStep < 1) slotsPerStep = 1;
            if (slotInBar % slotsPerStep != 0) return;
            int step = (slotInBar / slotsPerStep) % 16;
            int idx = std::clamp(percRhythm, 0, mm::kDrumPatternCount - 1);
            const auto& pat = mm::kDrumPatterns[idx];
            for (int h = 0; h < 4 && out.count < 6; ++h) {
                uint8_t p = pat.steps[step][h].pitch;
                uint8_t v = pat.steps[step][h].vel;
                if (p == 0 || v == 0) continue;
                out.pitches[out.count] = (int16_t)p;
                out.vels[out.count]    = (float)v / 127.f;
                ++out.count;
            }
            // Seed-driven micro-variations: rare ghost hi-hat on otherwise
            // empty 16th slots so consecutive bars don't feel identical.
            if (out.count == 0) {
                int seed = (int)std::round(params[mm::kParamSeed]);
                uint32_t r = (uint32_t)(slot * 2654435761u
                            + (uint32_t)seed * 374761393u + 0x9E3779B9u);
                if (!r) r = 1;
                mm::xorshift32(r);
                if ((r % 12u) == 0) {
                    out.pitches[0] = 42; // closed hi-hat ghost
                    out.vels[0]    = 0.32f;
                    out.count      = 1;
                }
            }
            return;
        }

        // ---- Voix (winds): "surfs" on the harmonic wave of the other
        //      instruments. Picks a chord tone (R/3/5/7) of the current
        //      bar's chord, sat one octave above the trigger, and holds
        //      it for half a bar. Changes only when the chord/half-bar
        //      changes, so it acts like a vocal line riding above the
        //      texture rather than competing with the rhythm.
        if (style == 0) {
            int subdiv      = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar = subdiv * 4;
            int slotInBar   = (int)(((slot % slotsPerBar) + slotsPerBar) % slotsPerBar);
            int subdivPerBeat = std::max(1, slotsPerBar / 4);
            int beat = slotInBar / subdivPerBeat;
            int sub  = slotInBar % subdivPerBeat;
            // Sing only on beat 1 (full bar) and beat 3 (half bar).
            if (sub != 0)               return;
            if (beat != 0 && beat != 2) return;

            int key  = (int)std::round(params[mm::kParamKey]) % 12;
            int mode = std::clamp((int)std::round(params[mm::kParamMode]), 0, mm::kScaleCount - 1);
            int oct  = std::clamp((int)std::round(params[mm::kParamOctave]), 3, 6);
            int prog = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int64_t bar = slot / slotsPerBar;
            int chordRoot = mm::kProgressions[prog][((bar % mm::kProgressionLength) + mm::kProgressionLength) % mm::kProgressionLength];
            const int* ivl = mm::kScaleIntervals[mode];
            const int  len  = mm::kScaleLengths[mode];

            // Pick a chord tone: beat 1 = root or 5th, beat 3 = 3rd or 7th.
            // Seeded so different seeds give different vocal contours but
            // the line still tracks the harmony.
            int seed = (int)std::round(params[mm::kParamSeed]);
            uint32_t r = (uint32_t)((bar * 2654435761u)
                       ^ (uint32_t)(seed * 374761393u)
                       ^ (uint32_t)(beat * 0x9E3779B9u));
            if (!r) r = 1;
            mm::xorshift32(r);
            int degOff;
            if (beat == 0) degOff = (r & 1u) ? 4 : 0;   // 5th or root
            else           degOff = (r & 1u) ? (len > 6 ? 6 : len - 1) : 2;  // 7th (or top) or 3rd
            int semi = ivl[(chordRoot + degOff) % len];
            // Sit one octave above the trigger so it's clearly a lead
            // floating over the harp/bass texture.
            int voiceOct = std::clamp(oct + 1, 4, 7);
            int pitch = std::clamp(key + semi + voiceOct * 12, 36, 96);
            out.pitches[0] = (int16_t)pitch;
            out.vels[0]    = 0.72f;
            out.count      = 1;
            return;
        }

        // ---- Default: all other styles return a single pitch --------------
        out.pitches[0] = choosePitchStyled(style, params, slot, progression);
        out.vels[0]    = 1.0f;
        out.count      = 1;
    }

    static float velForStyle(int style, int64_t slot, const double* params, float base) {
        int subdiv = mm::normalized_subdiv(params[mm::kParamSubdiv]);
        int barLen = subdiv * 4;
        int pos    = (int)(((slot % barLen) + barLen) % barLen);
        int beat   = pos / subdiv;
        int sub    = pos % subdiv;
        if (style == 3) {
            // Drums: classic kick/snare/hat dynamics.
            if (sub == 0 && (beat == 0 || beat == 2)) return 0.95f;
            if (sub == 0 && (beat == 1 || beat == 3)) return 0.75f;
            return 0.45f;
        }
        // Melodic styles: accent strong beats so phrases breathe.
        float accent = 1.0f;
        if (sub == 0 && beat == 0)      accent = 1.00f; // downbeat
        else if (sub == 0 && beat == 2) accent = 0.92f; // mid-bar accent
        else if (sub == 0)              accent = 0.82f; // beat 2 / 4
        else                            accent = 0.68f; // off-beat
        float v = base * accent;
        if (v > 1.0f) v = 1.0f;
        if (v < 0.05f) v = 0.05f;
        return v;
    }

    static double noteLenForStyle(int style, double slotBeats, double fracParam) {
        // Quadratic curve: 0% = 0.15 slot (staccato), 50% = 1.1 slot (legato),
        // 100% = 4.0 slots (sustained, notes overlap). Applies to melodic styles.
        double lenMult = 0.15 + fracParam * fracParam * 3.85;
        switch (style) {
        case 0: return std::max(slotBeats * 1.85, 1.85);         // Voix: tenue fixe ~2 temps
        case 2: return slotBeats * (0.6 + fracParam * 2.4);      // Basse: 0.6→3.0 slots
        case 3: return std::min(slotBeats * 0.12, 0.05);         // Percus: toujours court
        case 4: return slotBeats * (0.10 + fracParam * 0.90);    // Marimba: 0.1→1.0 slot
        case 5: return slotBeats * lenMult;                      // Piano arpeggio
        default: return slotBeats * lenMult;                     // Harpe / Melodie
        }
    }

    // Ring buffer for GUI display (audio thread writes, GUI thread reads)
    static constexpr int kNoteHistSize = 8;
    std::atomic<uint32_t> noteHistHead{0};
    int16_t noteHist[kNoteHistSize]{};
};

// =============================================================================
// MelodyMakerView â€“ minimal Win32 GUI editor (no VSTGUI dependency)
//
// Layout: 7 rows, each row = label + control (combobox or trackbar) + value.
// Combobox for stepped params (Key, Mode, Octave, Subdiv, Seed).
// Trackbar  for continuous params (Density, NoteLen).
// =============================================================================

static const wchar_t* kWndClass = L"MidnightMelodyMakerView";
static constexpr int kIdBase   = 1000;
static constexpr int kIdExport = 1007; // kIdBase + kParamCount
static constexpr int kIdWave   = 1008;
static constexpr int kIdStyle  = 1009;
static constexpr int kIdProg   = 1010;
static constexpr int kIdAuto   = 1011; // Auto-Key (listen) toggle
static constexpr int kIdDice   = 1012; // Randomize seed of active style
static constexpr int kIdPianoMel   = 1013; // Piano: enable right-hand melody
static constexpr int kIdPercRhy    = 1014; // Percussion: rhythm pattern
static constexpr int kIdPianoChord = 1015; // Piano: play chords (vs single notes)
static constexpr int kIdVol        = 1016; // Per-style SoundFont volume slider
static constexpr int kIdLockMode   = 1017; // Padlock for Mode
static constexpr int kIdLockProg   = 1018; // Padlock for Progression
static constexpr int kIdLockSubdiv = 1019; // Padlock for Subdiv
static constexpr int kIdExportWav  = 1020; // Export rendered audio (WAV)
static constexpr int kIdSavePreset = 1021; // Save current state to .mmp file
static constexpr int kIdLoadPreset = 1022; // Load state from .mmp file
static constexpr int kIdMeter      = 1023; // Global time-signature numerator combo
static constexpr int kIdHumanize   = 1024; // Per-style timing humanization slider
static constexpr int kIdRetard     = 1025; // Per-style groove retard slider
static constexpr int kIdSection   = 1026; // Global section selector
static constexpr int kIdStartBar  = 1027; // Per-style start-bar delay
static constexpr int kIdUndo      = 1028; // Editor undo
static constexpr int kIdRedo      = 1029; // Editor redo

// Arrangement section types (used for volume dynamics).

// ---------- Theme palette (Omnisphere-style anthracite blue-grey) ------------
static constexpr COLORREF kColBg        = RGB(26, 31, 40);    // #1A1F28 main bg
static constexpr COLORREF kColPanel     = RGB(35, 41, 52);    // #232934 panels
static constexpr COLORREF kColControl   = RGB(44, 51, 62);    // #2C333E controls
static constexpr COLORREF kColControlHi = RGB(56, 65, 78);    // hover/lighter
static constexpr COLORREF kColBorder    = RGB(58, 66, 80);    // #3A4250
static constexpr COLORREF kColHeader1   = RGB(20, 25, 34);    // top of gradient
static constexpr COLORREF kColHeader2   = RGB(38, 46, 60);    // bottom of gradient
static constexpr COLORREF kColAccent    = RGB(79, 195, 247);  // bright cyan
static constexpr COLORREF kColAccentDark= RGB(56, 145, 200);
static constexpr COLORREF kColAccentWarm= RGB(255, 159, 67);  // Omnisphere warm
static constexpr COLORREF kColText      = RGB(221, 225, 232); // #DDE1E8
static constexpr COLORREF kColTextDim   = RGB(122, 130, 148); // #7A8294
static constexpr COLORREF kColTextValue = RGB(255, 255, 255);
static constexpr COLORREF kColTabBg     = RGB(44, 51, 62);
static constexpr COLORREF kColTabActive = RGB(58, 130, 246);  // bright blue
static constexpr COLORREF kColWhite     = RGB(255, 255, 255);

#ifndef TBS_TRANSPARENTBKGND
#define TBS_TRANSPARENTBKGND 0x1000
#endif

// =============================================================================
// KnobWidget â€“ custom rotary control for continuous params (Density / NoteLen)
//   - Drag vertically (up = +, down = -). Shift = fine. Double-click = default.
//   - Painted with anti-aliased arcs via GDI (SetStretchBltMode + GdiPlus-free).
//   - Sends WM_HSCROLL / SB_THUMBPOSITION 0..1000 like a trackbar so existing
//     handler can read the value without changes.
// =============================================================================
class KnobWidget {
public:
    static constexpr const wchar_t* kClassName = L"MidnightMMKnob";
    int  posMin = 0, posMax = 1000, pos = 500, posDefault = 500;
    HWND hWnd   = nullptr;
    HWND hOwner = nullptr;
    int  ctrlId = 0;
    bool dragging = false;
    POINT dragOrigin{};
    int   dragOriginPos = 0;

    static void registerClass(HINSTANCE hi) {
        WNDCLASSW existing{};
        if (GetClassInfoW(hi, kClassName, &existing)) return;
        WNDCLASSW wc{};
        wc.lpfnWndProc   = staticProc;
        wc.hInstance     = hi;
        wc.hbrBackground = nullptr; // we paint everything
        wc.lpszClassName = kClassName;
        wc.hCursor       = LoadCursor(nullptr, IDC_HAND);
        wc.style         = CS_DBLCLKS;
        RegisterClassW(&wc);
    }

    static HWND create(HWND parent, int id, int x, int y, int w, int h) {
        HWND wnd = CreateWindowExW(0, kClassName, L"",
            WS_CHILD | WS_VISIBLE,
            x, y, w, h, parent, (HMENU)(LONG_PTR)id, g_hInst, nullptr);
        return wnd;
    }

    int getPos() const { return pos; }
    void setPos(int p) {
        p = std::clamp(p, posMin, posMax);
        if (p != pos) { pos = p; if (hWnd) InvalidateRect(hWnd, nullptr, FALSE); }
    }

private:
    static LRESULT CALLBACK staticProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
            self = new KnobWidget();
            self->hWnd   = h;
            self->hOwner = GetParent(h);
            self->ctrlId = (int)(LONG_PTR)cs->hMenu;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
            return TRUE;
        }
        if (!self) return DefWindowProcW(h, m, w, l);
        LRESULT r = self->proc(m, w, l);
        if (m == WM_NCDESTROY) {
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            delete self;
        }
        return r;
    }

    void notifyOwner() {
        // Mimic a trackbar: send WM_HSCROLL with SB_THUMBTRACK so the parent
        // refresh path (onTrackbarChange) handles it uniformly.
        if (hOwner)
            SendMessageW(hOwner, WM_HSCROLL,
                         MAKEWPARAM(SB_THUMBTRACK, (WORD)pos),
                         (LPARAM)hWnd);
    }

    LRESULT proc(UINT m, WPARAM w, LPARAM l) {
        switch (m) {
            case WM_LBUTTONDOWN:
                SetCapture(hWnd);
                dragging = true;
                GetCursorPos(&dragOrigin);
                dragOriginPos = pos;
                return 0;
            case WM_MOUSEMOVE:
                if (dragging) {
                    POINT p; GetCursorPos(&p);
                    int dy = dragOrigin.y - p.y; // up = positive
                    bool fine = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    int range = posMax - posMin;
                    int sensitivity = fine ? 600 : 150; // pixels for full sweep
                    int delta = (int)((double)dy * range / sensitivity);
                    setPos(dragOriginPos + delta);
                    notifyOwner();
                }
                return 0;
            case WM_LBUTTONUP:
                if (dragging) { ReleaseCapture(); dragging = false; }
                return 0;
            case WM_LBUTTONDBLCLK:
                setPos(posDefault);
                notifyOwner();
                return 0;
            case WM_MOUSEWHEEL: {
                int z = GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA;
                bool fine = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                int step = fine ? 4 : 25;
                setPos(pos + z * step);
                notifyOwner();
                return 0;
            }
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
                paintKnob(hdc);
                EndPaint(hWnd, &ps);
                return 0;
            }
        }
        return DefWindowProcW(hWnd, m, w, l);
    }

    void paintKnob(HDC hdc) {
        RECT rc; GetClientRect(hWnd, &rc);
        int W = rc.right, H = rc.bottom;
        // Background fill (same as parent)
        HBRUSH brBg = CreateSolidBrush(kColBg);
        FillRect(hdc, &rc, brBg);
        DeleteObject(brBg);

        // Geometry
        int cx = W / 2;
        int cy = H / 2 + 1;
        int radius = std::min(W, H) / 2 - 4;
        int trackThick = 5;

        // Background arc (track)
        HPEN penTrack = CreatePen(PS_SOLID | PS_GEOMETRIC, trackThick, kColControl);
        HPEN penOld   = (HPEN)SelectObject(hdc, penTrack);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        SetArcDirection(hdc, AD_CLOCKWISE);
        // Arc from 225Â° to -45Â° (going clockwise = 270Â° sweep, music-knob style)
        // We compute endpoints: start angle 225Â° (lower-left), end angle -45Â° (lower-right)
        const double PI = 3.14159265358979323846;
        double a0 = 225.0 * PI / 180.0; // lower-left
        double a1 = -45.0 * PI / 180.0; // lower-right
        int x0 = cx + (int)(radius * std::cos(a0));
        int y0 = cy - (int)(radius * std::sin(a0));
        int x1 = cx + (int)(radius * std::cos(a1));
        int y1 = cy - (int)(radius * std::sin(a1));
        Arc(hdc, cx - radius, cy - radius, cx + radius, cy + radius, x0, y0, x1, y1);

        // Foreground arc (filled portion proportional to value)
        double t = (double)(pos - posMin) / std::max(1, (posMax - posMin));
        t = std::clamp(t, 0.0, 1.0);
        double aSweep = (225.0 + 45.0) * PI / 180.0; // total = 270Â°
        double aEnd   = a0 - t * aSweep;
        int xe = cx + (int)(radius * std::cos(aEnd));
        int ye = cy - (int)(radius * std::sin(aEnd));
        SelectObject(hdc, penOld);
        DeleteObject(penTrack);
        HPEN penArc = CreatePen(PS_SOLID | PS_GEOMETRIC, trackThick, kColAccent);
        SelectObject(hdc, penArc);
        if (t > 0.001)
            Arc(hdc, cx - radius, cy - radius, cx + radius, cy + radius, x0, y0, xe, ye);
        SelectObject(hdc, GetStockObject(BLACK_PEN));
        DeleteObject(penArc);

        // Inner disc (knob body)
        int innerR = radius - 9;
        HBRUSH brBody = CreateSolidBrush(kColControlHi);
        HPEN   penBdy = CreatePen(PS_SOLID, 1, kColBorder);
        HBRUSH brOld  = (HBRUSH)SelectObject(hdc, brBody);
        SelectObject(hdc, penBdy);
        Ellipse(hdc, cx - innerR, cy - innerR, cx + innerR, cy + innerR);
        SelectObject(hdc, brOld);
        DeleteObject(brBody);
        SelectObject(hdc, GetStockObject(BLACK_PEN));
        DeleteObject(penBdy);

        // Indicator line
        double ai = aEnd;
        int ix1 = cx + (int)((innerR - 2) * std::cos(ai));
        int iy1 = cy - (int)((innerR - 2) * std::sin(ai));
        int ix2 = cx + (int)((innerR - 12) * std::cos(ai));
        int iy2 = cy - (int)((innerR - 12) * std::sin(ai));
        HPEN penInd = CreatePen(PS_SOLID, 3, kColAccent);
        HPEN penOldI = (HPEN)SelectObject(hdc, penInd);
        MoveToEx(hdc, ix2, iy2, nullptr);
        LineTo(hdc, ix1, iy1);
        SelectObject(hdc, penOldI);
        DeleteObject(penInd);
    }
};

class MelodyMakerView : public IPlugView
{
public:
    explicit MelodyMakerView(MelodyMakerVST3* p) : plugin(p) {}

    DECLARE_FUNKNOWN_METHODS

    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {
        return (std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API attached(void* parent, FIDString /*type*/) override {
        ensureWndClass();
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);

        // Theme resources
        brBg = CreateSolidBrush(kColBg);
        auto mkFont = [](int height, int weight, const wchar_t* face) {
            LOGFONTW lf{}; lf.lfHeight = height; lf.lfWeight = weight;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, face);
            return CreateFontIndirectW(&lf);
        };
        fontReg     = mkFont(-12, FW_NORMAL,   L"Segoe UI");
        fontBold    = mkFont(-12, FW_SEMIBOLD, L"Segoe UI");
        fontTitle   = mkFont(-22, FW_BOLD,     L"Segoe UI");
        fontSection = mkFont(-11, FW_BOLD,     L"Segoe UI");
        fontTab     = mkFont(-13, FW_SEMIBOLD, L"Segoe UI");
        fontMono    = mkFont(-13, FW_NORMAL,   L"Consolas");

        KnobWidget::registerClass(g_hInst);

        hParent = (HWND)parent;
        hWnd = CreateWindowExW(
            0, kWndClass, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, kViewWidth, kViewHeight,
            hParent, nullptr, g_hInst, nullptr);
        if (!hWnd) return kResultFalse;

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)this);
        createControls();
        refreshAll();
        SetTimer(hWnd, 1, 120, nullptr); // 120 ms refresh for note display
        return kResultOk;
    }

    tresult PLUGIN_API removed() override {
        if (hWnd) { KillTimer(hWnd, 1); DestroyWindow(hWnd); hWnd = nullptr; }
        if (brBg)        { DeleteObject(brBg);        brBg = nullptr; }
        if (fontReg)     { DeleteObject(fontReg);     fontReg = nullptr; }
        if (fontBold)    { DeleteObject(fontBold);    fontBold = nullptr; }
        if (fontTitle)   { DeleteObject(fontTitle);   fontTitle = nullptr; }
        if (fontSection) { DeleteObject(fontSection); fontSection = nullptr; }
        if (fontTab)     { DeleteObject(fontTab);     fontTab = nullptr; }
        if (fontMono)    { DeleteObject(fontMono);    fontMono = nullptr; }
        return kResultOk;
    }

    tresult PLUGIN_API onWheel(float) override { return kResultFalse; }
    tresult PLUGIN_API onKeyDown(char16, int16, int16) override { return kResultFalse; }
    tresult PLUGIN_API onKeyUp(char16, int16, int16) override { return kResultFalse; }

    tresult PLUGIN_API getSize(ViewRect* size) override {
        if (!size) return kInvalidArgument;
        size->left = 0; size->top = 0;
        size->right = kViewWidth; size->bottom = kViewHeight;
        return kResultOk;
    }
    tresult PLUGIN_API onSize(ViewRect*) override { return kResultOk; }
    tresult PLUGIN_API onFocus(TBool) override { return kResultOk; }
    tresult PLUGIN_API setFrame(IPlugFrame*) override { return kResultOk; }
    tresult PLUGIN_API canResize() override { return kResultFalse; }
    tresult PLUGIN_API checkSizeConstraint(ViewRect*) override { return kResultOk; }

private:
    static constexpr int kViewWidth      = 900;
    static constexpr int kViewHeight     = 640;  // compacted layout
    static constexpr int kPadX           = 18;
    static constexpr int kHeaderH        = 50;
    static constexpr int kTabsH          = 52;
    static constexpr int kSectionTitleH  = 24;
    static constexpr int kRowH           = 34;
    static constexpr int kSectionGap     = 14;
    static constexpr int kColumnGap      = 24;
    static constexpr int kColumnW        = (kViewWidth - 2 * kPadX - kColumnGap) / 2; // 420
    static constexpr int kRightColX      = kPadX + kColumnW + kColumnGap;             // 462
    static constexpr int kLabelW         = 78;
    static constexpr int kCtrlW          = kColumnW - kLabelW - 8 - 80 - 8;           // 246
    static constexpr int kValueW         = 80;
    static constexpr int kButtonH        = 38;
    static constexpr int kKnobSize       = 56;
    static constexpr int kTabCount       = 6;

    MelodyMakerVST3* plugin;
    HWND hParent = nullptr;
    HWND hWnd    = nullptr;
    HWND hCtrl[mm::kParamCount]   = {};
    HWND hValue[mm::kParamCount]  = {};
    HWND hLabel[mm::kParamCount]  = {};
    HWND hWaveLabel               = nullptr;
    HWND hProgLabel               = nullptr;
    HWND hNotesLabel              = nullptr;
    HWND hNoteDisplay             = nullptr;
    HWND hExportBtn               = nullptr;
    HWND hExportWavBtn            = nullptr; // Export rendered audio (WAV)
    HWND hSavePresetBtn           = nullptr; // Save preset (.mmp)
    HWND hLoadPresetBtn           = nullptr; // Load preset (.mmp)
    HWND hWaveCombo               = nullptr;
    HWND hProgCombo               = nullptr;
    HWND hAutoBtn                 = nullptr; // LISTEN / Auto-Key toggle
    HWND hDiceBtn                 = nullptr; // Randomize seed
    HWND hPianoMelChk             = nullptr; // Piano: "MÃ©lodie" toggle
    HWND hPianoChordChk           = nullptr; // Piano: "Accords" toggle
    HWND hPercRhyLabel            = nullptr;
    HWND hPercRhyCombo            = nullptr; // Percussion rhythm picker
    HWND hVolLabel                = nullptr; // Volume label
    HWND hVolSlider               = nullptr; // Volume trackbar (0..200)
    HWND hVolValue                = nullptr; // Volume value text
    HWND hLockMode                = nullptr; // Padlock toggle for Mode
    HWND hLockProg                = nullptr; // Padlock toggle for Progr.
    HWND hLockSubdiv              = nullptr; // Padlock toggle for Subdiv
    HWND hMeterCombo              = nullptr; // Global time-signature combo
    HWND hHumanizeLabel           = nullptr; // Humanize label
    HWND hHumanizeKnob            = nullptr; // Humanize knob (0..1000)
    HWND hHumanizeValue           = nullptr; // Humanize value text
    HWND hRetardLabel             = nullptr; // Retard label
    HWND hRetardKnob              = nullptr; // Retard knob (0..1000)
    HWND hRetardValue             = nullptr; // Retard value text
    HWND hSectionCombo  = nullptr; // Global section (Intro/Verse/Chorus/Bridge/Outro)
    HWND hStartBarLabel = nullptr; // Per-style start-bar label
    HWND hStartBarCombo = nullptr; // Per-style start-bar combo
    HWND hPianoRoll     = nullptr; // Miniature horizontal piano roll

    // Snapshot used by the piano roll painter (updated every timer tick)
    struct PRNote { double beatOn; double beatLen; int16_t pitch; float vel; uint8_t style; };
    std::vector<PRNote> prSnapshot; // GUI-thread only
    double              prCurrentBeat = 0.0; // beat position of the playhead
    static constexpr int kPRHeight     = 220; // px (params mode, Synthesia roll)
    static constexpr int kPRBeats      = 16;  // beats shown (scrolling window)
    static constexpr int kPianoStripH  = 60;  // px — piano keyboard at bottom of roll

    // ---- View mode (params vs editor) ----
    int  viewMode        = 0;   // 0 = params (Midnight), 1 = editor (Melody Maker)
    bool paramsCollapsed = false; // collapse/expand parameter panels
    RECT titleRectMidnight  = {};
    RECT titleRectMelody    = {};
    RECT paramsTitleRect    = {}; // hit-rect for PARAMÈTRES toggle header

    // ---- Editor-mode persistent edits (per current style) ----
    std::vector<PRNote> editorNotes;     // editable notes (filtered to one style)
    int                 editorStyle = -1; // style editorNotes corresponds to
    std::vector<std::vector<PRNote>> undoStack;
    std::vector<std::vector<PRNote>> redoStack;
    static constexpr size_t kUndoLimit = 64;

    // Editor-time interactive drag state
    bool   prDragging      = false;
    int    prDragNoteIndex = -1;
    double prDragStartBeat = 0.0;

    // Undo/redo buttons
    HWND hUndoBtn = nullptr;
    HWND hRedoBtn = nullptr;

    // Tab strip (custom-painted, no child windows)
    RECT tabRects[kTabCount] = {};
    RECT tabDotRects[kTabCount] = {};
    int  hoverTab = -1;

    // Theme resources (created in attached, freed in removed)
    HBRUSH brBg     = nullptr;
    HFONT  fontReg  = nullptr;
    HFONT  fontBold = nullptr;
    HFONT  fontTitle= nullptr;
    HFONT  fontSection = nullptr;
    HFONT  fontTab     = nullptr;
    HFONT  fontMono = nullptr;

    // Section paint info â€” y position of each section title (filled by applyLayout)
    int sectionY[4] = {0,0,0,0};
    int sectionX[4] = {0,0,0,0};
    int sectionW[4] = {0,0,0,0};
    static const wchar_t* sectionTitle(int i) {
        static const wchar_t* t[4] = { L"SOUND", L"RYTHME", L"HARMONIE", L"EXPRESSION" };
        return t[i];
    }

    static const wchar_t* tabLabel(int i) {
        static const wchar_t* t[kTabCount] = {
            L"Voix", L"Harpe", L"Basse", L"Percu.", L"Marimba", L"Piano"
        };
        return t[i];
    }

    static void ensureWndClass() {
        // Check if already registered (survives plugin reload within same process).
        WNDCLASSW existing{};
        if (GetClassInfoW(g_hInst, kWndClass, &existing)) return;

        WNDCLASSW wc{};
        wc.lpfnWndProc   = staticWndProc;
        wc.hInstance     = g_hInst;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kWndClass;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
    }

    static LRESULT CALLBACK staticWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<MelodyMakerView*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (self) return self->wndProc(h, m, w, l);
        return DefWindowProcW(h, m, w, l);
    }

    // Piano roll child window proc
    static constexpr const wchar_t* kPRClass = L"MidnightMMPianoRoll";
    static void ensurePRClass() {
        WNDCLASSW ex{};
        if (GetClassInfoW(g_hInst, kPRClass, &ex)) return;
        WNDCLASSW wc{};
        wc.lpfnWndProc   = prWndProc;
        wc.hInstance     = g_hInst;
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kPRClass;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
    }
    static LRESULT CALLBACK prWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_ERASEBKGND) return 1;
        auto* self = reinterpret_cast<MelodyMakerView*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_PAINT) {
            if (self) { self->paintPianoRoll(h); return 0; }
        }
        if (self && self->isEditorActive()) {
            int mx = GET_X_LPARAM(l), my = GET_Y_LPARAM(l);
            switch (m) {
                case WM_LBUTTONDOWN: {
                    SetCapture(h);
                    self->prEditorMouseDown(mx, my);
                    return 0;
                }
                case WM_MOUSEMOVE:
                    if (self->prDragging) self->prEditorMouseDrag(mx, my);
                    return 0;
                case WM_LBUTTONUP:
                    if (self->prDragging) {
                        ReleaseCapture();
                        self->prDragging = false;
                        self->prDragNoteIndex = -1;
                    }
                    return 0;
                case WM_RBUTTONDOWN:
                    self->prEditorRightClick(mx, my);
                    return 0;
            }
        }
        return DefWindowProcW(h, m, w, l);
    }
    // -- Coordinate helpers (used by both painter and edit handlers) --
    // Last computed pitch range, beat range, and key-strip width for the
    // current piano roll painting; updated on each WM_PAINT.
    int    prKeyW       = 0;    // left-strip width (0 in Synthesia mode, no left strip)
    int    prPianoStripH = kPianoStripH; // bottom keyboard strip height (updated at paint)
    int    prPitchLo    = 36;
    int    prPitchHi    = 84;
    double prStartBeat  = 0.0;
    double prEndBeat    = 16.0;
    int    prW          = 0;
    int    prH          = 0;

    bool isEditorActive() const {
        return viewMode == 1 && plugin && editorStyle == plugin->getStyleType();
    }
    static constexpr int kEditorBeats = 32; // fixed time window in editor mode

    // Convert roll client coords → (beat, pitch).  Returns false if outside the note area.
    // Synthesia layout: X = pitch, Y = time (top=oldest, bottom=newest), piano strip at bottom.
    bool prClientToBeatPitch(int mx, int my, double& beatOut, int& pitchOut) const {
        int rollH = prH - prPianoStripH;
        if (mx < 0 || mx >= prW || my < 0 || my >= rollH) return false;
        int range = prPitchHi - prPitchLo;
        if (range <= 0 || prW <= 0 || rollH <= 0) return false;
        pitchOut = prPitchLo + (int)((double)mx / prW * range);
        pitchOut = std::clamp(pitchOut, prPitchLo, prPitchHi - 1);
        double beatRange = prEndBeat - prStartBeat;
        if (beatRange <= 0) return false;
        beatOut = prStartBeat + (double)my / rollH * beatRange;
        return true;
    }
    // Convert X coord only → pitch (used for piano-strip clicks).
    bool prPianoXToPitch(int mx, int& pitchOut) const {
        if (mx < 0 || mx >= prW) return false;
        int range = prPitchHi - prPitchLo;
        if (range <= 0) return false;
        pitchOut = prPitchLo + (int)((double)mx / prW * range);
        pitchOut = std::clamp(pitchOut, prPitchLo, prPitchHi - 1);
        return true;
    }

    // ============================================================
    //  paintPianoRoll  —  Synthesia style
    //  X axis: pitch (low = left, high = right)
    //  Y axis: time  (top = oldest, bottom = most recent)
    //  Piano keyboard strip at the very bottom (kPianoStripH px).
    // ============================================================
    void paintPianoRoll(HWND h) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int W = rc.right, H = rc.bottom;
        prW = W; prH = H;
        prKeyW = 0; // no left strip in Synthesia mode
        prPianoStripH = kPianoStripH;

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

        // ---- Background ----
        HBRUSH brBack = CreateSolidBrush(RGB(18, 22, 30));
        FillRect(mem, &rc, brBack); DeleteObject(brBack);

        // ---- Pick note source ----
        int curStyle = plugin ? plugin->getStyleType() : 0;
        bool editor = isEditorActive();
        const std::vector<PRNote>* src = nullptr;
        std::vector<PRNote> filtered;
        if (editor) {
            src = &editorNotes;
        } else {
            filtered.reserve(prSnapshot.size());
            for (auto& n : prSnapshot)
                if ((int)n.style == curStyle) filtered.push_back(n);
            src = &filtered;
        }

        // ---- Time window ----
        double startBeat, endBeat;
        if (editor) {
            startBeat = 0.0; endBeat = (double)kEditorBeats;
        } else {
            endBeat = prCurrentBeat; startBeat = endBeat - (double)kPRBeats;
        }
        prStartBeat = startBeat;
        prEndBeat   = endBeat;

        // ---- Dynamic pitch range ----
        static const bool kIsBlack[12] = {0,1,0,1,0,0,1,0,1,0,1,0};
        int pLo = 48, pHi = 84; // default C3..C6
        if (!src->empty()) {
            int mn = 127, mx2 = 0;
            for (auto& n : *src) {
                if (n.pitch < mn) mn = n.pitch;
                if (n.pitch > mx2) mx2 = n.pitch;
            }
            mn = std::max(0, mn - 2);
            mx2 = std::min(127, mx2 + 2);
            mn -= (mn % 12);               // snap to C
            mx2 += (12 - (mx2 % 12)) % 12;
            while ((mx2 - mn) < 24) { if (mn >= 12) mn -= 12; else mx2 += 12; }
            pLo = mn; pHi = mx2;
        }
        prPitchLo = pLo;
        prPitchHi = pHi;
        const int pitchRange = pHi - pLo;

        int rollH = H - kPianoStripH;
        double beatRange = endBeat - startBeat;

        // Coordinate lambdas ---------------------------------------------------
        // pitchToX: left edge of a semitone column
        auto pitchToX1 = [&](int p) -> int {
            return (int)((double)(p - pLo) / pitchRange * W);
        };
        auto pitchToX2 = [&](int p) -> int {  // right edge = left edge of next
            return (int)((double)(p + 1 - pLo) / pitchRange * W);
        };
        // beatToY: Y coord in roll area (0=top/oldest, rollH=bottom/most-recent)
        auto beatToY = [&](double b) -> int {
            return (int)((b - startBeat) / beatRange * rollH);
        };

        // ---- Semitone column shading (black-key lanes slightly darker) ----
        for (int p = pLo; p < pHi; ++p) {
            if (!kIsBlack[(p + 120) % 12]) continue;
            int x1 = pitchToX1(p), x2 = pitchToX2(p);
            RECT rl = { x1, 0, x2, rollH };
            HBRUSH bl = CreateSolidBrush(RGB(25, 30, 40));
            FillRect(mem, &rl, bl); DeleteObject(bl);
        }

        // ---- Octave / beat grid ----
        // Vertical: C-note column borders
        for (int p = pLo; p <= pHi; ++p) {
            if (((p + 120) % 12) == 0) {
                int x = pitchToX1(p);
                HPEN pn = CreatePen(PS_SOLID, 1, RGB(55, 65, 80));
                HPEN op = (HPEN)SelectObject(mem, pn);
                MoveToEx(mem, x, 0, nullptr); LineTo(mem, x, rollH);
                SelectObject(mem, op); DeleteObject(pn);
            }
        }
        // Horizontal: beat grid
        for (int b = 0; b <= (editor ? kEditorBeats : (int)kPRBeats); ++b) {
            int y = beatToY(startBeat + b);
            y = std::clamp(y, 0, rollH - 1);
            COLORREF c = (b % 4 == 0) ? RGB(55, 65, 80) : RGB(35, 42, 52);
            HPEN pn = CreatePen(PS_SOLID, 1, c);
            HPEN op = (HPEN)SelectObject(mem, pn);
            MoveToEx(mem, 0, y, nullptr); LineTo(mem, W, y);
            SelectObject(mem, op); DeleteObject(pn);
        }
        // Bar numbers in editor mode (left margin text)
        if (editor) {
            SetBkMode(mem, TRANSPARENT);
            HFONT of = (HFONT)SelectObject(mem, fontReg);
            SetTextColor(mem, RGB(100, 115, 135));
            for (int b = 0; b <= kEditorBeats; b += 4) {
                int y = beatToY((double)b);
                y = std::clamp(y, 0, rollH - 14);
                wchar_t s[8]; swprintf_s(s, L"%d", b / 4 + 1);
                TextOutW(mem, 2, y + 1, s, (int)wcslen(s));
            }
            SelectObject(mem, of);
        }

        // ---- Per-style colors ----
        static const COLORREF kCol[] = {
            RGB(79,195,247), RGB(129,199,132), RGB(255,183,77),
            RGB(240,98,146), RGB(206,147,216), RGB(255,241,118) };
        static const COLORREF kColFill[] = {
            RGB(25,70,105),  RGB(35,75,42),   RGB(90,65,18),
            RGB(95,28,52),   RGB(72,45,88),   RGB(90,85,28) };

        // ---- Notes ----
        // Active pitches this frame (to highlight piano keys)
        bool activeKey[128] = {};

        for (auto& n : *src) {
            double nOn = n.beatOn, nOff = n.beatOn + std::max(n.beatLen, 0.05);
            if (nOff < startBeat || nOn > endBeat) continue;
            int p = (int)n.pitch;
            if (p < pLo || p >= pHi) continue;
            int s = std::clamp((int)n.style, 0, 5);

            int x1 = pitchToX1(p), x2 = std::max(pitchToX2(p), x1 + 2);
            int y1 = std::clamp(beatToY(nOn),  0, rollH);
            int y2 = std::clamp(beatToY(nOff), 0, rollH);
            if (y2 <= y1) y2 = y1 + 3;

            // Note body
            RECT rn = { x1 + 1, y1, x2 - 1, y2 };
            HBRUSH bf = CreateSolidBrush(kColFill[s]);
            FillRect(mem, &rn, bf); DeleteObject(bf);
            // Accent border
            HPEN pn = CreatePen(PS_SOLID, 1, kCol[s]);
            HPEN op = (HPEN)SelectObject(mem, pn);
            HBRUSH nb = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, rn.left, rn.top, rn.right, rn.bottom);
            SelectObject(mem, op); DeleteObject(pn);
            SelectObject(mem, nb);

            // Bright top cap (note head / attack indicator)
            HPEN cap = CreatePen(PS_SOLID, 2, kCol[s]);
            HPEN oc  = (HPEN)SelectObject(mem, cap);
            MoveToEx(mem, x1 + 1, y1 + 1, nullptr);
            LineTo   (mem, x2 - 1, y1 + 1);
            SelectObject(mem, oc); DeleteObject(cap);

            // Mark key as active if the note is currently sounding
            if (nOn <= prCurrentBeat && prCurrentBeat <= nOff) activeKey[p] = true;
        }

        // ---- Playhead (horizontal line) ----
        {
            double playBeat = editor
                ? std::fmod(prCurrentBeat, (double)kEditorBeats)
                : prCurrentBeat;
            if (playBeat < 0) playBeat += kEditorBeats;
            int py = std::clamp(beatToY(playBeat), 0, rollH - 1);
            HPEN pph = CreatePen(PS_SOLID, 2, editor ? RGB(255,255,120) : RGB(220,220,255));
            HPEN oph = (HPEN)SelectObject(mem, pph);
            MoveToEx(mem, 0, py, nullptr); LineTo(mem, W, py);
            SelectObject(mem, oph); DeleteObject(pph);
        }

        // ---- Piano keyboard strip (bottom kPianoStripH px) ----
        {
            int kbY = rollH; // top of keyboard strip
            // White key background
            HBRUSH wkBr = CreateSolidBrush(RGB(235,232,225));
            RECT rcKb = { 0, kbY, W, H };
            FillRect(mem, &rcKb, wkBr); DeleteObject(wkBr);

            // White key dividers
            for (int p = pLo; p < pHi; ++p) {
                if (kIsBlack[(p + 120) % 12]) continue;
                int x2 = pitchToX2(p);
                HPEN pn = CreatePen(PS_SOLID, 1, RGB(150,145,135));
                HPEN op = (HPEN)SelectObject(mem, pn);
                MoveToEx(mem, x2, kbY, nullptr); LineTo(mem, x2, H);
                SelectObject(mem, op); DeleteObject(pn);
                // Active highlight
                if (activeKey[p]) {
                    int xw1 = pitchToX1(p) + 1, xw2 = pitchToX2(p) - 1;
                    HBRUSH ah = CreateSolidBrush(kCol[curStyle]);
                    RECT ra = { xw1, kbY + kPianoStripH/2, xw2, H - 2 };
                    FillRect(mem, &ra, ah); DeleteObject(ah);
                }
            }

            // Black keys (drawn on top, 60% height, 65% width)
            int bkH = (int)(kPianoStripH * 0.62);
            for (int p = pLo; p < pHi; ++p) {
                if (!kIsBlack[(p + 120) % 12]) continue;
                double semW = (double)W / pitchRange;
                int cx = pitchToX1(p) + (int)(semW / 2.0);
                int bkW = std::max(3, (int)(semW * 0.65));
                int xb1 = cx - bkW / 2, xb2 = xb1 + bkW;
                COLORREF bkFill = activeKey[p] ? kCol[curStyle] : RGB(30, 28, 24);
                HBRUSH bb = CreateSolidBrush(bkFill);
                RECT rb = { xb1, kbY, xb2, kbY + bkH };
                FillRect(mem, &rb, bb); DeleteObject(bb);
                // Border
                HPEN pn = CreatePen(PS_SOLID, 1, RGB(15, 12, 10));
                HPEN op = (HPEN)SelectObject(mem, pn);
                HBRUSH nob = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
                Rectangle(mem, rb.left, rb.top, rb.right, rb.bottom);
                SelectObject(mem, op); DeleteObject(pn);
                SelectObject(mem, nob);
            }

            // Divider line between roll and keyboard
            HPEN div = CreatePen(PS_SOLID, 1, RGB(60, 70, 85));
            HPEN odiv = (HPEN)SelectObject(mem, div);
            MoveToEx(mem, 0, kbY, nullptr); LineTo(mem, W, kbY);
            SelectObject(mem, odiv); DeleteObject(div);
        }

        // ---- Border ----
        HPEN penB = CreatePen(PS_SOLID, 1, RGB(55,65,80));
        HPEN opB  = (HPEN)SelectObject(mem, penB);
        HBRUSH nb2 = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
        Rectangle(mem, 0, 0, W, H);
        SelectObject(mem, opB); DeleteObject(penB);
        SelectObject(mem, nb2);

        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h, &ps);
    }

    // ---- View mode + edit helpers ----
    void setViewMode(int m) {
        if (m == viewMode) return;
        viewMode = m;
        if (m == 1) enterEditorMode();
        else        exitEditorMode();
        applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
        InvalidateRect(hWnd, nullptr, TRUE);
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }
    void enterEditorMode() {
        // Snapshot current notes filtered by current style into editorNotes
        int s = plugin ? plugin->getStyleType() : 0;
        if (editorStyle != s) {
            editorNotes.clear();
            if (plugin && plugin->recMtx.try_lock()) {
                double rsb = plugin->recStartBeat;
                for (auto& n : plugin->recNotes) {
                    if ((int)n.style != s) continue;
                    PRNote pr{ rsb + n.beatOn, n.beatLen, n.pitch, n.vel, n.style };
                    editorNotes.push_back(pr);
                }
                plugin->recMtx.unlock();
            }
            editorStyle = s;
            undoStack.clear();
            redoStack.clear();
        }
    }
    void exitEditorMode() { /* keep editorNotes, just hide the editor UI */ }

    void pushUndo() {
        undoStack.push_back(editorNotes);
        if (undoStack.size() > kUndoLimit) undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    void doUndo() {
        if (undoStack.empty()) return;
        redoStack.push_back(editorNotes);
        editorNotes = undoStack.back();
        undoStack.pop_back();
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }
    void doRedo() {
        if (redoStack.empty()) return;
        undoStack.push_back(editorNotes);
        editorNotes = redoStack.back();
        redoStack.pop_back();
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }

    int hitTestEditorNote(int mx, int my) const {
        if (!isEditorActive()) return -1;
        int rollH = prH - prPianoStripH;
        if (mx < 0 || mx >= prW || my < 0 || my >= rollH) return -1;
        double beatRange = prEndBeat - prStartBeat;
        if (beatRange <= 0 || prW <= 0 || rollH <= 0) return -1;
        int range = prPitchHi - prPitchLo;
        if (range <= 0) return -1;
        for (int i = (int)editorNotes.size() - 1; i >= 0; --i) {
            const auto& n = editorNotes[i];
            // Pitch → X column
            int x1 = (int)((double)((int)n.pitch - prPitchLo) / range * prW);
            int x2 = (int)((double)((int)n.pitch + 1 - prPitchLo) / range * prW);
            if (x2 <= x1) x2 = x1 + 2;
            // Beat → Y row  (top = oldest, bottom = most recent)
            int y1 = (int)((n.beatOn - prStartBeat) / beatRange * rollH);
            int y2 = (int)((n.beatOn + std::max(n.beatLen, 0.05) - prStartBeat) / beatRange * rollH);
            if (y2 <= y1) y2 = y1 + 3;
            if (mx >= x1 && mx < x2 && my >= y1 && my <= y2) return i;
        }
        return -1;
    }

    void prEditorMouseDown(int mx, int my) {
        int rollH = prH - prPianoStripH;

        // ---- Click in the piano keyboard strip → tap a key / insert note ----
        if (my >= rollH) {
            int pitch;
            if (!prPianoXToPitch(mx, pitch)) return;
            pushUndo();
            // Insert at the current playhead beat (mod editor window) or beat 0
            double insertBeat = 0.0;
            if (isEditorActive())
                insertBeat = std::fmod(prCurrentBeat, (double)kEditorBeats);
            insertBeat = std::max(0.0, std::round(insertBeat * 4.0) / 4.0);
            int curStyle = plugin ? plugin->getStyleType() : 0;
            PRNote n{ insertBeat, 0.5, (int16_t)pitch, 0.85f, (uint8_t)curStyle };
            editorNotes.push_back(n);
            if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
            return;
        }

        // ---- Click in the note roll area ----
        int hit = hitTestEditorNote(mx, my);
        pushUndo();
        if (hit >= 0) {
            // Grab existing note to extend by dragging
            prDragging = true;
            prDragNoteIndex = hit;
            prDragStartBeat = editorNotes[hit].beatOn;
            return;
        }
        // Empty area: add a new note, pitch from X, beat from Y
        double beat; int pitch;
        if (!prClientToBeatPitch(mx, my, beat, pitch)) return;
        beat = std::max(0.0, std::round(beat * 4.0) / 4.0); // snap to 1/4
        int curStyle = plugin ? plugin->getStyleType() : 0;
        PRNote n{ beat, 0.25, (int16_t)pitch, 0.85f, (uint8_t)curStyle };
        editorNotes.push_back(n);
        prDragging = true;
        prDragNoteIndex = (int)editorNotes.size() - 1;
        prDragStartBeat = beat;
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }
    void prEditorMouseDrag(int mx, int my) {
        if (prDragNoteIndex < 0 || prDragNoteIndex >= (int)editorNotes.size()) return;
        // Drag extends length: compute beat from Y, keep pitch fixed
        int rollH = prH - prPianoStripH;
        double beatRange = prEndBeat - prStartBeat;
        if (rollH <= 0 || beatRange <= 0) return;
        double curBeat = prStartBeat + (double)std::clamp(my, 0, rollH) / rollH * beatRange;
        curBeat = std::round(curBeat * 4.0) / 4.0;
        double newLen = std::max(0.25, curBeat - prDragStartBeat + 0.25);
        editorNotes[prDragNoteIndex].beatLen = newLen;
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }
    void prEditorRightClick(int mx, int my) {
        int hit = hitTestEditorNote(mx, my);
        if (hit < 0) return;
        pushUndo();
        editorNotes.erase(editorNotes.begin() + hit);
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }

    LRESULT wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
        switch (msg) {
            case WM_COMMAND: {
                int id   = LOWORD(w);
                int code = HIWORD(w);
                if (id == kIdExport && code == BN_CLICKED) {
                    doExportMidi();
                    return 0;
                }
                if (id == kIdExportWav && code == BN_CLICKED) {
                    doExportWav();
                    return 0;
                }
                if (id == kIdSavePreset && code == BN_CLICKED) {
                    doSavePreset();
                    return 0;
                }
                if (id == kIdLoadPreset && code == BN_CLICKED) {
                    doLoadPreset();
                    return 0;
                }
                if (id == kIdUndo && code == BN_CLICKED) { doUndo(); return 0; }
                if (id == kIdRedo && code == BN_CLICKED) { doRedo(); return 0; }
                if (id == kIdAuto && code == BN_CLICKED) {
                    bool now = !plugin->getAutoKey();
                    plugin->setAutoKey(now);
                    if (!now) plugin->ctxClear();
                    SetWindowTextW(hAutoBtn,
                        now ? L"LISTEN  \u25CF  ON" : L"LISTEN  \u25CB  OFF");
                    InvalidateRect(hAutoBtn, nullptr, TRUE);
                    return 0;
                }
                if (id == kIdDice && code == BN_CLICKED) {
                    plugin->randomizeSeed();
                    refreshOne(mm::kParamSeed);
                    return 0;
                }
                if (id == kIdPianoMel && code == BN_CLICKED) {
                    LRESULT chk = SendMessageW(hPianoMelChk, BM_GETCHECK, 0, 0);
                    int st = plugin->getStyleType();
                    plugin->pianoMelodyPerStyle[st] = (chk == BST_CHECKED);
                    return 0;
                }
                if (id == kIdPianoChord && code == BN_CLICKED) {
                    LRESULT chk = SendMessageW(hPianoChordChk, BM_GETCHECK, 0, 0);
                    int st = plugin->getStyleType();
                    plugin->pianoChordPerStyle[st] = (chk == BST_CHECKED);
                    return 0;
                }
                if (id == kIdPercRhy && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hPercRhyCombo, CB_GETCURSEL, 0, 0);
                    int st = plugin->getStyleType();
                    plugin->percRhythmPerStyle[st] =
                        std::clamp(sel, 0, mm::kDrumPatternCount - 1);
                    return 0;
                }
                if (id == kIdWave && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hWaveCombo, CB_GETCURSEL, 0, 0);
                    if (sel != CB_ERR) {
                        int st = plugin->getStyleType();
                        plugin->setSfPreset(st, sel);
                        plugin->setWaveType(sel); // keep legacy state happy
                    }
                    return 0;
                }
                if (id == kIdProg && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hProgCombo, CB_GETCURSEL, 0, 0);
                    if (sel != CB_ERR) {
                        plugin->setProgType(sel);
                        // Refresh compat dots on Mode combo.
                        if (hCtrl[mm::kParamMode]) {
                            InvalidateRect(hCtrl[mm::kParamMode], nullptr, FALSE);
                            UpdateWindow(hCtrl[mm::kParamMode]);
                        }
                    }
                    return 0;
                }
                if ((id == kIdLockMode || id == kIdLockProg ||
                     id == kIdLockSubdiv) && code == BN_CLICKED) {
                    int s = plugin->getStyleType();
                    if (id == kIdLockMode) {
                        plugin->setLockMode(s, !plugin->lockModePerStyle[s]);
                        InvalidateRect(hLockMode, nullptr, TRUE);
                    } else if (id == kIdLockProg) {
                        plugin->setLockProg(s, !plugin->lockProgPerStyle[s]);
                        InvalidateRect(hLockProg, nullptr, TRUE);
                    } else {
                        plugin->setLockSubdiv(s, !plugin->lockSubdivPerStyle[s]);
                        InvalidateRect(hLockSubdiv, nullptr, TRUE);
                    }
                    return 0;
                }
                if (id == kIdMeter && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hMeterCombo, CB_GETCURSEL, 0, 0);
                    if (sel != CB_ERR) {
                        plugin->beatsPerBarOverride = (sel == 0) ? 0 : sel + 1;
                        if (plugin->beatsPerBarOverride >= 2)
                            plugin->beatsPerBarAtomic.store(plugin->beatsPerBarOverride, std::memory_order_relaxed);
                    }
                    return 0;
                }
                if (id == kIdSection && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hSectionCombo, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < kSecCount) {
                        plugin->currentSection.store(sel, std::memory_order_relaxed);
                        // Notify the host so it tracks the change immediately
                        // (without this the host may restore an old section on setState).
                        double norm = sel / 4.0;
                        plugin->beginEdit(MelodyMakerVST3::kParamSection);
                        plugin->performEdit(MelodyMakerVST3::kParamSection, norm);
                        plugin->endEdit(MelodyMakerVST3::kParamSection);
                        if (sel == kSecIntro && !plugin->introFadeDone) plugin->sectionVolumeRamp = 0.0f;
                        if (sel == kSecOutro) plugin->sectionVolumeRamp = 1.0f;
                    }
                    return 0;
                }
                if (id == kIdStartBar && code == CBN_SELCHANGE) {
                    static constexpr int kSBV[] = { 0,1,2,4,8 };
                    int sel = (int)SendMessageW(hStartBarCombo, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < 5)
                        plugin->startBarPerStyle[plugin->getStyleType()] = kSBV[sel];
                    return 0;
                }
                if (code == CBN_SELCHANGE && id >= kIdBase && id < kIdExport) {
                    onComboChange(id - kIdBase);
                    return 0;
                }
                break;
            }
            case WM_HSCROLL: {
                HWND tb = (HWND)l;
                int id = (int)GetWindowLongPtrW(tb, GWLP_ID);
                if (id >= kIdBase && id < kIdBase + (int)mm::kParamCount) {
                    onTrackbarChange(id - kIdBase);
                    return 0;
                }
                if (id == kIdVol) {
                    int pos = (int)SendMessageW(hVolSlider, TBM_GETPOS, 0, 0);
                    plugin->setSfVolume(plugin->getStyleType(), pos / 100.0f);
                    refreshVolValue();
                    return 0;
                }
                if (id == kIdHumanize) {
                    auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW((HWND)l, GWLP_USERDATA));
                    int pos = kw ? kw->getPos() : 0;
                    plugin->humanizePerStyle[plugin->getStyleType()] = pos / 1000.0f;
                    refreshHumanizeValue();
                    return 0;
                }
                if (id == kIdRetard) {
                    auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW((HWND)l, GWLP_USERDATA));
                    int pos = kw ? kw->getPos() : 0;
                    plugin->retardPerStyle[plugin->getStyleType()] = pos / 1000.0f;
                    refreshRetardValue();
                    return 0;
                }
                break;
            }
            case WM_TIMER:
                if (w == 1) {
                    refreshNoteDisplay();
                    // Update playhead beat position (used by both modes for visuals)
                    if (plugin)
                        prCurrentBeat = plugin->lastBeatPos.load(std::memory_order_relaxed);
                    // Only refresh the live snapshot when not in editor mode.
                    if (hPianoRoll && plugin && viewMode == 0) {
                        prSnapshot.clear();
                    if (plugin->recMtx.try_lock()) {
                        double rsb = plugin->recStartBeat;
                        for (auto& n : plugin->recNotes) {
                            PRNote pr;
                            pr.beatOn  = rsb + n.beatOn;
                            pr.beatLen = n.beatLen;
                            pr.pitch   = n.pitch;
                            pr.vel     = n.vel;
                            pr.style   = n.style;
                            prSnapshot.push_back(pr);
                        }
                        plugin->recMtx.unlock();
                    }
                        InvalidateRect(hPianoRoll, nullptr, FALSE);
                    } else if (hPianoRoll && viewMode == 1) {
                        // Editor mode: just repaint to update playhead
                        InvalidateRect(hPianoRoll, nullptr, FALSE);
                    }
                }
                return 0;
            case WM_ERASEBKGND:
                return 1; // we paint ourselves in WM_PAINT
            case WM_PAINT: {
                PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
                paintBackground(hdc);
                paintTabs(hdc);
                EndPaint(h, &ps);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
                // Title click: toggle view mode (Midnight vs Melody Maker)
                if (PtInRect(&titleRectMidnight, POINT{x, y})) {
                    setViewMode(0);
                    return 0;
                }
                if (PtInRect(&titleRectMelody, POINT{x, y})) {
                    setViewMode(1);
                    return 0;
                }
                // PARAMÈTRES collapsible header click
                if (paramsTitleRect.bottom > paramsTitleRect.top &&
                    PtInRect(&paramsTitleRect, POINT{x, y})) {
                    paramsCollapsed = !paramsCollapsed;
                    applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
                    InvalidateRect(hWnd, nullptr, TRUE);
                    return 0;
                }
                // First test the power dot â€” toggling does NOT change the
                // currently edited tab.
                for (int i = 0; i < kTabCount; ++i) {
                    if (PtInRect(&tabDotRects[i], POINT{x, y})) {
                        plugin->toggleStyleEnabled(i);
                        RECT rcTabs = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                        InvalidateRect(hWnd, &rcTabs, FALSE);
                        return 0;
                    }
                }
                // Otherwise tab body click â†’ select that tab (and enable it).
                for (int i = 0; i < kTabCount; ++i) {
                    if (PtInRect(&tabRects[i], POINT{x, y})) {
                        plugin->setStyleType(i); // setStyleType also enables it
                        applyVisibilityForStyle(i);
                        // Refresh all on-screen controls from the new style.
                        refreshAll();
                        populateInstrumentCombo(i);
                        if (hVolSlider)
                            SendMessageW(hVolSlider, TBM_SETPOS, TRUE,
                                (LPARAM)(int)std::round(
                                    plugin->sfVolumePerStyle[i] * 100.0f));
                        refreshVolValue();
                        if (hHumanizeKnob)
                            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA)))
                                kw->setPos((int)std::round(plugin->humanizePerStyle[i] * 1000.0f));
                        refreshHumanizeValue();
                        if (hRetardKnob)
                            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA)))
                                kw->setPos((int)std::round(plugin->retardPerStyle[i] * 1000.0f));
                        refreshRetardValue();
                        if (hStartBarCombo) {
                            static constexpr int kSBV[] = { 0,1,2,4,8 };
                            int sv = plugin->startBarPerStyle[i];
                            int si = 0; for (int k=0;k<5;k++) if (kSBV[k]==sv) si=k;
                            SendMessageW(hStartBarCombo, CB_SETCURSEL, si, 0);
                        }
                        if (hSectionCombo)
                            SendMessageW(hSectionCombo, CB_SETCURSEL,
                                std::clamp(plugin->currentSection.load(std::memory_order_relaxed), 0, 4), 0);
                        if (hProgCombo) SendMessageW(hProgCombo, CB_SETCURSEL,
                                                     plugin->getProgType(), 0);
                        InvalidateRect(hWnd, nullptr, TRUE);
                        break;
                    }
                }
                return 0;
            }
            case WM_MOUSEMOVE: {
                int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
                int newHover = -1;
                for (int i = 0; i < kTabCount; ++i)
                    if (PtInRect(&tabRects[i], POINT{x, y})) { newHover = i; break; }
                if (newHover != hoverTab) {
                    hoverTab = newHover;
                    // Repaint just the tab strip
                    RECT rcTabs = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                    InvalidateRect(hWnd, &rcTabs, FALSE);
                    if (newHover >= 0) {
                        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
                        TrackMouseEvent(&tme);
                    }
                }
                return 0;
            }
            case WM_MOUSELEAVE:
                if (hoverTab != -1) {
                    hoverTab = -1;
                    RECT rcTabs = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                    InvalidateRect(hWnd, &rcTabs, FALSE);
                }
                return 0;
            case WM_CTLCOLORSTATIC: {
                HDC hdc = (HDC)w;
                HWND ctl = (HWND)l;
                SetBkMode(hdc, TRANSPARENT);
                bool isValue = false;
                for (int i = 0; i < (int)mm::kParamCount; ++i)
                    if (ctl == hValue[i]) { isValue = true; break; }
                if (ctl == hNoteDisplay)      SetTextColor(hdc, kColAccent);
                else if (isValue)             SetTextColor(hdc, kColTextValue);
                else                          SetTextColor(hdc, kColText);
                return (LRESULT)brBg;
            }
            case WM_CTLCOLORBTN:
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX: {
                HDC hdc = (HDC)w;
                SetBkMode(hdc, OPAQUE);
                SetBkColor(hdc, kColControl);
                SetTextColor(hdc, kColText);
                static HBRUSH brCtl = nullptr;
                if (!brCtl) brCtl = CreateSolidBrush(kColControl);
                return (LRESULT)brCtl;
            }
            case WM_MEASUREITEM: {
                auto* mis = (MEASUREITEMSTRUCT*)l;
                if (mis && mis->CtlType == ODT_COMBOBOX) {
                    mis->itemHeight = 22;
                    return TRUE;
                }
                break;
            }
            case WM_DRAWITEM: {
                auto* dis = (DRAWITEMSTRUCT*)l;
                if (!dis) break;
                if (dis->CtlID == (UINT)kIdExport) {
                    drawAccentButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdExportWav) {
                    drawAccentButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdSavePreset ||
                    dis->CtlID == (UINT)kIdLoadPreset) {
                    drawAccentButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdAuto) {
                    drawListenButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdDice) {
                    drawDiceButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdLockMode) {
                    int s = plugin->getStyleType();
                    drawLockButton(dis, plugin->lockModePerStyle[s]);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdLockProg) {
                    int s = plugin->getStyleType();
                    drawLockButton(dis, plugin->lockProgPerStyle[s]);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdLockSubdiv) {
                    int s = plugin->getStyleType();
                    drawLockButton(dis, plugin->lockSubdivPerStyle[s]);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdUndo || dis->CtlID == (UINT)kIdRedo) {
                    drawArrowButton(dis, dis->CtlID == (UINT)kIdUndo);
                    return TRUE;
                }
                if (dis->CtlType == ODT_COMBOBOX) {
                    drawComboItem(dis);
                    return TRUE;
                }
                break;
            }
        }
        return DefWindowProcW(h, msg, w, l);
    }

    // ---- Custom painting helpers --------------------------------------------
    void paintBackground(HDC hdc) {
        RECT rc; GetClientRect(hWnd, &rc);

        // Fill base bg
        FillRect(hdc, &rc, brBg);

        // Header gradient
        RECT rcH = { 0, 0, rc.right, kHeaderH };
        TRIVERTEX vtx[2] = {};
        vtx[0].x = rcH.left;  vtx[0].y = rcH.top;
        vtx[0].Red   = (COLOR16)(GetRValue(kColHeader1) << 8);
        vtx[0].Green = (COLOR16)(GetGValue(kColHeader1) << 8);
        vtx[0].Blue  = (COLOR16)(GetBValue(kColHeader1) << 8);
        vtx[1].x = rcH.right; vtx[1].y = rcH.bottom;
        vtx[1].Red   = (COLOR16)(GetRValue(kColHeader2) << 8);
        vtx[1].Green = (COLOR16)(GetGValue(kColHeader2) << 8);
        vtx[1].Blue  = (COLOR16)(GetBValue(kColHeader2) << 8);
        GRADIENT_RECT gr = { 0, 1 };
        GradientFill(hdc, vtx, 2, &gr, 1, GRADIENT_FILL_RECT_V);

        // Title text (split into two clickable zones: MIDNIGHT | MELODY MAKER)
        SetBkMode(hdc, TRANSPARENT);
        HFONT old = (HFONT)SelectObject(hdc, fontTitle);
        // Title clipped to left portion of header (x=18..500).
        // Controls (Section, LISTEN, Mesure) occupy the right portion (x=508..882).
        const wchar_t* sMid = L"MIDNIGHT";
        const wchar_t* sMel = L"  MELODY MAKER";
        SIZE szM{}, szMM{};
        GetTextExtentPoint32W(hdc, sMid, (int)wcslen(sMid), &szM);
        GetTextExtentPoint32W(hdc, sMel, (int)wcslen(sMel), &szMM);
        int yT = (kHeaderH - szM.cy) / 2;
        int xT = kPadX;
        // Update click rects (slightly enlarged vertical hit area)
        titleRectMidnight = { xT, 0, xT + szM.cx, kHeaderH };
        titleRectMelody   = { xT + szM.cx, 0, xT + szM.cx + szMM.cx, kHeaderH };

        // Draw MIDNIGHT (active when viewMode==0)
        SetTextColor(hdc, viewMode == 0 ? kColAccent : kColWhite);
        TextOutW(hdc, xT, yT, sMid, (int)wcslen(sMid));
        // Draw MELODY MAKER (active when viewMode==1)
        SetTextColor(hdc, viewMode == 1 ? kColAccent : kColWhite);
        TextOutW(hdc, xT + szM.cx, yT, sMel, (int)wcslen(sMel));

        SelectObject(hdc, fontReg);
        SetTextColor(hdc, kColTextDim);
        RECT rcT = { kPadX, 4, 500, kHeaderH - 4 };
        DrawTextW(hdc, L"VST3  \xB7  Generative MIDI", -1, &rcT,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // "Section" label above section combo (x=508..638, y=2..13).
        {
            RECT rcSec = { 508, 2, 638, 13 };
            DrawTextW(hdc, L"Section", -1, &rcSec, DT_CENTER | DT_SINGLELINE);
        }
        // "Mesure" label above the time-signature combo (x=794..882, y=2..13).
        {
            RECT rcMesure = { kViewWidth - kPadX - 88, 2, kViewWidth - kPadX, 13 };
            DrawTextW(hdc, L"Mesure", -1, &rcMesure, DT_CENTER | DT_SINGLELINE);
        }

        // Section titles + thin separators
        SelectObject(hdc, fontSection);
        HPEN penSep = CreatePen(PS_SOLID, 1, kColBorder);
        HPEN penOld = (HPEN)SelectObject(hdc, penSep);
        for (int i = 0; i < 4; ++i) {
            int y = sectionY[i];
            if (y <= 0) continue;
            int sx = sectionX[i] > 0 ? sectionX[i] : kPadX;
            int sw = sectionW[i] > 0 ? sectionW[i] : (rc.right - 2 * kPadX);
            SetTextColor(hdc, kColAccent);
            RECT rcS = { sx, y, sx + sw, y + 16 };
            DrawTextW(hdc, sectionTitle(i), -1, &rcS,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            int sepY = y + 18;
            MoveToEx(hdc, sx, sepY, nullptr);
            LineTo(hdc, sx + sw, sepY);
        }
        SelectObject(hdc, penOld);
        DeleteObject(penSep);

        // "PARAMÈTRES" collapsible header (params mode only)
        if (viewMode == 0) {
            const int yTop = kHeaderH + kTabsH + 4;
            const int hdrH = 22;
            paramsTitleRect = { 0, yTop, rc.right, yTop + hdrH };
            HBRUSH hdrBr = CreateSolidBrush(RGB(28, 34, 44));
            FillRect(hdc, &paramsTitleRect, hdrBr); DeleteObject(hdrBr);
            SetTextColor(hdc, paramsCollapsed ? kColWhite : kColAccent);
            SetBkMode(hdc, TRANSPARENT);
            const wchar_t* arrow = paramsCollapsed ? L"  \u25B6  PARAM\u00c8TRES" : L"  \u25BC  PARAM\u00c8TRES";
            RECT rcHdr = paramsTitleRect;
            DrawTextW(hdc, arrow, -1, &rcHdr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {
            paramsTitleRect = {};
        }

        SelectObject(hdc, old);
    }


    // Paint the 5-tab instrument selector below the header.
    void paintTabs(HDC hdc) {
        RECT rcStrip = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
        HBRUSH brStrip = CreateSolidBrush(kColBg);
        FillRect(hdc, &rcStrip, brStrip);
        DeleteObject(brStrip);

        int active = std::clamp(plugin->getStyleType(), 0, kTabCount - 1);
        int margin = 12;
        int gap    = 6;
        int totalW = kViewWidth - 2 * margin;
        int tabW   = (totalW - (kTabCount - 1) * gap) / kTabCount;
        int tabH   = kTabsH - 12;
        int top    = kHeaderH + 6;
        int radius = 14;

        SetBkMode(hdc, TRANSPARENT);
        HFONT oldF = (HFONT)SelectObject(hdc, fontTab);

        for (int i = 0; i < kTabCount; ++i) {
            int x = margin + i * (tabW + gap);
            tabRects[i] = { x, top, x + tabW, top + tabH };

            bool isActive  = (i == active);
            bool isHover   = (i == hoverTab) && !isActive;
            bool isEnabled = plugin->isStyleEnabled(i);
            COLORREF fill  = isActive ? kColTabActive
                           : isHover  ? kColControlHi
                                      : kColTabBg;
            // Disabled tabs are drawn dimmer to make state obvious.
            COLORREF txt   = isActive ? kColWhite
                           : isEnabled ? kColText
                                       : kColTextDim;

            HBRUSH br = CreateSolidBrush(fill);
            HPEN   pn = CreatePen(PS_SOLID, 1, isActive ? kColTabActive : kColBorder);
            HBRUSH brOld = (HBRUSH)SelectObject(hdc, br);
            HPEN   pnOld = (HPEN)SelectObject(hdc, pn);
            RoundRect(hdc, x, top, x + tabW, top + tabH, radius, radius);
            SelectObject(hdc, brOld);
            SelectObject(hdc, pnOld);
            DeleteObject(br);
            DeleteObject(pn);

            // Power dot (top-left of the tab) â€” click toggles enable.
            int dotR = 7;
            int dotCx = x + 14;
            int dotCy = top + tabH / 2;
            tabDotRects[i] = { dotCx - dotR, dotCy - dotR,
                               dotCx + dotR, dotCy + dotR };
            COLORREF dotFill = isEnabled
                ? (isActive ? kColWhite : kColAccent)
                : kColControl;
            COLORREF dotEdge = isEnabled ? kColAccent : kColTextDim;
            HBRUSH brDot = CreateSolidBrush(dotFill);
            HPEN   pnDot = CreatePen(PS_SOLID, 2, dotEdge);
            HBRUSH brDotOld = (HBRUSH)SelectObject(hdc, brDot);
            HPEN   pnDotOld = (HPEN)SelectObject(hdc, pnDot);
            Ellipse(hdc, tabDotRects[i].left, tabDotRects[i].top,
                         tabDotRects[i].right, tabDotRects[i].bottom);
            SelectObject(hdc, brDotOld);
            SelectObject(hdc, pnDotOld);
            DeleteObject(brDot);
            DeleteObject(pnDot);

            SetTextColor(hdc, txt);
            RECT rcL = { x + 2 * dotR + 14, top, x + tabW - 6, top + tabH };
            DrawTextW(hdc, tabLabel(i), -1, &rcL,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(hdc, oldF);
    }

    void drawAccentButton(DRAWITEMSTRUCT* dis) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        bool focused = (dis->itemState & ODS_FOCUS)    != 0;
        COLORREF fill = pressed ? kColAccentDark : kColAccent;
        HBRUSH br = CreateSolidBrush(fill);
        FillRect(dis->hDC, &rc, br);
        DeleteObject(br);
        if (focused) {
            HBRUSH brB = CreateSolidBrush(kColWhite);
            FrameRect(dis->hDC, &rc, brB);
            DeleteObject(brB);
        }
        wchar_t txt[128];
        int len = GetWindowTextW(dis->hwndItem, txt, 128);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, kColWhite);
        HFONT old = (HFONT)SelectObject(dis->hDC, fontBold);
        DrawTextW(dis->hDC, txt, len, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, old);
    }

    // Owner-draw the LISTEN toggle (filled when on, hollow when off).
    void drawListenButton(DRAWITEMSTRUCT* dis) {
        RECT rc = dis->rcItem;
        bool on = plugin->getAutoKey();
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;

        COLORREF fill = on
            ? (pressed ? kColAccentDark : kColAccent)
            : (pressed ? kColControlHi  : kColControl);
        COLORREF edge = on ? kColAccent : kColTextDim;
        COLORREF txtC = on ? kColWhite  : kColText;

        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, edge);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br);
        DeleteObject(pn);

        wchar_t txt[128];
        int len = GetWindowTextW(dis->hwndItem, txt, 128);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, txtC);
        HFONT oldF = (HFONT)SelectObject(dis->hDC, fontBold);
        DrawTextW(dis->hDC, txt, len, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, oldF);
    }

    // Owner-draw the dice button (small square with a die glyph).
    void drawDiceButton(DRAWITEMSTRUCT* dis) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF fill = pressed ? kColAccent     : kColControl;
        COLORREF edge = pressed ? kColAccent     : kColTextDim;
        COLORREF txtC = pressed ? kColWhite      : kColAccent;
        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, edge);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br);
        DeleteObject(pn);

        // Draw a 5-pip die face (top-left, top-right, center,
        // bottom-left, bottom-right) â€” works on any GDI font.
        SetBkMode(dis->hDC, TRANSPARENT);
        HBRUSH dotBr = CreateSolidBrush(txtC);
        HBRUSH dotOld = (HBRUSH)SelectObject(dis->hDC, dotBr);
        HPEN   dotPn  = CreatePen(PS_SOLID, 1, txtC);
        HPEN   dotPnO = (HPEN)SelectObject(dis->hDC, dotPn);
        int cx = (rc.left + rc.right) / 2;
        int cy = (rc.top  + rc.bottom) / 2;
        int r  = 2;
        int dx = (rc.right - rc.left) / 4;
        int dy = (rc.bottom - rc.top) / 4;
        auto pip = [&](int x, int y) {
            Ellipse(dis->hDC, x - r, y - r, x + r + 1, y + r + 1);
        };
        pip(cx - dx, cy - dy);
        pip(cx + dx, cy - dy);
        pip(cx,      cy);
        pip(cx - dx, cy + dy);
        pip(cx + dx, cy + dy);
        SelectObject(dis->hDC, dotOld);
        SelectObject(dis->hDC, dotPnO);
        DeleteObject(dotBr);
        DeleteObject(dotPn);
    }

    // Owner-draw a curved-arrow undo/redo button. `undo` selects ↶, else ↷.
    void drawArrowButton(DRAWITEMSTRUCT* dis, bool undo) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF fill = pressed ? kColControlHi : kColControl;
        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, kColTextDim);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 5, 5);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br); DeleteObject(pn);

        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, kColWhite);
        HFONT of = (HFONT)SelectObject(dis->hDC, fontBold);
        const wchar_t* glyph = undo ? L"\u21B6" : L"\u21B7";
        DrawTextW(dis->hDC, glyph, 1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, of);
    }

    // Owner-draw a small padlock toggle. `locked` = closed shackle, accent
    // fill; unlocked = open shackle, neutral fill.
    void drawLockButton(DRAWITEMSTRUCT* dis, bool locked) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF fill = locked
            ? (pressed ? kColAccentDark : kColAccent)
            : (pressed ? kColControlHi  : kColControl);
        COLORREF edge = locked ? kColAccent : kColTextDim;
        COLORREF glyph = locked ? kColWhite : kColTextDim;

        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, edge);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 5, 5);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br);
        DeleteObject(pn);

        // Draw padlock glyph centered.
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int cx = rc.left + w / 2;
        int cy = rc.top  + h / 2;
        int bodyW = std::max(8, w / 2);
        int bodyH = std::max(6, h / 3);
        int bodyT = cy + 1;                          // body top
        int bodyL = cx - bodyW / 2;
        int bodyR = bodyL + bodyW;
        int bodyB = bodyT + bodyH;
        int archR = bodyW / 2 - 1;                   // shackle radius
        int archCx = cx;
        int archCy = bodyT;

        HPEN gPen   = CreatePen(PS_SOLID, 2, glyph);
        HPEN gPenO  = (HPEN)SelectObject(dis->hDC, gPen);
        HBRUSH gBr  = CreateSolidBrush(glyph);
        HBRUSH gBrO = (HBRUSH)SelectObject(dis->hDC, gBr);

        // Body (filled rounded rect).
        RoundRect(dis->hDC, bodyL, bodyT, bodyR, bodyB, 3, 3);

        // Shackle (arc above the body, half circle).
        SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
        if (locked) {
            // Closed shackle: half circle anchored on body top.
            Arc(dis->hDC,
                archCx - archR, archCy - archR,
                archCx + archR, archCy + archR,
                archCx - archR, archCy,
                archCx + archR, archCy);
        } else {
            // Open shackle: rotated arc â€” left side detached, right side
            // attached. Draw an arc from ~190Â° to ~10Â° (open at left).
            Arc(dis->hDC,
                archCx - archR + 2, archCy - archR,
                archCx + archR + 2, archCy + archR,
                archCx + archR + 2, archCy,
                archCx - archR + 2, archCy);
        }

        SelectObject(dis->hDC, gBrO);
        SelectObject(dis->hDC, gPenO);
        DeleteObject(gPen);
        DeleteObject(gBr);
    }

    // Owner-draw a combobox item (Omnisphere-style dark dropdown).
    void drawComboItem(DRAWITEMSTRUCT* dis) {
        if ((int)dis->itemID < 0) {
            // Empty / no selection: just clear.
            FillRect(dis->hDC, &dis->rcItem, brBg);
            return;
        }
        bool selected = (dis->itemState & ODS_SELECTED) != 0;
        bool isEdit   = (dis->itemState & ODS_COMBOBOXEDIT) != 0;
        COLORREF bg = isEdit  ? kColControl
                    : selected ? kColAccentDark
                               : kColPanel;
        HBRUSH brI = CreateSolidBrush(bg);
        FillRect(dis->hDC, &dis->rcItem, brI);
        DeleteObject(brI);

        // Subtle separator between items
        if (!isEdit && !selected) {
            HPEN pn = CreatePen(PS_SOLID, 1, kColBorder);
            HPEN po = (HPEN)SelectObject(dis->hDC, pn);
            MoveToEx(dis->hDC, dis->rcItem.left + 4, dis->rcItem.bottom - 1, nullptr);
            LineTo(dis->hDC, dis->rcItem.right - 4, dis->rcItem.bottom - 1);
            SelectObject(dis->hDC, po);
            DeleteObject(pn);
        }

        // -- Compatibility color dot (Mode ↔ Prog) ----------------------------
        // Identify which combo is being drawn and look up the current selection
        // of the other combo to derive the compat rating.
        int compatVal = 0; // 0=neutral 1=green 2=red
        bool isModeCmb = (dis->CtlID == (UINT)(kIdBase + mm::kParamMode));
        bool isProgCmb = (dis->CtlID == (UINT)kIdProg);
        if (isModeCmb && hProgCombo) {
            int curProg = (int)SendMessageW(hProgCombo, CB_GETCURSEL, 0, 0);
            if (curProg != CB_ERR)
                compatVal = mm::mode_prog_compat((int)dis->itemID, curProg);
        } else if (isProgCmb && hCtrl[mm::kParamMode]) {
            int curMode = (int)SendMessageW(hCtrl[mm::kParamMode], CB_GETCURSEL, 0, 0);
            if (curMode != CB_ERR)
                compatVal = mm::mode_prog_compat(curMode, (int)dis->itemID);
        }

        // Draw a small filled circle on the right edge of the item.
        if (compatVal != 0) {
            COLORREF dotCol = (compatVal == 1)
                ? RGB(72, 205, 72)   // green  = recommended
                : RGB(220, 65, 65);  // red    = discouraged
            int itemH   = dis->rcItem.bottom - dis->rcItem.top;
            int dotR    = 4;
            int dotX    = dis->rcItem.right - 12;
            int dotY    = dis->rcItem.top  + itemH / 2;
            HBRUSH brDot = CreateSolidBrush(dotCol);
            HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, brDot);
            HPEN   pnOld = (HPEN)SelectObject(dis->hDC,
                               CreatePen(PS_SOLID, 1, dotCol));
            Ellipse(dis->hDC,
                    dotX - dotR, dotY - dotR,
                    dotX + dotR, dotY + dotR);
            DeleteObject(SelectObject(dis->hDC, pnOld));
            SelectObject(dis->hDC, brOld);
            DeleteObject(brDot);
        }
        // ---------------------------------------------------------------------

        wchar_t txt[256] = {};
        SendMessageW(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)txt);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, selected ? kColWhite : kColText);
        HFONT old = (HFONT)SelectObject(dis->hDC, fontReg);
        RECT rcText = dis->rcItem;
        rcText.left += 8;
        rcText.right -= (compatVal != 0) ? 20 : 4; // leave room for dot
        DrawTextW(dis->hDC, txt, -1, &rcText,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dis->hDC, old);
    }

    void createControls() {
        HINSTANCE hi = g_hInst;

        // ---------- Param rows (Key/Mode/Octave/Subdiv/Density/Seed/NoteLen)
        for (int i = 0; i < (int)mm::kParamCount; ++i) {
            wchar_t wname[64];
            MultiByteToWideChar(CP_UTF8, 0, mm::kParamDefs[i].name, -1, wname, 64);
            HWND lbl = CreateWindowW(L"STATIC", wname,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
            SendMessageW(lbl, WM_SETFONT, (WPARAM)fontReg, TRUE);
            hLabel[i] = lbl;

            int ctrlId = kIdBase + i;
            HWND ctrl = nullptr;
            if (mm::kParamDefs[i].is_stepped) {
                ctrl = CreateWindowW(L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
                    CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
                    0, 0, kCtrlW, 220, hWnd,
                    (HMENU)(LONG_PTR)ctrlId, hi, nullptr);
                fillCombo(i, ctrl);
            } else {
                // Custom knob widget for Density / NoteLen
                ctrl = KnobWidget::create(hWnd, ctrlId, 0, 0, kKnobSize, kKnobSize);
            }
            if (ctrl && mm::kParamDefs[i].is_stepped)
                SendMessageW(ctrl, WM_SETFONT, (WPARAM)fontReg, TRUE);
            hCtrl[i] = ctrl;

            // Stepped params already display their value inside the
            // combobox â€“ don't add a duplicated value label on the right.
            if (mm::kParamDefs[i].is_stepped) {
                hValue[i] = nullptr;
            } else {
                HWND val = CreateWindowW(L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                    0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
                SendMessageW(val, WM_SETFONT, (WPARAM)fontBold, TRUE);
                hValue[i] = val;
            }
        }

        // ---------- Sound (waveform) row
        hWaveLabel = CreateWindowW(L"STATIC", L"Instrument",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hWaveLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hWaveCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
            CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, kCtrlW + kValueW, 320, hWnd,
            (HMENU)(LONG_PTR)kIdWave, hi, nullptr);
        SendMessageW(hWaveCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        // Items are populated per-style in populateInstrumentCombo().
        populateInstrumentCombo(plugin->getStyleType());

        // ---------- Progression row
        hProgLabel = CreateWindowW(L"STATIC", L"Progr.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hProgLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hProgCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
            CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, kCtrlW + kValueW, 260, hWnd,
            (HMENU)(LONG_PTR)kIdProg, hi, nullptr);
        SendMessageW(hProgCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        for (int i = 0; i < mm::kProgressionCount; ++i) {
            wchar_t wn[96];
            MultiByteToWideChar(CP_UTF8, 0, mm::kProgressionNames[i], -1, wn, 96);
            SendMessageW(hProgCombo, CB_ADDSTRING, 0, (LPARAM)wn);
        }
        SendMessageW(hProgCombo, CB_SETCURSEL, plugin->getProgType(), 0);

        // ---------- Notes display + Export button
        hNotesLabel = CreateWindowW(L"STATIC", L"Notes  \u25B8",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 60, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hNotesLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);

        hNoteDisplay = CreateWindowW(L"STATIC", L"\u2014",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kViewWidth - 2 * kPadX - 64, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hNoteDisplay, WM_SETFONT, (WPARAM)fontMono, TRUE);

        hExportBtn = CreateWindowW(L"BUTTON", L"EXPORT SESSION  \u2192  MIDI",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdExport, hi, nullptr);
        SendMessageW(hExportBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        hExportWavBtn = CreateWindowW(L"BUTTON", L"EXPORT AUDIO  \u2192  WAV",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdExportWav, hi, nullptr);
        SendMessageW(hExportWavBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        hSavePresetBtn = CreateWindowW(L"BUTTON", L"SAVE PRESET",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdSavePreset, hi, nullptr);
        SendMessageW(hSavePresetBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        hLoadPresetBtn = CreateWindowW(L"BUTTON", L"LOAD PRESET",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdLoadPreset, hi, nullptr);
        SendMessageW(hLoadPresetBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- LISTEN (Auto-Key) toggle in the header
        hAutoBtn = CreateWindowW(L"BUTTON",
            plugin->getAutoKey() ? L"LISTEN  \u25CF  ON" : L"LISTEN  \u25CB  OFF",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 140, 26, hWnd,
            (HMENU)(LONG_PTR)kIdAuto, hi, nullptr);
        SendMessageW(hAutoBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- Dice button: randomize Seed of the active style
        hDiceBtn = CreateWindowW(L"BUTTON", L"\U0001F3B2",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 28, 28, hWnd,
            (HMENU)(LONG_PTR)kIdDice, hi, nullptr);
        SendMessageW(hDiceBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- Piano: "MÃ©lodie main droite" toggle (BS_AUTOCHECKBOX)
        hPianoMelChk = CreateWindowW(L"BUTTON", L"M\u00e9lodie",
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            0, 0, 120, 22, hWnd,
            (HMENU)(LONG_PTR)kIdPianoMel, hi, nullptr);
        SendMessageW(hPianoMelChk, WM_SETFONT, (WPARAM)fontReg, TRUE);

        // ---------- Piano: "Accords" toggle
        hPianoChordChk = CreateWindowW(L"BUTTON", L"Accords",
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            0, 0, 120, 22, hWnd,
            (HMENU)(LONG_PTR)kIdPianoChord, hi, nullptr);
        SendMessageW(hPianoChordChk, WM_SETFONT, (WPARAM)fontReg, TRUE);

        // ---------- Percussion: rhythm pattern picker
        hPercRhyLabel = CreateWindowW(L"STATIC", L"Rythme",
            WS_CHILD | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hPercRhyLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hPercRhyCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
            CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, 200, 220, hWnd,
            (HMENU)(LONG_PTR)kIdPercRhy, hi, nullptr);
        SendMessageW(hPercRhyCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        for (int i = 0; i < mm::kDrumPatternCount; ++i) {
            wchar_t wn[64]; MultiByteToWideChar(CP_UTF8, 0,
                mm::kDrumPatterns[i].name, -1, wn, 64);
            SendMessageW(hPercRhyCombo, CB_ADDSTRING, 0, (LPARAM)wn);
        }
        SendMessageW(hPercRhyCombo, CB_SETCURSEL, 0, 0);

        // ---------- Volume row (per-style SoundFont channel volume)
        hVolLabel = CreateWindowW(L"STATIC", L"Volume",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hVolLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hVolSlider = CreateWindowW(TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,
            0, 0, kCtrlW, 24, hWnd,
            (HMENU)(LONG_PTR)kIdVol, hi, nullptr);
        SendMessageW(hVolSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
        SendMessageW(hVolSlider, TBM_SETPOS, TRUE,
            (LPARAM)(int)std::round(plugin->sfVolumePerStyle[plugin->getStyleType()] * 100.0f));
        hVolValue = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hVolValue, WM_SETFONT, (WPARAM)fontBold, TRUE);
        refreshVolValue();

        // ---------- Humanize knob (per-style random timing jitter) ----------
        hHumanizeLabel = CreateWindowW(L"STATIC", L"Humanize",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hHumanizeLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hHumanizeKnob = KnobWidget::create(hWnd, kIdHumanize, 0, 0, kKnobSize, kKnobSize);
        { auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA));
          if (kw) { kw->posDefault = 0;
            kw->setPos((int)std::round(plugin->humanizePerStyle[plugin->getStyleType()] * 1000.0f)); } }
        hHumanizeValue = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hHumanizeValue, WM_SETFONT, (WPARAM)fontBold, TRUE);
        refreshHumanizeValue();

        // ---------- Retard knob (per-style groove lag) ----------------------
        hRetardLabel = CreateWindowW(L"STATIC", L"Retard",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hRetardLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hRetardKnob = KnobWidget::create(hWnd, kIdRetard, 0, 0, kKnobSize, kKnobSize);
        { auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA));
          if (kw) { kw->posDefault = 0;
            kw->setPos((int)std::round(plugin->retardPerStyle[plugin->getStyleType()] * 1000.0f)); } }
        hRetardValue = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hRetardValue, WM_SETFONT, (WPARAM)fontBold, TRUE);
        refreshRetardValue();

        // ---------- Lock buttons (Mode / Progression / Subdiv) -----------
        // Owner-drawn padlock toggles broadcasting their value to all tabs
        // when locked. Created here, positioned in applyVisibilityForStyle.
        auto makeLock = [&](int id) {
            HWND b = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 24, 24, hWnd,
                (HMENU)(LONG_PTR)id, hi, nullptr);
            return b;
        };
        hLockMode   = makeLock(kIdLockMode);
        hLockProg   = makeLock(kIdLockProg);
        hLockSubdiv = makeLock(kIdLockSubdiv);

        // ---------- Global: Mesure (time signature numerator) combo
        hMeterCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            kViewWidth - kPadX - 88, 14, 88, 180, hWnd,
            (HMENU)(LONG_PTR)kIdMeter, hi, nullptr);
        SendMessageW(hMeterCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        static const wchar_t* meterLabels[] = {
            L"Auto", L"2/4", L"3/4", L"4/4", L"5/4", L"6/4", L"7/4"
        };
        for (auto* lbl : meterLabels)
            SendMessageW(hMeterCombo, CB_ADDSTRING, 0, (LPARAM)lbl);
        // Restore saved override: 0=Auto, 2=2/4 → index 1, 3→2 … 7→6
        int meterIdx = 0;
        if (plugin->beatsPerBarOverride >= 2 && plugin->beatsPerBarOverride <= 7)
            meterIdx = plugin->beatsPerBarOverride - 1; // Auto=0, 2/4=1...
        SendMessageW(hMeterCombo, CB_SETCURSEL, meterIdx, 0);

        // ---- Header: global section combo ----
        hSectionCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            508, 14, 130, 200, hWnd, (HMENU)(LONG_PTR)kIdSection, hi, nullptr);
        SendMessageW(hSectionCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        { const wchar_t* s0[] = { L"Intro", L"Verse", L"Chorus", L"Bridge", L"Outro" };
          for (auto* sl : s0) SendMessageW(hSectionCombo, CB_ADDSTRING, 0, (LPARAM)sl); }
        SendMessageW(hSectionCombo, CB_SETCURSEL,
            std::clamp(plugin->currentSection.load(std::memory_order_relaxed), 0, 4), 0);

        // ---- Per-style: start-bar delay ----
        hStartBarLabel = CreateWindowW(L"STATIC", L"Debut",
            WS_CHILD | SS_LEFT, 0, 0, 1, 1, hWnd, nullptr, hi, nullptr);
        SendMessageW(hStartBarLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hStartBarCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
            0, 0, 1, 200, hWnd, (HMENU)(LONG_PTR)kIdStartBar, hi, nullptr);
        SendMessageW(hStartBarCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        { const wchar_t* s1[] = { L"0 bar", L"+1 bar", L"+2 bars", L"+4 bars", L"+8 bars" };
          for (auto* sl : s1) SendMessageW(hStartBarCombo, CB_ADDSTRING, 0, (LPARAM)sl); }
        { static constexpr int kSBV[] = { 0,1,2,4,8 };
          int sv = plugin->startBarPerStyle[plugin->getStyleType()];
          int si = 0; for (int k=0;k<5;k++) if (kSBV[k]==sv) si=k;
          SendMessageW(hStartBarCombo, CB_SETCURSEL, si, 0); }

        // ---------- Miniature horizontal piano roll -------------------------
        ensurePRClass();
        hPianoRoll = CreateWindowExW(0, kPRClass, L"",
            WS_CHILD | WS_VISIBLE,
            kPadX, 0, kViewWidth - 2 * kPadX, kPRHeight,
            hWnd, nullptr, g_hInst, nullptr);
        if (hPianoRoll)
            SetWindowLongPtrW(hPianoRoll, GWLP_USERDATA, (LONG_PTR)this);

        // ---------- Undo / Redo buttons (visible in editor mode only) ----
        hUndoBtn = CreateWindowW(L"BUTTON", L"\u21B6",
            WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 28, 24, hWnd,
            (HMENU)(LONG_PTR)kIdUndo, hi, nullptr);
        SendMessageW(hUndoBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);
        hRedoBtn = CreateWindowW(L"BUTTON", L"\u21B7",
            WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 28, 24, hWnd,
            (HMENU)(LONG_PTR)kIdRedo, hi, nullptr);
        SendMessageW(hRedoBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        applyVisibilityForStyle(plugin->getStyleType());
    }

    // -------------------------------------------------------------------------
    // Lay out the visible widgets into 3 sections + tabs at top + bottom export
    //   Tabs (header)  : 5 instruments selector
    //   1. SOUND       : Son
    //   2. RYTHME      : Subdiv, Density (knob), NoteLen (knob)
    //   3. HARMONIE    : Mode, Progr., Seed
    //   Bottom         : Notes display + Export button
    //
    // Key is always hidden (trigger note pitch fixes the tonal centre).
    // Octave is shown as an offset control (+0, +1, +2, -1).
    // -------------------------------------------------------------------------
    void applyVisibilityForStyle(int style) {
        bool isPerc  = (style == 3);
        bool isPiano = (style == 5);
        bool showMode    = !isPerc;
        bool showNoteLen = !isPerc;
        bool showProg    = !isPerc;
        bool showDensity = !isPerc; // perc rhythm has fixed density (pattern)
        bool showOctave  = !isPerc;

        auto hideRow = [&](HWND lbl, HWND ctl, HWND val) {
            if (lbl) ShowWindow(lbl, SW_HIDE);
            if (ctl) ShowWindow(ctl, SW_HIDE);
            if (val) ShowWindow(val, SW_HIDE);
        };
        for (int i = 0; i < (int)mm::kParamCount; ++i)
            hideRow(hLabel[i], hCtrl[i], hValue[i]);
        hideRow(hWaveLabel, hWaveCombo, nullptr);
        hideRow(hProgLabel, hProgCombo, nullptr);
        hideRow(hVolLabel, hVolSlider, hVolValue);
        hideRow(hHumanizeLabel, hHumanizeKnob, hHumanizeValue);
        hideRow(hRetardLabel, hRetardKnob, hRetardValue);
        if (hPianoMelChk)  ShowWindow(hPianoMelChk,  SW_HIDE);
        if (hPianoChordChk) ShowWindow(hPianoChordChk, SW_HIDE);
        if (hPercRhyLabel) ShowWindow(hPercRhyLabel, SW_HIDE);
        if (hPercRhyCombo) ShowWindow(hPercRhyCombo, SW_HIDE);
        if (hLockMode)     ShowWindow(hLockMode,     SW_HIDE);
        if (hLockProg)     ShowWindow(hLockProg,     SW_HIDE);
        if (hLockSubdiv)   ShowWindow(hLockSubdiv,   SW_HIDE);
        if (hStartBarLabel) ShowWindow(hStartBarLabel, SW_HIDE);
        if (hStartBarCombo) ShowWindow(hStartBarCombo, SW_HIDE);

        // ============ EDITOR MODE ============================================
        // Hide all parameter UI, expand the piano roll across the full content
        // area, show undo/redo buttons. Tabs and titles remain visible.
        if (viewMode == 1) {
            if (hAutoBtn)        ShowWindow(hAutoBtn,        SW_HIDE);
            if (hMeterCombo)     ShowWindow(hMeterCombo,     SW_HIDE);
            if (hSectionCombo)   ShowWindow(hSectionCombo,   SW_HIDE);
            if (hExportBtn)      ShowWindow(hExportBtn,      SW_HIDE);
            if (hExportWavBtn)   ShowWindow(hExportWavBtn,   SW_HIDE);
            if (hSavePresetBtn)  ShowWindow(hSavePresetBtn,  SW_HIDE);
            if (hLoadPresetBtn)  ShowWindow(hLoadPresetBtn,  SW_HIDE);
            for (int i = 0; i < 4; ++i) { sectionY[i] = 0; sectionX[i] = 0; sectionW[i] = 0; }

            const int yTopE = kHeaderH + kTabsH + kSectionGap;
            int prX = kPadX;
            int prY = yTopE;
            int prWi = kViewWidth - 2 * kPadX;
            int prHe = kViewHeight - yTopE - kSectionGap;
            if (hPianoRoll) {
                MoveWindow(hPianoRoll, prX, prY, prWi, prHe, TRUE);
                ShowWindow(hPianoRoll, SW_SHOW);
            }
            if (hUndoBtn) {
                MoveWindow(hUndoBtn, prX + prWi - 64, prY - 30, 28, 24, TRUE);
                ShowWindow(hUndoBtn, SW_SHOW);
            }
            if (hRedoBtn) {
                MoveWindow(hRedoBtn, prX + prWi - 32, prY - 30, 28, 24, TRUE);
                ShowWindow(hRedoBtn, SW_SHOW);
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            return;
        }
        // Params mode: hide editor-only widgets
        if (hUndoBtn) ShowWindow(hUndoBtn, SW_HIDE);
        if (hRedoBtn) ShowWindow(hRedoBtn, SW_HIDE);

        // Place the LISTEN toggle in the header between Section and Mesure combos.
        // Layout (right to left): Mesure(88) gap(8) LISTEN(140) gap(8) Section(130)
        if (hAutoBtn) {
            int btnW = 140, btnH = 26;
            MoveWindow(hAutoBtn, kViewWidth - kPadX - 88 - 8 - btnW,
                       (kHeaderH - btnH) / 2, btnW, btnH, TRUE);
            ShowWindow(hAutoBtn, SW_SHOW);
        }

        // Y where the PARAMÈTRES header ends (drawn at kHeaderH+kTabsH+4, height 22)
        static constexpr int kParamHdrH = 22;
        static constexpr int kParamHdrY = kHeaderH + kTabsH + 4;
        static constexpr int kContentY  = kParamHdrY + kParamHdrH + 4;

        // ---- Collapsed params: show only the piano roll ----
        if (paramsCollapsed) {
            if (hExportBtn)    ShowWindow(hExportBtn,    SW_HIDE);
            if (hExportWavBtn) ShowWindow(hExportWavBtn, SW_HIDE);
            if (hSavePresetBtn) ShowWindow(hSavePresetBtn, SW_HIDE);
            if (hLoadPresetBtn) ShowWindow(hLoadPresetBtn, SW_HIDE);
            for (int i = 0; i < 4; ++i) { sectionY[i] = 0; sectionX[i] = 0; sectionW[i] = 0; }
            if (hPianoRoll) {
                int prHe = kViewHeight - kContentY - 4;
                MoveWindow(hPianoRoll, kPadX, kContentY, kViewWidth - 2 * kPadX, prHe, TRUE);
                ShowWindow(hPianoRoll, SW_SHOW);
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            return;
        }

        const int yTop = kContentY;

        // ============ LEFT COLUMN: SOUND + RYTHME ============
        int yL = yTop;

        // === SOUND ===
        sectionY[0] = yL; sectionX[0] = kPadX; sectionW[0] = kColumnW;
        yL += kSectionTitleH;
        placeRowCol(yL, hWaveLabel, hWaveCombo, nullptr, kPadX, kColumnW, /*spanCtrl=*/true);
        yL += kRowH;
        placeRowCol(yL, hVolLabel, hVolSlider, hVolValue, kPadX, kColumnW, false);
        yL += kRowH;
        { static constexpr int kSBV[] = { 0,1,2,4,8 };
          int sv2 = plugin->startBarPerStyle[style];
          int si2 = 0; for (int k=0;k<5;k++) if (kSBV[k]==sv2) si2=k;
          if (hStartBarCombo) SendMessageW(hStartBarCombo, CB_SETCURSEL, si2, 0); }
        placeRowCol(yL, hStartBarLabel, hStartBarCombo, nullptr, kPadX, kColumnW, /*spanCtrl=*/true);
        yL += kRowH;
        yL += kSectionGap;

        // === RYTHME ===
        sectionY[1] = yL; sectionX[1] = kPadX; sectionW[1] = kColumnW;
        yL += kSectionTitleH;
        placeRowCol(yL, hLabel[mm::kParamSubdiv], hCtrl[mm::kParamSubdiv], hValue[mm::kParamSubdiv], kPadX, kColumnW, false);
        // Subdiv lock: at the right edge of the Subdiv row.
        if (hLockSubdiv) {
            int lockX = kPadX + kColumnW - 26;
            MoveWindow(hLockSubdiv, lockX, yL + 4, 24, 24, TRUE);
            ShowWindow(hLockSubdiv, SW_SHOW);
            InvalidateRect(hLockSubdiv, nullptr, TRUE);
        }
        yL += kRowH;
        if (showOctave) {
            placeRowCol(yL, hLabel[mm::kParamOctave], hCtrl[mm::kParamOctave], hValue[mm::kParamOctave], kPadX, kColumnW, false);
            yL += kRowH;
        }

        // ============ RIGHT COLUMN: HARMONIE ============
        int yR = yTop;
        sectionY[2] = yR; sectionX[2] = kRightColX; sectionW[2] = kColumnW;
        yR += kSectionTitleH;
        if (showMode) {
            placeRowCol(yR, hLabel[mm::kParamMode], hCtrl[mm::kParamMode], hValue[mm::kParamMode], kRightColX, kColumnW, false);
            if (hLockMode) {
                int lockX = kRightColX + kColumnW - 26;
                MoveWindow(hLockMode, lockX, yR + 4, 24, 24, TRUE);
                ShowWindow(hLockMode, SW_SHOW);
                InvalidateRect(hLockMode, nullptr, TRUE);
            }
            yR += kRowH;
        }
        if (showProg) {
            placeRowCol(yR, hProgLabel, hProgCombo, nullptr, kRightColX, kColumnW, /*spanCtrl=*/true);
            // Shrink the prog combo to leave space for the padlock.
            if (hProgCombo && hLockProg) {
                RECT rc; GetWindowRect(hProgCombo, &rc);
                POINT pt = { rc.left, rc.top };
                ScreenToClient(hWnd, &pt);
                int newW = (rc.right - rc.left) - 30;
                MoveWindow(hProgCombo, pt.x, pt.y, newW, rc.bottom - rc.top, TRUE);
                int lockX = kRightColX + kColumnW - 26;
                MoveWindow(hLockProg, lockX, yR + 4, 24, 24, TRUE);
                ShowWindow(hLockProg, SW_SHOW);
                InvalidateRect(hLockProg, nullptr, TRUE);
            }
            yR += kRowH;
        }
        // Percussion-only: rhythm-pattern picker replaces Mode/Prog rows.
        if (isPerc && hPercRhyCombo) {
            placeRowCol(yR, hPercRhyLabel, hPercRhyCombo, nullptr, kRightColX, kColumnW, /*spanCtrl=*/true);
            // Sync combo selection with this style's stored pattern.
            int idx = std::clamp(plugin->percRhythmPerStyle[style],
                                 0, mm::kDrumPatternCount - 1);
            SendMessageW(hPercRhyCombo, CB_SETCURSEL, idx, 0);
            yR += kRowH;
        }
        placeRowCol(yR, hLabel[mm::kParamSeed], hCtrl[mm::kParamSeed], hValue[mm::kParamSeed], kRightColX, kColumnW, false);
        // Dice button right next to the Seed control.
        if (hDiceBtn && hCtrl[mm::kParamSeed]) {
            RECT rv; GetWindowRect(hCtrl[mm::kParamSeed], &rv);
            POINT pt = { rv.right, rv.top };
            ScreenToClient(hWnd, &pt);
            MoveWindow(hDiceBtn, pt.x + 8, pt.y, 28, 28, TRUE);
            ShowWindow(hDiceBtn, SW_SHOW);
        }
        yR += kRowH;
        // Piano-only: "Accords" + "Melodie" toggles below the Seed row.
        if (isPiano) {
            if (hPianoChordChk) {
                MoveWindow(hPianoChordChk, kRightColX + kLabelW + 8, yR + 4, 100, 22, TRUE);
                SendMessageW(hPianoChordChk, BM_SETCHECK,
                    plugin->pianoChordPerStyle[style] ? BST_CHECKED : BST_UNCHECKED, 0);
                ShowWindow(hPianoChordChk, SW_SHOW);
            }
            if (hPianoMelChk) {
                MoveWindow(hPianoMelChk, kRightColX + kLabelW + 8 + 108, yR + 4, 100, 22, TRUE);
                SendMessageW(hPianoMelChk, BM_SETCHECK,
                    plugin->pianoMelodyPerStyle[style] ? BST_CHECKED : BST_UNCHECKED, 0);
                ShowWindow(hPianoMelChk, SW_SHOW);
            }
            yR += kRowH;
        }
        // ============ EXPRESSION section: 2x2 knob grid in right column ============
        yR += kSectionGap;
        sectionY[3] = yR; sectionX[3] = kRightColX; sectionW[3] = kColumnW;
        yR += kSectionTitleH;
        {
            // Each half-cell is kColumnW/2 wide (210px):
            //   label(62) + gap(6) + knob(56) + gap(6) + value(80) = 210
            const int kHalfW   = kColumnW / 2;
            const int kSmLbl   = 62;
            const int kGap     = 6;
            const int kValW    = kHalfW - kSmLbl - kGap - kKnobSize - kGap; // 80px

            auto placeKnobHalf = [&](int y, int cellX, HWND lbl, HWND knob, HWND val) {
                if (lbl)  { MoveWindow(lbl,  cellX,                       y + (kKnobSize/2) - 8, kSmLbl, 20, TRUE); ShowWindow(lbl,  SW_SHOW); }
                if (knob) { MoveWindow(knob, cellX + kSmLbl + kGap,       y, kKnobSize, kKnobSize,            TRUE); ShowWindow(knob, SW_SHOW); }
                if (val)  { MoveWindow(val,  cellX + kSmLbl + kGap + kKnobSize + kGap, y + (kKnobSize/2) - 8, kValW, 20, TRUE); ShowWindow(val, SW_SHOW); }
            };

            // Sync knob positions from stored per-style values
            if (hHumanizeKnob)
                if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA)))
                    kw->setPos((int)std::round(plugin->humanizePerStyle[style] * 1000.0f));
            if (hRetardKnob)
                if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA)))
                    kw->setPos((int)std::round(plugin->retardPerStyle[style] * 1000.0f));
            refreshHumanizeValue();
            refreshRetardValue();

            // Row 1: Density | NoteLen  (hidden for Perc which has no note lengths)
            if (showDensity || showNoteLen) {
                if (showDensity)
                    placeKnobHalf(yR, kRightColX,           hLabel[mm::kParamDensity], hCtrl[mm::kParamDensity], hValue[mm::kParamDensity]);
                else
                    hideRow(hLabel[mm::kParamDensity], hCtrl[mm::kParamDensity], hValue[mm::kParamDensity]);
                if (showNoteLen)
                    placeKnobHalf(yR, kRightColX + kHalfW,  hLabel[mm::kParamNoteLen],  hCtrl[mm::kParamNoteLen],  hValue[mm::kParamNoteLen]);
                else
                    hideRow(hLabel[mm::kParamNoteLen], hCtrl[mm::kParamNoteLen], hValue[mm::kParamNoteLen]);
                yR += kKnobSize + 8;
            }

            // Row 2: Humanize | Retard  (always shown)
            placeKnobHalf(yR, kRightColX,           hHumanizeLabel, hHumanizeKnob, hHumanizeValue);
            placeKnobHalf(yR, kRightColX + kHalfW,  hRetardLabel,   hRetardKnob,   hRetardValue);
            yR += kKnobSize + 8;
        }
        // ============ Bottom: piano roll + export (full width) ============
        int yBot = std::max(yL, yR) + kSectionGap;
        // Piano roll: full-width, 80px tall
        if (hPianoRoll) {
            MoveWindow(hPianoRoll, kPadX, yBot, kViewWidth - 2 * kPadX, kPRHeight, TRUE);
            ShowWindow(hPianoRoll, SW_SHOW);
        }
        // Keep legacy text row hidden (still used by refreshNoteDisplay for non-visual update)
        if (hNotesLabel)  ShowWindow(hNotesLabel,  SW_HIDE);
        if (hNoteDisplay) ShowWindow(hNoteDisplay, SW_HIDE);
        yBot += kPRHeight + 6;
        if (hExportBtn) {
            int half = (kViewWidth - 2 * kPadX - 8) / 2;
            MoveWindow(hExportBtn, kPadX, yBot, half, kButtonH, TRUE);
            ShowWindow(hExportBtn, SW_SHOW);
        }
        if (hExportWavBtn) {
            int half = (kViewWidth - 2 * kPadX - 8) / 2;
            MoveWindow(hExportWavBtn, kPadX + half + 8, yBot,
                       kViewWidth - 2 * kPadX - half - 8, kButtonH, TRUE);
            ShowWindow(hExportWavBtn, SW_SHOW);
        }
        // Row 2: SAVE / LOAD preset (smaller buttons).
        {
            int yRow2 = yBot + kButtonH + 6;
            int hSmall = std::max(22, kButtonH - 8);
            int half = (kViewWidth - 2 * kPadX - 8) / 2;
            if (hSavePresetBtn) {
                MoveWindow(hSavePresetBtn, kPadX, yRow2, half, hSmall, TRUE);
                ShowWindow(hSavePresetBtn, SW_SHOW);
            }
            if (hLoadPresetBtn) {
                MoveWindow(hLoadPresetBtn, kPadX + half + 8, yRow2,
                           kViewWidth - 2 * kPadX - half - 8, hSmall, TRUE);
                ShowWindow(hLoadPresetBtn, SW_SHOW);
            }
        }

        InvalidateRect(hWnd, nullptr, TRUE);
    }

    // Place a label + control (+optional value) inside the given column box.
    // If spanCtrl is true, the control fills (colW - labelW - 8) (no value column).
    void placeRowCol(int y, HWND lbl, HWND ctl, HWND val, int colX, int colW, bool spanCtrl) {
        if (lbl) {
            MoveWindow(lbl, colX, y + 6, kLabelW, 20, TRUE);
            ShowWindow(lbl, SW_SHOW);
        }
        int cx = colX + kLabelW + 8;
        int cw = spanCtrl ? (colW - kLabelW - 8) : kCtrlW;
        if (ctl) {
            MoveWindow(ctl, cx, y + 4, cw, 24, TRUE);
            ShowWindow(ctl, SW_SHOW);
        }
        if (val && !spanCtrl) {
            int vx = cx + cw + 8;
            MoveWindow(val, vx, y + 6, kValueW, 20, TRUE);
            ShowWindow(val, SW_SHOW);
        }
    }

    // Knob variant â€” knob is square (kKnobSize), label on left, value on right.
    void placeKnobRowCol(int y, HWND lbl, HWND knob, HWND val, int colX) {
        if (lbl) {
            MoveWindow(lbl, colX, y + (kKnobSize / 2) - 8, kLabelW, 20, TRUE);
            ShowWindow(lbl, SW_SHOW);
        }
        int cx = colX + kLabelW + 8;
        if (knob) {
            MoveWindow(knob, cx, y, kKnobSize, kKnobSize, TRUE);
            ShowWindow(knob, SW_SHOW);
        }
        if (val) {
            int vx = cx + kKnobSize + 12;
            MoveWindow(val, vx, y + (kKnobSize / 2) - 8, kValueW, 20, TRUE);
            ShowWindow(val, SW_SHOW);
        }
    }

    // Old single-column wrappers kept for compatibility (not used in 2-col layout).
    void placeRow(int y, HWND lbl, HWND ctl, HWND val, bool show) {
        if (!show) return;
        placeRowCol(y, lbl, ctl, val, kPadX, kViewWidth - 2 * kPadX, false);
    }
    void placeKnobRow(int y, HWND lbl, HWND knob, HWND val) {
        placeKnobRowCol(y, lbl, knob, val, kPadX);
    }

    void fillCombo(int paramId, HWND combo) {
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        switch (paramId) {
            case mm::kParamKey:
                for (int k = 0; k < 12; ++k) {
                    wchar_t wn[8]; MultiByteToWideChar(CP_UTF8, 0, mm::kKeyNames[k], -1, wn, 8);
                    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wn);
                }
                break;
            case mm::kParamMode:
                for (int m = 0; m < mm::kScaleCount; ++m) {
                    wchar_t wn[32]; MultiByteToWideChar(CP_UTF8, 0, mm::kModeNames[m], -1, wn, 32);
                    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wn);
                }
                break;
            case mm::kParamOctave: {
                // Values 3-6 stored, displayed as offset relative to trigger note
                // (default=4 → +0, 5 → +1, 6 → +2, 3 → -1)
                static const wchar_t* octLabels[] = { L"-1 oct", L"+0 (normal)", L"+1 oct", L"+2 oct" };
                for (auto* lbl : octLabels) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)lbl);
                break;
            }
            case mm::kParamSubdiv: {
                static const wchar_t* labels[] = { L"1/1 (whole)", L"1/2", L"1/4", L"1/8", L"1/16", L"1/32" };
                for (auto* s : labels) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)s);
                break;
            }
            case mm::kParamSeed:
                for (int s = 0; s <= 999; ++s) {
                    wchar_t wn[8]; swprintf_s(wn, L"%d", s);
                    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wn);
                }
                break;
            default: break;
        }
    }

    void refreshAll() {
        for (int i = 0; i < (int)mm::kParamCount; ++i) refreshOne(i);
    }

    // Rebuild the Instrument combo for the active style and select its
    // currently stored preset index.
    void populateInstrumentCombo(int style) {
        if (!hWaveCombo) return;
        style = std::clamp(style, 0, 5);
        SendMessageW(hWaveCombo, CB_RESETCONTENT, 0, 0);
        const int n = kSfPresetCount[style];
        for (int i = 0; i < n; ++i) {
            const SfPreset& p = kSfPresetsByStyle[style][i];
            if (!p.name || !*p.name) continue;
            SendMessageW(hWaveCombo, CB_ADDSTRING, 0, (LPARAM)p.name);
        }
        int idx = std::clamp(plugin->sfPresetIdxPerStyle[style], 0, n - 1);
        SendMessageW(hWaveCombo, CB_SETCURSEL, idx, 0);
    }

    void refreshVolValue() {
        if (!hVolValue) return;
        float v = plugin->sfVolumePerStyle[plugin->getStyleType()];
        wchar_t buf[32];
        swprintf_s(buf, L"%d %%", (int)std::round(v * 100.0f));
        SetWindowTextW(hVolValue, buf);
    }

    void refreshHumanizeValue() {
        if (!hHumanizeValue) return;
        float v = plugin->humanizePerStyle[plugin->getStyleType()];
        wchar_t buf[32];
        swprintf_s(buf, L"%d %%", (int)std::round(v * 100.0f));
        SetWindowTextW(hHumanizeValue, buf);
    }

    void refreshRetardValue() {
        if (!hRetardValue) return;
        float v = plugin->retardPerStyle[plugin->getStyleType()];
        wchar_t buf[32];
        swprintf_s(buf, L"%d %%", (int)std::round(v * 100.0f));
        SetWindowTextW(hRetardValue, buf);
    }

    void refreshOne(int i) {
        double v = plugin->getActualParamValue(i);
        if (mm::kParamDefs[i].is_stepped) {
            int idx = comboIndexFromValue(i, v);
            SendMessageW(hCtrl[i], CB_SETCURSEL, idx, 0);
        } else {
            const auto& d = mm::kParamDefs[i];
            double norm = (v - d.min_value) / std::max(1e-9, d.max_value - d.min_value);
            int pos = (int)std::round(norm * 1000.0);
            wchar_t cls[64] = {};
            GetClassNameW(hCtrl[i], cls, 64);
            if (wcscmp(cls, KnobWidget::kClassName) == 0) {
                auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hCtrl[i], GWLP_USERDATA));
                if (kw) kw->setPos(pos);
            } else {
                SendMessageW(hCtrl[i], TBM_SETPOS, TRUE, pos);
            }
        }
        updateValueLabel(i, v);
    }

    void updateValueLabel(int i, double v) {
        if (!hValue[i]) return;
        wchar_t buf[64];
        switch (i) {
            case mm::kParamKey: {
                int k = std::clamp((int)std::round(v), 0, 11);
                wchar_t wn[8]; MultiByteToWideChar(CP_UTF8, 0, mm::kKeyNames[k], -1, wn, 8);
                swprintf_s(buf, L"%s", wn); break;
            }
            case mm::kParamMode: {
                int m = std::clamp((int)std::round(v), 0, mm::kScaleCount - 1);
                wchar_t wn[32]; MultiByteToWideChar(CP_UTF8, 0, mm::kModeNames[m], -1, wn, 32);
                swprintf_s(buf, L"%s", wn); break;
            }
            case mm::kParamOctave: swprintf_s(buf, L"%d", (int)std::round(v)); break;
            case mm::kParamSubdiv: swprintf_s(buf, L"1/%d", mm::normalized_subdiv(v)); break;
            case mm::kParamDensity: swprintf_s(buf, L"%.0f%%", v * 100.0); break;
            case mm::kParamNoteLen: swprintf_s(buf, L"%.0f%%", v * 100.0); break;
            case mm::kParamSeed:    swprintf_s(buf, L"%d", (int)std::round(v)); break;
            default:                swprintf_s(buf, L"%.3f", v); break;
        }
        SetWindowTextW(hValue[i], buf);
    }

    int comboIndexFromValue(int paramId, double v) {
        switch (paramId) {
            case mm::kParamKey:    return std::clamp((int)std::round(v), 0, 11);
            case mm::kParamMode:   return std::clamp((int)std::round(v), 0, mm::kScaleCount - 1);
            case mm::kParamOctave: return std::clamp((int)std::round(v) - 3, 0, 3);
            case mm::kParamSubdiv: {
                int s = mm::normalized_subdiv(v);
                if (s == 1)  return 0;
                if (s == 2)  return 1;
                if (s == 4)  return 2;
                if (s == 8)  return 3;
                if (s == 16) return 4;
                return 5; // 32
            }
            case mm::kParamSeed:   return std::clamp((int)std::round(v), 0, 999);
            default: return 0;
        }
    }

    double valueFromComboIndex(int paramId, int idx) {
        switch (paramId) {
            case mm::kParamKey:    return (double)idx;
            case mm::kParamMode:   return (double)idx;
            case mm::kParamOctave: return 3.0 + idx;
            case mm::kParamSubdiv: { static const int s[] = {1,2,4,8,16,32}; return s[std::clamp(idx,0,5)]; }
            case mm::kParamSeed:   return (double)idx;
            default: return 0.0;
        }
    }

    static void pitchToWstr(int16_t p, wchar_t* out, int len) {
        static const wchar_t* names[] =
            {L"C",L"C#",L"D",L"D#",L"E",L"F",L"F#",L"G",L"G#",L"A",L"A#",L"B"};
        if (p < 0 || p > 127) { swprintf_s(out, len, L"?"); return; }
        swprintf_s(out, len, L"%s%d", names[p % 12], (p / 12) - 1);
    }

    void refreshNoteDisplay() {
        if (!hNoteDisplay || !plugin) return;
        uint32_t head = plugin->noteHistHead.load(std::memory_order_relaxed);
        if (head == 0) { SetWindowTextW(hNoteDisplay, L"\u2014"); return; }

        constexpr int N = MelodyMakerVST3::kNoteHistSize;
        int count = (int)std::min(head, (uint32_t)N);
        wchar_t buf[128]{};
        int pos = 0;
        // oldest to newest, left to right
        for (int i = count - 1; i >= 0 && pos < 120; --i) {
            uint32_t idx = (head - 1 - (uint32_t)i + (uint32_t)N * 4) % (uint32_t)N;
            int16_t pitch = plugin->noteHist[idx];
            wchar_t nb[8];
            pitchToWstr(pitch, nb, 8);
            if (pos > 0) { buf[pos++] = L' '; buf[pos++] = 0xBB; buf[pos++] = L' '; }
            int nlen = (int)wcslen(nb);
            wcscpy_s(buf + pos, 128 - pos, nb);
            pos += nlen;
        }
        SetWindowTextW(hNoteDisplay, buf);
    }

    // -----------------------------------------------------------------------
    // MIDI export helpers
    // -----------------------------------------------------------------------
    static void midiVarLen(std::vector<uint8_t>& b, uint32_t v) {
        uint8_t buf[4]; int n = 0;
        do { buf[n++] = (uint8_t)(v & 0x7F); v >>= 7; } while (v);
        for (int i = n - 1; i >= 0; --i)
            b.push_back(i > 0 ? (buf[i] | 0x80) : buf[i]);
    }
    static void midiBE16(std::vector<uint8_t>& b, uint16_t v) {
        b.push_back((uint8_t)(v >> 8)); b.push_back((uint8_t)(v & 0xFF));
    }
    static void midiBE32(std::vector<uint8_t>& b, uint32_t v) {
        b.push_back((uint8_t)(v >> 24)); b.push_back((uint8_t)((v >> 16) & 0xFF));
        b.push_back((uint8_t)((v >> 8) & 0xFF)); b.push_back((uint8_t)(v & 0xFF));
    }

    // Build a standard MIDI file (format 1) from the given recorded notes.
    // Returns an empty vector if notes is empty.
    std::vector<uint8_t> buildMidiBytes(
            const std::vector<MelodyMakerVST3::RecordedNote>& notes) {
        static constexpr uint16_t TPQN = 480;
        if (notes.empty()) return {};

        struct BEv { uint32_t tick; bool isOn; uint8_t pitch; uint8_t vel; };
        constexpr int kStyles = MelodyMakerVST3::kStyleCount;
        std::vector<BEv> perStyle[kStyles];

        for (auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            int s = std::clamp((int)n.style, 0, kStyles - 1);
            uint32_t onTick  = (uint32_t)std::round(n.beatOn * TPQN);
            uint32_t offTick = (uint32_t)std::round((n.beatOn + n.beatLen) * TPQN);
            if (offTick <= onTick) offTick = onTick + 1;
            uint8_t vel = (uint8_t)std::clamp((int)std::round(n.vel * 127.f), 1, 127);
            uint8_t p   = (uint8_t)std::clamp((int)n.pitch, 0, 127);
            perStyle[s].push_back({onTick,  true,  p, vel});
            perStyle[s].push_back({offTick, false, p, 0});
        }

        float bpm = plugin->lastBpm.load(std::memory_order_relaxed);
        if (bpm < 1.f) bpm = 120.f;
        uint32_t usPerBeat = (uint32_t)std::round(60000000.0 / bpm);

        auto pushTC = [](std::vector<uint8_t>& midi,
                         const std::vector<uint8_t>& body) {
            for (char c : {'M','T','r','k'}) midi.push_back((uint8_t)c);
            midiBE32(midi, (uint32_t)body.size());
            midi.insert(midi.end(), body.begin(), body.end());
        };
        auto pushName = [](std::vector<uint8_t>& trk, const char* name) {
            trk.push_back(0x00); trk.push_back(0xFF); trk.push_back(0x03);
            size_t n = std::strlen(name);
            midiVarLen(trk, (uint32_t)n);
            for (size_t i = 0; i < n; ++i) trk.push_back((uint8_t)name[i]);
        };

        std::vector<uint8_t> tempoTrk;
        tempoTrk.push_back(0x00); tempoTrk.push_back(0xFF);
        tempoTrk.push_back(0x58); tempoTrk.push_back(0x04);
        tempoTrk.push_back(0x04); tempoTrk.push_back(0x02);
        tempoTrk.push_back(0x18); tempoTrk.push_back(0x08);
        tempoTrk.push_back(0x00); tempoTrk.push_back(0xFF);
        tempoTrk.push_back(0x51); tempoTrk.push_back(0x03);
        tempoTrk.push_back((usPerBeat >> 16) & 0xFF);
        tempoTrk.push_back((usPerBeat >>  8) & 0xFF);
        tempoTrk.push_back( usPerBeat        & 0xFF);
        pushName(tempoTrk, "Midnight Melody Maker");
        tempoTrk.push_back(0x00); tempoTrk.push_back(0xFF);
        tempoTrk.push_back(0x2F); tempoTrk.push_back(0x00);

        static const uint8_t kProg[kStyles] = { 0, 46, 33, 0, 12, 0 };
        static const char* kName[kStyles] = {
            "Melodie","Harpe","Basse","Percussions","Marimba","Piano"
        };
        std::vector<std::vector<uint8_t>> styleTracks;
        for (int s = 0; s < kStyles; ++s) {
            auto& evs = perStyle[s];
            if (evs.empty()) continue;
            std::sort(evs.begin(), evs.end(), [](const BEv& a, const BEv& b) {
                return a.tick != b.tick ? a.tick < b.tick : !a.isOn;
            });
            std::vector<uint8_t> trk;
            pushName(trk, kName[s]);
            uint8_t chan = (s == 3) ? 9 : (uint8_t)std::min(8, s);
            if (s != 3) {
                trk.push_back(0x00);
                trk.push_back(0xC0 | chan);
                trk.push_back(kProg[s]);
            }
            uint32_t prev = 0;
            for (auto& ev : evs) {
                midiVarLen(trk, ev.tick - prev); prev = ev.tick;
                if (ev.isOn) {
                    trk.push_back(0x90 | chan); trk.push_back(ev.pitch); trk.push_back(ev.vel);
                } else {
                    trk.push_back(0x80 | chan); trk.push_back(ev.pitch); trk.push_back(0x00);
                }
            }
            trk.push_back(0x00); trk.push_back(0xFF);
            trk.push_back(0x2F); trk.push_back(0x00);
            styleTracks.push_back(std::move(trk));
        }

        uint16_t numTracks = (uint16_t)(1 + styleTracks.size());
        std::vector<uint8_t> midi;
        for (char c : {'M','T','h','d'}) midi.push_back((uint8_t)c);
        midiBE32(midi, 6); midiBE16(midi, 1);
        midiBE16(midi, numTracks); midiBE16(midi, TPQN);
        pushTC(midi, tempoTrk);
        for (auto& t : styleTracks) pushTC(midi, t);
        return midi;
    }

    void doExportMidi() {
        static constexpr uint16_t TPQN = 480; // ticks per quarter note

        // Snapshot the notes recorded during playback
        std::vector<MelodyMakerVST3::RecordedNote> notes;
        {
            std::lock_guard<std::mutex> lk(plugin->recMtx);
            notes = plugin->recNotes;
        }

        if (notes.empty()) {
            MessageBoxW(hWnd,
                L"Aucune note enregistr\u00e9e.\n\n"
                L"Lance Play dans FL Studio avec des notes dans la piano roll,\n"
                L"laisse g\u00e9n\u00e9rer quelques mesures, stop, puis clique ici.",
                L"Export MIDI", MB_OK | MB_ICONINFORMATION);
            return;
        }

        struct Ev { uint32_t tick; bool isOn; uint8_t pitch; uint8_t vel; };

        // Bucket notes per style (one MIDI track per instrument).
        constexpr int kStyles = MelodyMakerVST3::kStyleCount; // 5
        std::vector<Ev> perStyle[kStyles];

        for (auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            int s = std::clamp((int)n.style, 0, kStyles - 1);
            uint32_t onTick  = (uint32_t)std::round(n.beatOn * TPQN);
            uint32_t offTick = (uint32_t)std::round((n.beatOn + n.beatLen) * TPQN);
            if (offTick <= onTick) offTick = onTick + 1;
            uint8_t vel = (uint8_t)std::clamp((int)std::round(n.vel * 127.f), 1, 127);
            uint8_t p   = (uint8_t)std::clamp((int)n.pitch, 0, 127);
            perStyle[s].push_back({onTick,  true,  p, vel});
            perStyle[s].push_back({offTick, false, p, 0});
        }

        // Embed correct tempo so the MIDI matches the host project BPM
        float bpm = plugin->lastBpm.load(std::memory_order_relaxed);
        if (bpm < 1.f) bpm = 120.f;
        uint32_t usPerBeat = (uint32_t)std::round(60000000.0 / bpm);

        auto pushTrackChunk = [](std::vector<uint8_t>& midi,
                                 const std::vector<uint8_t>& body) {
            for (char c : {'M','T','r','k'}) midi.push_back((uint8_t)c);
            midiBE32(midi, (uint32_t)body.size());
            midi.insert(midi.end(), body.begin(), body.end());
        };

        auto pushTrackName = [](std::vector<uint8_t>& trk, const char* name) {
            trk.push_back(0x00);
            trk.push_back(0xFF); trk.push_back(0x03);
            size_t n = std::strlen(name);
            midiVarLen(trk, (uint32_t)n);
            for (size_t i = 0; i < n; ++i) trk.push_back((uint8_t)name[i]);
        };

        // Track 1: tempo / time signature only.
        std::vector<uint8_t> tempoTrack;
        // Time signature 4/4 (24 MIDI clocks per beat, 8 32nds per quarter)
        tempoTrack.push_back(0x00);
        tempoTrack.push_back(0xFF); tempoTrack.push_back(0x58); tempoTrack.push_back(0x04);
        tempoTrack.push_back(0x04); tempoTrack.push_back(0x02);
        tempoTrack.push_back(0x18); tempoTrack.push_back(0x08);
        // Tempo
        tempoTrack.push_back(0x00);
        tempoTrack.push_back(0xFF); tempoTrack.push_back(0x51); tempoTrack.push_back(0x03);
        tempoTrack.push_back((usPerBeat >> 16) & 0xFF);
        tempoTrack.push_back((usPerBeat >>  8) & 0xFF);
        tempoTrack.push_back( usPerBeat        & 0xFF);
        pushTrackName(tempoTrack, "Midnight Melody Maker");
        // End of track
        tempoTrack.push_back(0x00); tempoTrack.push_back(0xFF);
        tempoTrack.push_back(0x2F); tempoTrack.push_back(0x00);

        // One MTrk per non-empty style.
        // GM program suggestions per style (helps the host pick a sound).
        // 0=MÃ©lodie 1=Harpe 2=Basse 3=Percu 4=Marimba 5=Piano
        static const uint8_t kProgram[kStyles] = {
            0,   // 1 Acoustic Grand Piano (MÃ©lodie placeholder)
            46,  // 47 Orchestral Harp
            33,  // 34 Electric Bass (finger)
            0,   // unused on ch 9
            12,  // 13 Marimba
            0,   // 1 Acoustic Grand Piano
        };
        static const char* kTrackName[kStyles] = {
            "Melodie", "Harpe", "Basse", "Percussions", "Marimba", "Piano"
        };

        std::vector<std::vector<uint8_t>> styleTracks;
        for (int s = 0; s < kStyles; ++s) {
            auto& evs = perStyle[s];
            if (evs.empty()) continue;
            std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
                if (a.tick != b.tick) return a.tick < b.tick;
                return !a.isOn; // note-offs before note-ons at same tick
            });

            std::vector<uint8_t> trk;
            pushTrackName(trk, kTrackName[s]);
            // Channel: percussions on MIDI ch 10 (0x09), others on ch 1..4.
            uint8_t chan = (s == 3) ? 9 : (uint8_t)std::min(8, s);
            // Program change at delta 0 (skip for drums on ch 10).
            if (s != 3) {
                trk.push_back(0x00);
                trk.push_back(0xC0 | chan);
                trk.push_back(kProgram[s]);
            }
            uint32_t prev = 0;
            for (auto& ev : evs) {
                midiVarLen(trk, ev.tick - prev);
                prev = ev.tick;
                if (ev.isOn) {
                    trk.push_back(0x90 | chan);
                    trk.push_back(ev.pitch);
                    trk.push_back(ev.vel);
                } else {
                    trk.push_back(0x80 | chan);
                    trk.push_back(ev.pitch);
                    trk.push_back(0x00);
                }
            }
            // End of track
            trk.push_back(0x00); trk.push_back(0xFF);
            trk.push_back(0x2F); trk.push_back(0x00);
            styleTracks.push_back(std::move(trk));
        }

        // Build MIDI file (Format 1 = synchronous multi-track)
        uint16_t numTracks = (uint16_t)(1 + styleTracks.size());
        std::vector<uint8_t> midi;
        for (char c : {'M','T','h','d'}) midi.push_back((uint8_t)c);
        midiBE32(midi, 6);
        midiBE16(midi, 1);          // format 1
        midiBE16(midi, numTracks);
        midiBE16(midi, TPQN);
        pushTrackChunk(midi, tempoTrack);
        for (auto& t : styleTracks) pushTrackChunk(midi, t);

        // Open Save File dialog so the user can choose name and location
        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);

        wchar_t filePath[MAX_PATH] = L"MelodyMaker.mid";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Fichiers MIDI (*.mid)\0*.mid\0Tous les fichiers (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Sauvegarder le MIDI";
        ofn.lpstrDefExt     = L"mid";
        ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (!GetSaveFileNameW(&ofn)) return; // user cancelled

        HANDLE hf = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible de cr\u00e9er le fichier.",
                        L"Export MIDI", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD written = 0;
        WriteFile(hf, midi.data(), (DWORD)midi.size(), &written, nullptr);
        CloseHandle(hf);

        wchar_t msg[MAX_PATH + 384];
        swprintf_s(msg,
            L"Fichier MIDI sauvegard\u00e9 (%d notes, %d pistes, %.0f BPM) :\n%s\n\n"
            L"Format 1 multi-piste : une piste MIDI par instrument activ\u00e9.\n"
            L"Importez dans votre DAW pour retrouver les voix s\u00e9par\u00e9es.",
            (int)notes.size(), (int)styleTracks.size(),
            (double)bpm, filePath);
        MessageBoxW(hWnd, msg, L"Export MIDI \u2014 Multi-piste", MB_OK | MB_ICONINFORMATION);
    }

    // Render the recorded session through TSF + the loaded SoundFont and
    // save the result as a stereo 16-bit WAV. This produces the exact sound
    // currently coming out of the plugin (same SF presets / volumes).
    void doExportWav() {
        // Snapshot the recorded notes
        std::vector<MelodyMakerVST3::RecordedNote> notes;
        {
            std::lock_guard<std::mutex> lk(plugin->recMtx);
            notes = plugin->recNotes;
        }
        if (notes.empty()) {
            MessageBoxW(hWnd,
                L"Aucune note enregistr\u00e9e.\n\n"
                L"Lance Play dans ton DAW, laisse g\u00e9n\u00e9rer quelques mesures,\n"
                L"stop, puis r\u00e9-essaie l'export.",
                L"Export WAV", MB_OK | MB_ICONINFORMATION);
            return;
        }

        float bpm = plugin->lastBpm.load(std::memory_order_relaxed);
        if (bpm < 1.f) bpm = 120.f;
        const int    SR     = 48000;
        const double secsPerBeat = 60.0 / (double)bpm;

        // Total length in samples (longest note end + 2 s tail for releases).
        double endBeats = 0.0;
        for (const auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            double e = n.beatOn + n.beatLen;
            if (e > endBeats) endBeats = e;
        }
        const int64_t totalSamples =
            (int64_t)std::ceil((endBeats * secsPerBeat + 2.0) * SR);
        if (totalSamples <= 0) return;

        // Ask the user where to save before doing the heavy work.
        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);
        wchar_t filePath[MAX_PATH] = L"MelodyMaker.wav";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Fichiers WAV (*.wav)\0*.wav\0Tous les fichiers (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Sauvegarder le rendu audio (WAV)";
        ofn.lpstrDefExt     = L"wav";
        ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return;

        // Locate the bundled SF2 (same logic as MelodyMakerVST3::loadSoundFont).
        wchar_t modPath[MAX_PATH] = {};
        if (!GetModuleFileNameW(g_hInst, modPath, MAX_PATH)) {
            MessageBoxW(hWnd, L"Module introuvable.", L"Export WAV",
                        MB_OK | MB_ICONERROR);
            return;
        }
        for (int i = (int)wcslen(modPath) - 1; i >= 0; --i) {
            if (modPath[i] == L'\\' || modPath[i] == L'/') { modPath[i] = 0; break; }
        }
        std::wstring sfPath = std::wstring(modPath) + L"\\..\\Resources\\GeneralUser.sf2";
        char sfPath8[MAX_PATH * 4] = {};
        WideCharToMultiByte(CP_ACP, 0, sfPath.c_str(), -1,
                            sfPath8, sizeof(sfPath8), nullptr, nullptr);

        // Spin up a private TSF instance so we don't disturb live playback.
        tsf* offline = tsf_load_filename(sfPath8);
        if (!offline) {
            MessageBoxW(hWnd, L"Chargement de la SoundFont impossible.",
                        L"Export WAV", MB_OK | MB_ICONERROR);
            return;
        }
        tsf_set_output(offline, TSF_STEREO_INTERLEAVED, SR, -3.0f);
        tsf_set_max_voices(offline, 64);
        for (int s = 0; s < MelodyMakerVST3::kStyleCount; ++s) {
            int ch  = MelodyMakerVST3::kSfChannelOf(s);
            int idx = std::clamp(plugin->sfPresetIdxPerStyle[s],
                                 0, kSfPresetCount[s] - 1);
            const SfPreset& p = kSfPresetsByStyle[s][idx];
            tsf_channel_set_presetnumber(offline, ch, p.program,
                                         p.bank == 128 ? 1 : 0);
            tsf_channel_set_volume(offline, ch, plugin->sfVolumePerStyle[s]);
        }

        // Build a sorted event list (note-off before note-on at same sample).
        struct Ev { int64_t samp; bool isOn; uint8_t pitch; uint8_t vel; uint8_t chan; };
        std::vector<Ev> evs;
        evs.reserve(notes.size() * 2);
        for (const auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            int s = std::clamp((int)n.style, 0,
                               MelodyMakerVST3::kStyleCount - 1);
            uint8_t ch  = (uint8_t)MelodyMakerVST3::kSfChannelOf(s);
            uint8_t p   = (uint8_t)std::clamp((int)n.pitch, 0, 127);
            uint8_t v   = (uint8_t)std::clamp((int)std::round(n.vel * 127.f),
                                              1, 127);
            int64_t on  = (int64_t)std::round(n.beatOn * secsPerBeat * SR);
            int64_t off = (int64_t)std::round(
                (n.beatOn + n.beatLen) * secsPerBeat * SR);
            if (off <= on) off = on + 1;
            evs.push_back({on,  true,  p, v, ch});
            evs.push_back({off, false, p, 0, ch});
        }
        std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
            if (a.samp != b.samp) return a.samp < b.samp;
            return !a.isOn; // offs first
        });

        // Render in chunks, writing interleaved stereo float into a buffer.
        std::vector<float> pcm((size_t)totalSamples * 2, 0.0f);
        size_t outPos = 0;
        size_t evIdx  = 0;
        const size_t kChunk = 1024;
        while (outPos < (size_t)totalSamples) {
            // Fire all events whose sample <= outPos.
            while (evIdx < evs.size() && evs[evIdx].samp <= (int64_t)outPos) {
                const Ev& e = evs[evIdx++];
                if (e.isOn)
                    tsf_channel_note_on (offline, e.chan, e.pitch, e.vel / 127.f);
                else
                    tsf_channel_note_off(offline, e.chan, e.pitch);
            }
            // Render up to the next event (or chunk size).
            size_t nextEvSamp = (evIdx < evs.size())
                ? (size_t)evs[evIdx].samp : (size_t)totalSamples;
            size_t want = std::min<size_t>(kChunk,
                std::min<size_t>(nextEvSamp - outPos,
                                 (size_t)totalSamples - outPos));
            if (want == 0) want = 1;
            tsf_render_float(offline, pcm.data() + outPos * 2,
                             (int)want, 0);
            outPos += want;
        }
        tsf_close(offline);

        // Find peak for normalisation hint (do NOT actually normalise â€” keep
        // the same level the user hears live).
        float peak = 0.f;
        for (float v : pcm) { float a = std::fabs(v); if (a > peak) peak = a; }
        // Soft clip prevention.
        float gain = 1.0f;
        if (peak > 0.98f) gain = 0.98f / peak;

        // Convert to 16-bit PCM little-endian.
        std::vector<int16_t> pcm16(pcm.size());
        for (size_t i = 0; i < pcm.size(); ++i) {
            float v = std::clamp(pcm[i] * gain, -1.0f, 1.0f);
            pcm16[i] = (int16_t)std::lrint(v * 32767.0f);
        }

        // Build minimal WAV (PCM16 stereo).
        const uint16_t channels   = 2;
        const uint32_t byteRate   = (uint32_t)SR * channels * 2;
        const uint16_t blockAlign = channels * 2;
        const uint32_t dataBytes  = (uint32_t)(pcm16.size() * sizeof(int16_t));
        std::vector<uint8_t> wav;
        auto put32 = [&](uint32_t v) {
            wav.push_back((uint8_t)(v & 0xFF));
            wav.push_back((uint8_t)((v >>  8) & 0xFF));
            wav.push_back((uint8_t)((v >> 16) & 0xFF));
            wav.push_back((uint8_t)((v >> 24) & 0xFF));
        };
        auto put16 = [&](uint16_t v) {
            wav.push_back((uint8_t)(v & 0xFF));
            wav.push_back((uint8_t)((v >> 8) & 0xFF));
        };
        for (char c : {'R','I','F','F'}) wav.push_back((uint8_t)c);
        put32(36u + dataBytes);
        for (char c : {'W','A','V','E'}) wav.push_back((uint8_t)c);
        for (char c : {'f','m','t',' '}) wav.push_back((uint8_t)c);
        put32(16);                  // fmt size
        put16(1);                   // PCM
        put16(channels);
        put32((uint32_t)SR);
        put32(byteRate);
        put16(blockAlign);
        put16(16);                  // bits per sample
        for (char c : {'d','a','t','a'}) wav.push_back((uint8_t)c);
        put32(dataBytes);
        size_t hdr = wav.size();
        wav.resize(hdr + dataBytes);
        std::memcpy(wav.data() + hdr, pcm16.data(), dataBytes);

        HANDLE hf = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible de cr\u00e9er le fichier.",
                        L"Export WAV", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD written = 0;
        WriteFile(hf, wav.data(), (DWORD)wav.size(), &written, nullptr);
        CloseHandle(hf);

        double durSec = (double)totalSamples / (double)SR;
        wchar_t msg[MAX_PATH + 384];
        swprintf_s(msg,
            L"Audio rendu sauvegard\u00e9 (%d notes, %.1f s, %.0f BPM, %d Hz stereo) :\n%s\n\n"
            L"Le fichier WAV contient le son exact qui sort du plugin\n"
            L"avec les SoundFonts et volumes actuels.",
            (int)notes.size(), durSec, (double)bpm, SR, filePath);
        MessageBoxW(hWnd, msg, L"Export WAV \u2014 Rendu audio",
                    MB_OK | MB_ICONINFORMATION);
    }

    // Tiny in-memory IBStream (vector-backed) for preset save/load.
    // Used only on the GUI thread, no real refcounting needed.
    class MemBufStream : public IBStream {
    public:
        std::vector<uint8_t> buf;
        int64                 pos = 0;
        explicit MemBufStream(std::vector<uint8_t> initial = {})
            : buf(std::move(initial)) {}
        tresult PLUGIN_API read (void* dst, int32 n, int32* nRead) override {
            int64 avail = (int64)buf.size() - pos;
            int32 toRead = (int32)std::min<int64>(avail, n);
            if (toRead > 0) std::memcpy(dst, buf.data() + pos, (size_t)toRead);
            pos += toRead;
            if (nRead) *nRead = toRead;
            return kResultOk;
        }
        tresult PLUGIN_API write (void* src, int32 n, int32* nWritten) override {
            if (n < 0) { if (nWritten) *nWritten = 0; return kResultOk; }
            if ((size_t)(pos + n) > buf.size()) buf.resize((size_t)(pos + n));
            std::memcpy(buf.data() + pos, src, (size_t)n);
            pos += n;
            if (nWritten) *nWritten = n;
            return kResultOk;
        }
        tresult PLUGIN_API seek (int64 p, int32 mode, int64* result) override {
            int64 np = pos;
            if (mode == kIBSeekSet)      np = p;
            else if (mode == kIBSeekCur) np = pos + p;
            else if (mode == kIBSeekEnd) np = (int64)buf.size() + p;
            if (np < 0) np = 0;
            pos = np;
            if (result) *result = pos;
            return kResultOk;
        }
        tresult PLUGIN_API tell (int64* p) override {
            if (p) *p = pos; return kResultOk;
        }
        // FUnknown stubs (no real refcount; lifetime is the local var).
        tresult PLUGIN_API queryInterface(const TUID, void** obj) override {
            if (obj) *obj = nullptr; return kNoInterface;
        }
        uint32  PLUGIN_API addRef ()  override { return 1; }
        uint32  PLUGIN_API release () override { return 1; }
    };

    void doSavePreset() {
        // Capture current plugin state into an in-memory stream.
        MemBufStream mem;
        if (plugin->getState(&mem) != kResultOk) {
            MessageBoxW(hWnd, L"\u00c9chec de la r\u00e9cup\u00e9ration de l'\u00e9tat.",
                        L"Sauvegarder le preset", MB_OK | MB_ICONERROR);
            return;
        }

        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);
        wchar_t filePath[MAX_PATH] = L"MelodyMaker.mmp";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Preset Melody Maker (*.mmp)\0*.mmp\0Tous (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Sauvegarder un preset";
        ofn.lpstrDefExt     = L"mmp";
        ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return;

        // File format: 4-byte magic 'MMPR' + 4-byte version (1) + raw state bytes.
        const uint8_t magic[4] = { 'M','M','P','R' };
        const uint32_t ver = 1u;

        HANDLE hf = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible de cr\u00e9er le fichier.",
                        L"Sauvegarder le preset", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD wr = 0;
        WriteFile(hf, magic, 4, &wr, nullptr);
        WriteFile(hf, &ver,  4, &wr, nullptr);
        if (!mem.buf.empty())
            WriteFile(hf, mem.buf.data(), (DWORD)mem.buf.size(), &wr, nullptr);
        CloseHandle(hf);

        // Auto-save MIDI alongside the preset if notes have been recorded.
        bool midiSaved = false;
        wchar_t midPath[MAX_PATH];
        {
            std::vector<MelodyMakerVST3::RecordedNote> notes;
            { std::lock_guard<std::mutex> lk(plugin->recMtx); notes = plugin->recNotes; }
            if (!notes.empty()) {
                std::vector<uint8_t> midiBytes = buildMidiBytes(notes);
                if (!midiBytes.empty()) {
                    wcscpy_s(midPath, filePath);
                    wchar_t* dot = wcsrchr(midPath, L'.');
                    if (dot) *dot = L'\0';
                    wcscat_s(midPath, L".mid");
                    HANDLE hm = CreateFileW(midPath, GENERIC_WRITE, 0, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hm != INVALID_HANDLE_VALUE) {
                        DWORD mw = 0;
                        WriteFile(hm, midiBytes.data(), (DWORD)midiBytes.size(), &mw, nullptr);
                        CloseHandle(hm);
                        midiSaved = true;
                    }
                }
            }
        }

        wchar_t msg[MAX_PATH + 400];
        if (midiSaved)
            swprintf_s(msg, L"Preset + MIDI sauvegard\u00e9s :\n  %s\n  %s", filePath, midPath);
        else
            swprintf_s(msg, L"Preset sauvegard\u00e9 (%zu octets) :\n%s",
                       mem.buf.size() + 8, filePath);
        MessageBoxW(hWnd, msg, L"Sauvegarder le preset", MB_OK | MB_ICONINFORMATION);
    }

    void doLoadPreset() {
        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);
        wchar_t filePath[MAX_PATH] = L"";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Preset Melody Maker (*.mmp)\0*.mmp\0Tous (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Charger un preset";
        ofn.lpstrDefExt     = L"mmp";
        ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return;

        HANDLE hf = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible d'ouvrir le fichier.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(hf, &sz) || sz.QuadPart < 8 || sz.QuadPart > (1<<24)) {
            CloseHandle(hf);
            MessageBoxW(hWnd, L"Fichier de preset invalide.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        std::vector<uint8_t> file((size_t)sz.QuadPart);
        DWORD rd = 0;
        ReadFile(hf, file.data(), (DWORD)file.size(), &rd, nullptr);
        CloseHandle(hf);
        if (rd != file.size() ||
            file[0] != 'M' || file[1] != 'M' ||
            file[2] != 'P' || file[3] != 'R') {
            MessageBoxW(hWnd, L"Ce n'est pas un preset Melody Maker.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        // file[4..7] = version (currently 1; ignored beyond presence).
        std::vector<uint8_t> body(file.begin() + 8, file.end());
        MemBufStream mem(std::move(body));
        if (plugin->setState(&mem) != kResultOk) {
            MessageBoxW(hWnd, L"\u00c9tat illisible.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        // Refresh the entire UI from the freshly loaded state.
        int s = plugin->getStyleType();
        applyVisibilityForStyle(s);
        refreshAll();
        populateInstrumentCombo(s);
        if (hVolSlider)
            SendMessageW(hVolSlider, TBM_SETPOS, TRUE,
                (LPARAM)(int)std::round(plugin->sfVolumePerStyle[s] * 100.0f));
        refreshVolValue();
        // Refresh new parameters not handled by applyVisibilityForStyle.
        if (hHumanizeKnob)
            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA)))
                kw->setPos((int)std::round(plugin->humanizePerStyle[s] * 1000.0f));
        refreshHumanizeValue();
        if (hRetardKnob)
            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA)))
                kw->setPos((int)std::round(plugin->retardPerStyle[s] * 1000.0f));
        refreshRetardValue();
        if (hSectionCombo)
            SendMessageW(hSectionCombo, CB_SETCURSEL,
                std::clamp(plugin->currentSection.load(std::memory_order_relaxed), 0, 4), 0);
        if (hStartBarCombo) {
            static constexpr int kSBV[] = { 0,1,2,4,8 };
            int sbv = plugin->startBarPerStyle[s];
            int sbi = 0; for (int k=0;k<5;k++) if (kSBV[k]==sbv) sbi=k;
            SendMessageW(hStartBarCombo, CB_SETCURSEL, sbi, 0);
        }
        if (hProgCombo) SendMessageW(hProgCombo, CB_SETCURSEL,
                                     plugin->getProgType(), 0);
        InvalidateRect(hWnd, nullptr, TRUE);

        MessageBoxW(hWnd, L"Preset charg\u00e9 avec succ\u00e8s.",
                    L"Charger le preset", MB_OK | MB_ICONINFORMATION);
    }

    void sendParam(int i, double actual) {
        // Update local plugin state immediately so process() picks it up
        // even before the host echoes the change back.
        plugin->paramValues[i] = actual;
        // Mirror to active tab, then broadcast to other locked tabs only.
        int activeStyle = plugin->getStyleType();
        plugin->paramValuesPerStyle[activeStyle][i] = actual;
        if (i == mm::kParamSubdiv || i == mm::kParamMode)
            plugin->broadcastParamLocked(i, activeStyle);
        double norm = MelodyMakerVST3::actualToNorm(i, actual);
        plugin->beginEdit(i);
        plugin->performEdit(i, norm);
        plugin->endEdit(i);
        plugin->setParamNormalized(i, norm);
        updateValueLabel(i, actual);
    }

    void onComboChange(int paramId) {
        int idx = (int)SendMessageW(hCtrl[paramId], CB_GETCURSEL, 0, 0);
        if (idx == CB_ERR) return;
        sendParam(paramId, valueFromComboIndex(paramId, idx));
        // Refresh compat dots: when Mode changes, repaint Prog combo (and vice versa).
        if (paramId == mm::kParamMode && hProgCombo) {
            InvalidateRect(hProgCombo, nullptr, FALSE);
            UpdateWindow(hProgCombo);
        }
    }

    void onTrackbarChange(int paramId) {
        HWND ctrl = hCtrl[paramId];
        int pos = 0;
        // Knob widget? Read its stored pos directly.
        wchar_t cls[64] = {};
        GetClassNameW(ctrl, cls, 64);
        if (wcscmp(cls, KnobWidget::kClassName) == 0) {
            auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(ctrl, GWLP_USERDATA));
            pos = kw ? kw->getPos() : 0;
        } else {
            pos = (int)SendMessageW(ctrl, TBM_GETPOS, 0, 0);
        }
        const auto& d = mm::kParamDefs[paramId];
        double v = d.min_value + (pos / 1000.0) * (d.max_value - d.min_value);
        sendParam(paramId, v);
    }
};

IMPLEMENT_FUNKNOWN_METHODS(MelodyMakerView, IPlugView, IPlugView::iid)

IPlugView* PLUGIN_API MelodyMakerVST3::createView(FIDString name) {
    if (name && std::strcmp(name, ViewType::kEditor) == 0)
        return new MelodyMakerView(this);
    return nullptr;
}

// =============================================================================
// Plugin factory
// =============================================================================
BEGIN_FACTORY_DEF(
    "Midnight",
    "https://github.com/midnight",
    "mailto:support@midnight.example.com")

    DEF_CLASS2(
        INLINE_UID(0xA3F7E291, 0x5CB24D18, 0x87A04E2C, 0x9B8D1F05),
        PClassInfo::kManyInstances,
        kVstAudioEffectClass,
        "Midnight Melody Maker",
        0,              // component flags
        "Instrument",   // sub-category â†’ FL Studio shows it as a Generator
        "1.0.0",
        kVstVersionString,
        MelodyMakerVST3::createInstance)

END_FACTORY

