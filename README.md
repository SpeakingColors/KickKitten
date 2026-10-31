# Kick Kitten

Free, open source trance kick drum synth (VST3, Linux first). Studio 87.

Status: spec stage. See [docs/spec-v0.1.md](docs/spec-v0.1.md).

## Sound bank

Layers A, B and C pick from a folder tree of clicks, kick attacks, snaps, hats, zaps and tonal tops. Most are made from maths at start-up. The "Your samples" folder holds short (100 to 120 ms) attack cuts from the author's own samples.

## Sauce (trance kick processing)

The Sauce panel runs a chain inside the render: band split (sub below 110 Hz, body 110 to 800 Hz, top above 800 Hz), parallel saturation per band (Thump, Punch, Crack), a soft swell in the low tail (Bloom), heavy parallel compression (Glue) and a loudness stage with smooth peak shaving that keeps the peak where it was (Loud). Amount 0 turns it off. It is built from common kick practice (parallel layering, punch band around 130 to 200 Hz, saturation then re-filtering, transient and body split), not from any vendor's internals. Defaults are starting points to tune by ear.

## v1.11
- Tone EQ in the Sauce column: Mud (cut near 280 Hz, up to 6 dB), Chest (boost near 90 Hz, up to 4 dB), Shine (boost near 5 kHz, up to 6 dB).
- Glue retuned: 10 ms attack, 35 ms release, ratio 6 to 1.
- Start range now up to 4 kHz.
- New presets: Hard-Hitter F, Psy Knock G, Deep Progressive G#. Values are starting points, not verified against any commercial kick.
