// Engine tests: run with ctest or directly. Exit code 0 = all passed.
#include "Engine.h"
#include "Presets.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <memory>

using namespace fe;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static constexpr double kFs = 48000.0;

static Params quiet (int mode, float rate)
{
    Params p; p.mode = mode; p.repeatRate = rate; p.intensity = 0.0f; p.echoVolume = 0.7f;
    p.reverbVolume = 0.0f; p.dry = false; p.wowFlutter = 0.0f; p.tapeAge = 0.0f; p.noise = 0.0f; p.splice = false;
    return p;
}

static std::vector<float> run (Engine& e, const std::vector<float>& in)
{
    std::vector<float> out (in.size());
    const int block = 256;
    for (size_t i = 0; i < in.size(); i += block)
        e.process (in.data() + i, out.data() + i, (int) std::min<size_t> (block, in.size() - i));
    return out;
}

static std::vector<float> impulse (double seconds, double at = 0.05)
{
    std::vector<float> v ((size_t) (seconds * kFs), 0.0f);
    for (int i = 0; i < 24; ++i) v[(size_t) (at * kFs) + i] = 0.4f * std::sin (kPi * i / 24.0);   // short band-limited click
    return v;
}

// returns the time (ms, relative to the impulse) of the largest peak in the output
static double peakTimeMs (const std::vector<float>& out, double from = 0.0)
{
    size_t best = 0; float bv = 0;
    for (size_t i = (size_t) (from * kFs); i < out.size(); ++i) if (std::fabs (out[i]) > bv) { bv = std::fabs (out[i]); best = i; }
    return (best / kFs - 0.05) * 1000.0;
}

static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / std::max<size_t> (1, b - a));
}

static void testHeadDelays()
{
    CHECK (std::fabs (Engine::headDelayMs (0, 1.0) - 69.0) < 0.5, "head1 fast %.2f", Engine::headDelayMs (0, 1.0));
    CHECK (std::fabs (Engine::headDelayMs (0, 0.0) - 177.0) < 0.5, "head1 slow");
    CHECK (std::fabs (Engine::headDelayMs (2, 0.0) - 486.75) < 6, "head3 slow %.1f", Engine::headDelayMs (2, 0.0));

    for (int head = 0; head < 3; ++head)
        for (float rate : { 0.0f, 0.5f, 1.0f })
        {
            auto e = std::make_unique<Engine>();
            e->setParams (quiet (head + 1, rate));
            e->prepare (kFs, 256);
            const auto out = run (*e, impulse (1.0));
            const double expected = Engine::headDelayMs (head, rate);
            const double got = peakTimeMs (out);
            CHECK (std::fabs (got - expected) < 4.0, "mode %d rate %.1f: echo at %.1f ms, expected %.1f", head + 1, rate, got, expected);
        }
}

static void testModeTable()
{
    // number of distinct echoes for each mode, with reverb off
    for (int m = 1; m <= 12; ++m)
    {
        auto e = std::make_unique<Engine>();
        auto p = quiet (m, 1.0f);
        e->setParams (p);
        e->prepare (kFs, 256);
        const auto out = run (*e, impulse (0.6));
        int heads = 0;
        for (int k = 0; k < 3; ++k)
        {
            const size_t c = (size_t) ((0.05 + Engine::headDelayMs (k, 1.0) / 1000.0) * kFs);
            const double r = rms (out, c - 200, c + 200);
            if (r > 1e-3) ++heads;
            CHECK ((r > 1e-3) == kModes[m - 1].head[k], "mode %d head %d active=%d rms=%g", m, k + 1, (int) kModes[m - 1].head[k], r);
        }
        (void) heads;
    }
}

static void testReverbOnly()
{
    auto e = std::make_unique<Engine>();
    auto p = quiet (12, 0.5f); p.reverbVolume = 0.8f; p.echoVolume = 1.0f;
    e->setParams (p); e->prepare (kFs, 256);
    const auto out = run (*e, impulse (2.0));
    CHECK (rms (out, (size_t) (0.1 * kFs), (size_t) (0.6 * kFs)) > 1e-4, "spring reverb produced no tail");
    CHECK (rms (out, (size_t) (1.8 * kFs), (size_t) (2.0 * kFs)) < rms (out, (size_t) (0.1 * kFs), (size_t) (0.3 * kFs)), "reverb does not decay");

    // modes 1-4 have no reverb even with Reverb Volume up
    auto e2 = std::make_unique<Engine>();
    auto p2 = quiet (1, 0.5f); p2.reverbVolume = 1.0f; p2.echoVolume = 0.0f;
    e2->setParams (p2); e2->prepare (kFs, 256);
    const auto out2 = run (*e2, impulse (1.0));
    CHECK (rms (out2, 0, out2.size()) < 1e-6, "reverb audible in mode 1");
}

static void testSelfOscillation()
{
    // high intensity, no input after a single kick: the loop must build up and stay bounded
    auto e = std::make_unique<Engine>();
    auto p = quiet (4, 0.5f); p.intensity = 0.95f; p.noise = 0.3f;
    e->setParams (p); e->prepare (kFs, 256);
    const auto out = run (*e, impulse (8.0));
    const double late = rms (out, (size_t) (7.0 * kFs), out.size());
    CHECK (late > 0.05, "no self-oscillation at intensity 0.95 (rms %g)", late);
    float mx = 0; bool finite = true;
    for (float v : out) { mx = std::max (mx, std::fabs (v)); finite = finite && std::isfinite (v); }
    CHECK (finite && mx < 4.0f, "self-oscillation unbounded (max %g)", mx);

    // low intensity decays away
    auto e2 = std::make_unique<Engine>();
    auto p2 = quiet (4, 0.5f); p2.intensity = 0.4f;
    e2->setParams (p2); e2->prepare (kFs, 256);
    const auto out2 = run (*e2, impulse (6.0));
    CHECK (rms (out2, (size_t) (5.0 * kFs), out2.size()) < 1e-3, "intensity 0.4 does not decay");

    // power switch kills it
    e->powerCycle();
    auto p3 = p; p3.intensity = 0.0f; e->setParams (p3);
    const auto out3 = run (*e, std::vector<float> ((size_t) kFs, 0.0f));
    CHECK (rms (out3, (size_t) (0.6 * kFs), out3.size()) < 1e-3, "power cycle did not clear the tape");
}

// Changing Repeat Rate while a tone is on the tape bends its pitch (motor speed change).
static void testPitchBend()
{
    auto e = std::make_unique<Engine>();
    auto p = quiet (1, 0.0f);
    e->setParams (p); e->prepare (kFs, 256);
    std::vector<float> tone ((size_t) (0.6 * kFs));
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = 0.2f * (float) std::sin (2 * kPi * 500.0 * i / kFs);
    run (*e, tone);
    // stop input, speed the motor up: echo of the tone should come out sharp
    p.repeatRate = 1.0f; p.echoOn = false; e->setParams (p);
    std::vector<float> silence ((size_t) (0.12 * kFs), 0.0f);
    auto out = run (*e, silence);
    // count zero crossings in the middle part
    int zc = 0; const size_t a = (size_t) (0.04 * kFs), b = (size_t) (0.10 * kFs);
    for (size_t i = a + 1; i < b; ++i) if ((out[i - 1] < 0) != (out[i] < 0)) ++zc;
    const double hz = zc / 2.0 / ((b - a) / kFs);
    CHECK (hz > 530.0, "no upward pitch bend on rate increase (%.1f Hz)", hz);
}

static void testPresetsStable()
{
    int n = 0; const Preset* ps = factoryPresets (n);
    for (int i = 0; i < n; ++i)
    {
        auto e = std::make_unique<Engine>();
        e->setParams (ps[i].p); e->prepare (44100.0, 512);
        std::vector<float> in ((size_t) (44100 * 4)); Rng r (7);
        for (size_t k = 0; k < in.size(); ++k) in[k] = (k < 44100 ? 0.3f : 0.0f) * (float) r.bi();
        std::vector<float> out (in.size());
        for (size_t k = 0; k < in.size(); k += 512) e->process (in.data() + k, out.data() + k, (int) std::min<size_t> (512, in.size() - k));
        bool ok = true; float mx = 0;
        for (float v : out) { ok = ok && std::isfinite (v); mx = std::max (mx, std::fabs (v)); }
        CHECK (ok && mx < 6.0f, "preset '%s' unstable (max %g)", ps[i].name, mx);
    }
}

int main()
{
    testHeadDelays();
    testModeTable();
    testReverbOnly();
    testSelfOscillation();
    testPitchBend();
    testPresetsStable();
    if (failures == 0) std::printf ("All engine tests passed.\n");
    return failures == 0 ? 0 : 1;
}
