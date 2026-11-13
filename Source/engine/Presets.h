// Factory presets. Each is a full Params snapshot.
#pragma once

#include "Engine.h"

namespace fe
{
struct Preset { const char* name; Params p; };

inline Params makeParams (int mode, float rate, float intensity, float echoVol, float revVol,
                          float bass, float treble, float input = 0.5f, float wf = 0.5f, float age = 0.2f)
{
    Params p;
    p.mode = mode; p.repeatRate = rate; p.intensity = intensity; p.echoVolume = echoVol;
    p.reverbVolume = revVol; p.bass = bass; p.treble = treble; p.input = input;
    p.wowFlutter = wf; p.tapeAge = age;
    return p;
}

inline const Preset* factoryPresets (int& count)
{
    static const Preset presets[] = {
        { "Stock Unit",            makeParams (4,  0.50f, 0.35f, 0.50f, 0.35f,  0.0f,  0.0f) },
        { "Slapback Head 1",       makeParams (1,  0.85f, 0.10f, 0.55f, 0.00f,  0.0f,  0.1f) },
        { "Gallop (Heads 2+3)",    makeParams (4,  0.40f, 0.45f, 0.55f, 0.00f, -0.1f, -0.2f) },
        { "Dub Plate",             makeParams (9,  0.30f, 0.68f, 0.65f, 0.40f,  0.2f, -0.45f, 0.55f) },
        { "Runaway Oscillation",   makeParams (11, 0.45f, 0.86f, 0.55f, 0.30f,  0.1f, -0.3f, 0.5f, 0.6f, 0.3f) },
        { "Spring Tank Only",      makeParams (12, 0.50f, 0.00f, 0.00f, 0.60f,  0.0f,  0.0f) },
        { "Cavern (All Heads)",    makeParams (11, 0.20f, 0.55f, 0.50f, 0.55f, -0.2f, -0.35f) },
        { "Worn Loop",             makeParams (6,  0.35f, 0.50f, 0.55f, 0.25f,  0.1f, -0.5f, 0.6f, 0.9f, 0.85f) },
        { "Hot Preamp Crunch",     makeParams (5,  0.70f, 0.30f, 0.45f, 0.25f,  0.2f,  0.2f, 0.85f) },
        { "Long Head 3 Wash",      makeParams (7,  0.00f, 0.62f, 0.55f, 0.45f, -0.3f, -0.4f) },
    };
    count = (int) (sizeof (presets) / sizeof (presets[0]));
    return presets;
}
} // namespace fe
