#pragma once
// Auto-generated from plugin_copy — do not edit directly.

#include "logger.h"

#include "../../common/algo.h"
#include "../../ml_training/data/patterns/generated_phrases.h"

// TinySoundFont â€“ single-header SoundFont2 synthesizer (MIT)
// Provides high-quality sampled instrument sounds from any .sf2 file.
#include "../../common/tsf.h"

// SoundFont Maker â€“ procedural .sf2 generator (header-only).
#include "../../common/sf2_maker.h"

// SoundFont Editor — delta-patcher sur SF2 existants (header-only).
#include "../../common/sf2_editor.h"

// FX â€“ chorus / delay / reverb / cassette noise (header-only).
#include "../../common/fx.h"

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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "msimg32.lib")
#include <commdlg.h>

#include "ui_constants.h"

extern HINSTANCE g_hInst;

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
// `source` selects the underlying SF2: 0 = GeneralUser GS, 1 = Midnight Lofi.
// -----------------------------------------------------------------------------
struct SfPreset { const wchar_t* name; int program; int bank; int source = 0; };

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
      {L"Lofi Harp",       46,  0, 1}, {L"",                 0,  0} },
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
      {L"Orgue Hammond",   16,  0}, {L"Lofi Piano",       0,  0, 1} },
};
static constexpr int kSfPresetCount[6] = { 16, 15, 12, 8, 12, 16 };

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

        // Six stereo audio outputs — one per instrument style.
        // In FL Studio Patcher each bus maps to a separate mixer track.
        addAudioOutput(STR16("Melodie"),  SpeakerArr::kStereo);
        addAudioOutput(STR16("Arpege"),   SpeakerArr::kStereo);
        addAudioOutput(STR16("Basse"),    SpeakerArr::kStereo);
        addAudioOutput(STR16("Percu."),   SpeakerArr::kStereo);
        addAudioOutput(STR16("Contre"),   SpeakerArr::kStereo);
        addAudioOutput(STR16("Piano"),    SpeakerArr::kStereo);

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
        for (int s = 0; s < 6; ++s) fxChainPerStyle[s].prepare((float)sampleRate);
        applyAllFxParams();
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API setBusArrangements(
        SpeakerArrangement* inputs,  int32 numIns,
        SpeakerArrangement* outputs, int32 numOuts) override
    {
        // Accept no audio inputs; any number of stereo outputs.
        if (numIns != 0) return kResultFalse;
        for (int32 i = 0; i < numOuts; ++i)
            if (outputs[i] != SpeakerArr::kStereo) return kResultFalse;
        return kResultTrue;
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

        // -- Drain GUI note-preview queue (piano key taps from piano roll UI) --
        {
            IEventList* outEv = data.outputEvents;
            uint32_t r = guiNoteRead.load(std::memory_order_relaxed);
            uint32_t w = guiNoteWrite.load(std::memory_order_acquire);
            int guiStyle = getStyleType();
            int16 guiChan = (int16)guiStyle;
            while (r != w) {
                GuiNote gn = guiNoteRing[r % kGuiQueueSize];
                r++;
                if (gn.noteOff) {
                    // Note-off: stop all voices on this pitch
                    for (auto& v : voicePool) {
                        if (v.pitch == gn.pitch && !v.noteOff) {
                            v.noteOff  = true;
                            v.offSample = 0;
                        }
                    }
                    if (outEv) {
                        Event off{}; off.type = Event::kNoteOffEvent;
                        off.sampleOffset = 0; off.noteOff.channel = guiChan;
                        off.noteOff.pitch = gn.pitch; off.noteOff.noteId = -1;
                        outEv->addEvent(off);
                    }
                } else {
                    int32 nid = nextNoteId++;
                    triggerVoice(gn.pitch, gn.vel, -1, false, nid, gn.style);
                    if (outEv) {
                        Event on{}; on.type = Event::kNoteOnEvent;
                        on.sampleOffset = 0; on.noteOn.channel = guiChan;
                        on.noteOn.pitch = gn.pitch;
                        on.noteOn.velocity = (std::clamp)(gn.vel * sfVolumePerStyle[gn.style], 0.05f, 1.0f);
                        on.noteOn.noteId = nid;
                        outEv->addEvent(on);
                    }
                }
            }
            guiNoteRead.store(r, std::memory_order_release);
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
                                        MM_LOG("SECTION CHANGE: pitch=%d holdBeats=%.2f => sec=%d (0=Intro,1=Verse,2=Chorus,3=Bridge,4=Outro) ramp=%.3f",
                                            ev.noteOff.pitch, holdBeats, sec, sectionVolumeRamp);
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
        if (data.outputs) {
            for (int32 bi = 0; bi < data.numOutputs; ++bi) {
                AudioBusBuffers& bus = data.outputs[bi];
                bus.silenceFlags = 0;
                for (int32 ch = 0; ch < bus.numChannels; ++ch)
                    if (bus.channelBuffers32 && bus.channelBuffers32[ch])
                        std::memset(bus.channelBuffers32[ch], 0,
                                    sizeof(float) * static_cast<size_t>(data.numSamples));
            }
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
        MM_LOG_EVERY(500, "transport: rolling=%d beatPos=%.3f bpm=%.1f triggerCount=%d sfReady=%d",
            (int)rolling, beatPos, bpm, triggerCount, (int)sfReady);

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
            if (recMtx.try_lock()) {
                // Keep user-drawn notes; drop auto-generated ones.
                recNotes = pinnedNotes;
                audioNotes = pinnedNotes;
                recMtx.unlock();
            }
            recStartBeat = beatPos;
        }
        // Detect transport start: begin fresh recording
        if (rolling && prevBeatPos < 0.0) {
            if (recMtx.try_lock()) {
                recNotes = pinnedNotes;
                audioNotes = pinnedNotes;
                recMtx.unlock();
            }
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
            MM_LOG_CHANGE("rolling", 0);
            triggerCount = 0;
            // Send note-off events for any MIDI notes still tracked as active.
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
                }
                activeNotes.clear();
            }
            // Always silence the internal synth (covers TSF release tails even
            // when activeNotes is already empty, e.g. first pause after a bar).
            sfAllNotesOff();
            for (auto& vo : voicePool) { vo.noteOff = true; vo.env = 0.0; vo.offSample = -1; }
            lastSlot = -1;
            renderVoices(data.outputs, data.numOutputs, nframes);
            return kResultOk;
        }

        // Only generate melody while at least one trigger note is held.
        // --- Fire user-pinned notes (independent of trigger state) ---
        {
            double bps_  = std::max(0.05, bpm / 60.0);
            double bps   = bps_ / sampleRate; // beatsPerSample
            double blockEnd = beatPos + static_cast<double>(nframes) * bps;
            for (auto& pn : audioNotes) {
                if (!isStyleAudible(pn.style)) continue; // mute / solo gate
                double absBeat = recStartBeat + pn.beatOn;
                if (absBeat < beatPos || absBeat >= blockEnd) continue;
                double offsetBeats  = absBeat - beatPos;
                int32  so  = static_cast<int32>(std::max(0.0,
                                 std::floor(offsetBeats / bps)));
                if (so >= static_cast<int32>(nframes)) continue;
                int16_t pitch = pn.pitch;
                float   vel   = std::clamp(pn.vel, 0.05f, 1.0f);
                int32   nid   = nextNoteId++;
                int16   chan  = static_cast<int16>(pn.style);
                if (midiNoteOut && outEvents) {
                    Event on{};
                    on.type            = Event::kNoteOnEvent;
                    on.sampleOffset    = so;
                    on.noteOn.channel  = chan;
                    on.noteOn.pitch    = static_cast<int16>(pitch);
                    on.noteOn.velocity = (std::clamp)(vel * sfVolumePerStyle[pn.style], 0.05f, 1.0f);
                    on.noteOn.noteId   = nid;
                    outEvents->addEvent(on);
                }
                double  noteBeats = std::max(pn.beatLen, 1.0 / 16.0);
                int64_t offSample = static_cast<int64_t>(so)
                                  + static_cast<int64_t>(std::round(noteBeats / bps));
                triggerVoice(pitch, vel,
                    offSample < static_cast<int64_t>(nframes) ? offSample : -1,
                    /*isPerc=*/false, nid, pn.style);
                if (offSample < static_cast<int64_t>(nframes)) {
                    if (outEvents) {
                        Event off{};
                        off.type             = Event::kNoteOffEvent;
                        off.sampleOffset     = static_cast<int32>(offSample);
                        off.noteOff.channel  = chan;
                        off.noteOff.pitch    = static_cast<int16>(pitch);
                        off.noteOff.noteId   = nid;
                        off.noteOff.velocity = 0.f;
                        outEvents->addEvent(off);
                    }
                } else {
                    activeNotes.push_back({
                        static_cast<int16_t>(pitch), chan, nid,
                        offSample - static_cast<int64_t>(nframes)
                    });
                }
            }
        }

        if (!hasTrigger()) {
            MM_LOG_CHANGE("hasTrigger", 0);
            renderVoices(data.outputs, data.numOutputs, nframes);
            return kResultOk;
        }
        MM_LOG_CHANGE("hasTrigger", 1);

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
            // firedThisSlot[s]=true once style s emits a note this slot.
            // Styles processed later (e.g. bass=2) can read earlier ones
            // (Melodie=0, Arpege=1) for cross-style JAM gating.
            bool firedThisSlot[kStyleCount] = {};

            // Pre-compute whether drums (style 3) fire this slot so that
            // bass JAM gating can use it even though bass runs before drums.
            bool drumFiresThisSlot = false;
            if ((enabledMask >> 3) & 1u) {
                int slotsPerStep3 = std::max(1, barLen / 16);
                int posInBar3 = (int)(((slot % barLen) + barLen) % barLen);
                if (posInBar3 % slotsPerStep3 == 0) {
                    int step3 = (posInBar3 / slotsPerStep3) % 16;
                    int pidx3 = std::clamp(percRhythmPerStyle[3], 0, mm::kDrumPatternCount - 1);
                    const auto& pat3 = mm::kDrumPatterns[pidx3];
                    for (int h = 0; h < 4; ++h) {
                        if (pat3.steps[step3][h].pitch != 0 && pat3.steps[step3][h].vel != 0) {
                            drumFiresThisSlot = true; break;
                        }
                    }
                }
            }

            for (int styleIdx = 0; styleIdx < kStyleCount; ++styleIdx) {
                if (!((enabledMask >> styleIdx) & 1u)) continue;
                if (!isStyleAudible(styleIdx)) continue;  // mute / solo gate
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
                    double effDensity;
                    if (style == 2) {
                        // Basse: temps forts (1,3) toujours, beat 2/4 seulement à haute densité,
                        // off-beats quasi jamais. Produit une ligne de basse sparse et groovy.
                        if (isStrong) {
                            effDensity = 0.80 + 0.20 * sDens;   // ~80-100% sur beats 1,3
                        } else if (isDown) {
                            effDensity = (sDens > 0.65) ? (sDens - 0.65) / 0.35 : 0.0;
                        } else {
                            effDensity = (sDens > 0.88) ? (sDens - 0.88) / 0.12 : 0.0;
                        }
                    } else {
                        // Strong beats (beat 1/3): floor at 65%, reaches 100% at max density.
                        // Downbeats (all beat onsets): start firing at Density >= 25%.
                        // Off-beats (subdivisions): start firing at Density >= 55%.
                        if (isStrong) {
                            effDensity = 0.65 + 0.35 * sDens;
                        } else if (isDown) {
                            effDensity = (sDens > 0.25) ? (sDens - 0.25) / 0.75 : 0.0;
                        } else {
                            effDensity = (sDens > 0.55) ? (sDens - 0.55) / 0.45 : 0.0;
                        }
                    }
                    if (roll > effDensity) {
                        MM_LOG_EVERY(200, "density gate: style=%d slot=%lld roll=%.3f eff=%.3f isStrong=%d isDown=%d",
                            styleIdx, (long long)slot, roll, effDensity, (int)isStrong, (int)isDown);
                        continue;
                    }

                    // JAM mode for bass: gate on drums (pre-computed) + any
                    // melodic style that already fired. Strong beats always pass
                    // so the bass anchors the chord regardless.
                    if (style == 2 && jamPerStyle[styleIdx]) {
                        bool anyFired = drumFiresThisSlot
                                     || firedThisSlot[0] || firedThisSlot[1]
                                     || firedThisSlot[4] || firedThisSlot[5];
                        if (!anyFired && !isStrong) continue;
                    }
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
                        jamPerStyle[styleIdx],
                        percRhythmPerStyle[3], // style 3 = Percu., used for JAM rhythm ref
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
                        if (velocity < 0.02f) {
                            MM_LOG_EVERY(50, "velocity near-zero after ramp: vel=%.4f ramp=%.4f sec=%d — skip note",
                                velocity, sectionVolumeRamp, currentSection.load());
                            continue;  // skip: vel=0 NoteOn acts as NoteOff in many hosts
                        }
                        int32 noteId  = nextNoteId++;
                    // Fixed MIDI channel per style for multi-timbral hosts (e.g. Omnisphere).
                    int16 channel = (int16)styleIdx;  // one fixed channel per style (0=Voix..5=Piano)

                    if (midiNoteOut && outEvents) {
                        Event on{};
                        on.type              = Event::kNoteOnEvent;
                        on.sampleOffset      = styleOffset;
                        on.noteOn.channel    = channel;
                        on.noteOn.pitch      = static_cast<int16>(pitch);
                        on.noteOn.velocity   = (std::clamp)(velocity * sfVolumePerStyle[styleIdx], 0.0f, 1.0f);
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
                    firedThisSlot[styleIdx] = true;

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
        // v2.9: user-pinned notes (hand-drawn in the piano roll).
        // Format per note: beatOn(double) beatLen(double) pitch(int32) vel(float) style(int32)
        {
            std::lock_guard<std::mutex> lk(recMtx);
            s.writeInt32(static_cast<int32>(pinnedNotes.size()));
            for (auto& n : pinnedNotes) {
                s.writeDouble(n.beatOn);
                s.writeDouble(n.beatLen);
                s.writeInt32(static_cast<int32>(n.pitch));
                s.writeFloat(n.vel);
                s.writeInt32(static_cast<int32>(n.style));
            }
        }
        // v3.0: mute + solo bitmasks
        s.writeInt32(static_cast<int32>(muteStyles.load(std::memory_order_relaxed)));
        s.writeInt32(static_cast<int32>(soloStyles.load(std::memory_order_relaxed)));
        // v3.1: per-style JAM mode
        for (int st = 0; st < kStyleCount; ++st) s.writeInt32(jamPerStyle[st] ? 1 : 0);
        // v3.2: per-style FX params
        for (int st = 0; st < kStyleCount; ++st) {
            auto& p = fxParamsPerStyle[st];
            s.writeInt32(p.chorusOn ? 1 : 0); s.writeFloat(p.chorusRate); s.writeFloat(p.chorusDepth); s.writeFloat(p.chorusMix);
            s.writeInt32(p.delayOn  ? 1 : 0); s.writeFloat(p.delayTime);  s.writeFloat(p.delayFb);     s.writeFloat(p.delayMix);
            s.writeInt32(p.reverbOn ? 1 : 0); s.writeFloat(p.reverbSize); s.writeFloat(p.reverbDamp);  s.writeFloat(p.reverbMix);
            s.writeInt32(p.noiseOn  ? 1 : 0); s.writeFloat(p.noiseLevel); s.writeFloat(p.noiseFlutter);s.writeFloat(p.noiseTone);
        }
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
                    sfPresetIdxPerStyle[sti] = std::clamp((int)sp, 0, std::max(1, effectivePresetCount(sti)) - 1);
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
            // v2.9: user-pinned notes. Best-effort.
            {
                int32 nc = 0;
                if (s.readInt32(nc) && nc > 0 && nc <= 65536) {
                    std::lock_guard<std::mutex> lk(recMtx);
                    pinnedNotes.clear();
                    for (int32 ni = 0; ni < nc; ++ni) {
                        double bo = 0.0, bl = 0.25;
                        int32  p = 60, stl = 0;
                        float  v = 0.8f;
                        if (!s.readDouble(bo)) break;
                        if (!s.readDouble(bl)) break;
                        if (!s.readInt32(p))   break;
                        if (!s.readFloat(v))   break;
                        if (!s.readInt32(stl)) break;
                        pinnedNotes.push_back({
                            bo, bl,
                            static_cast<int16_t>(std::clamp((int)p, 0, 127)),
                            std::clamp(v, 0.0f, 1.0f),
                            static_cast<uint8_t>(std::clamp((int)stl, 0, 5))
                        });
                    }
                    recNotes  = pinnedNotes;
                    audioNotes = pinnedNotes;
                }
            }
            // v3.0: mute + solo bitmasks. Best-effort.
            { int32 m = 0; if (s.readInt32(m)) muteStyles.store(static_cast<uint32_t>(m), std::memory_order_relaxed); }
            { int32 m = 0; if (s.readInt32(m)) soloStyles.store(static_cast<uint32_t>(m), std::memory_order_relaxed); }
            // v3.1: per-style JAM mode. Best-effort.
            for (int sti = 0; sti < kStyleCount; ++sti) {
                int32 jm = 0; if (s.readInt32(jm)) jamPerStyle[sti] = (jm != 0);
            }
            // v3.2: per-style FX params. Best-effort.
            for (int sti = 0; sti < kStyleCount; ++sti) {
                auto& p = fxParamsPerStyle[sti];
                int32 t = 0; float f = 0.f;
                if (s.readInt32(t)) p.chorusOn = (t != 0); if (s.readFloat(f)) p.chorusRate = f; if (s.readFloat(f)) p.chorusDepth = f; if (s.readFloat(f)) p.chorusMix = f;
                if (s.readInt32(t)) p.delayOn  = (t != 0); if (s.readFloat(f)) p.delayTime  = f; if (s.readFloat(f)) p.delayFb    = f; if (s.readFloat(f)) p.delayMix  = f;
                if (s.readInt32(t)) p.reverbOn = (t != 0); if (s.readFloat(f)) p.reverbSize = f; if (s.readFloat(f)) p.reverbDamp = f; if (s.readFloat(f)) p.reverbMix  = f;
                if (s.readInt32(t)) p.noiseOn  = (t != 0); if (s.readFloat(f)) p.noiseLevel = f; if (s.readFloat(f)) p.noiseFlutter = f; if (s.readFloat(f)) p.noiseTone = f;
            }
            applyAllFxParams();
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

    // -------------------------------------------------------------------------
    // Per-style FX (chorus / delay / reverb / cassette noise)
    // -------------------------------------------------------------------------
    struct FxParams {
        // Chorus
        bool  chorusOn   = false;
        float chorusRate = 0.7f;   // Hz
        float chorusDepth = 5.0f;  // ms
        float chorusMix  = 0.5f;   // 0..1
        // Delay
        bool  delayOn    = false;
        float delayTime  = 350.0f; // ms
        float delayFb    = 0.40f;  // 0..0.92
        float delayMix   = 0.30f;  // 0..1
        // Reverb
        bool  reverbOn   = false;
        float reverbSize = 0.55f;  // 0..1
        float reverbDamp = 0.30f;  // 0..1
        float reverbMix  = 0.25f;  // 0..1
        // Cassette noise
        bool  noiseOn    = false;
        float noiseLevel  = 0.10f; // 0..1
        float noiseFlutter = 0.50f;// 0..1
        float noiseTone   = 0.40f; // 0..1
    };
    FxParams fxParamsPerStyle[6]{};
    fx::FxChain fxChainPerStyle[6]{};
    std::atomic<bool> fxParamsDirty{true};
    std::mutex fxMutex;

    // --- Live SoundFont preview (in-memory, no disk write) ---
    // Holds the raw SF2 bytes + loaded TSF synth for the current preview.
    // Replaced atomically under sfMutex when the user tweaks Maker params.
    struct LiveSf {
        std::vector<uint8_t> data;  // raw SF2 bytes (keeps buffer alive for tsf)
        tsf*                 synth = nullptr;
    };
    LiveSf liveSfPerStyle[6]; // 6 == kStyleCount (declared later in the class)

    // External SF2 loaded by the user ("Charger SF2...").
    // Bytes bruts du fichier original — le delta est appliqué à une copie avant chargement.
    std::vector<uint8_t> externalSf2PerStyle[6];
    std::wstring         externalSf2PathPerStyle[6]; // chemin affiché dans l'UI
    sfed::Sf2Delta       sf2DeltaPerStyle[6];        // delta par style
    sfed::Sf2GenSnapshot sf2BaseGensPerStyle[6];     // valeurs absolues du preset chargé (pour labels)
    uint16_t sf2SelBankPerStyle[6]    = {};          // banque MIDI du preset sélectionné dans le combo
    uint16_t sf2SelProgPerStyle[6]    = {};          // programme MIDI du preset sélectionné dans le combo
    // Liste de presets extraite de TSF après chaque reloadExternalSf.
    // Accessible depuis l'UI thread sans lock (reloadExternalSf n'est jamais appelé depuis l'audio).
    // Garanti cohérent avec ce que TSF joue réellement, contrairement à sfed::listPresets.
    std::vector<sfed::Sf2Preset> tsfPresetsPerStyle[6];

    // Recharge le synth depuis un SF2 externe (déjà chargé dans externalSf2PerStyle[style])
    // en appliquant le delta courant. Retourne true en cas de succès.
    bool reloadExternalSf(int style) {
        if (style < 0 || style >= 6) return false;
        if (externalSf2PerStyle[style].empty()) return false;
        std::vector<uint8_t> bytes = externalSf2PerStyle[style];
        uint16_t bank = sf2SelBankPerStyle[style];
        uint16_t prog = sf2SelProgPerStyle[style];
        // N'applique le patch SF2 que si au moins un paramètre delta est non-nul.
        // Quand delta = 0, on charge le fichier original tel quel — plus rapide et
        // évite tout risque de corruption pour un simple changement de preset.
        if (!sfed::sf2DeltaIsZero(sf2DeltaPerStyle[style])) {
            bool patched = sfed::applyDeltaTargeted(bytes, sf2DeltaPerStyle[style], bank, prog);
            if (!patched) {
                // Fallback: patch tous les générateurs (edge-case)
                sfed::applyDelta(bytes, sf2DeltaPerStyle[style]);
            }
        }
        tsf* newSynth = tsf_load_memory(bytes.data(), (int)bytes.size());
        if (!newSynth) return false;
        tsf_set_output(newSynth, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
        tsf_set_max_voices(newSynth, 48);
        int ch = kSfChannelOf(style);
        // tsf_channel_set_bank_preset gère correctement n'importe quelle banque MIDI.
        // Pour les percussions (bank 128), TSF utilise la banque GM drums.
        if (!tsf_channel_set_bank_preset(newSynth, ch, (bank == 128 ? 128 : bank), prog)) {
            // Si le preset n'est pas trouvé dans cette banque, repli sur prog/bank 0
            tsf_channel_set_presetnumber(newSynth, ch, prog, bank == 128 ? 1 : 0);
        }
        tsf_channel_set_volume(newSynth, ch, sfVolumePerStyle[style]);
        tsf* oldSynth = nullptr;
        {
            std::lock_guard<std::mutex> lk(sfMutex);
            if (sfSynth[style]) tsf_channel_sounds_off_all(sfSynth[style], ch);
            oldSynth = liveSfPerStyle[style].synth;
            liveSfPerStyle[style].data  = std::move(bytes);
            liveSfPerStyle[style].synth = newSynth;
            sfSynth[style] = newSynth;
        }
        if (oldSynth) tsf_close(oldSynth);
        // Snapshot de la liste de presets TSF : index stable = même ordre que le combo UI.
        // Utilise l'API publique (tsf_get_presetbank / tsf_get_presetprog) pour ne pas
        // dépendre de la définition complète de struct tsf dans les TU sans TSF_IMPLEMENTATION.
        tsfPresetsPerStyle[style].clear();
        int pCount = tsf_get_presetcount(newSynth);
        for (int i = 0; i < pCount; ++i) {
            sfed::Sf2Preset p{};
            const char* nm = tsf_get_presetname(newSynth, i);
            if (nm) { std::strncpy(p.name, nm, 20); p.name[20] = '\0'; }
            p.bank    = (uint16_t)tsf_get_presetbank(newSynth, i);
            p.program = (uint16_t)tsf_get_presetprog(newSynth, i);
            tsfPresetsPerStyle[style].push_back(p);
        }
        return true;
    }
    // Safe to call from the UI thread. Acquires sfMutex only for the pointer swap.
    // Returns true on success.
    bool reloadLiveSf(int style, const sfm::SfmConfig& cfg) {
        if (style < 0 || style >= 6) return false;
        std::vector<uint8_t> bytes = sfm::SfmGenerator::generateToMemory(cfg);
        if (bytes.empty()) return false;

        tsf* newSynth = tsf_load_memory(bytes.data(), (int)bytes.size());
        if (!newSynth) return false;
        tsf_set_output(newSynth, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
        tsf_set_max_voices(newSynth, 24);
        int ch = kSfChannelOf(style);
        tsf_channel_set_presetnumber(newSynth, ch, 0, 0);
        tsf_channel_set_volume(newSynth, ch, sfVolumePerStyle[style]);

        // Swap under sfMutex — audio thread holds this only for render calls.
        tsf* oldSynth = nullptr;
        {
            std::lock_guard<std::mutex> lk(sfMutex);
            if (sfSynth[style]) tsf_channel_sounds_off_all(sfSynth[style], ch);
            oldSynth = liveSfPerStyle[style].synth;
            liveSfPerStyle[style].data  = std::move(bytes);
            liveSfPerStyle[style].synth = newSynth;
            sfSynth[style] = newSynth;
        }
        if (oldSynth) tsf_close(oldSynth);
        return true;
    }

    void applyFxParamsToChain(int s) {
        if (s < 0 || s >= 6) return;
        const FxParams& p = fxParamsPerStyle[s];
        fx::FxChain& c = fxChainPerStyle[s];
        c.chorus.enabled = p.chorusOn;
        c.chorus.setRate(p.chorusRate);
        c.chorus.setDepth(p.chorusDepth);
        c.chorus.setMix(p.chorusMix);
        c.del.enabled = p.delayOn;
        c.del.setTime(p.delayTime);
        c.del.setFeedback(p.delayFb);
        c.del.setMix(p.delayMix);
        c.reverb.enabled = p.reverbOn;
        c.reverb.setSize(p.reverbSize);
        c.reverb.setDamp(p.reverbDamp);
        c.reverb.setMix(p.reverbMix);
        c.noise.enabled = p.noiseOn;
        c.noise.setLevel(p.noiseLevel);
        c.noise.setFlutter(p.noiseFlutter);
        c.noise.setTone(p.noiseTone);
    }
    void applyAllFxParams() {
        for (int s = 0; s < 6; ++s) applyFxParamsToChain(s);
    }
    // Global arrangement section.
    std::atomic<int> currentSection{kSecVerse};
    float sectionVolumeRamp = 1.0f;  // audio-thread-only fader
    bool  introFadeDone = false;     // true once the Intro fade has completed once
    // When false (default): plugin renders audio via TSF only, no MIDI note
    // events are sent to outputEvents (prevents double-sound in FL Studio).
    // Set to true only if you want to drive an external synth via MIDI out.
    bool  midiNoteOut = false;
    // Per-style note delay in bars before instrument begins.
    int  startBarPerStyle[6] = { 0, 0, 0, 0, 0, 0 };

    // Per-style JAM mode: when true, the instrument follows other instruments
    // rather than leading. Style 0 (Mélodie): fires on perc grid steps, uses
    // arpège chord-tone pitches instead of corpus phrase degrees.
    bool jamPerStyle[6] = { false, false, false, false, false, false };

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

    // ---- Lock-free GUI -> audio note queue ----
    // GUI pushes note previews; audio thread drains and fires them.
    struct GuiNote { int16_t pitch; float vel; bool noteOff; int style; };
    static constexpr int kGuiQueueSize = 32;
    GuiNote  guiNoteRing[kGuiQueueSize]{};
    std::atomic<uint32_t> guiNoteWrite{0};
    std::atomic<uint32_t> guiNoteRead{0};
    void guiNoteOn(int pitch, float vel, int style) {
        uint32_t w = guiNoteWrite.load(std::memory_order_relaxed);
        uint32_t r = guiNoteRead .load(std::memory_order_acquire);
        if (w - r >= (uint32_t)kGuiQueueSize) return;
        guiNoteRing[w % kGuiQueueSize] = { (int16_t)pitch, vel, false, style };
        guiNoteWrite.store(w + 1, std::memory_order_release);
    }
    void guiNoteOff(int pitch, int style) {
        uint32_t w = guiNoteWrite.load(std::memory_order_relaxed);
        uint32_t r = guiNoteRead .load(std::memory_order_acquire);
        if (w - r >= (uint32_t)kGuiQueueSize) return;
        guiNoteRing[w % kGuiQueueSize] = { (int16_t)pitch, 0.f, true, style };
        guiNoteWrite.store(w + 1, std::memory_order_release);
    }
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
    // pinnedNotes: user-drawn notes (never cleared by transport restart).
    // Protected by recMtx for GUI<->audio exchange.
    std::vector<RecordedNote> pinnedNotes;
    // audioNotes: audio-thread-private copy of pinnedNotes (no lock needed).
    // Refreshed from pinnedNotes on each transport restart.
    std::vector<RecordedNote> audioNotes;
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

    // ---- Mute / Solo bitmasks ------------------------------------------
    // muteStyles: bit i set → style i is muted (silenced).
    // soloStyles: any bit set → only styles with their bit set are heard.
    // Solo takes priority over mute. Both are per-style, not per-trigger.
    std::atomic<uint32_t> muteStyles{0u};
    std::atomic<uint32_t> soloStyles{0u};
    bool isStyleMuted(int s) const {
        return (muteStyles.load(std::memory_order_relaxed) >> (s & 31)) & 1u;
    }
    bool isStyleSoloed(int s) const {
        return (soloStyles.load(std::memory_order_relaxed) >> (s & 31)) & 1u;
    }
    // Returns true if style s should produce audio this block.
    // Disabled (OFF) instruments are always silent, regardless of M/S state.
    bool isStyleAudible(int s) const {
        if (!isStyleEnabled(s)) return false;                  // OFF overrides everything
        uint32_t solo = soloStyles.load(std::memory_order_relaxed);
        if (solo != 0u) return (solo >> (s & 31)) & 1u;       // solo active: only solos
        return !isStyleMuted(s);                               // no solo: respect mute
    }
    void toggleMute(int s) {
        uint32_t cur = muteStyles.load(std::memory_order_relaxed);
        muteStyles.store(cur ^ (1u << (s & 31)), std::memory_order_relaxed);
    }
    void toggleSolo(int s) {
        uint32_t cur = soloStyles.load(std::memory_order_relaxed);
        uint32_t bit = 1u << (s & 31);
        // Solo is exclusive: toggling one clears all others.
        // If it was already the only solo, clear everything.
        soloStyles.store((cur == bit) ? 0u : bit, std::memory_order_relaxed);
    }

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
    tsf*       sfSynth[kStyleCount] = {};  // active TSF per style (alias into sfBank)
    tsf*       sfBank[2][kStyleCount] = {}; // [0]=GeneralUser, [1]=Midnight Lofi (owners)
    bool       sfReady      = false;
    std::mutex sfMutex;          // serialize tsf_note_on/off vs render
    // Track per-noteId TSF style/channel so we can route note-off correctly.
    struct SfActive { int32 noteId; int16_t pitch; int8_t channel; int8_t styleIdx; bool used; };
    static constexpr int kSfMaxActive = 64;
    SfActive sfActive[kSfMaxActive] = {};

    // -------------------------------------------------------------------------
    // User-created SoundFonts (procedural, generated by SoundFont Maker tab).
    // Each entry owns one TSF synth (the SF2 file lives on disk under
    // %APPDATA%/Midnight/SoundFonts/).  Phase 1: each user SF targets a single
    // style (default: Piano).  When selected as the active preset for a style,
    // sfSynth[style] is aliased to userSfs[i].synth.
    // -------------------------------------------------------------------------
    struct UserSfEntry {
        sfm::SfmConfig cfg;
        std::wstring   sf2Path;        // full path to .sf2 on disk
        std::wstring   nameWide;       // display name (owns the wchar buffer)
        int            targetStyle;    // 0..5  (the style whose dropdown lists it)
        tsf*           synth;          // independent TSF instance (no copy needed)
    };
    std::vector<UserSfEntry> userSfs;
    // Per-style runtime presets, appended after kSfPresetsByStyle[style][0..N-1].
    // Each entry's `name` points into the matching userSfs[i].nameWide buffer.
    std::vector<SfPreset>    extraPresetsByStyle[kStyleCount];

    // Effective dropdown size = static catalog + user-injected entries.
    int effectivePresetCount(int style) const {
        if (style < 0 || style >= kStyleCount) return 0;
        return kSfPresetCount[style] + (int)extraPresetsByStyle[style].size();
    }
    SfPreset effectivePreset(int style, int idx) const {
        if (style < 0 || style >= kStyleCount) return {};
        int staticN = kSfPresetCount[style];
        if (idx < staticN) return kSfPresetsByStyle[style][idx];
        int j = idx - staticN;
        if (j < (int)extraPresetsByStyle[style].size())
            return extraPresetsByStyle[style][j];
        return {};
    }
    // Map a style+presetIdx onto the matching UserSfEntry, or nullptr.
    UserSfEntry* findUserSfFor(int style, int idx) {
        int staticN = kSfPresetCount[style];
        if (idx < staticN) return nullptr;
        int j = idx - staticN;
        if (j < 0 || j >= (int)extraPresetsByStyle[style].size()) return nullptr;
        // Linear scan: fast enough since user typically has <10 SFs.
        const SfPreset& target = extraPresetsByStyle[style][j];
        for (auto& u : userSfs) {
            if (u.targetStyle == style && u.nameWide.c_str() == target.name)
                return &u;
        }
        return nullptr;
    }


    // GM program per style (channel 9 reserved for drums).
    // 0 Piano (MÃ©lodie), 46 Harp, 33 E-Bass, drums, 12 Marimba, 0 Piano (chord)
    static constexpr int kSfChannelOf(int style) {
        // Each style gets its own TSF instance; use ch 0 within it, except drums (ch 9).
        return style == 3 ? 9 : 0;
    }

    void loadSoundFont() {
        if (sfReady && sfSynth[0]) {
            for (int s = 0; s < 6; ++s)
                if (sfSynth[s]) tsf_set_output(sfSynth[s], TSF_STEREO_INTERLEAVED, (int)sampleRate, 0.0f);
            return;
        }
        // Locate the bundled SF2s:
        //   <bundle>/Contents/Resources/GeneralUser.sf2          (mandatory)
        //   <bundle>/Contents/Resources/Midnight/midnight_lofi.sf2 (optional)
        wchar_t modPath[MAX_PATH] = {};
        if (!GetModuleFileNameW(g_hInst, modPath, MAX_PATH)) return;
        for (int i = (int)wcslen(modPath) - 1; i >= 0; --i) {
            if (modPath[i] == L'\\' || modPath[i] == L'/') { modPath[i] = 0; break; }
        }
        std::wstring resDir = std::wstring(modPath) + L"\\..\\Resources";

        auto loadBank = [&](int bankIdx, const std::wstring& sfPath) -> bool {
            char sfPath8[MAX_PATH * 4] = {};
            WideCharToMultiByte(CP_ACP, 0, sfPath.c_str(), -1,
                                sfPath8, sizeof(sfPath8), nullptr, nullptr);
            tsf* base = tsf_load_filename(sfPath8);
            if (!base) return false;
            tsf_set_output(base, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
            tsf_set_max_voices(base, 32);
            sfBank[bankIdx][0] = base;
            for (int s = 1; s < kStyleCount; ++s) {
                tsf* c = tsf_copy(base);
                if (!c) return false;
                tsf_set_output(c, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
                tsf_set_max_voices(c, 32);
                sfBank[bankIdx][s] = c;
            }
            return true;
        };

        // GeneralUser is required.
        if (!loadBank(0, resDir + L"\\GeneralUser.sf2")) {
            closeSoundFont();
            return;
        }
        // Midnight Lofi is optional â€“ keep going if missing.
        loadBank(1, resDir + L"\\Midnight\\midnight_lofi.sf2");

        // Bind active synth per style based on each style's currently selected preset.
        for (int s = 0; s < kStyleCount; ++s) {
            int idx = std::clamp(sfPresetIdxPerStyle[s], 0, kSfPresetCount[s] - 1);
            const SfPreset& p = kSfPresetsByStyle[s][idx];
            int src = (p.source == 1 && sfBank[1][s]) ? 1 : 0;
            sfSynth[s] = sfBank[src][s];
            int ch = kSfChannelOf(s);
            tsf_channel_set_presetnumber(sfSynth[s], ch, p.program, p.bank == 128 ? 1 : 0);
            tsf_channel_set_volume(sfSynth[s], ch, sfVolumePerStyle[s]);
        }
        sfReady = true;
        MM_LOG("loadSoundFont: sfReady=true");

        // ---- User-procedural SoundFonts (Phase 1) -------------------------
        // Create the default "Lofi Piano (Custom)" if the user folder is
        // empty, then scan the folder and register every found SF as a
        // runtime preset on its target style.
        seedDefaultUserSf();
        scanAndLoadUserSf();
        // Re-apply per-style bindings: a saved preset index might point into
        // the freshly-loaded user range now.
        for (int s = 0; s < kStyleCount; ++s) {
            int idx = std::clamp(sfPresetIdxPerStyle[s], 0, effectivePresetCount(s) - 1);
            sfPresetIdxPerStyle[s] = idx;
            applyPresetForStyle(s, idx);
        }
    }

    void closeSoundFont() {
        // Close copies first (1..N-1), then bases (index 0).
        for (int k = 0; k < 2; ++k) {
            for (int s = kStyleCount - 1; s >= 0; --s) {
                if (sfBank[k][s]) { tsf_close(sfBank[k][s]); sfBank[k][s] = nullptr; }
            }
        }
        // User SFs (each owns its own TSF).
        for (auto& u : userSfs) {
            if (u.synth) { tsf_close(u.synth); u.synth = nullptr; }
        }
        userSfs.clear();
        // Live preview SFs (in-memory buffers).
        for (int s = 0; s < kStyleCount; ++s) {
            if (liveSfPerStyle[s].synth) {
                tsf_close(liveSfPerStyle[s].synth);
                liveSfPerStyle[s].synth = nullptr;
            }
            liveSfPerStyle[s].data.clear();
        }
        for (int s = 0; s < kStyleCount; ++s) {
            extraPresetsByStyle[s].clear();
            sfSynth[s] = nullptr;
        }
        sfReady = false;
        MM_LOG("closeSoundFont: sfReady=false");
    }

    // -------------------------------------------------------------------------
    // User SoundFont folder management
    // Layout: %APPDATA%/Midnight/SoundFonts/<name>.sf2
    //         %APPDATA%/Midnight/SoundFonts/<name>.cfg  (binary SfmConfig)
    // -------------------------------------------------------------------------
    static std::wstring getUserSfFolderW() {
        wchar_t appData[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr,
                                    SHGFP_TYPE_CURRENT, appData))) {
            return L"";
        }
        std::wstring p = std::wstring(appData) + L"\\Midnight\\SoundFonts";
        // Create directories if missing (best-effort).
        std::wstring midnight = std::wstring(appData) + L"\\Midnight";
        CreateDirectoryW(midnight.c_str(), nullptr);
        CreateDirectoryW(p.c_str(),        nullptr);
        return p;
    }

    static std::string wideToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string s(n > 0 ? n - 1 : 0, '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
        return s;
    }
    static std::wstring utf8ToWide(const std::string& s) {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring w(n > 0 ? n - 1 : 0, L'\0');
        if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
        return w;
    }

    // Tiny binary format for SfmConfig (next to the .sf2):
    //   magic "SFMC" + version u32 + nameLen u32 + name bytes
    //   + synthType u32 + 6x float (a/d/s/r, modIndex, modDecay)
    //   + lowNote/highNote/step i32 + gain float + targetStyle i32
    static bool writeSfmCfg(const std::wstring& cfgPath,
                            const sfm::SfmConfig& cfg, int targetStyle) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, cfgPath.c_str(), L"wb") != 0 || !f) return false;
        auto wU32 = [&](uint32_t x){ fwrite(&x, 4, 1, f); };
        auto wF   = [&](float x)    { fwrite(&x, 4, 1, f); };
        auto wI32 = [&](int32_t x) { fwrite(&x, 4, 1, f); };
        const char magic[4] = {'S','F','M','C'};
        fwrite(magic, 1, 4, f);
        wU32(1);
        wU32((uint32_t)cfg.name.size());
        if (!cfg.name.empty()) fwrite(cfg.name.data(), 1, cfg.name.size(), f);
        wU32((uint32_t)cfg.synthType);
        wF(cfg.attack); wF(cfg.decay); wF(cfg.sustain); wF(cfg.release);
        wF(cfg.modIndex); wF(cfg.modDecay);
        wI32(cfg.lowNote); wI32(cfg.highNote); wI32(cfg.step);
        wF(cfg.gain);
        wI32(targetStyle);
        fclose(f);
        return true;
    }
    static bool readSfmCfg(const std::wstring& cfgPath,
                           sfm::SfmConfig& cfg, int& targetStyle) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, cfgPath.c_str(), L"rb") != 0 || !f) return false;
        auto rU32 = [&](uint32_t& x){ return fread(&x, 4, 1, f) == 1; };
        auto rF   = [&](float& x)    { return fread(&x, 4, 1, f) == 1; };
        auto rI32 = [&](int32_t& x) { return fread(&x, 4, 1, f) == 1; };
        char magic[4] = {};
        if (fread(magic, 1, 4, f) != 4 ||
            magic[0]!='S'||magic[1]!='F'||magic[2]!='M'||magic[3]!='C') {
            fclose(f); return false;
        }
        uint32_t ver = 0; if (!rU32(ver)) { fclose(f); return false; }
        uint32_t nlen = 0; if (!rU32(nlen)) { fclose(f); return false; }
        cfg.name.assign(nlen, '\0');
        if (nlen && fread(cfg.name.data(), 1, nlen, f) != nlen) { fclose(f); return false; }
        uint32_t st = 0; rU32(st); cfg.synthType = (sfm::SfmSynth)st;
        rF(cfg.attack); rF(cfg.decay); rF(cfg.sustain); rF(cfg.release);
        rF(cfg.modIndex); rF(cfg.modDecay);
        int32_t lo=0, hi=0, step=0; rI32(lo); rI32(hi); rI32(step);
        cfg.lowNote = lo; cfg.highNote = hi; cfg.step = step;
        rF(cfg.gain);
        int32_t ts = 5; rI32(ts); targetStyle = ts;
        fclose(f);
        (void)ver;
        return true;
    }

    // Generate (or regenerate) a user SF on disk. Updates registry+TSF.
    // Returns true on success.
    bool createOrUpdateUserSf(const sfm::SfmConfig& cfg, int targetStyle) {
        std::wstring folder = getUserSfFolderW();
        if (folder.empty()) return false;
        std::wstring nameW = utf8ToWide(cfg.name);
        if (nameW.empty()) return false;
        std::wstring sf2W = folder + L"\\" + nameW + L".sf2";
        std::wstring cfgW = folder + L"\\" + nameW + L".cfg";
        std::string  sf2A = wideToUtf8(sf2W);
        if (!sfm::SfmGenerator::generate(cfg, sf2A)) return false;
        writeSfmCfg(cfgW, cfg, targetStyle);
        // If already loaded, replace; else add.
        for (auto& u : userSfs) {
            if (u.sf2Path == sf2W) {
                if (u.synth) { tsf_close(u.synth); u.synth = nullptr; }
                u.cfg = cfg; u.targetStyle = std::clamp(targetStyle, 0, kStyleCount - 1);
                u.synth = tsf_load_filename(sf2A.c_str());
                if (u.synth) {
                    tsf_set_output(u.synth, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
                    tsf_set_max_voices(u.synth, 24);
                }
                rebuildExtraPresets();
                return true;
            }
        }
        UserSfEntry e;
        e.cfg = cfg;
        e.sf2Path = sf2W;
        e.nameWide = nameW;
        e.targetStyle = std::clamp(targetStyle, 0, kStyleCount - 1);
        e.synth = tsf_load_filename(sf2A.c_str());
        if (e.synth) {
            tsf_set_output(e.synth, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
            tsf_set_max_voices(e.synth, 24);
        }
        userSfs.push_back(std::move(e));
        rebuildExtraPresets();
        return true;
    }

    // Rebuild extraPresetsByStyle[] from current userSfs.
    // The SfPreset.name fields point into UserSfEntry::nameWide, which is owned
    // by the entry and lives as long as the userSfs vector slot.
    void rebuildExtraPresets() {
        for (int s = 0; s < kStyleCount; ++s) extraPresetsByStyle[s].clear();
        for (auto& u : userSfs) {
            int s = std::clamp(u.targetStyle, 0, kStyleCount - 1);
            SfPreset p;
            p.name    = u.nameWide.c_str();
            p.program = 0;
            p.bank    = 0;
            p.source  = 2; // 2 = user-generated
            extraPresetsByStyle[s].push_back(p);
        }
    }

    // First-run seed: if the user folder is empty, create one default SF
    // ("Lofi Piano") so the dropdown shows something out of the box.
    void seedDefaultUserSf() {
        std::wstring folder = getUserSfFolderW();
        if (folder.empty()) return;
        std::wstring probe = folder + L"\\*.sf2";
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(probe.c_str(), &fd);
        bool empty = (h == INVALID_HANDLE_VALUE);
        if (h != INVALID_HANDLE_VALUE) FindClose(h);
        if (!empty) return;
        sfm::SfmConfig cfg = sfm::makeDefaultLofiPiano();
        // Avoid filename clash with the bundled "Lofi Piano" preset.
        cfg.name = "Lofi Piano (Custom)";
        createOrUpdateUserSf(cfg, /*targetStyle=*/5); // Piano
    }

    // Scan the user folder, load each (.sf2 + .cfg) pair into userSfs.
    void scanAndLoadUserSf() {
        std::wstring folder = getUserSfFolderW();
        if (folder.empty()) return;
        std::wstring probe = folder + L"\\*.cfg";
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(probe.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring cfgPath = folder + L"\\" + fd.cFileName;
            sfm::SfmConfig cfg; int targetStyle = 5;
            if (!readSfmCfg(cfgPath, cfg, targetStyle)) continue;
            std::wstring nameW = utf8ToWide(cfg.name);
            std::wstring sf2W  = folder + L"\\" + nameW + L".sf2";
            // (Re)generate if the .sf2 is missing.
            if (GetFileAttributesW(sf2W.c_str()) == INVALID_FILE_ATTRIBUTES) {
                sfm::SfmGenerator::generate(cfg, wideToUtf8(sf2W));
            }
            UserSfEntry e;
            e.cfg = cfg;
            e.sf2Path = sf2W;
            e.nameWide = nameW;
            e.targetStyle = std::clamp(targetStyle, 0, kStyleCount - 1);
            std::string sf2A = wideToUtf8(sf2W);
            e.synth = tsf_load_filename(sf2A.c_str());
            if (e.synth) {
                tsf_set_output(e.synth, TSF_STEREO_INTERLEAVED, (int)sampleRate, -3.0f);
                tsf_set_max_voices(e.synth, 24);
            }
            userSfs.push_back(std::move(e));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        rebuildExtraPresets();
    }


    void sfNoteOn(int style, int16_t pitch, float vel, int32 noteId) {
        if (!sfReady || style < 0 || style >= kStyleCount || !sfSynth[style]) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        int ch = kSfChannelOf(style);
        tsf_channel_note_on(sfSynth[style], ch, pitch, vel);
        // Remember for matching note-off.
        for (auto& a : sfActive) {
            if (!a.used) {
                a.used = true; a.noteId = noteId;
                a.pitch = pitch; a.channel = (int8_t)ch;
                a.styleIdx = (int8_t)style;
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
                int st = std::clamp((int)a.styleIdx, 0, kStyleCount - 1);
                if (sfSynth[st]) tsf_channel_note_off(sfSynth[st], a.channel, a.pitch);
                a.used = false;
                return;
            }
        }
    }

    void sfAllNotesOff() {
        if (!sfReady) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        for (int s = 0; s < kStyleCount; ++s) {
            if (!sfSynth[s]) continue;
            for (int ch = 0; ch < 10; ++ch)
                tsf_channel_sounds_off_all(sfSynth[s], ch);
        }
        for (auto& a : sfActive) a.used = false;
    }

    // Apply a preset (index into the *effective* dropdown) to a style.
    // Internal: used by both loadSoundFont() and setSfPreset().
    void applyPresetForStyle(int style, int idx) {
        if (style < 0 || style >= kStyleCount) return;
        int n = effectivePresetCount(style);
        if (n <= 0) return;
        idx = std::clamp(idx, 0, n - 1);
        int ch = kSfChannelOf(style);
        SfPreset p = effectivePreset(style, idx);
        if (sfSynth[style]) tsf_channel_sounds_off_all(sfSynth[style], ch);
        if (p.source == 2) {
            // User SF: route to the entry's own TSF.
            UserSfEntry* u = findUserSfFor(style, idx);
            if (u && u->synth) {
                sfSynth[style] = u->synth;
                tsf_channel_set_presetnumber(sfSynth[style], ch, 0, 0);
                tsf_channel_set_volume(sfSynth[style], ch, sfVolumePerStyle[style]);
            }
        } else {
            int src = (p.source == 1 && sfBank[1][style]) ? 1 : 0;
            if (!sfBank[src][style]) src = 0;
            sfSynth[style] = sfBank[src][style];
            if (!sfSynth[style]) return;
            tsf_channel_set_presetnumber(sfSynth[style], ch, p.program, p.bank == 128 ? 1 : 0);
            tsf_channel_set_volume(sfSynth[style], ch, sfVolumePerStyle[style]);
        }
    }

    // Public-ish helpers used by the GUI to change preset / volume per style.
    void setSfPreset(int style, int idx) {
        if (style < 0 || style >= kStyleCount) return;
        int n = effectivePresetCount(style);
        if (n <= 0) return;
        idx = std::clamp(idx, 0, n - 1);
        sfPresetIdxPerStyle[style] = idx;
        if (!sfReady) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        applyPresetForStyle(style, idx);
    }

    void setSfVolume(int style, float v) {
        if (style < 0 || style >= kStyleCount) return;
        sfVolumePerStyle[style] = std::clamp(v, 0.0f, 1.5f);
        if (!sfReady || !sfSynth[style]) return;
        std::lock_guard<std::mutex> lk(sfMutex);
        tsf_channel_set_volume(sfSynth[style], kSfChannelOf(style),
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
        MM_LOG_EVERY(200, "renderVoices: numOutputs=%d nframes=%u", numOutputs, nframes);
        for (int bi = 0; bi < numOutputs; ++bi) {
            MM_LOG_EVERY(200, "  bus[%d]: numChannels=%d bufPtr=%p", bi, outBuses[bi].numChannels,
                (void*)(outBuses[bi].channelBuffers32 ? outBuses[bi].channelBuffers32[0] : nullptr));
        }
        AudioBusBuffers& bus = outBuses[0];
        if (!bus.channelBuffers32) return;
        float* L = bus.numChannels > 0 ? bus.channelBuffers32[0] : nullptr;
        float* R = bus.numChannels > 1 ? bus.channelBuffers32[1] : L;
        if (!L) return;

        // SoundFont path: render each style to its own output bus.
        if (sfReady) {
            {
                std::lock_guard<std::mutex> lk(sfMutex);
                thread_local std::vector<float> sfBuf;
                if (sfBuf.size() < nframes * 2) sfBuf.resize(nframes * 2);
                for (int st = 0; st < kStyleCount; ++st) {
                    if (!sfSynth[st]) continue;
                    int busIdx = (st < numOutputs) ? st : 0;
                    AudioBusBuffers& sBus = outBuses[busIdx];
                    if (!sBus.channelBuffers32) { MM_LOG_ONCE("bus[%d] channelBuffers32=null for style %d", busIdx, st); continue; }
                    float* sL = sBus.numChannels > 0 ? sBus.channelBuffers32[0] : nullptr;
                    float* sR = sBus.numChannels > 1 ? sBus.channelBuffers32[1] : sL;
                    if (!sL) continue;
                    tsf_render_float(sfSynth[st], sfBuf.data(), (int)nframes, 0);
                    // Per-style FX chain (chorus / delay / reverb / cassette noise).
                    fxChainPerStyle[st].processInterleaved(sfBuf.data(), (int)nframes);
                    for (uint32_t i = 0; i < nframes; ++i) {
                        sL[i] += sfBuf[2 * i];
                        if (sR != sL) sR[i] += sfBuf[2 * i + 1];
                    }
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
        case 2: { // Basse: suit la progression, ancre beat 1 (root), 5ème sur beat 3
            int bassOct    = std::max(2, oct - 1);
            int subdiv     = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar= subdiv * 4;
            int64_t bar    = slot / slotsPerBar;
            int posInBar   = (int)(((slot % slotsPerBar) + slotsPerBar) % slotsPerBar);
            int beatInBar  = posInBar / subdiv;
            int subInBeat  = posInBar % subdiv;
            int prog       = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int chordRoot  = mm::kProgressions[prog][((bar % mm::kProgressionLength)
                             + mm::kProgressionLength) % mm::kProgressionLength];
            // Root semitone of the current chord
            int rootSemi   = ivl[chordRoot % len];
            // Fifth of the current chord (4 scale degrees up)
            int fifthSemi  = ivl[(chordRoot + 4) % len];
            // Approach note: chromatic half-step below root (for beat 4 walk-up)
            int approachSemi = rootSemi - 1;
            int semi;
            if (subInBeat != 0) {
                // Off-beat subdivisions: stay on root
                semi = rootSemi;
            } else if (beatInBar == 0) {
                // Beat 1: always the chord root
                semi = rootSemi;
            } else if (beatInBar == 2) {
                // Beat 3: root 2/3 of the time, fifth 1/3
                uint32_t r = (uint32_t)(slot * 1592795u + (uint32_t)seed * 1234567u + 7u);
                mm::xorshift32(r);
                semi = ((r % 3) == 0) ? fifthSemi : rootSemi;
            } else if (beatInBar == 3) {
                // Beat 4 at high density: walk-up approach to next chord root
                uint32_t r = (uint32_t)(slot * 8675309u + (uint32_t)seed * 9876543u + 3u);
                mm::xorshift32(r);
                semi = ((r % 2) == 0) ? approachSemi : rootSemi;
            } else {
                // Beat 2: stay on root
                semi = rootSemi;
            }
            return (int16_t)std::clamp(key + semi + bassOct * 12, 0, 127);
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
        case 4: { // Marimba: chord-tone ostinato — bass/treble alternation (R/5/3/5)
            // Fires every slot; pattern step = beatInBar mod 4.
            // Creates the classic vibraphone/marimba low-high alternation.
            int subdiv2     = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar2= subdiv2 * 4;
            int slotInBar2  = (int)(((slot % slotsPerBar2) + slotsPerBar2) % slotsPerBar2);
            int sPB2        = std::max(1, slotsPerBar2 / 4);
            int beatInBar2  = slotInBar2 / sPB2;
            int64_t bar2    = slot / slotsPerBar2;
            int prog2       = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int chordRoot2  = mm::kProgressions[prog2][((bar2 % mm::kProgressionLength)
                              + mm::kProgressionLength) % mm::kProgressionLength];
            // 4-step ostinato: Root(low) / 5th(high) / 3rd(low) / 5th(high)
            static constexpr int8_t mDeg[4]  = { 0, 4, 2, 4 };
            static constexpr int8_t mOct[4]  = { 0, 1, 0, 1 };
            int mStep = beatInBar2 & 3;
            int mSemi = ivl[(chordRoot2 + (int)mDeg[mStep]) % len];
            int mO    = std::clamp(oct + (int)mOct[mStep], 3, 7);
            return (int16_t)std::clamp(key + mSemi + mO * 12, 0, 127);
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
                               bool jam, int jamPercRhythm,
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

        // ---- Mélodie (style 0): phrase mélodique depuis le corpus ichigos.
        //      Les notes se déclenchent aux durées réelles de la phrase (dur_16ths).
        //      Plus de rigidité beat-par-beat : une double-croche est une double-croche
        //      quel que soit le subdiv choisi.
        //
        //      Mode JAM : quand actif, le rythme est calé sur la grille de la
        //      percussion, et la hauteur suit le contour de l'arpège (style 1).
        if (style == 0) {
            // ---- JAM mode: lock rhythm to perc grid, pitch to arpège --------
            if (jam) {
                int subdiv      = mm::normalized_subdiv(params[mm::kParamSubdiv]);
                int slotsPerBar = subdiv * 4;
                int slotInBar   = (int)(((slot % slotsPerBar) + slotsPerBar) % slotsPerBar);
                // Map slot to the 16-step drum grid
                int slotsPerStep = std::max(1, slotsPerBar / 16);
                if (slotInBar % slotsPerStep != 0) return;  // sub-step → skip
                int step = (slotInBar / slotsPerStep) % 16;
                // Fire only when the perc pattern has a hit on this step
                int percIdx = std::clamp(jamPercRhythm, 0, mm::kDrumPatternCount - 1);
                const auto& pat = mm::kDrumPatterns[percIdx];
                bool percHit = false;
                for (int h = 0; h < 4; ++h)
                    if (pat.steps[step][h].pitch != 0 && pat.steps[step][h].vel != 0)
                        { percHit = true; break; }
                if (!percHit) return;

                // Pitch: follow arpège (style 1) chord-tone cycling
                int key  = (int)std::round(params[mm::kParamKey]) % 12;
                int mode = std::clamp((int)std::round(params[mm::kParamMode]), 0, mm::kScaleCount - 1);
                int oct  = std::clamp((int)std::round(params[mm::kParamOctave]), 3, 6);
                int64_t bar   = slot / slotsPerBar;
                int prog = std::clamp(progression, 0, mm::kProgressionCount - 1);
                int chordRoot = mm::kProgressions[prog][((bar % mm::kProgressionLength)
                                + mm::kProgressionLength) % mm::kProgressionLength];
                const int* ivl = mm::kScaleIntervals[mode];
                const int  len  = mm::kScaleLengths[mode];
                int t0 = ivl[(chordRoot + 0) % len];
                int t1 = ivl[(chordRoot + 2) % len];
                int t2 = ivl[(chordRoot + 4) % len];
                // Same 14-step sawtooth as Arpège, one octave higher for voice clarity
                static constexpr int kASteps = 14;
                int pos = (int)(((slot % kASteps) + kASteps) % kASteps);
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
                    default: tone = t2; octShift = -1; break;
                }
                int voiceOct = std::clamp(oct + 1 + octShift, 3, 7);
                int pitch = std::clamp(key + tone + voiceOct * 12, 36, 96);
                // Velocity mirrors perc accent: kick/strong → forte
                float vel = (pat.steps[step][0].vel > 90) ? 0.85f
                          : (pat.steps[step][0].vel > 60) ? 0.70f : 0.58f;
                out.pitches[0] = (int16_t)pitch;
                out.vels[0]    = vel;
                out.count      = 1;
                return;
            }
            int subdiv      = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar = subdiv * 4;

            int key    = (int)std::round(params[mm::kParamKey]) % 12;
            int mode   = std::clamp((int)std::round(params[mm::kParamMode]), 0, mm::kScaleCount - 1);
            int oct    = std::clamp((int)std::round(params[mm::kParamOctave]), 3, 6);
            int prog   = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int seed   = (int)std::round(params[mm::kParamSeed]);
            int64_t bar = slot / slotsPerBar;

            // Mode tag : 0=majeur, 1=mineur
            static constexpr int kMajorScales[] = {0, 3, 5, 12, 13, 20, 22, -1};
            int mode_tag = 1;
            for (int mi = 0; kMajorScales[mi] >= 0; ++mi)
                if (mode == kMajorScales[mi]) { mode_tag = 0; break; }

            // Archétype de progression (L1 sur les 4 premiers degrés)
            uint8_t prog4[4];
            for (int bi = 0; bi < 4; ++bi)
                prog4[bi] = (uint8_t)mm::kProgressions[prog][bi];
            int prog_arch = phrases::prog_to_arch(prog4);

            // Sélection de la phrase : change tous les 2 bars
            int64_t cycle = bar >> 1;
            int bar_seed  = (int)(seed ^ (int)(cycle * 2654435761u));
            bar_seed      = ((bar_seed % 50) + 50) % 50;

            const phrases::Phrase& ph = phrases::get(bar_seed, mode_tag, prog_arch);
            int n = ph.n_notes;
            if (n == 0) return;

            // Position de ce slot dans le cycle de 2 bars, en 32èmes de note.
            // Chaque slot couvre la plage [slotStart32, slotEnd32).
            int64_t cycleStartSlot = cycle * 2 * slotsPerBar;
            int64_t slotInCycle    = slot - cycleStartSlot;
            int64_t slotStart32    = slotInCycle * 8 / subdiv;
            int64_t slotEnd32      = (slotInCycle + 1) * 8 / subdiv;

            // Contexte harmonique : accord de la barre courante
            int chordRoot = mm::kProgressions[prog][((bar % mm::kProgressionLength)
                            + mm::kProgressionLength) % mm::kProgressionLength];
            const int* ivl   = mm::kScaleIntervals[mode];
            const int  len   = mm::kScaleLengths[mode];
            int voiceOct     = std::clamp(oct + 1, 4, 7);

            // Parcourt les notes de la phrase pour trouver celle qui démarre
            // dans la plage de ce slot. Aucun filtrage beat-by-beat.
            int64_t cumStart32 = 0;
            for (int ni = 0; ni < n; ++ni) {
                if (cumStart32 >= slotEnd32) break;          // déjà après ce slot
                int64_t noteDur32 = (int64_t)ph.notes[ni].dur_16ths * 2;
                if (cumStart32 >= slotStart32) {
                    // Cette note démarre dans la fenêtre du slot courant → on joue
                    int degOff = (int)ph.notes[ni].degree;
                    int semi   = ivl[(chordRoot + degOff) % len];
                    int pitch  = std::clamp(key + semi + voiceOct * 12, 36, 96);

                    // Accent : 1ère note de la phrase forte, temps forts moyens,
                    // subdivisions légères (basé sur la position en 32èmes).
                    float vel;
                    if (ni == 0)                       vel = 0.85f;  // attaque de phrase
                    else if (cumStart32 % 16 == 0)     vel = 0.76f;  // sur un temps (noire)
                    else if (cumStart32 %  8 == 0)     vel = 0.70f;  // sur une croche
                    else                               vel = 0.60f;  // subdivision libre
                    if (ph.weight >= 3) vel = std::min(1.0f, vel * 1.05f); // bonus corpus

                    out.pitches[0] = (int16_t)pitch;
                    out.vels[0]    = vel;
                    out.count      = 1;
                    return;  // une seule note par slot
                }
                cumStart32 += noteDur32;
            }
            return;
        }

        // ---- Contre (style 4): counter-melody — complementary lead voice.
        //      Fires every beat (sub==0), uses chord tones that complement
        //      Voix. Sits at oct (one below Voix's oct+1). Phrase contours
        //      are the "mirror" of Voix shapes, offset by 1 bar in phrasing.
        if (style == 4) {
            int subdiv4       = mm::normalized_subdiv(params[mm::kParamSubdiv]);
            int slotsPerBar4  = subdiv4 * 4;
            int slotInBar4    = (int)(((slot % slotsPerBar4) + slotsPerBar4) % slotsPerBar4);
            int sPB4          = std::max(1, slotsPerBar4 / 4);
            int beat4         = slotInBar4 / sPB4;
            int sub4          = slotInBar4 % sPB4;
            if (sub4 != 0) return; // only on beats

            int key4   = (int)std::round(params[mm::kParamKey]) % 12;
            int mode4  = std::clamp((int)std::round(params[mm::kParamMode]), 0, mm::kScaleCount - 1);
            int oct4   = std::clamp((int)std::round(params[mm::kParamOctave]), 3, 6);
            int prog4  = std::clamp(progression, 0, mm::kProgressionCount - 1);
            int seed4  = (int)std::round(params[mm::kParamSeed]);
            int64_t bar4 = slot / slotsPerBar4;
            int cRoot4 = mm::kProgressions[prog4][((bar4 % mm::kProgressionLength)
                          + mm::kProgressionLength) % mm::kProgressionLength];
            const int* ivl4 = mm::kScaleIntervals[mode4];
            const int  len4  = mm::kScaleLengths[mode4];

            // Six mirror-contours (complementary to Voix's 6 shapes).
            // Voix:   arch(0 2 4 2), descent(4 2 0 1), rise(0 1 2 4),
            //         inv-arch(2 4 2 0), pendulum(0 4 2 4), wave(2 4 5 4)
            // Counter: moves in contrary motion, using the "other" chord tones.
            static constexpr int8_t cc[6][4] = {
                { 4,  2,  0,  2 },  // mirror arch     (5  3  R  3)
                { 0,  2,  4,  5 },  // mirror descent  (R  3  5  6)
                { 5,  4,  2,  0 },  // mirror rise     (6  5  3  R)
                { 4,  2,  4,  6 },  // mirror inv-arch (5  3  5  7)
                { 2,  0,  4,  0 },  // mirror pendulum (3  R  5  R)
                { 4,  2,  1,  0 },  // mirror wave     (5  3  2  R)
            };
            // Phrase changes every 2 bars, offset 1 bar from Voix
            uint32_t r4 = (uint32_t)(((bar4 + 1) >> 1) * 2654435761u)
                        ^ (uint32_t)((seed4 + 37) * 374761393u);
            if (!r4) r4 = 1;
            mm::xorshift32(r4);
            int ci4    = (int)(mm::xorshift32(r4) % 6);
            int deg4   = (int)cc[ci4][beat4 & 3];
            int semi4  = ivl4[(cRoot4 + deg4) % len4];
            // Sit at oct (Voix is at oct+1) — inner voice below the lead
            int cOct4  = std::clamp(oct4, 3, 6);
            int pitch4 = std::clamp(key4 + semi4 + cOct4 * 12, 36, 96);
            // Slightly softer than Voix lead
            float vel4 = (beat4 == 0) ? 0.76f : (beat4 == 2) ? 0.68f : 0.54f;
            out.pitches[0] = (int16_t)pitch4;
            out.vels[0]    = vel4;
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
        case 0: return 0.75 + fracParam * 0.75;                  // Voix: 0.75→1.5 beats (legato réglable)
        case 2: return slotBeats * (0.6 + fracParam * 2.4);      // Basse: 0.6→3.0 slots
        case 3: return std::min(slotBeats * 0.12, 0.05);         // Percus: toujours court
        case 4: return 0.65 + fracParam * 0.85;                  // Contre: 0.65→1.5 beats (legato réglable)
        case 5: return slotBeats * lenMult;                      // Piano arpeggio
        default: return slotBeats * lenMult;                     // Harpe / Melodie
        }
    }

    // Ring buffer for GUI display (audio thread writes, GUI thread reads)
    static constexpr int kNoteHistSize = 8;
    std::atomic<uint32_t> noteHistHead{0};
    int16_t noteHist[kNoteHistSize]{};
};
