#include "Engine.h"

namespace fe
{
constexpr double Engine::kHeadDist[3];

double Engine::head1DelayMs (double rate)
{
    rate = clampd (rate, 0.0, 1.0);
    // motor speed is roughly exponential in the Repeat Rate pot's travel
    return kHead1SlowMs * std::pow (kHead1FastMs / kHead1SlowMs, rate);
}

static double speedFor (double rate, double fs) { return 1.0 / (Engine::head1DelayMs (rate) * 0.001 * fs); }

void Engine::prepare (double sampleRate, int)
{
    fs = sampleRate;
    vNominal = speedFor (0.5, fs);

    // longest delay: head 3 at slowest speed, plus wow and splice bump headroom
    const double maxDelay = kHead1SlowMs * 0.001 * kHeadDist[2] * 1.25 * fs + 64;
    int n = 1; while (n < (int) maxDelay) n <<= 1;
    tapeAudio.assign ((size_t) n, 0.0f);
    tapePos.assign ((size_t) n, 0.0);
    mask = n - 1;

    const double v0 = speedFor (params.repeatRate, fs);
    for (int a = 0; a < n; ++a) tapePos[(size_t) a] = (a - n + 1) * v0;
    writeIndex = n;
    P = 0;
    for (auto& h : headIndex) h = 0;

    motor1.setTime (fs, 0.16); motor2.setTime (fs, 0.16);
    motor1.reset (v0); motor2.reset (v0);
    motorDrift.set (fs, 0.15, 0x51u);
    wowDrift.set (fs, 0.9, 0x77u);
    scrapeDrift.set (fs, 70.0, 0x99u);
    spliceBumpDecay = std::exp (-1.0 / (0.035 * fs));
    clickDecay = std::exp (-1.0 / (0.0015 * fs));

    preHp.highpass (fs, 28.0, 0.707);
    preLp.lowpass (fs, 15500.0, 0.707);
    recEmph.highShelf (fs, 2500.0, 7.0);
    pbDeEmph.highShelf (fs, 2500.0, -7.0);
    pbHp.highpass (fs, 55.0, 0.6);
    loopDc.set (fs, 6.0);
    hissTilt.set (fs, 9000.0);
    lastFilterSpeed = -1; lastBass = lastTreble = 99; lastAge = -1;
    updateSpeedFilters (v0);

    for (auto* s : { &sIn, &sEcho, &sRev, &sFb, &sDry, &sOut, &sRecIn, &sRevSend }) s->setTime (fs, 0.02);
    for (auto& s : sHead) s.setTime (fs, 0.012);

    vuLp.setTime (fs, 0.3);

    // A fixed set of worn spots on this "unit's" loop. Spot i appears once tapeAge passes its
    // threshold, and deepens as the tape ages further.
    Rng r (0xC0FFEEu);
    for (size_t i = 0; i < spots.size(); ++i)
    {
        auto& s = spots[i];
        s.centre = r.uni() * kLoopLength;
        s.width = 0.004 + 0.03 * r.uni() * r.uni();
        s.depth = 0.35 + 0.6 * r.uni();
        s.threshold = 0.12 + 0.85 * ((double) i / spots.size()) * (0.8 + 0.2 * r.uni());
    }

    spring.prepare (fs);
    powerCycle();
}

void Engine::powerCycle()
{
    std::fill (tapeAudio.begin(), tapeAudio.end(), 0.0f);
    spring.reset();
    fbSignal = 0;
    for (auto* b : { &preHp, &preLp, &recEmph, &pbDeEmph, &gapLoss1, &gapLoss2, &headBump, &pbHp, &bassSh, &trebleSh }) b->reset();
    loopDc.reset();
    spliceBump = 0; clickEnv = {};
}

void Engine::updateSpeedFilters (double v)
{
    const double age = params.tapeAge;
    const double ratio = v / vNominal;
    // Playback head gap / spacing loss: the slower the tape, the duller the repeats.
    const double fc = clampd (5600.0 * std::pow (ratio, 0.85) * (1.0 - 0.5 * age), 900.0, fs * 0.45);
    gapLoss1.lowpass (fs, fc, 0.6);
    gapLoss2.lowpass (fs, fc * 1.35, 0.75);
    // Head bump (low-frequency contour effect), also scales with speed.
    headBump.peak (fs, 95.0 * ratio, 1.1, 2.5);
    lastFilterSpeed = v;
    lastAge = age;
}

double Engine::wearGain (double loopPos) const
{
    double g = 1.0;
    const double age = params.tapeAge;
    // oxide unevenness: slow level ripple around the loop
    if (age > 0.0)
        g *= 1.0 - age * 0.10 * (0.5 + 0.5 * std::sin (2.0 * kPi * loopPos * 3.0 / kLoopLength + 1.3))
                  - age * 0.05 * (0.5 + 0.5 * std::sin (2.0 * kPi * loopPos * 11.0 / kLoopLength));
    for (const auto& s : spots)
    {
        if (age <= s.threshold) continue;
        double d = std::fabs (loopPos - s.centre);
        if (d > kLoopLength * 0.5) d = kLoopLength - d;
        if (d > s.width * 4.0) continue;
        const double amt = s.depth * std::min (1.0, (age - s.threshold) / 0.25);
        g *= 1.0 - amt * std::exp (-0.5 * (d * d) / (s.width * s.width));
    }
    if (params.splice)
    {
        double d = std::fmin (loopPos, kLoopLength - loopPos);
        if (d < 0.2) g *= 1.0 - 0.8 * std::exp (-0.5 * (d * d) / (0.025 * 0.025));
    }
    return g;
}

double Engine::readHead (int k, double target)
{
    int64_t& h = headIndex[k];
    const int64_t newest = writeIndex - 1;
    if (newest - h > mask - 4) h = newest - mask + 4;   // fell too far behind (should not happen)
    while (h + 1 < newest && tapePos[(size_t) ((h + 1) & mask)] <= target) ++h;

    const double p0 = tapePos[(size_t) (h & mask)], p1 = tapePos[(size_t) ((h + 1) & mask)];
    const double t = p1 > p0 ? clampd ((target - p0) / (p1 - p0), 0.0, 1.0) : 0.0;
    const double ym1 = tapeAudio[(size_t) ((h - 1) & mask)], y0 = tapeAudio[(size_t) (h & mask)];
    const double y1 = tapeAudio[(size_t) ((h + 1) & mask)];
    const double y2 = (h + 2 <= newest) ? tapeAudio[(size_t) ((h + 2) & mask)] : y1;
    // 4-point Hermite
    const double c1 = 0.5 * (y1 - ym1), c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2, c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

void Engine::process (const float* in, float* out, int numSamples)
{
    const Params p = params;
    const int mode = std::min (12, std::max (1, p.mode)) - 1;
    const ModeInfo& mi = kModes[mode];
    const int nHeads = (int) mi.head[0] + (int) mi.head[1] + (int) mi.head[2];

    // control targets
    const double inGain = p.input <= 0.001f ? 0.0 : dbToGain (40.0 * (p.input - 0.5)) * std::min (1.0, p.input / 0.06);
    const double fbGain = 1.32 * std::pow ((double) p.intensity, 1.15) / (1.0 + 0.38 * std::max (0, nHeads - 1));
    const double headNorm = 1.0 / (1.0 + 0.25 * std::max (0, nHeads - 1));
    const double echoG = 1.6 * std::pow ((double) p.echoVolume, 1.6);
    const double revG = mi.reverb ? 2.2 * std::pow ((double) p.reverbVolume, 1.6) : 0.0;
    const double revSend = mi.reverb ? 1.0 : 0.0;
    const double dryG = p.dry ? 1.0 : 0.0;
    const double outG = dbToGain (clampd (p.outputDb, -20.0, 20.0));
    const double vTarget = speedFor (p.repeatRate, fs);
    const double wf = 2.0 * p.wowFlutter * (1.0 + 1.6 * p.tapeAge);
    const double hissAmt = p.noise * (0.0012 + 0.0025 * p.tapeAge);

    if (p.bass != lastBass) { bassSh.lowShelf (fs, 110.0, 12.0 * p.bass, 0.7); lastBass = p.bass; }
    if (p.treble != lastTreble) { trebleSh.highShelf (fs, 3000.0, 12.0 * p.treble, 0.7); lastTreble = p.treble; }

    double vuAcc = vuLp.z;
    bool peak = false;

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- motor & transport ----
        const double vMotor = motor2.process (motor1.process (vTarget)) * (1.0 + 0.0004 * motorDrift.process());
        const double ratio = vMotor / vNominal;
        capPhase += 7.3 * ratio / fs;     // capstan rotation rate follows motor speed
        rollerPhase += 1.9 * ratio / fs;  // pinch roller
        if (capPhase >= 1.0) capPhase -= 1.0;
        if (rollerPhase >= 1.0) rollerPhase -= 1.0;
        const double wow = 0.0016 * wowDrift.process();
        const double flutter = 0.0007 * std::sin (2.0 * kPi * capPhase) + 0.0004 * std::sin (2.0 * kPi * rollerPhase);
        const double scrape = 0.00018 * scrapeDrift.process();
        spliceBump *= spliceBumpDecay;
        const double v = vMotor * (1.0 + wf * (wow + flutter + scrape) - spliceBump);

        const double Pprev = P;
        P += v;

        // splice passes the capstan: brief speed hiccup
        if (p.splice)
        {
            const double a = std::floor ((Pprev - kCapstanDist) / kLoopLength), b = std::floor ((P - kCapstanDist) / kLoopLength);
            if (b != a) spliceBump += 0.006 + 0.008 * p.tapeAge;
        }

        if (++filterCounter >= 32)
        {
            filterCounter = 0;
            if (std::fabs (v - lastFilterSpeed) > lastFilterSpeed * 0.002 || p.tapeAge != lastAge) updateSpeedFilters (v);
        }

        // ---- input preamp ----
        double x = (double) in[i] * sIn.process (inGain);
        x = preHp.process (x);
        {
            // discrete transistor stage: asymmetric soft clip, unity slope at small signals
            const double k = 1.1, bb = 0.22, tb = std::tanh (k * bb);
            x = (std::tanh (k * (x + bb)) - tb) / (k * (1.0 - tb * tb));
        }
        x = preLp.process (x);
        if (std::fabs (x) > 0.85) peak = true;

        // ---- record ----
        double rec = x * sRecIn.process (p.echoOn ? 1.0 : 0.0) + fbSignal;
        vuAcc = vuLp.a * vuAcc + (1.0 - vuLp.a) * rec * rec;
        rec = recEmph.process (rec);
        rec = std::tanh (rec * 1.15) / 1.15;                     // tape saturation
        double loopPos = std::fmod (P, kLoopLength); if (loopPos < 0) loopPos += kLoopLength;
        rec *= wearGain (loopPos);

        tapeAudio[(size_t) (writeIndex & mask)] = (float) rec;
        tapePos[(size_t) (writeIndex & mask)] = P;
        ++writeIndex;

        // ---- playback heads ----
        double play = 0;
        for (int k = 0; k < 3; ++k)
        {
            const double target = P - kHeadDist[k];
            const double s = readHead (k, target);
            const double g = sHead[(size_t) k].process (mi.head[k] ? headNorm : 0.0);
            if (p.splice)
            {
                const double a = std::floor ((Pprev - kHeadDist[k]) / kLoopLength), b = std::floor (target / kLoopLength);
                if (a != b) clickEnv[(size_t) k] = 0.012 + 0.02 * p.tapeAge;
            }
            const double click = clickEnv[(size_t) k] * (noiseRng.bi() * 0.5 + 0.5);
            clickEnv[(size_t) k] *= clickDecay;
            play += g * (s + click);
        }

        play = pbDeEmph.process (play);
        play = gapLoss2.process (gapLoss1.process (play));
        play = headBump.process (play);
        play = pbHp.process (play);
        play += hissTilt.process (noiseRng.bi()) * hissAmt * 3.0 * (nHeads > 0 ? 1.0 : 0.0);

        double E = trebleSh.process (bassSh.process (play));
        E = loopDc.process (E);
        fbSignal = E * sFb.process (fbGain);

        // ---- spring reverb & mix ----
        const double send = sRevSend.process (revSend);
        const double R = send > 1e-5 ? spring.process ((x + E * 0.8) * send) : spring.process (0.0);

        const double y = (x * sDry.process (dryG) + E * sEcho.process (echoG) + R * sRev.process (revG)) * sOut.process (outG);
        out[i] = (float) y;
    }

    vuLp.z = vuAcc;
    if (peak) peakHold = (int) (0.15 * fs);
    else peakHold = std::max (0, peakHold - numSamples);
    vuOut.store ((float) std::sqrt (std::max (0.0, vuAcc)) * 1.414f, std::memory_order_relaxed);
    peakOut.store (peakHold > 0, std::memory_order_relaxed);
    double lp = std::fmod (P, kLoopLength); if (lp < 0) lp += kLoopLength;
    tapePosOut.store (lp, std::memory_order_relaxed);
    speedOut.store (motor2.z, std::memory_order_relaxed);
}
} // namespace fe
