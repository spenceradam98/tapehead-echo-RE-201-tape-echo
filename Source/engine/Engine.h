// Tape echo engine: a model of a 1970s three-playback-head tape loop echo with spring reverb
// (the RE-201 class of unit). Plain C++17, no JUCE dependency, unit-testable.
//
// Signal flow (mono, as on the hardware):
//
//   in -> Input Volume -> preamp (HPF, asymmetric transistor saturation, band limit) --+--> dry
//                                                                                     |
//        Echo/Normal switch                                                           |
//   +-- feedback <-- Intensity <--+                                                   |
//   v                             |                                                   |
//   record amp (pre-emphasis, tape saturation) -> [erase][rec] tape loop [pb1][pb2][pb3][capstan]
//                                                        |  heads chosen by Mode Selector
//                                                        v
//        playback amp (de-emphasis, gap/spacing loss that tracks tape speed, head bump, hiss)
//        -> Bass / Treble -> echo signal E ----> Echo Volume ------------------------> out
//                                     \--> spring reverb (fed by preamp + echoes) -> Reverb Volume -> out
//
// The tape is modelled physically: the motor (Repeat Rate) moves the tape at a speed v, every
// sample is written together with the tape position it was written at, and each playback
// head reads the audio found at (position now - head distance). Changing the speed therefore
// bends the pitch of the repeats already on the tape exactly like the motor does, and wow,
// flutter and splice bumps are speed modulations of that same transport.
#pragma once

#include "Dsp.h"
#include "SpringReverb.h"
#include <array>
#include <atomic>

namespace fe
{
struct Params
{
    // ---- front panel ----
    float input = 0.5f;          // Input Volume 0..1 (0 = off, 0.5 = unity, 1 = +20 dB)
    int mode = 4;                // Mode Selector 1..12
    float repeatRate = 0.5f;     // 0 = slowest motor (longest delay) .. 1 = fastest
    float intensity = 0.35f;     // feedback 0..1; self-oscillates from roughly 0.75 up
    float echoVolume = 0.5f;     // 0..1
    float reverbVolume = 0.35f;  // 0..1
    float bass = 0.0f;           // -1..+1 (cut/boost, echo path only)
    float treble = 0.0f;         // -1..+1
    bool echoOn = true;          // Echo/Normal (dub) switch: false stops feeding the tape
    // ---- service / tape condition (not on the hardware panel) ----
    float wowFlutter = 0.5f;     // 0..1, 0.5 = a healthy unit
    float tapeAge = 0.2f;        // 0..1: HF loss, more wow/flutter, oxide dropouts, hiss
    float noise = 0.35f;         // tape hiss amount 0..1
    bool splice = true;          // the tape loop has a splice (periodic dropout + bump)
    bool dry = true;             // dry signal through (off = wet only, for sends)
    float outputDb = 0.0f;       // -20..+20
};

struct ModeInfo { bool head[3]; bool reverb; };

// Mode Selector, positions 1..12 (from the head chart printed on the unit).
constexpr ModeInfo kModes[12] = {
    { { true,  false, false }, false },   // 1
    { { false, true,  false }, false },   // 2
    { { false, false, true  }, false },   // 3
    { { false, true,  true  }, false },   // 4
    { { true,  false, false }, true  },   // 5
    { { false, true,  false }, true  },   // 6
    { { false, false, true  }, true  },   // 7
    { { true,  true,  false }, true  },   // 8
    { { false, true,  true  }, true  },   // 9
    { { true,  false, true  }, true  },   // 10
    { { true,  true,  true  }, true  },   // 11
    { { false, false, false }, true  },   // 12: reverb only
};

class Engine
{
public:
    // Head distances along the tape, in units of the record->head 1 distance. Ratios match
    // the published head delay ranges (69-177, 131-337, 189-489 ms).
    static constexpr double kHeadDist[3] = { 1.0, 1.9, 2.75 };
    static constexpr double kCapstanDist = 3.35;    // capstan/pinch roller sits after head 3
    static constexpr double kLoopLength = 46.0;     // tape loop length in the same units
    static constexpr double kHead1FastMs = 69.0;
    static constexpr double kHead1SlowMs = 177.0;

    static double head1DelayMs (double repeatRate);                    // nominal, no wow
    static double headDelayMs (int head, double repeatRate) { return head1DelayMs (repeatRate) * kHeadDist[head]; }

    void prepare (double sampleRate, int maxBlockSize);
    void setParams (const Params& p) { params = p; }
    const Params& getParams() const { return params; }

    // Mono in, mono out. in and out may alias.
    void process (const float* in, float* out, int numSamples);

    // Power switch: erase the tape and silence the reverb (stops a runaway oscillation).
    void powerCycle();

    // ---- state for the UI (written by the audio thread) ----
    float getVu() const { return vuOut.load (std::memory_order_relaxed); }           // 0..~1.5 RMS of the record level
    bool getPeakLamp() const { return peakOut.load (std::memory_order_relaxed); }
    double getTapePosition() const { return tapePosOut.load (std::memory_order_relaxed); }  // loop units
    double getCurrentHeadDelayMs (int head) const { return speedOut.load() > 0 ? kHeadDist[head] / speedOut.load() / fs * 1000.0 : 0; }

private:
    void updateSpeedFilters (double v);
    double wearGain (double loopPos) const;
    double readHead (int k, double target);

    Params params;
    double fs = 48000;

    // tape
    std::vector<float> tapeAudio;
    std::vector<double> tapePos;
    int64_t writeIndex = 0;
    int64_t headIndex[3] {};
    int mask = 0;
    double P = 0;               // tape position at the record head (cumulative, loop units)
    double vNominal = 0;        // speed at repeatRate 0.5, units/sample
    OnePoleLP motor1, motor2;
    Drift motorDrift, wowDrift, scrapeDrift;
    double capPhase = 0, rollerPhase = 0;
    double spliceBump = 0, spliceBumpDecay = 0.999;
    std::array<double, 3> clickEnv {};
    double clickDecay = 0.99;

    // preamp / record / playback
    Biquad preHp, preLp, recEmph, pbDeEmph, gapLoss1, gapLoss2, headBump, pbHp, bassSh, trebleSh;
    DcBlocker loopDc;
    double lastFilterSpeed = -1, lastBass = 99, lastTreble = 99, lastAge = -1;
    double fbSignal = 0;
    Rng noiseRng { 0x1234567u };
    OnePoleLP hissTilt;

    // reverb
    SpringReverb spring;

    // smoothed controls
    OnePoleLP sIn, sEcho, sRev, sFb, sDry, sOut, sRecIn;
    std::array<OnePoleLP, 3> sHead;
    OnePoleLP sRevSend;

    // wear spots (fixed per unit, enabled progressively by tapeAge)
    struct Spot { double centre, width, depth, threshold; };
    std::array<Spot, 18> spots {};

    // meters
    OnePoleLP vuLp;
    int peakHold = 0;
    std::atomic<float> vuOut { 0 };
    std::atomic<bool> peakOut { false };
    std::atomic<double> tapePosOut { 0 };
    std::atomic<double> speedOut { 0 };
    int filterCounter = 0;
};
} // namespace fe
