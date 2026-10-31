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

## v1.12
- Glue is now its own panel: Mix, Threshold, Ratio, Attack (1 to 100 ms), Release, plus a meter with level, threshold line and gain reduction in dB. It works without Sauce Amount.
- Limiter: On/Off and Push (0 to 12 dB). Raises the kick, then stops the peaks from growing.
- Sauce is much stronger (stronger saturation, Bloom, Crack and Loud ranges).
- Waveform panel: Export .wav (24-bit mono) and drag the waveform straight into a DAW or folder (Chromium browsers; the real plugin will use native drag).
- Pitch envelope: every dot moves on its own, in any direction, including the first and last; the first dot can also move right.
- Window is now 1640 x 946.

## v1.13
- Render tab (next to Glue & limiter): Key, Vel, Generate, Export .wav, and a draggable waveform of the whole kick. Shows "out of date" when you change a knob after generating.
- Layer Start knob is now 0 to 40 ms with 0.1 ms steps (was 0 to 200 ms), so you can trim the start of clicks precisely.

## v1.14
- Glue and limiter panel fits the window: Limiter On/Off moved next to the meter chips, Push sits with the timing knobs.
- Tone EQ is much stronger (Mud up to -12 dB, Chest up to +8 dB, Shine shelf up to +15 dB above 2.5 kHz).
- Moving any Sauce knob switches Sauce Amount on when it was 0.
- New default preset "Trance Classic 138": about 1/8 note long, Sauce on, glue 30 ms attack, 4:1, limiter on. Starting point only, not verified against any commercial kick.

## v1.15
- Learned from measuring 45 trance kicks (numbers only, no audio copied into this repo): real trance kicks hold near full level for about 180 ms and then drop (Shape control, 4 to 5), their pitch falls slowly, about 580 Hz at 2 ms, 250 at 8 ms, 150 at 30 ms, 100 at 56 ms, 70 at 100 ms, then 55 Hz, and the click lasts about 5 ms to 10 percent.
- New Shape knob (decay curve), Decay up to 500 ms, Click length up to 40 ms.
- "Trance Classic 138" is rebuilt from those measurements (pitch curve within a few Hz, level shape, length and crest factor close to the measured median).
- Six new "Trance clicks" generated from the measured click envelope and spectrum.
- Default Len for clicks and attacks is now much longer (45 to 100 ms), so they are audible straight away.

## v1.16
- 45 "Trance attacks" and 45 "Trance clicks" are built into the plugin, in the layer menu (first folders). They are the first 70 ms and the first 30 ms (high-passed) of 45 trance kicks from a royalty-free sample pack the author bought and has licensed for commercial use. Everything ships inside the one installer, no loose files.

## v1.17
- Built-in chops renamed and moved into the normal groups: "Kick attacks" (Kick attack 1 to 45), "Clicks" (Kick click 1 to 45), "Kicks" (Kick 1, Kick 2, and others). No pack or brand names anywhere.
- 10 new trance presets that use those attacks and clicks (Trance Snap, Pop, Tick, Knock, Crack, Sizzle, Long, Deep, Rolling, Hard).
- New Swell knob (Fall group): the low body grows in over the first ms like real trance kicks. Fixes boxy low mids.
