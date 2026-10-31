# Trance Kick Maker: spec v0.1 (name pending, cat theme)

Free open source Linux VST3 instrument (MIDI in, kick out). Studio 87 look, PurrOne wood cheeks, cat head logo, Soda Pop colours, dB meters, info bubbles, no telemetry.

## Fact check of the research text
Correct:
- 138 BPM: beat 434.78 ms, 16th 108.70 ms.
- Note Hz: F#1 46.25, G1 49.00, G#1 51.91, A1 55.00, B1 61.74.
- G5 about 784 Hz, B5 about 988 Hz.
Contradictions (settle by ear in the lab):
- Tail: "130 to 160 ms" in one place, "90 to 110 ms" in another. Real trance kicks often run longer; the "must end before the 16th" rule is not universal.
- Phases: sub tail 45-150 ms vs 25-90 ms. Click 0-15 ms vs 0-5 ms.
Unverified (treat as opinion): TR-909 pitched down 2-4 semitones, Jomox AirBase99 and DuMonde / Scot Project claims, exact EQ numbers.
Fix for the plugin: every time value is a knob, defaults come from the lab, not from the text.

## Sound engine (three layers)
1. Click: noise burst or high-passed square, 2-8 kHz, 0-15 ms.
2. Body: sine, pitch falls ~300 Hz to ~120 Hz, 5-45 ms.
3. Sub: sine settling on the tail note.
One pitch envelope (start 800-1000 Hz, sweep 25-35 ms, exponential curve) drives body+sub. Amp: attack 0, hold 10-15 ms, decay 90-160 ms. Soft saturation (Purr Trio: Drive/Transistor/Fuzz), then closed-loop level match.

## Controls
- Key / Tail note (F#1 to B1, plus free Hz)
- Pitch start, Sweep time, Curve
- Click level / freq / length
- Body level, Sub level
- Hold, Decay
- Drive (Purr Trio), Output, soft clipper
- Tail vs beat readout: ms and % of 16th at host BPM (default 138)
- Phase helper: shows kick first cycle polarity, flip button
- Presets: Uplifting 138, Dark, Punchy, Subby

## Displays (all live, both ways)
Pitch envelope graph (draggable), waveform with ms ruler, spectrum with tail Hz marker, out meter in dB.

## Quality
No aliasing (oversampled saturation, no clicks on retrigger), zero-click phase reset each hit, 20 ms max smoothing, bundled everything, offline, icon, manual PDF.

## Steps
1. You approve spec + name.
2. Interactive mockup (v1) with live sound.
3. Listen/tune in the lab.
4. JUCE port from the approved mockup.
5. Repo, screenshot, release zip.
