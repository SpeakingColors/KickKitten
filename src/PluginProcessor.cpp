#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include "engine/FactoryData.inc"

using namespace kk;

//==============================================================================
static juce::String paramName (int i)
{
    const juce::String idn (SPEC[i].id), lab (SPEC[i].label);
    if (idn.length() >= 3 && idn[0] == 'l' && juce::CharacterFunctions::isDigit (idn[1]) && idn[1] >= '0' && idn[1] <= '2')
        return "Layer " + juce::String (idn[1] - '0' + 1) + " " + lab.toLowerCase();
    return lab.substring (0, 1) + lab.substring (1).toLowerCase();
}
static bool isSwitch (const char* id) { return std::strcmp (id, "lim") == 0 || std::strcmp (id, "palign") == 0; }

juce::AudioProcessorValueTreeState::ParameterLayout KickProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < id::NPARAMS; ++i)
    {
        const auto& s = SPEC[i];
        const juce::ParameterID pid { s.id, 1 };
        if (isSwitch (s.id)) { l.add (std::make_unique<juce::AudioParameterBool> (pid, paramName (i), s.def > 0.5)); continue; }
        juce::NormalisableRange<float> r ((float) s.mn, (float) s.mx);
        if (s.lg)
            r = juce::NormalisableRange<float> ((float) s.mn, (float) s.mx,
                [] (float a, float b, float n) { return a * std::pow (b / a, n); },
                [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                [] (float a, float b, float v) { return juce::jlimit (a, b, v); });
        const int dec = s.dec; const juce::String unit (s.unit);
        juce::AudioParameterFloatAttributes at;
        at = at.withStringFromValueFunction ([dec, unit] (float v, int) { return juce::String (v, dec) + unit; })
               .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
        l.add (std::make_unique<juce::AudioParameterFloat> (pid, paramName (i), r, (float) s.def, at));
    }
    return l;
}

//==============================================================================
KickProcessor::KickProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("kick-render"),
      apvts (*this, nullptr, "STATE", makeLayout())
{
    bank.load (BinaryData::bank_bin, (size_t) BinaryData::bank_binSize);
    for (int i = 0; i < id::NPARAMS; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (SPEC[i].id);
        apvts.addParameterListener (SPEC[i].id, this);
    }
    loadFactory (0);
    startThread (juce::Thread::Priority::low);
}

KickProcessor::~KickProcessor()
{
    for (int i = 0; i < id::NPARAMS; ++i) apvts.removeParameterListener (SPEC[i].id, this);
    signalThreadShouldExit(); wake.signal(); stopThread (4000);
}

bool KickProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void KickProcessor::prepareToPlay (double rate, int)
{
    if (std::abs (sr.load() - rate) > 0.5) { sr = rate; requestRender(); }
    sr = rate;
    voice = {}; old = {}; loopPhase = 1e9; stepN = 0;
}

void KickProcessor::requestRender() { ++gen; wake.signal(); }

//==============================================================================
Params KickProcessor::snapshot() const
{
    Params p;
    for (int i = 0; i < id::NPARAMS; ++i) p.v[(size_t) i] = (double) *raw[(size_t) i];
    const juce::ScopedLock sl (stateLock);
    for (int i = 0; i < 3; ++i) { p.src[i] = src[i]; p.lpol[i] = lpol[i]; }
    p.pol = pol; p.env = env;
    return p;
}
int KickProcessor::getSrc (int l) const { const juce::ScopedLock sl (stateLock); return src[l]; }
void KickProcessor::setSrc (int l, int v) { { const juce::ScopedLock sl (stateLock); src[l] = v; } presetEdited = true; requestRender(); }
int KickProcessor::getLayerPol (int l) const { const juce::ScopedLock sl (stateLock); return lpol[l]; }
void KickProcessor::setLayerPol (int l, int v) { { const juce::ScopedLock sl (stateLock); lpol[l] = v; } presetEdited = true; requestRender(); }
double KickProcessor::getPol() const { const juce::ScopedLock sl (stateLock); return pol; }
void KickProcessor::setPol (double v) { { const juce::ScopedLock sl (stateLock); pol = v; } presetEdited = true; requestRender(); }
std::vector<EnvDot> KickProcessor::getEnv() const { const juce::ScopedLock sl (stateLock); return env; }
void KickProcessor::setEnv (const std::vector<EnvDot>& e) { { const juce::ScopedLock sl (stateLock); env = e; } presetEdited = true; requestRender(); }
juce::String KickProcessor::ownName (int l) const { const juce::ScopedLock sl (stateLock); return ownNames[l]; }

void KickProcessor::setParam (const char* idn, double v)
{
    if (auto* p = apvts.getParameter (idn)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) v)); p->endChangeGesture(); presetEdited = true; }
}

void KickProcessor::setOwnSample (int layer, const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (! rd) { error = "Could not read that file."; return; }
    const int n = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 2.0));
    if (n < 64) { error = "That file is too short."; return; }
    juce::AudioBuffer<float> b ((int) rd->numChannels, n); rd->read (&b, 0, n, 0, true, true);
    auto s = std::make_shared<Sample>(); s->sr = rd->sampleRate; s->data.assign ((size_t) n, 0.f);
    float pk = 1e-9f;
    for (int i = 0; i < n; ++i) { float a = 0; for (int c = 0; c < b.getNumChannels(); ++c) a += b.getSample (c, i); a /= (float) b.getNumChannels(); s->data[(size_t) i] = a; pk = juce::jmax (pk, std::abs (a)); }
    for (auto& v : s->data) v *= 0.9f / pk;
    for (int i = 0; i < 64 && i < n; ++i) s->data[(size_t) (n - 1 - i)] *= (float) i / 64.f;
    juce::MemoryBlock mb ((size_t) n * 2);
    auto* o = (juce::int16*) mb.getData(); for (int i = 0; i < n; ++i) o[i] = (juce::int16) juce::roundToInt (juce::jlimit (-1.f, 1.f, s->data[(size_t) i]) * 32767.f);
    { const juce::ScopedLock sl (stateLock); own[layer] = s; ownNames[layer] = f.getFileNameWithoutExtension(); ownRaw[layer] = mb; src[layer] = 7; }
    presetEdited = true; requestRender();
}

//==============================================================================
// Background render: the base kick first, then the keyboard notes one by one.
std::shared_ptr<const KickResult> KickProcessor::renderBase (const Params& p, double rate)
{
    std::shared_ptr<kk::Sample> ownCopy[3];
    { const juce::ScopedLock sl (stateLock); for (int i = 0; i < 3; ++i) ownCopy[i] = own[i]; }
    auto lookup = [&] (int layer, int s) -> const Sample* { return s == 7 ? ownCopy[layer].get() : bank.get (s); };
    auto r = std::make_shared<KickResult>();
    r->sr = rate; r->pcm = renderKick (p, rate, lookup, &r->glue);
    r->tailRef = p[id::tailHz];
    r->tp = truePeak (r->pcm); r->lufs = lufs400 (r->pcm, rate);
    { const size_t n = std::min (r->pcm.size(), (size_t) std::floor (0.12 * rate)); double s = 0; for (size_t i = 0; i < n; ++i) s += (double) r->pcm[i] * r->pcm[i]; r->rms = 10 * std::log10 (s / (double) std::max<size_t> (1, n) + 1e-12); }
    constexpr int NF = 16384;
    juce::dsp::FFT fft (14);
    std::vector<float> d ((size_t) NF * 2, 0.f); double ws = 0; const size_t n = std::min (r->pcm.size(), (size_t) NF);
    for (int i = 0; i < NF; ++i) { const double w = 0.5 * (1 + std::cos(juce::MathConstants<double>::pi * i / NF)); ws += w; if ((size_t) i < n) d[(size_t) i] = (float) (r->pcm[(size_t) i] * w); }
    fft.performFrequencyOnlyForwardTransform (d.data());
    r->mags.resize (NF / 2); for (int k = 0; k < NF / 2; ++k) r->mags[(size_t) k] = (float) (2.0 * d[(size_t) k] / ws);
    return r;
}

void KickProcessor::run()
{
    uint64_t done = 0; std::shared_ptr<const Published> pub; int sweepNext = 0; Params cur;
    while (! threadShouldExit())
    {
        const uint64_t g = gen.load();
        if (g != done)
        {
            cur = snapshot(); const double rate = sr.load();
            auto base = renderBase (cur, rate);
            if (threadShouldExit()) break;
            auto np = std::make_shared<Published>(); np->base = base; np->gen = g;
            { juce::SpinLock::ScopedLockType l (pubLock); current = np; }
            graveyard.emplace_back (np, juce::Time::getMillisecondCounter());
            for (size_t i = 0; i < graveyard.size();)
                if (graveyard[i].first.use_count() == 1 && graveyard[i].first != np && juce::Time::getMillisecondCounter() - graveyard[i].second > 5000) graveyard.erase (graveyard.begin() + (long) i); else ++i;
            pub = np; done = g; sweepNext = 0; continue;
        }
        int want = noteWanted.exchange (-1);
        if (want < 0 && keyMode.load() == 1 && pub)
        {
            const int b0 = baseMidi.load();                       // the three octaves on the on-screen keys, then a wider range
            static const int order[] = { 0 };
            (void) order;
            while (sweepNext < 37 + 60)
            {
                const int m = sweepNext < 37 ? b0 + sweepNext : 12 + (sweepNext - 37);
                ++sweepNext;
                if (m >= 0 && m < 128 && ! pub->notes[(size_t) m]) { want = m; break; }
            }
        }
        if (want >= 0 && want < 128 && pub && ! pub->notes[(size_t) want])
        {
            const double f = 440.0 * std::pow (2.0, (want - 69) / 12.0), ratio = f / cur[id::tailHz];
            Params q = cur; q[id::tailHz] = f; q[id::startHz] = cur[id::startHz] * ratio;
            for (auto& e : q.env) e.hz *= ratio;
            std::shared_ptr<kk::Sample> ownCopy[3];
            { const juce::ScopedLock sl (stateLock); for (int i = 0; i < 3; ++i) ownCopy[i] = own[i]; }
            auto lookup = [&] (int layer, int s) -> const Sample* { return s == 7 ? ownCopy[layer].get() : bank.get (s); };
            auto v = std::make_shared<std::vector<float>> (renderKick (q, sr.load(), lookup, nullptr));
            if (gen.load() != done) continue;
            auto np = std::make_shared<Published> (*pub); np->notes[(size_t) want] = v;
            { juce::SpinLock::ScopedLockType l (pubLock); current = np; }
            graveyard.emplace_back (np, juce::Time::getMillisecondCounter());
            for (size_t i = 0; i < graveyard.size();)
                if (graveyard[i].first.use_count() == 1 && graveyard[i].first != np && juce::Time::getMillisecondCounter() - graveyard[i].second > 5000) graveyard.erase (graveyard.begin() + (long) i); else ++i;
            pub = np; continue;
        }
        if (gen.load() == done) wake.wait (150);
    }
}

//==============================================================================
void KickProcessor::startVoice (int midi, bool)
{
    if (! audioPub || ! audioPub->base || audioPub->base->pcm.empty()) return;
    if (voice.on) { old = voice; old.fade = 1.f; old.fadeStep = (float) (1.0 / (0.001 * sr.load())); }
    voice = {}; voice.pub = audioPub; voice.on = true; voice.pos = 0; voice.rate = 1;
    voice.buf = &audioPub->base->pcm;
    if (midi >= 0 && keyMode.load() == 1)
    {
        if (audioPub->notes[(size_t) midi]) voice.buf = audioPub->notes[(size_t) midi].get();
        else { voice.rate = 440.0 * std::pow (2.0, (midi - 69) / 12.0) / audioPub->base->tailRef; noteWanted = midi; wake.signal(); }
    }
    lastNote = midi; flash = 1;
}

void KickProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = buf.getNumChannels();
    buf.clear();
    { juce::SpinLock::ScopedTryLockType l (pubLock); if (l.isLocked() && current != audioPub) audioPub = current; }
    keyState.processNextMidiBuffer (midi, 0, n, true);
    auto it = midi.begin(); const double rate = sr.load();
    const double stepLen = 60.0 / juce::jmax (30.0, bpm.load()) / 4.0 * rate;
    if (! loopOn.load()) { loopPhase = 1e9; stepN = 0; }
    float peak = 0;
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.end() && (*it).samplePosition <= i)
        {
            const auto m = (*it).getMessage(); ++it;
            if (m.isNoteOn()) startVoice (m.getNoteNumber(), true);
        }
        if (hitReq.exchange (false)) startVoice (-1, false);
        if (loopOn.load())
        {
            if (loopPhase >= stepLen)
            {
                loopPhase -= (loopPhase > 1e8 ? loopPhase : stepLen); if (loopPhase < 0) loopPhase = 0;
                if (stepN % 4 == 0) startVoice (-1, false);
                else if (bassOn.load() && audioPub && audioPub->base) { bassFreq = audioPub->base->tailRef * 2; bassRemain = (int) (stepLen * 0.85); bassPh = 0; }
                ++stepN;
            }
            loopPhase += 1.0;
        }
        float s = 0;
        auto pull = [&] (Voice& v, float gain)
        {
            if (! v.on || ! v.buf) return 0.f;
            const auto& b = *v.buf; const long ii = (long) v.pos;
            if (ii >= (long) b.size() - 1) { v.on = false; return 0.f; }
            const float fr = (float) (v.pos - (double) ii), x = b[(size_t) ii] * (1.f - fr) + b[(size_t) ii + 1] * fr;
            v.pos += v.rate; (void) gain; return x;
        };
        s += pull (voice, 1.f);
        if (old.on) { s += pull (old, old.fade) * old.fade; old.fade -= old.fadeStep; if (old.fade <= 0) { old.on = false; old.pub.reset(); } }
        if (bassRemain > 0)
        {
            bassPh += bassFreq / rate; if (bassPh >= 1) bassPh -= 1;
            const double saw = 2 * bassPh - 1, a = 1 - std::exp (-2 * juce::MathConstants<double>::pi * 420.0 / rate);
            lp1 += a * (saw - lp1); lp2 += a * (lp1 - lp2);
            const double env = std::min (1.0, (double) (stepLen * 0.85 - bassRemain) / (0.004 * rate)) * std::min (1.0, bassRemain / (0.012 * rate));
            s += (float) (lp2 * 0.16 * 1.6 * env); --bassRemain;
        }
        for (int c = 0; c < nch; ++c) buf.getWritePointer (c)[i] = s;
        peak = juce::jmax (peak, std::abs (s));
    }
    if (! voice.on && voice.pub) voice.pub.reset();
    meterL = peak; if (peak > meterPeak.load()) meterPeak = peak;
}

//==============================================================================
juce::File KickProcessor::presetDir() const
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Studio 87").getChildFile ("Kick Kitten").getChildFile ("presets");
    d.createDirectory(); return d;
}

juce::ValueTree KickProcessor::currentTree()
{
    auto t = apvts.copyState();
    t.setProperty ("kkver", 1, nullptr);
    { const juce::ScopedLock sl (stateLock);
      for (int i = 0; i < 3; ++i)
      {
          t.setProperty ("src" + juce::String (i), src[i], nullptr); t.setProperty ("lpol" + juce::String (i), lpol[i], nullptr);
          if (src[i] == 7 && ownRaw[i].getSize() > 0) { t.setProperty ("own" + juce::String (i), ownRaw[i].toBase64Encoding(), nullptr); t.setProperty ("ownsr" + juce::String (i), own[i] ? own[i]->sr : 44100.0, nullptr); t.setProperty ("ownname" + juce::String (i), ownNames[i], nullptr); }
      }
      t.setProperty ("pol", pol, nullptr);
      juce::String e; for (auto& d : env) e << d.ms << "," << d.hz << "," << d.bend << ";";
      t.setProperty ("env", e, nullptr); }
    t.setProperty ("keyMode", keyMode.load(), nullptr); t.setProperty ("baseMidi", baseMidi.load(), nullptr); t.setProperty ("bassRoot", bassRoot.load(), nullptr);
    t.setProperty ("presetName", presetName, nullptr); t.setProperty ("factoryIndex", factoryIndex, nullptr);
    return t;
}

void KickProcessor::loadStateTree (const juce::ValueTree& t, bool keepName)
{
    for (int i = 0; i < id::NPARAMS; ++i) if (auto* p = apvts.getParameter (SPEC[i].id)) p->setValueNotifyingHost (p->getDefaultValue());
    for (auto c : t) if (c.hasType ("PARAM"))
        if (auto* p = apvts.getParameter (c.getProperty ("id").toString())) p->setValueNotifyingHost (p->convertTo0to1 ((float) (double) c.getProperty ("value")));
    { const juce::ScopedLock sl (stateLock);
      for (int i = 0; i < 3; ++i)
      {
          src[i] = (int) t.getProperty ("src" + juce::String (i), 0); lpol[i] = (int) t.getProperty ("lpol" + juce::String (i), 1) < 0 ? -1 : 1;
          own[i].reset(); ownRaw[i].reset(); ownNames[i] = {};
          const auto b64 = t.getProperty ("own" + juce::String (i)).toString();
          if (b64.isNotEmpty()) { juce::MemoryBlock mb; if (mb.fromBase64Encoding (b64)) { auto s = std::make_shared<Sample>(); s->sr = (double) t.getProperty ("ownsr" + juce::String (i), 44100.0); const size_t n = mb.getSize() / 2; s->data.resize (n); auto* in = (const juce::int16*) mb.getData(); for (size_t k = 0; k < n; ++k) s->data[k] = in[k] / 32767.f; own[i] = s; ownRaw[i] = mb; ownNames[i] = t.getProperty ("ownname" + juce::String (i)).toString(); } }
          if (src[i] == 7 && ! own[i]) src[i] = 0;
      }
      pol = (double) t.getProperty ("pol", 1.0) < 0 ? -1.0 : 1.0;
      env.clear(); for (auto& d : juce::StringArray::fromTokens (t.getProperty ("env").toString(), ";", ""))
      { auto v = juce::StringArray::fromTokens (d, ",", ""); if (v.size() == 3) env.push_back ({ v[0].getDoubleValue(), v[1].getDoubleValue(), v[2].getDoubleValue() }); }
      if (env.size() < 2) env.clear(); }
    keyMode = (int) t.getProperty ("keyMode", 1); baseMidi = juce::jlimit (24, 60, (int) t.getProperty ("baseMidi", 24)); bassRoot = (int) t.getProperty ("bassRoot", -1);
    if (! keepName) { presetName = t.getProperty ("presetName", "").toString(); factoryIndex = (int) t.getProperty ("factoryIndex", -1); }
    presetEdited = false; requestRender();
}

void KickProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // the host-visible state: APVTS children are written as <PARAM id= value=> so old saves keep loading
    auto t = currentTree();
    juce::ValueTree out ("STATE");
    for (auto k = 0; k < t.getNumProperties(); ++k) out.setProperty (t.getPropertyName (k), t.getProperty (t.getPropertyName (k)), nullptr);
    for (auto c : t) out.appendChild (c.createCopy(), nullptr);
    if (auto xml = out.createXml()) copyXmlToBinary (*xml, dest);
}
void KickProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName ("STATE")) loadStateTree (juce::ValueTree::fromXml (*xml), false);
}

void KickProcessor::loadFactory (int index)
{
    if (index < 0 || index >= kFactoryCount) return;
    const auto& f = kFactory[index];
    juce::ValueTree t ("STATE");
    for (auto& kv : juce::StringArray::fromTokens (f.kv, ";", ""))
    {
        const auto key = kv.upToFirstOccurrenceOf ("=", false, false), val = kv.fromFirstOccurrenceOf ("=", false, false);
        if (key == "pol" || key == "l0src" || key == "l1src" || key == "l2src" || key == "l0pol" || key == "l1pol" || key == "l2pol")
        { t.setProperty (key == "l0pol" ? "lpol0" : key == "l1pol" ? "lpol1" : key == "l2pol" ? "lpol2" : key == "l0src" ? "src0" : key == "l1src" ? "src1" : key == "l2src" ? "src2" : "pol", (key == "pol" ? (val.getDoubleValue() < 0 ? -1.0 : 1.0) : (double) val.getIntValue()), nullptr); continue; }
        juce::ValueTree c ("PARAM"); c.setProperty ("id", key, nullptr); c.setProperty ("value", val.getDoubleValue(), nullptr); t.appendChild (c, nullptr);
    }
    juce::String e; for (auto& d : juce::StringArray::fromTokens (f.env, ";", "")) e << d << ";";
    t.setProperty ("env", e, nullptr);
    const int km = keyMode.load(), bm = baseMidi.load(), br = bassRoot.load();
    t.setProperty ("keyMode", km, nullptr); t.setProperty ("baseMidi", bm, nullptr); t.setProperty ("bassRoot", br, nullptr);
    loadStateTree (t, true);
    presetName = f.name; factoryIndex = index;
}

juce::AudioProcessorEditor* KickProcessor::createEditor() { return new KickEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KickProcessor(); }

// tables
const BankEntry kBankInfo[] = { KK_BANK };
const int kBankCount = (int) (sizeof (kBankInfo) / sizeof (kBankInfo[0]));
const FactoryPreset kFactory[] = { KK_FACTORY };
const int kFactoryCount = (int) (sizeof (kFactory) / sizeof (kFactory[0]));
