# Effect captures

`prepare.py`, `analyze.py` and `kit.py` are LIMINAL's capture tools (AdrianAbbas/LIMINAL,
`tools/captures`, same developer), copied unchanged. They turn the developer's pedal
captures in the private Samples repository (the capture-kit sweeps through each patch)
into impulse responses: `shimmer.wav` (the shimmer patch's static take), `darkhall.wav`
(the dark reverb + delay), `chorusverb.wav` (the chorus modulator into a reverb), plus
`profiles.json`. LIMINAL's `docs/PEDAL_CAPTURES.md` documents the quality checks and the
measurements.

    python3 -I tools/captures/prepare.py ../Samples build/samples/captures

CI runs it in `scripts/ci-fetch-samples.sh`; the IRs ship inside the instrument's bundle
(`Contents/Resources/Samples/captures`) and are never committed. DUNGEON CRAWLER (the
instrument) lets a few instruments send part of their sound through one of them before
the place (`Source/Instrument/Captures.*`, `captureOf` in `Designs.cpp`); the FX does not
use them. No unit names appear in the product.
