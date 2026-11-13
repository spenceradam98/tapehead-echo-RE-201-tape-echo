// Spring reverb tank model.
//
// A spring tank is a dispersive transmission line: high frequencies travel faster than low
// ones, so a click comes out as a descending "chirp", and the wave bounces between the two
// transducers many times. Each spring here is modelled as a feedback loop containing
//   - a cascade of stretched first-order allpasses (H(z) = (a + z^-K) / (1 + a z^-K)), which
//     produces the frequency-dependent group delay (dispersion, the "drip"),
//   - a bulk delay (the transit time across the spring, a few tens of ms),
//   - a low-pass for the losses, and a feedback gain that sets the decay time.
// Three springs of different length are summed, and the tank is band-limited by its drive
// and pickup transducers (roughly 120 Hz to 4.5 kHz) as on a real tank.
#pragma once

#include "Dsp.h"
#include <array>

namespace fe
{
class SpringReverb
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const double r = fs / 48000.0;
        static const double lenMs[kSprings] = { 33.7, 39.1, 44.9 };
        static const double apCoef[kSprings] = { 0.62, 0.58, 0.65 };
        for (int s = 0; s < kSprings; ++s)
        {
            auto& sp = springs[(size_t) s];
            sp.len = std::max (8, (int) (lenMs[s] * 0.001 * fs) - kAllpasses * kStretch);
            sp.delay.allocate (sp.len + 64);
            sp.a = apCoef[s];
            for (auto& ap : sp.ap) { ap.allocate ((int) std::ceil (kStretch * r) + 2); ap.k = std::max (1, (int) std::lround (kStretch * r)); }
            sp.damp.set (fs, 3800.0 - 300.0 * s);
            sp.mod.set (fs, 0.7 + 0.3 * s, 0xA5u + (uint32_t) s * 77u);
            sp.fb = 0;
        }
        inHp.highpass (fs, 120.0, 0.6);
        inLp.lowpass (fs, 4800.0, 0.6);
        outHp.highpass (fs, 140.0, 0.7);
        outLp.lowpass (fs, 4500.0, 0.75);
        outPeak.peak (fs, 1800.0, 1.2, 2.5);   // pickup resonance gives the familiar "boing" colour
        setDecay (2.6);
        reset();
    }

    void reset()
    {
        for (auto& sp : springs)
        {
            sp.delay.clear();
            for (auto& ap : sp.ap) ap.clear();
            sp.damp.reset(); sp.fb = 0;
        }
        inHp.reset(); inLp.reset(); outHp.reset(); outLp.reset(); outPeak.reset();
    }

    // RT60 in seconds.
    void setDecay (double rt60)
    {
        for (int s = 0; s < kSprings; ++s)
        {
            auto& sp = springs[(size_t) s];
            const double loopSec = (sp.len + kAllpasses * kStretch * 1.6) / fs;
            sp.g = std::pow (10.0, -3.0 * loopSec / rt60);
        }
    }

    inline double process (double x)
    {
        // drive transducer: band-limit and a touch of saturation (the reverb driver amp)
        double in = inLp.process (inHp.process (x));
        in = std::tanh (in * 1.4) / 1.4;

        double out = 0;
        for (int s = 0; s < kSprings; ++s)
        {
            auto& sp = springs[(size_t) s];
            double v = in * (s == 1 ? -1.0 : 1.0) + sp.fb;
            for (auto& ap : sp.ap) v = ap.process (v, sp.a);
            sp.delay.push (v);
            // tiny transit-time modulation: springs are never perfectly still
            const double d = sp.len * (1.0 + 0.0009 * sp.mod.process());
            const double y = sp.delay.tapFrac (std::max (1.0, d));
            sp.fb = sp.damp.process (y) * sp.g;
            out += y;
        }
        out = outPeak.process (outLp.process (outHp.process (out)));
        return out * 0.55;
    }

private:
    static constexpr int kSprings = 3;
    static constexpr int kAllpasses = 24;
    static constexpr int kStretch = 2;

    struct StretchedAllpass
    {
        std::vector<double> xb, yb; int w = 0, k = 2, n = 4;
        void allocate (int len) { n = len; xb.assign ((size_t) n, 0.0); yb.assign ((size_t) n, 0.0); w = 0; }
        void clear() { std::fill (xb.begin(), xb.end(), 0.0); std::fill (yb.begin(), yb.end(), 0.0); }
        inline double process (double x, double a)
        {
            int r = w - k; if (r < 0) r += n;
            const double y = a * x + xb[(size_t) r] - a * yb[(size_t) r];
            xb[(size_t) w] = x; yb[(size_t) w] = y;
            if (++w >= n) w = 0;
            return y;
        }
    };

    struct Spring
    {
        std::array<StretchedAllpass, kAllpasses> ap;
        DelayLine delay;
        OnePoleLP damp;
        Drift mod;
        int len = 1000;
        double a = 0.6, g = 0.8, fb = 0;
    };

    double fs = 48000;
    std::array<Spring, kSprings> springs;
    Biquad inHp, inLp, outHp, outLp, outPeak;
};
} // namespace fe
