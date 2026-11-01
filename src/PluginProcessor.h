#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "engine/Bank.h"
#include "engine/Match.h"

struct BankEntry { const char* name; const char* group; const char* sub; int len; };
struct FactoryPreset { const char* name; const char* tag; const char* kv; const char* env; };
extern const BankEntry kBankInfo[];
extern const int kBankCount;
extern const FactoryPreset kFactory[];
extern const int kFactoryCount;

// One finished render of the current sound (immutable once published).
struct KickResult
{
    std::vector<float> pcm;          // the kick at the host sample rate
    double sr = 44100.0, tailRef = 55.0;
    kk::GlueInfo glue;
    double tp = 0, lufs = -120, rms = -120;
    std::vector<float> mags;         // magnitude spectrum of pcm (16384 point, 8192 bins)
    std::vector<float> body;         // body layer only is not kept; reserved
};
struct Published
{
    std::shared_ptr<const KickResult> base;
    std::array<std::shared_ptr<const std::vector<float>>, 128> notes {};
    uint64_t gen = 0;
};

class KickProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Thread
{
public:
    KickProcessor();
    ~KickProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Kick Kitten"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- sound state that is not a host parameter (guarded by stateLock) ----
    std::vector<float> renderOffline (const kk::Params&, double& rate);   // one finished kick on the calling thread (Render tab)
    uint64_t paramGen() const { return gen.load(); }
    kk::Params matchTo (const kk::RefInfo&, const std::function<void(int)>&, const std::atomic<bool>*);   // reference match on a copy of the current settings
    kk::Params snapshot() const;                 // all values as the engine wants them
    int  getSrc (int layer) const;               void setSrc (int layer, int v);
    int  getLayerPol (int layer) const;          void setLayerPol (int layer, int v);
    double getPol() const;                       void setPol (double v);
    std::vector<kk::EnvDot> getEnv() const;      void setEnv (const std::vector<kk::EnvDot>&);
    void setOwnSample (int layer, const juce::File& f, juce::String& error);
    juce::String ownName (int layer) const;
    const kk::Sample* ownSampleView (int layer) const { const juce::ScopedLock sl (stateLock); return own[layer].get(); }
    void setParam (const char* id, double realValue);          // with host gesture
    double getParam (int idx) const { return (double) *raw[(size_t) idx]; }

    // ---- presets ----
    void loadFactory (int index);
    void loadStateTree (const juce::ValueTree& t, bool keepName);
    juce::ValueTree currentTree();
    juce::File presetDir() const;
    juce::String presetName;                     // shown in the header
    bool presetEdited = false;
    int  factoryIndex = -1;

    // ---- playing ----
    juce::MidiKeyboardState keyState;
    std::atomic<int> keyMode { 1 };              // 0 same kick on every key, 1 pitched
    std::atomic<int> baseMidi { 24 };
    std::atomic<int> bassRoot { -1 };            // -1 off, 0..11 root note for the bass check
    std::atomic<bool> loopOn { false }, bassOn { false }, hitReq { false };
    std::atomic<double> bpm { 138.0 };
    std::atomic<int> lastNote { -1 };            // for the "played" chip
    std::atomic<float> meterL { 0 }, meterPeak { 0 };
    std::atomic<int> flash { 0 };

    std::shared_ptr<const Published> published() const { juce::SpinLock::ScopedLockType l (pubLock); return current; }
    void requestRender();
    int editorWidth = 0;
    double sampleRateNow() const { return sr.load(); }
    kk::Bank bank;

    juce::AudioProcessorValueTreeState apvts;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged (const juce::String&, float) override { requestRender(); }
    void run() override;
    void syncEnvFromKnobs();   // Start, Tail, Sweep and Curve knobs move the pitch dots (as in the mockup); runs on the render thread
    std::atomic<int> syncHold { 0 }; double lastSync[4] { 900, 55, 30, 1 };
    std::shared_ptr<const KickResult> renderBase (const kk::Params&, double rate);
    void startVoice (int midi, bool useNote);
    void rebuildBankHolder();

    std::array<std::atomic<float>*, (size_t) kk::id::NPARAMS> raw {};
    mutable juce::CriticalSection stateLock;
    int src[3] { 0, 0, 0 }, lpol[3] { 1, 1, 1 }; double pol = 1.0; std::vector<kk::EnvDot> env;
    std::shared_ptr<kk::Sample> own[3]; juce::String ownNames[3]; juce::MemoryBlock ownRaw[3];

    mutable juce::SpinLock pubLock;
    std::shared_ptr<const Published> current;
    std::vector<std::pair<std::shared_ptr<const Published>, juce::uint32>> graveyard;
    std::atomic<uint64_t> gen { 1 };
    juce::WaitableEvent wake;
    std::atomic<double> sr { 44100.0 };
    std::atomic<int> noteWanted { -1 };

    // audio-thread voice
    struct Voice { std::shared_ptr<const Published> pub; const std::vector<float>* buf = nullptr; double pos = 0, rate = 1; float fade = 1, fadeStep = 0; bool on = false; };
    Voice voice, old; std::shared_ptr<const Published> audioPub;
    double loopPhase = 0; int stepN = 0; double bassPh = 0, lp1 = 0, lp2 = 0; double bassFreq = 55; int bassRemain = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickProcessor)
};
