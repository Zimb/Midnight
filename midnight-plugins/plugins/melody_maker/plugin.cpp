// =============================================================================
// MidnightMelodyMaker - CLAP Note Generator plugin
//
// Generates notes algorithmically on beat subdivisions, in a chosen scale.
// This is a pure C++17 implementation using only the CLAP headers (no JUCE,
// no DPF). It compiles into a single .clap shared library.
//
// Parameters
//   - Key       : 0..11   (C, C#, D, ...)
//   - Mode      : 0..3    (major, minor, dorian, mixolydian)
//   - Octave    : 3..6
//   - Subdiv    : 1, 2, 4, 8  (notes per beat)
//   - Density   : 0..1    (probability that a subdivision actually fires)
//   - Seed      : 0..999  (deterministic randomness)
//   - NoteLen   : 0..1    (fraction of subdivision length the note holds)
//
// Notes are emitted as CLAP note events on the single output note port. The
// host (Zrythm, Bitwig, Reaper, FL Studio, etc.) routes them to the next
// instrument in the chain.
// =============================================================================

#include <clap/clap.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace mm {

// -----------------------------------------------------------------------------
// Theory
// -----------------------------------------------------------------------------

// Major / natural minor / dorian / mixolydian intervals (in semitones).
static constexpr int kScaleIntervals[4][7] = {
    {0, 2, 4, 5, 7, 9, 11},  // major
    {0, 2, 3, 5, 7, 8, 10},  // minor
    {0, 2, 3, 5, 7, 9, 10},  // dorian
    {0, 2, 4, 5, 7, 9, 10},  // mixolydian
};

static const char* kModeNames[4] = {"Major", "Minor", "Dorian", "Mixolydian"};
static const char* kKeyNames[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B"};

// Cheap deterministic PRNG (xorshift32).
static uint32_t xorshift32(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// -----------------------------------------------------------------------------
// Parameter ids
// -----------------------------------------------------------------------------

enum ParamId : clap_id {
    kParamKey = 0,
    kParamMode,
    kParamOctave,
    kParamSubdiv,
    kParamDensity,
    kParamSeed,
    kParamNoteLen,
    kParamCount,
};

struct ParamDef {
    const char* name;
    double min_value;
    double max_value;
    double default_value;
    bool is_stepped;
};

static const ParamDef kParamDefs[kParamCount] = {
    {"Key",       0.0,  11.0,    0.0, true},   // 0..11
    {"Mode",      0.0,   3.0,    0.0, true},   // 0..3
    {"Octave",    3.0,   6.0,    4.0, true},   // 3..6
    {"Subdiv",    1.0,   8.0,    2.0, true},   // 1, 2, 4, 8 (rounded to nearest)
    {"Density",   0.0,   1.0,    0.8, false},  // probability per slot
    {"Seed",      0.0, 999.0,    7.0, true},   // any int
    {"NoteLen",   0.05,  1.0,   0.85, false},  // fraction of slot
};

// -----------------------------------------------------------------------------
// Plugin descriptor
// -----------------------------------------------------------------------------

static const char* const kFeatures[] = {
    CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
    CLAP_PLUGIN_FEATURE_UTILITY,
    nullptr,
};

static const clap_plugin_descriptor kDescriptor = {
    CLAP_VERSION_INIT,
    "com.midnight.melody-maker",            // id
    "Midnight Melody Maker",                // name
    "Midnight",                             // vendor
    "https://github.com/midnight",          // url
    "",                                     // manual_url
    "",                                     // support_url
    "0.1.0",                                // version
    "Algorithmic note generator (scale, "
    "subdivision, density).",               // description
    kFeatures,                              // features
};

// -----------------------------------------------------------------------------
// Plugin instance
// -----------------------------------------------------------------------------

struct Plugin {
    clap_plugin_t plugin{};
    const clap_host_t* host = nullptr;

    double sample_rate = 48000.0;

    // Parameters (atomic so audio + main thread can both touch them).
    std::atomic<double> params[kParamCount];

    // Active notes pending NOTE_OFF (sample-frame countdown within the song).
    struct ActiveNote {
        int16_t key;
        int16_t channel;
        int32_t note_id;
        int64_t off_sample;  // absolute sample position
    };
    std::vector<ActiveNote> active_notes;

    int64_t sample_pos = 0;       // running sample counter when no transport
    double  beat_pos   = 0.0;     // current song position in beats
    double  tempo_bpm  = 120.0;   // last known tempo
    bool    rolling    = true;    // transport rolling state

    // Track the last subdivision index we have already emitted, so we don't
    // emit the same slot twice when blocks straddle boundaries.
    int64_t last_emitted_slot = -1;
    int32_t next_note_id = 1;

    Plugin() {
        for (int i = 0; i < kParamCount; ++i) {
            params[i].store(kParamDefs[i].default_value);
        }
    }

    double param(ParamId id) const { return params[id].load(); }
};

static inline Plugin* self(const clap_plugin_t* p) {
    return static_cast<Plugin*>(p->plugin_data);
}

// Round subdivision parameter to the closest allowed value (1, 2, 4, 8).
static int normalized_subdiv(double v) {
    static const int allowed[] = {1, 2, 4, 8};
    int best = 2;
    double best_d = 1e9;
    for (int a : allowed) {
        double d = std::fabs(v - a);
        if (d < best_d) { best_d = d; best = a; }
    }
    return best;
}

// Pick a pitch for slot index `slot` deterministically from seed.
static int16_t choose_pitch(const Plugin* p, int64_t slot) {
    int key   = static_cast<int>(std::round(p->param(kParamKey))) % 12;
    int mode  = std::clamp(static_cast<int>(std::round(p->param(kParamMode))), 0, 3);
    int oct   = std::clamp(static_cast<int>(std::round(p->param(kParamOctave))), 0, 9);
    int seed  = static_cast<int>(std::round(p->param(kParamSeed)));

    uint32_t s = static_cast<uint32_t>(slot * 2654435761u
                                       + static_cast<uint32_t>(seed) * 97u
                                       + 1u);
    if (s == 0) s = 1;
    uint32_t r = xorshift32(s);

    // Choose a scale degree. Bias toward chord tones (1, 3, 5) on strong slots.
    static const int strong_pool[] = {0, 2, 4, 0, 4, 2};       // root/3rd/5th
    static const int weak_pool[]   = {1, 3, 5, 6, 0, 2, 4};

    int subdiv = normalized_subdiv(p->param(kParamSubdiv));
    bool is_strong = (slot % subdiv) == 0;
    int idx = is_strong
        ? strong_pool[r % (sizeof(strong_pool) / sizeof(int))]
        : weak_pool[r % (sizeof(weak_pool) / sizeof(int))];

    // Optional small octave wobble.
    int oct_off = (xorshift32(s) % 5);
    static const int oct_choices[] = {0, 0, 0, 1, -1};
    oct_off = oct_choices[oct_off];

    int semitone = kScaleIntervals[mode][idx] + key + (oct + oct_off) * 12;
    return static_cast<int16_t>(std::clamp(semitone, 0, 127));
}

// -----------------------------------------------------------------------------
// CLAP extensions: note ports
// -----------------------------------------------------------------------------

static uint32_t note_ports_count(const clap_plugin_t* /*p*/, bool is_input) {
    return is_input ? 0 : 1;
}

static bool note_ports_get(const clap_plugin_t* /*p*/, uint32_t index,
                           bool is_input, clap_note_port_info_t* info) {
    if (is_input || index != 0) return false;
    info->id = 0;
    std::snprintf(info->name, sizeof(info->name), "Notes Out");
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect  = CLAP_NOTE_DIALECT_CLAP;
    return true;
}

static const clap_plugin_note_ports_t kNotePortsExt = {
    note_ports_count,
    note_ports_get,
};

// -----------------------------------------------------------------------------
// CLAP extensions: audio ports (none, but host expects the extension)
// -----------------------------------------------------------------------------

static uint32_t audio_ports_count(const clap_plugin_t* /*p*/, bool /*is_input*/) {
    return 0;
}

static bool audio_ports_get(const clap_plugin_t* /*p*/, uint32_t /*index*/,
                            bool /*is_input*/, clap_audio_port_info_t* /*info*/) {
    return false;
}

static const clap_plugin_audio_ports_t kAudioPortsExt = {
    audio_ports_count,
    audio_ports_get,
};

// -----------------------------------------------------------------------------
// CLAP extensions: params
// -----------------------------------------------------------------------------

static uint32_t params_count(const clap_plugin_t* /*p*/) {
    return static_cast<uint32_t>(kParamCount);
}

static bool params_get_info(const clap_plugin_t* /*p*/, uint32_t index,
                            clap_param_info_t* info) {
    if (index >= kParamCount) return false;
    const ParamDef& d = kParamDefs[index];
    std::memset(info, 0, sizeof(*info));
    info->id = static_cast<clap_id>(index);
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (d.is_stepped) info->flags |= CLAP_PARAM_IS_STEPPED;
    info->cookie = nullptr;
    std::snprintf(info->name, sizeof(info->name), "%s", d.name);
    info->module[0] = 0;
    info->min_value = d.min_value;
    info->max_value = d.max_value;
    info->default_value = d.default_value;
    return true;
}

static bool params_get_value(const clap_plugin_t* p, clap_id id, double* value) {
    if (id >= kParamCount) return false;
    *value = self(p)->params[id].load();
    return true;
}

static bool params_value_to_text(const clap_plugin_t* /*p*/, clap_id id,
                                 double value, char* buf, uint32_t size) {
    if (id >= kParamCount) return false;
    switch (id) {
        case kParamKey:
            std::snprintf(buf, size, "%s",
                          kKeyNames[std::clamp(int(std::round(value)), 0, 11)]);
            return true;
        case kParamMode:
            std::snprintf(buf, size, "%s",
                          kModeNames[std::clamp(int(std::round(value)), 0, 3)]);
            return true;
        case kParamOctave:
            std::snprintf(buf, size, "%d", int(std::round(value)));
            return true;
        case kParamSubdiv:
            std::snprintf(buf, size, "1/%d", normalized_subdiv(value));
            return true;
        case kParamDensity:
        case kParamNoteLen:
            std::snprintf(buf, size, "%.0f%%", value * 100.0);
            return true;
        case kParamSeed:
            std::snprintf(buf, size, "%d", int(std::round(value)));
            return true;
        default:
            std::snprintf(buf, size, "%.3f", value);
            return true;
    }
}

static bool params_text_to_value(const clap_plugin_t* /*p*/, clap_id /*id*/,
                                 const char* /*display*/, double* /*value*/) {
    return false;
}

static void apply_param_event(Plugin* p, const clap_event_param_value_t* ev) {
    if (ev->param_id >= kParamCount) return;
    p->params[ev->param_id].store(ev->value);
}

static void params_flush(const clap_plugin_t* p, const clap_input_events_t* in,
                         const clap_output_events_t* /*out*/) {
    Plugin* pl = self(p);
    const uint32_t n = in->size(in);
    for (uint32_t i = 0; i < n; ++i) {
        const clap_event_header_t* h = in->get(in, i);
        if (h->space_id == CLAP_CORE_EVENT_SPACE_ID
            && h->type == CLAP_EVENT_PARAM_VALUE) {
            apply_param_event(pl, reinterpret_cast<const clap_event_param_value_t*>(h));
        }
    }
}

static const clap_plugin_params_t kParamsExt = {
    params_count,
    params_get_info,
    params_get_value,
    params_value_to_text,
    params_text_to_value,
    params_flush,
};

// -----------------------------------------------------------------------------
// CLAP extensions: state (save/load all params)
// -----------------------------------------------------------------------------

static bool state_save(const clap_plugin_t* p, const clap_ostream_t* stream) {
    Plugin* pl = self(p);
    for (int i = 0; i < kParamCount; ++i) {
        double v = pl->params[i].load();
        const char* bytes = reinterpret_cast<const char*>(&v);
        size_t remaining = sizeof(double);
        while (remaining > 0) {
            int64_t written = stream->write(stream, bytes, remaining);
            if (written <= 0) return false;
            remaining -= static_cast<size_t>(written);
            bytes += written;
        }
    }
    return true;
}

static bool state_load(const clap_plugin_t* p, const clap_istream_t* stream) {
    Plugin* pl = self(p);
    for (int i = 0; i < kParamCount; ++i) {
        double v = 0.0;
        char* bytes = reinterpret_cast<char*>(&v);
        size_t remaining = sizeof(double);
        while (remaining > 0) {
            int64_t got = stream->read(stream, bytes, remaining);
            if (got <= 0) return false;
            remaining -= static_cast<size_t>(got);
            bytes += got;
        }
        pl->params[i].store(v);
    }
    return true;
}

static const clap_plugin_state_t kStateExt = {
    state_save,
    state_load,
};

// -----------------------------------------------------------------------------
// Note generation
// -----------------------------------------------------------------------------

static void emit_note_on(const clap_output_events_t* out, uint32_t time,
                         int16_t key, int16_t channel, int32_t note_id,
                         double velocity) {
    clap_event_note_t ev{};
    ev.header.size     = sizeof(ev);
    ev.header.time     = time;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type     = CLAP_EVENT_NOTE_ON;
    ev.header.flags    = 0;
    ev.note_id         = note_id;
    ev.port_index      = 0;
    ev.channel         = channel;
    ev.key             = key;
    ev.velocity        = velocity;
    out->try_push(out, &ev.header);
}

static void emit_note_off(const clap_output_events_t* out, uint32_t time,
                          int16_t key, int16_t channel, int32_t note_id) {
    clap_event_note_t ev{};
    ev.header.size     = sizeof(ev);
    ev.header.time     = time;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type     = CLAP_EVENT_NOTE_OFF;
    ev.header.flags    = 0;
    ev.note_id         = note_id;
    ev.port_index      = 0;
    ev.channel         = channel;
    ev.key             = key;
    ev.velocity        = 0.0;
    out->try_push(out, &ev.header);
}

static void flush_due_note_offs(Plugin* pl, const clap_output_events_t* out,
                                int64_t block_start, uint32_t nframes) {
    int64_t block_end = block_start + nframes;
    auto it = pl->active_notes.begin();
    while (it != pl->active_notes.end()) {
        if (it->off_sample <= block_end) {
            int64_t local = it->off_sample - block_start;
            if (local < 0) local = 0;
            if (local >= nframes) local = nframes - 1;
            emit_note_off(out, static_cast<uint32_t>(local),
                          it->key, it->channel, it->note_id);
            it = pl->active_notes.erase(it);
        } else {
            ++it;
        }
    }
}

static void all_notes_off(Plugin* pl, const clap_output_events_t* out) {
    for (const auto& n : pl->active_notes) {
        emit_note_off(out, 0, n.key, n.channel, n.note_id);
    }
    pl->active_notes.clear();
}

// -----------------------------------------------------------------------------
// CLAP plugin callbacks
// -----------------------------------------------------------------------------

static bool plugin_init(const clap_plugin_t* /*p*/) { return true; }

static void plugin_destroy(const clap_plugin_t* p) {
    delete self(p);
}

static bool plugin_activate(const clap_plugin_t* p, double sample_rate,
                            uint32_t /*min_frames*/, uint32_t /*max_frames*/) {
    Plugin* pl = self(p);
    pl->sample_rate = sample_rate;
    pl->sample_pos = 0;
    pl->beat_pos = 0.0;
    pl->last_emitted_slot = -1;
    pl->active_notes.clear();
    return true;
}

static void plugin_deactivate(const clap_plugin_t* /*p*/) {}

static bool plugin_start_processing(const clap_plugin_t* /*p*/) { return true; }
static void plugin_stop_processing(const clap_plugin_t* /*p*/) {}

static void plugin_reset(const clap_plugin_t* p) {
    Plugin* pl = self(p);
    pl->active_notes.clear();
    pl->last_emitted_slot = -1;
}

static clap_process_status plugin_process(const clap_plugin_t* p,
                                          const clap_process_t* process) {
    Plugin* pl = self(p);
    const uint32_t nframes = process->frames_count;
    const clap_input_events_t* in = process->in_events;
    const clap_output_events_t* out = process->out_events;

    // Apply incoming param/transport events first (sample-accurate not needed
    // for this simple generator).
    if (in) {
        const uint32_t nev = in->size(in);
        for (uint32_t i = 0; i < nev; ++i) {
            const clap_event_header_t* h = in->get(in, i);
            if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
            switch (h->type) {
                case CLAP_EVENT_PARAM_VALUE:
                    apply_param_event(
                        pl,
                        reinterpret_cast<const clap_event_param_value_t*>(h));
                    break;
                default:
                    break;
            }
        }
    }

    // Read transport.
    const clap_event_transport_t* tr = process->transport;
    if (tr) {
        if (tr->flags & CLAP_TRANSPORT_HAS_TEMPO) {
            pl->tempo_bpm = tr->tempo;
        }
        pl->rolling = (tr->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
        if (tr->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) {
            // song_pos_beats is fixed-point (CLAP_BEATTIME_FACTOR).
            double beats =
                static_cast<double>(tr->song_pos_beats)
                / static_cast<double>(CLAP_BEATTIME_FACTOR);
            pl->beat_pos = beats;
        }
    }

    int subdiv = normalized_subdiv(pl->param(kParamSubdiv));
    double density = std::clamp(pl->param(kParamDensity), 0.0, 1.0);
    double note_len_frac = std::clamp(pl->param(kParamNoteLen), 0.05, 1.0);
    int seed = static_cast<int>(std::round(pl->param(kParamSeed)));

    // Beats per second.
    double bps = std::max(0.05, pl->tempo_bpm / 60.0);
    double beats_per_sample = bps / pl->sample_rate;

    int64_t block_start_sample = pl->sample_pos;

    // First, schedule any pending NOTE_OFF that fall in this block.
    flush_due_note_offs(pl, out, block_start_sample, nframes);

    if (pl->rolling) {
        double block_start_beat = pl->beat_pos;
        double block_end_beat = block_start_beat + nframes * beats_per_sample;

        // Slot duration in beats:
        double slot_beats = 1.0 / static_cast<double>(subdiv);

        // First slot index >= block_start_beat.
        int64_t first_slot = static_cast<int64_t>(
            std::ceil(block_start_beat / slot_beats - 1e-9));
        if (first_slot <= pl->last_emitted_slot)
            first_slot = pl->last_emitted_slot + 1;

        for (int64_t slot = first_slot;; ++slot) {
            double slot_beat = slot * slot_beats;
            if (slot_beat >= block_end_beat) break;

            double offset_beats = slot_beat - block_start_beat;
            uint32_t time = static_cast<uint32_t>(
                std::floor(offset_beats / beats_per_sample));
            if (time >= nframes) break;

            // Density gate: deterministic pseudo-random per slot.
            uint32_t s = static_cast<uint32_t>(slot * 2246822519u
                                               + static_cast<uint32_t>(seed) * 374761393u
                                               + 1u);
            if (s == 0) s = 1;
            uint32_t r = xorshift32(s);
            double roll = static_cast<double>(r) / static_cast<double>(0xFFFFFFFFu);
            pl->last_emitted_slot = slot;
            if (roll > density) continue;

            int16_t key = choose_pitch(pl, slot);
            int16_t channel = 0;
            int32_t note_id = pl->next_note_id++;
            double velocity = 0.7 + 0.25 * (
                static_cast<double>(xorshift32(s)) /
                static_cast<double>(0xFFFFFFFFu));
            if (velocity > 1.0) velocity = 1.0;

            emit_note_on(out, time, key, channel, note_id, velocity);

            // Schedule NOTE_OFF.
            double note_beats = slot_beats * note_len_frac;
            int64_t off_abs = block_start_sample
                + static_cast<int64_t>(time)
                + static_cast<int64_t>(std::round(note_beats / beats_per_sample));
            // Avoid zero-length: at least 1 sample.
            if (off_abs <= block_start_sample + time) {
                off_abs = block_start_sample + time + 1;
            }

            // If the note off falls inside this block, emit it now.
            int64_t local_off = off_abs - block_start_sample;
            if (local_off < nframes) {
                emit_note_off(out, static_cast<uint32_t>(local_off),
                              key, channel, note_id);
            } else {
                pl->active_notes.push_back({key, channel, note_id, off_abs});
            }
        }

        pl->beat_pos = block_end_beat;
    } else {
        // Transport stopped: kill anything still playing once.
        if (!pl->active_notes.empty()) {
            all_notes_off(pl, out);
        }
        pl->last_emitted_slot = -1;
    }

    pl->sample_pos = block_start_sample + nframes;
    return CLAP_PROCESS_CONTINUE;
}

static const void* plugin_get_extension(const clap_plugin_t* /*p*/, const char* id) {
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0)  return &kNotePortsExt;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &kAudioPortsExt;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0)      return &kParamsExt;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0)       return &kStateExt;
    return nullptr;
}

static void plugin_on_main_thread(const clap_plugin_t* /*p*/) {}

// -----------------------------------------------------------------------------
// Factory
// -----------------------------------------------------------------------------

static const clap_plugin_t* create_plugin(const clap_plugin_factory_t* /*f*/,
                                          const clap_host_t* host,
                                          const char* plugin_id) {
    if (!plugin_id || std::strcmp(plugin_id, kDescriptor.id) != 0) return nullptr;

    auto* pl = new Plugin();
    pl->host = host;
    pl->plugin.desc                = &kDescriptor;
    pl->plugin.plugin_data         = pl;
    pl->plugin.init                = plugin_init;
    pl->plugin.destroy             = plugin_destroy;
    pl->plugin.activate            = plugin_activate;
    pl->plugin.deactivate          = plugin_deactivate;
    pl->plugin.start_processing    = plugin_start_processing;
    pl->plugin.stop_processing     = plugin_stop_processing;
    pl->plugin.reset               = plugin_reset;
    pl->plugin.process             = plugin_process;
    pl->plugin.get_extension       = plugin_get_extension;
    pl->plugin.on_main_thread      = plugin_on_main_thread;
    return &pl->plugin;
}

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* /*f*/) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_descriptor(
        const clap_plugin_factory_t* /*f*/, uint32_t index) {
    return index == 0 ? &kDescriptor : nullptr;
}

static const clap_plugin_factory_t kFactory = {
    factory_get_plugin_count,
    factory_get_descriptor,
    create_plugin,
};

// -----------------------------------------------------------------------------
// Plugin entry point
// -----------------------------------------------------------------------------

static bool entry_init(const char* /*plugin_path*/) { return true; }
static void entry_deinit() {}

static const void* entry_get_factory(const char* factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) return &kFactory;
    return nullptr;
}

}  // namespace mm

extern "C" {
CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    mm::entry_init,
    mm::entry_deinit,
    mm::entry_get_factory,
};
}
