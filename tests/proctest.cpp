// Runs the real processor offline: sound on a note, pitched notes, hit, state round trip, old-state defaults.
#include "../src/PluginProcessor.h"
static int fails = 0;
#define CHECK(c, msg) do { if (! (c)) { std::printf ("FAIL: %s\n", msg); ++fails; } else std::printf ("ok:   %s\n", msg); } while (0)
static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
static std::vector<float> play (KickProcessor& p, juce::MidiBuffer in, int blocks = 40, int bs = 512)
{
    std::vector<float> out; juce::AudioBuffer<float> b (2, bs);
    for (int i = 0; i < blocks; ++i) { juce::MidiBuffer m; if (i == 0) m = in; b.clear(); p.processBlock (b, m); for (int s = 0; s < bs; ++s) out.push_back (b.getSample (0, s)); }
    return out;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    KickProcessor p; p.setPlayConfigDetails (0, 2, 44100.0, 512); p.prepareToPlay (44100.0, 512);
    for (int i = 0; i < 100 && ! (p.published() && p.published()->base); ++i) pump (50);
    CHECK (p.published() && p.published()->base, "first render published");
    pump (100);
    juce::AudioBuffer<float> tmp (2, 512); juce::MidiBuffer none; p.processBlock (tmp, none);   // lets the audio thread pick up the render
    auto base = p.published()->base;
    // 1 hit via MIDI, keys set to Same
    p.keyMode = 0; juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    auto out = play (p, on);
    double err = 0; for (size_t i = 0; i < base->pcm.size() - 2 && i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - base->pcm[i]));
    CHECK (err < 1e-6, "Same mode plays the rendered kick sample for sample");
    // pitched: octave up should shorten the cycle: compare zero crossing rate in the tail 100 to 200 ms
    p.keyMode = 1; pump (1500);
    p.processBlock (tmp, none);
    auto zc = [] (const std::vector<float>& v, size_t a, size_t b) { int z = 0; for (size_t i = a + 1; i < b && i < v.size(); ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++z; return z; };
    juce::MidiBuffer lo, hi; lo.addEvent (juce::MidiMessage::noteOn (1, 33, 0.8f), 0); hi.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);   // A1 and A2
    auto a = play (p, lo), b2 = play (p, hi);
    const int za = zc (a, 4410, 8820), zb = zc (b2, 4410, 8820);
    std::printf ("      zero crossings 100-200 ms: A1 %d, A2 %d\n", za, zb);
    CHECK (zb > za * 1.6 && zb < za * 2.4, "Pitched mode: one octave up doubles the tail frequency");
    // state round trip
    p.setParam ("decay", 333.0); p.setParam ("drive", 22.0); p.setSrc (2, 20); p.setLayerPol (1, -1); p.setPol (-1.0);
    p.setEnv ({ { 0, 700, 0.1 }, { 10, 200, 0 }, { 60, 50, 0 } });
    juce::MemoryBlock mb; p.getStateInformation (mb);
    KickProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
    const auto s1 = p.snapshot(), s2 = q.snapshot(); bool same = true; for (int i = 0; i < kk::id::NPARAMS; ++i) if (std::abs (s1.v[(size_t) i] - s2.v[(size_t) i]) > 1e-3) { same = false; std::printf ("      differs: %s %g vs %g\n", kk::SPEC[i].id, s1.v[(size_t) i], s2.v[(size_t) i]); }
    CHECK (same, "state round trip keeps every parameter");
    CHECK (s2.src[2] == 20 && s2.lpol[1] == -1 && s2.pol < 0 && s2.env.size() == 3 && std::abs (s2.env[0].hz - 700) < 1e-9, "state round trip keeps sources, polarity and pitch envelope");
    // an old or empty state still loads and gives defaults
    { juce::ValueTree t ("STATE"); auto xml = t.createXml(); juce::MemoryBlock m2; juce::AudioProcessor::copyXmlToBinary (*xml, m2); KickProcessor r; r.setStateInformation (m2.getData(), (int) m2.getSize());
      std::printf ("      decay after empty state %g, default %g\n", r.snapshot()[kk::id::decay], kk::SPEC[kk::id::decay].def); CHECK (std::abs (r.snapshot()[kk::id::decay] - kk::SPEC[kk::id::decay].def) < 1e-3, "empty state loads defaults"); }
    // every factory preset renders without NaN and under the ceiling
    bool ok = true; for (int i = 0; i < kFactoryCount; ++i) { q.loadFactory (i); const auto sn = q.snapshot(); auto lookup = [&] (int, int s) -> const kk::Sample* { return q.bank.get (s); }; auto r = kk::renderKick (sn, 44100.0, lookup); float pk = 0; for (float v : r) { if (! std::isfinite (v)) ok = false; pk = std::max (pk, std::abs (v)); } if (pk > 0.9999f || pk < 0.05f) ok = false; }
    CHECK (ok, "all factory presets render finite, audible and under full scale");
    // knobs move the pitch dots (as in the mockup)
    { KickProcessor m; m.loadFactory (0); juce::Thread::sleep (600); const auto e0 = m.getEnv(); CHECK (e0.size() >= 2, "factory preset has pitch dots");
      m.setParam ("tailHz", 48.0); m.setParam ("startHz", 1200.0); juce::Thread::sleep (800); const auto e1 = m.getEnv();
      CHECK (std::abs (e1.back().hz - 48.0) < 0.01 && std::abs (e1.front().hz - 1200.0) < 0.01, "Tail and Start knobs move the first and last dot");
      std::printf ("      last dot %g -> %g, first %g -> %g\n", e0.back().hz, e1.back().hz, e0.front().hz, e1.front().hz);
      m.loadFactory (1); juce::Thread::sleep (600); const auto e2 = m.getEnv(); m.loadFactory (1); juce::Thread::sleep (300); CHECK (m.getEnv().size() == e2.size() && std::abs (m.getEnv().back().hz - e2.back().hz) < 1e-9, "loading a preset does not rewrite its dots"); }
    // reference match through the processor: a known kick as reference, start from another one
    { KickProcessor m; m.loadFactory (0); auto sn = m.snapshot(); sn[kk::id::decay] = 380; sn[kk::id::tailHz] = 50; sn.env.clear(); auto lookup = [&] (int, int s2) -> const kk::Sample* { return m.bank.get (s2); };
      auto ref = kk::renderKick (sn, 44100.0, lookup); kk::RefInfo ri; const bool mk = kk::makeRef (ref, 44100.0, ri); CHECK (mk && std::abs (ri.tail - 50) < 3, "reference tail pitch is found");
      m.loadFactory (1); const auto before = m.snapshot(); const auto after = m.matchTo (ri, nullptr, nullptr);
      auto ld = [&] (int, int s2) -> const kk::Sample* { return m.bank.get (s2); };
      const double d0 = kk::featDist (kk::featOf (kk::renderKick (before, 44100.0, ld), 44100.0), ri.feat), d1 = kk::featDist (kk::featOf (kk::renderKick (after, 44100.0, ld), 44100.0), ri.feat);
      std::printf ("      match distance %.1f -> %.1f\n", d0, d1); CHECK (d1 < d0, "match brings the kick closer to the reference"); }
    std::printf (fails ? "PROC TEST FAILED\n" : "PROC TEST OK\n"); return fails ? 1 : 0;
}
