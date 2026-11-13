# Tapehead Echo

A tape-loop echo and spring reverb plugin (VST3 + standalone) modelled on the behaviour of the
classic 1970s three-playback-head tape echo (the RE-201 class of unit). See `docs/FEATURES.md`.

## Build

```
cmake -B build -DCMAKE_BUILD_TYPE=Release            # fetches JUCE 8.0.4
# or: cmake -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
ctest --test-dir build                                # engine tests
```

macOS builds target macOS 11+ universal (Intel + Apple Silicon). No AVX2 or other
`-march` flags are used; the DSP is scalar C++. Windows builds use the static runtime.

Layout: `Source/engine` is plain C++ (tape transport, heads, preamp, spring reverb, presets)
with tests in `tests/`. `Source/plugin` is the JUCE wrapper and UI.
