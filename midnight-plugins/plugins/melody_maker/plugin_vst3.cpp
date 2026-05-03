// =============================================================================
// plugin_vst3.cpp — VST3 plugin entry point
// TSF_IMPLEMENTATION must be defined here ONCE before plugin_vst3.h includes tsf.h
// =============================================================================

#define TSF_IMPLEMENTATION
#include "plugin_vst3.h"

// --------------------------------------------------------------------------
// DLL globals
// --------------------------------------------------------------------------
HINSTANCE g_hInst = nullptr;

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInst = hDll;
        DisableThreadLibraryCalls(hDll);
    }
    return TRUE;
}

BEGIN_FACTORY_DEF(
    "Midnight",
    "https://github.com/midnight",
    "mailto:support@midnight.example.com")

    DEF_CLASS2(
        INLINE_UID(0xA3F7E291, 0x5CB24D18, 0x87A04E2C, 0x9B8D1F05),
        PClassInfo::kManyInstances,
        kVstAudioEffectClass,
        "Midnight Melody Maker (beta)",
        0,              // component flags
        "Instrument",   // sub-category â†’ FL Studio shows it as a Generator
        "1.0.0",
        kVstVersionString,
        MelodyMakerVST3::createInstance)

END_FACTORY

