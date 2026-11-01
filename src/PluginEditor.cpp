#include "PluginEditor.h"
#include <functional>
#include <BinaryData.h>

using namespace juce;
using namespace kk;

namespace
{
struct LItem { const char *tag, *cls, *id, *kid, *text, *tip; int x, y, w, h; };
#include "ui/LayoutTable.inc"

const LItem* byId (const char* id) { for (auto& i : kItems) if (std::strcmp (i.id, id) == 0 && *id) return &i; return nullptr; }
const LItem* byKid (const char* k) { for (auto& i : kItems) if (std::strcmp (i.kid, k) == 0 && *k) return &i; return nullptr; }
Rectangle<int> rect (const LItem* i) { return i ? Rectangle<int> (i->x, i->y, i->w, i->h) : Rectangle<int>(); }
bool clsIs (const LItem& i, const char* first) { return String (i.cls).upToFirstOccurrenceOf (" ", false, false) == first; }

namespace col
{
    const Colour ground { 0xff121214 }, panel { 0xff1b1b1e }, display { 0xff08080a }, line { 0xff34343a }, text { 0xfff2f2f4 }, dim { 0xffa0a0a8 }, cap { 0xff7fc8f0 }, grid { 0xff23262b },
                 curve { 0xff02EBBA }, curve2 { 0xffFFA600 }, read { 0xff02EBBA }, click { 0xffFF1383 }, body { 0xffFFA600 }, sub { 0xff02EBBA }, ok { 0xff6CC9F5 }, warn { 0xffA43DFF }, clip { 0xffFF1383 },
                 knob { 0xffe9e9ee }, track { 0xff3a3a42 }, btn { 0xff2b2b31 }, btnh { 0xff38383f }, sel { 0xff4a3a12 }, selb { 0xffFFA600 };
}
struct Faces
{
    Typeface::Ptr reg, semi, bold, mreg, msemi;
    Faces()
    {
        reg = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedRegular_ttf, (size_t) BinaryData::BarlowCondensedRegular_ttfSize);
        semi = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, (size_t) BinaryData::BarlowCondensedSemiBold_ttfSize);
        bold = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, (size_t) BinaryData::BarlowCondensedBold_ttfSize);
        mreg = Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoRegular_ttf, (size_t) BinaryData::JetBrainsMonoRegular_ttfSize);
        msemi = Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoSemiBold_ttf, (size_t) BinaryData::JetBrainsMonoSemiBold_ttfSize);
    }
};
Faces& faces() { static Faces f; return f; }
// Barlow Condensed and JetBrains Mono are bundled (SIL Open Font License), so the look is the same offline on every system.
Font sans (float h, bool bold = false, float tracking = 0.04f)
{ return Font (FontOptions (bold ? faces().bold : faces().reg).withPointHeight (h).withKerningFactor (tracking)); }
Font mono (float h, bool bold = false)
{ return Font (FontOptions (bold ? faces().msemi : faces().mreg).withPointHeight (h)); }
// symbols the bundled latin fonts do not hold (arrows, triangles) use the system font
bool needsSystemFont (const String& t) { for (auto c : t) if (c > 0xFF && c != 0x2212) return true; return false; }
Font symFont (float h) { return Font (FontOptions (h)); }

const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
struct NoteInfo { String name; double cents; };
NoteInfo hz2note (double f)
{
    const double n = 12 * std::log2 (f / 440.0) + 69; const int r = (int) std::lround (n);
    return { String (kNoteNames[((r % 12) + 12) % 12]) + String ((int) std::floor (r / 12.0) - 1), (n - r) * 100.0 };
}
String num (double v, int dec) { return dec <= 0 ? String ((int) std::lround (v)) : String (v, dec); }
String fmtVal (const SpecInfo& s, double v) { return num (v, s.dec) + s.unit; }
String fmtSm (const SpecInfo& s, double v)
{
    const String u = String (s.unit).trim();
    String n = std::abs (v) >= 1000 ? String (v / 1000.0, 1) + "k" : num (v, s.dec);
    if (v > 0 && u == "st") n = "+" + n;
    return n + ((u == "ms" || u == "st") ? u : String());
}
double clampd (double v, double a, double b) { return std::min (b, std::max (a, v)); }
}

struct KickEditor::Content : public Component, private Timer
{
    //==========================================================================
    struct Hov : public Component
    {
        Content& c; String tip;
        explicit Hov (Content& o) : c (o) {}
        void mouseEnter (const MouseEvent&) override { if (tip.isNotEmpty()) c.tipFor (this); }
        void mouseExit (const MouseEvent&) override { c.hideTip(); }
        void mouseDown (const MouseEvent&) override { c.hideTip(); }
    };

    struct Knob : public Hov
    {
        int idx; bool small; float lastNorm = -1.f;
        Knob (Content& o, int i, bool sm) : Hov (o), idx (i), small (sm) { setMouseCursor (MouseCursor::UpDownResizeCursor); }
        RangedAudioParameter* par() const { return c.proc.apvts.getParameter (SPEC[idx].id); }
        void tick() { const float n = par()->getValue(); if (n != lastNorm) { lastNorm = n; repaint(); } }
        void paint (Graphics& g) override
        {
            const auto& s = SPEC[idx]; const double v = c.proc.getParam (idx);
            const float n = jlimit (0.f, 1.f, par()->getValue());
            const float lh = small ? 11.f : 13.f, dia = small ? 34.f : 46.f, vh = small ? 12.f : 15.f;
            auto b = getLocalBounds().toFloat();
            g.setColour (col::dim); g.setFont (sans (small ? 9.f : 11.f, false, small ? 0.04f : 0.1f));
            g.drawText (String (s.label).toUpperCase(), b.removeFromTop (lh), Justification::centred, false);
            const auto d = b.removeFromTop (dia).withSizeKeepingCentre (dia, dia);
            const float r = 19.f * dia / 48.f, sw = 3.5f * dia / 48.f;
            const auto ctr = d.getCentre(); const float a0 = degreesToRadians (-135.f), a1 = degreesToRadians (135.f), av = a0 + (a1 - a0) * n;
            Path tr; tr.addCentredArc (ctr.x, ctr.y, r, r, 0, a0, a1, true);
            g.setColour (col::track); g.strokePath (tr, PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
            if (n > 0.005f) { Path vp; vp.addCentredArc (ctr.x, ctr.y, r, r, 0, a0, av, true); g.setColour (col::knob); g.strokePath (vp, PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded)); }
            const float pl = 13.f * dia / 48.f; g.setColour (col::text);
            g.drawLine (ctr.x, ctr.y, ctr.x + std::sin (av) * pl, ctr.y - std::cos (av) * pl, 2.f * dia / 48.f * 1.2f);
            g.setColour (col::read); g.setFont (mono (small ? 9.f : 11.f, true));
            g.drawText (small ? fmtSm (s, v) : fmtVal (s, v), b.removeFromTop (vh), Justification::centred, false);
        }
        void mouseDown (const MouseEvent& e) override { Hov::mouseDown (e); start = par()->getValue(); par()->beginChangeGesture(); }
        void mouseDrag (const MouseEvent& e) override
        {
            const float fine = e.mods.isShiftDown() ? 1000.f : 200.f;
            par()->setValueNotifyingHost (jlimit (0.f, 1.f, start - (float) e.getDistanceFromDragStartY() / fine)); c.proc.presetEdited = true;
        }
        void mouseUp (const MouseEvent&) override { par()->endChangeGesture(); }
        void mouseDoubleClick (const MouseEvent&) override { par()->beginChangeGesture(); par()->setValueNotifyingHost (par()->getDefaultValue()); par()->endChangeGesture(); c.proc.presetEdited = true; }
        void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
        { par()->beginChangeGesture(); par()->setValueNotifyingHost (jlimit (0.f, 1.f, par()->getValue() + (w.deltaY > 0 ? 0.01f : -0.01f) * (w.deltaY == 0 ? 0.f : 1.f))); par()->endChangeGesture(); }
        float start = 0;
    };

    enum class BS { normal, mini, chip, arrow, pick };
    struct Btn : public Hov
    {
        String text; BS style; bool on = false, disabled = false; std::function<void()> onClick; Colour textCol = col::text;
        Btn (Content& o, BS s, String t) : Hov (o), text (std::move (t)), style (s) { setMouseCursor (MouseCursor::PointingHandCursor); }
        void paint (Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (0.5f);
            const bool hot = isMouseOver();
            Colour bg = on ? col::sel : (hot ? col::btnh : col::btn); if (style == BS::chip) bg = col::display;
            g.setColour (bg); g.fillRoundedRectangle (b, style == BS::chip ? 3.f : 4.f);
            g.setColour (on ? col::selb : (style == BS::chip && hot ? col::selb : col::line)); g.drawRoundedRectangle (b, style == BS::chip ? 3.f : 4.f, 1.f);
            g.setColour (disabled ? col::dim.withAlpha (0.5f) : textCol);
            const bool sym = needsSystemFont (text);
            if (style == BS::chip) { g.setFont (sym ? symFont (11.f) : mono (11.f, true)); g.setColour (on ? col::read : textCol); }
            else if (style == BS::pick) g.setFont (sym ? symFont (12.f) : (textCol == col::read ? mono (12.f, true) : sans (13.f, false, 0.03f))); else g.setFont (sym ? symFont (13.f) : sans (style == BS::mini ? 11.f : 13.f, false, 0.03f));
            g.drawText (text, b.reduced (style == BS::pick ? 8.f : 3.f, 0.f), style == BS::pick ? Justification::centredLeft : Justification::centred, true);
        }
        void mouseUp (const MouseEvent& e) override { if (! disabled && e.mouseWasClicked() && contains (e.getPosition()) && onClick) onClick(); }
        void mouseEnter (const MouseEvent& e) override { Hov::mouseEnter (e); repaint(); }
        void mouseExit (const MouseEvent& e) override { Hov::mouseExit (e); repaint(); }
    };

    struct Chip : public Component
    {
        String text; Colour colour = col::read;
        void paint (Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (col::display); g.fillRoundedRectangle (b, 3.f); g.setColour (col::line); g.drawRoundedRectangle (b, 3.f, 1.f);
            g.setColour (colour); g.setFont (needsSystemFont (text) ? symFont (11.f) : mono (11.f, true)); g.drawText (text, b, Justification::centred, true);
        }
    };

    // a display: dark rounded box, the drawing function paints inside
    struct Canvas : public Hov
    {
        std::function<void (Graphics&, int, int)> draw; std::function<void (const MouseEvent&)> onDragFn, onClickFn;
        explicit Canvas (Content& o) : Hov (o) {}
        void mouseDrag (const MouseEvent& e) override { if (onDragFn) onDragFn (e); }
        void mouseUp (const MouseEvent& e) override { if (onClickFn && e.mouseWasClicked()) onClickFn (e); }
        void paint (Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (col::display); g.fillRoundedRectangle (b, 3.f);
            { Graphics::ScopedSaveState ss (g); g.reduceClipRegion (getLocalBounds().reduced (1)); if (draw) draw (g, getWidth(), getHeight()); }
            g.setColour (col::line); g.drawRoundedRectangle (b.reduced (0.5f), 3.f, 1.f);
        }
    };
    static void txt (Graphics& g, const String& s, float x, float y, Colour c, Justification j, float h = 10.f, bool bold = false)
    {
        Graphics::ScopedSaveState ss (g);
        g.setColour (c); g.setFont (mono (h, bold));
        const float w = 400.f;
        if (j == Justification::left || j == Justification::centredLeft) g.drawText (s, Rectangle<float> (x, y - h, w, h + 3.f), Justification::bottomLeft, false);
        else if (j == Justification::right) g.drawText (s, Rectangle<float> (x - w, y - h, w, h + 3.f), Justification::bottomRight, false);
        else g.drawText (s, Rectangle<float> (x - w / 2, y - h, w, h + 3.f), Justification::centredBottom, false);
    }

    //==========================================================================
    KickEditor& owner; KickProcessor& proc;
    OwnedArray<Knob> knobs; OwnedArray<Btn> btns; OwnedArray<Chip> chips; OwnedArray<Canvas> canvases;
    Component* piano = nullptr;
    struct TipBox : public Component
    {
        String text; AttributedString as; float tw = 300.f;
        void set (const String& t) { text = t; as = AttributedString(); as.setJustification (Justification::topLeft); as.append (t, Font (FontOptions (12.5f)), col::text); TextLayout tl; tl.createLayout (as, tw); setSize ((int) tw + 16, (int) tl.getHeight() + 12); }
        void paint (Graphics& g) override { auto b = getLocalBounds().toFloat().reduced (1.f); g.setColour (Colour (0xf0101014)); g.fillRoundedRectangle (b, 5.f); g.setColour (col::cap); g.drawRoundedRectangle (b, 5.f, 1.f); as.draw (g, b.reduced (8.f, 6.f)); }
    } tipBox;
    Hov* tipTarget = nullptr; int tipDelay = 0; bool bubblesOn = true;
    int renderTab = 0;
    std::shared_ptr<const Published> pub; uint64_t lastGen = 0;
    float lvl = -99.f, holdDb = -99.f; int holdT = 0;
    String refMsg;
    Btn *pkRead = nullptr, *hitBtnP = nullptr, *keyChipBtn = nullptr, *refBtn = nullptr, *renderSave = nullptr, *renderCopy = nullptr; String bpmTip; bool dragStarted = false; uint32_t qDown = 0;
    const Sample* ownView (int layer);
    void askText (const String& title, const String& msg, const String& initial, std::function<void (String)> done);
    void writePreset (const String& name);
    void dragWavOut(); void loadReference (const File& f);
    bool keyPressed (const KeyPress&) override; bool keyStateChanged (bool) override; void mouseDown (const MouseEvent&) override;
    static int keyOffset (juce_wchar c);

    Content (KickEditor& o, KickProcessor& p);
    void tipFor (Hov* h) { if (bubblesOn) { tipTarget = h; tipDelay = 14; } }
    void hideTip() { tipTarget = nullptr; tipDelay = 0; tipBox.setVisible (false); }
    void timerCallback() override;
    void paint (Graphics& g) override;
    void paintWood (Graphics& g, Rectangle<int> r, bool left);
    void paintChrome (Graphics& g);

    // helpers used by the builders
    Btn* addBtn (const LItem* it, BS style, const String& text, std::function<void()> fn)
    { auto* b = btns.add (new Btn (*this, style, text)); if (it) { b->setBounds (rect (it)); b->tip = it->tip; } b->onClick = std::move (fn); addAndMakeVisible (b); return b; }
    Canvas* addCanvas (const LItem* it, std::function<void (Graphics&, int, int)> fn)
    { auto* v = canvases.add (new Canvas (*this)); v->setBounds (rect (it)); v->tip = it->tip; v->draw = std::move (fn); addAndMakeVisible (v); return v; }
    Chip* addChip (const LItem* it) { auto* c2 = chips.add (new Chip()); c2->setBounds (rect (it)); addAndMakeVisible (c2); return c2; }
    Chip* addChipAt (Rectangle<int> r) { auto* c2 = chips.add (new Chip()); c2->setBounds (r); addAndMakeVisible (c2); return c2; }

    void build();
    // part 2 (defined below)
    void buildPitch(); void buildScopes(); void buildPresetBar(); void buildLayers(); void buildPlay(); void buildGlueRender();
    void refreshUi();
    void stepPreset (int dir);
    void showPresetMenu();
    void doSave(); void doRename(); void doDelete(); void doExport(); void doImport(); void showGuide(); void showSettings(); void showAbout();
    void pickSource (int layer); void stepSource (int layer, int dir); void applySrc (int layer, int v, bool gesture); void loadSample (int layer);
    void auditionSoon();
    void exportWav();
    static String hintFor (const String& t);

    // state used by the draw lambdas
    Chip *chipTail = nullptr, *chipPct = nullptr, *chipFirst = nullptr, *chipPk = nullptr, *chipRms = nullptr, *chipLufs = nullptr, *chipGPk = nullptr, *chipGGr = nullptr, *bassChip = nullptr, *playedChip = nullptr;
    Btn *nameBtn = nullptr, *limBtn = nullptr, *alignBtn = nullptr, *polBtn = nullptr, *kmS = nullptr, *kmP = nullptr, *loopBtn = nullptr, *bassBtn = nullptr, *tabGlue = nullptr, *tabRender = nullptr, *envResetBtn = nullptr;
    Btn* pickBtn[3] {}; Btn* plBtn[3] {};
    Canvas *cvPitch = nullptr, *cvSpec = nullptr, *cvWave = nullptr, *cvGap = nullptr, *cvGlue = nullptr, *cvMeter = nullptr, *cvLw[3] {};
    TextEditor bpmBox; Btn *selKey = nullptr, *selBass = nullptr;
    std::vector<Component*> glueGroup, renderGroup;
    Btn* zPct = nullptr;
    std::vector<EnvDot> dragEnv; int dragK = -1; int dragType = 0; // 0 none, 1 dot, 2 segment
    double dragOx = 0, dragOy = 0, dragSeg = 0;
    std::vector<float> refMags; double refSr = 44100; String refName;
    std::vector<int> order;
    juce::FileChooser* chooser = nullptr; std::unique_ptr<FileChooser> chooserHolder;
    std::unique_ptr<AlertWindow> dlg;
};

//==============================================================================
// helpers shared by the display code
namespace
{
std::vector<EnvDot> legacyEnv (const Params& P)
{
    const double sw = P[id::sweep], tl = P[id::tailHz], st = P[id::startHz], cv = P[id::curve];
    auto fe = [&] (double t) { return tl + (st - tl) * std::exp (-3 * std::pow (t / sw, cv)); };
    const double T[3] = { 0, std::min (sw, 90.0), std::min (100.0, std::max (sw * 2.4, sw + 4)) };
    std::vector<EnvDot> pts { { T[0], fe (T[0]), 0 }, { T[1], fe (T[1]), 0 }, { T[2], fe (T[2]), 0 } };
    pts[2].hz = tl;
    for (int i = 0; i < 2; ++i)
    {
        const double m = (T[i] + T[i + 1]) / 2, a = pts[(size_t) i].hz, b = pts[(size_t) i + 1].hz, v = (std::log (fe (m)) - std::log (a)) / (std::log (b) - std::log (a));
        if (std::isfinite (v) && v > 0.02 && v < 0.98) pts[(size_t) i].bend = clampd (std::log2 (std::log (v) / std::log (0.5)) / 2, -1, 1);
    }
    return pts;
}
std::vector<float> specCurve (const std::vector<float>& mags, double sr, int wpx, double f0, double f1)
{
    constexpr int NF = 16384; std::vector<float> out ((size_t) wpx), sm ((size_t) wpx); const double df = sr / NF;
    for (int x = 0; x < wpx; ++x)
    {
        const double fa = f0 * std::pow (f1 / f0, (double) x / wpx), fb = f0 * std::pow (f1 / f0, (double) (x + 1) / wpx), ka = fa / df, kb = fb / df; double p;
        if (kb - ka < 1)
        {
            const double k = (ka + kb) / 2; const int i = (int) std::floor (k); const double fr = k - i;
            const double da = 20 * std::log10 (mags[(size_t) i] + 1e-9), db = 20 * std::log10 (mags[(size_t) std::min ((size_t) i + 1, mags.size() - 1)] + 1e-9); p = da + (db - da) * fr;
        }
        else { double s = 0; int c = 0; for (int k = (int) std::ceil (ka); k <= (int) std::floor (kb) && k < (int) mags.size(); ++k) { s += (double) mags[(size_t) k] * mags[(size_t) k]; ++c; } p = 10 * std::log10 (s / std::max (1, c) + 1e-18); }
        out[(size_t) x] = (float) p;
    }
    for (int x = 0; x < wpx; ++x) sm[(size_t) x] = (out[(size_t) std::max (0, x - 1)] + out[(size_t) x] * 2 + out[(size_t) std::min (wpx - 1, x + 1)]) / 4;
    return sm;
}
std::vector<int> srcLens()
{
    std::vector<int> v { 12, 45, 40, 60, 80, 100, 90, 80 };
    for (int i = 0; i < kBankCount; ++i) v.push_back (kBankInfo[i].len);
    return v;
}
String srcName (int v)
{
    static const char* base[8] = { "Off", "Click", "Tick", "Snap", "Zap", "Noise top", "Ping", "Sample" };
    if (v < 8) return base[v];
    return v - 8 < kBankCount ? String (kBankInfo[v - 8].name) : String ("Off");
}
String dbStr (double d) { return String (d, 1) + " dB"; }
}

String KickEditor::Content::hintFor (const String& t)
{
    if (t.startsWith ("PITCH")) return "double click adds or removes a dot; drag dots and the line";
    if (t.startsWith ("SAUCE")) return "Weight, punch, crack and loudness that make trance kicks hit hard. Amount 0 is off, and moving any knob below switches it on.";
    return {};
}

//==============================================================================
struct PitchCanvas : public KickEditor::Content::Canvas
{
    using Canvas = KickEditor::Content::Canvas;
    explicit PitchCanvas (KickEditor::Content& o) : Canvas (o) {}
    struct Geo { double l, r, t, b; };
    Geo gm() const { return { 44, (double) getWidth() - 46, 12, (double) getHeight() - 22 }; }
    static double fyOf (double f, const Geo& g) { return g.t + (1 - std::log (f / 20) / std::log (200.0)) * (g.b - g.t); }
    static double fxOf (double ms, const Geo& g) { return g.l + ms / 100 * (g.r - g.l); }
    static double toHz (double y, const Geo& g) { return 20 * std::pow (200.0, 1 - (y - g.t) / (g.b - g.t)); }
    static double toMs (double x, const Geo& g) { return (x - g.l) / (g.r - g.l) * 100; }
    std::vector<EnvDot> dots() const { auto e = c.proc.getEnv(); return e.size() >= 2 ? e : legacyEnv (c.proc.snapshot()); }
    struct H { int type = 0, k = -1; };   // 1 dot, 2 segment
    H hit (Point<double> p) const
    {
        const auto g = gm(); const auto d = dots();
        for (int k = (int) d.size() - 1; k >= 0; --k) if (std::hypot (p.x - fxOf (d[(size_t) k].ms, g), p.y - fyOf (clampd (d[(size_t) k].hz, 20, 4000), g)) < 14) return { 1, k };
        if (p.x >= g.l && p.x <= g.r)
        {
            const double ms = toMs (p.x, g); const double f = envValue (d, ms);
            if (std::abs (p.y - fyOf (clampd (f, 20, 4000), g)) < 8) { for (int i = 0; i + 1 < (int) d.size(); ++i) if (ms >= d[(size_t) i].ms && ms < d[(size_t) i + 1].ms) return { 2, i }; }
        }
        return {};
    }
    void ensure() { if (c.proc.getEnv().size() < 2) { c.proc.setEnv (legacyEnv (c.proc.snapshot())); } }
    void mouseMove (const MouseEvent& e) override { const auto h = hit (e.position.toDouble()); setMouseCursor (h.type == 2 ? MouseCursor::UpDownResizeCursor : h.type == 1 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor); }
    void mouseDown (const MouseEvent& e) override
    {
        Canvas::mouseDown (e); const auto p = e.position.toDouble(); const auto h = hit (p); const auto g = gm();
        dragType = 0; if (! h.type) return;
        ensure(); auto d = c.proc.getEnv();
        if (h.type == 1) { dragType = 1; dragK = h.k; dragOx = p.x - fxOf (d[(size_t) h.k].ms, g); dragOy = p.y - fyOf (clampd (d[(size_t) h.k].hz, 20, 4000), g); }
        else { const double ms = toMs (p.x, g); int i = 0; for (int k = 0; k + 1 < (int) d.size(); ++k) if (ms >= d[(size_t) k].ms) i = k; dragType = 2; dragK = i; }
        if (dragType == 1 && (dragK == 0 || dragK == (int) d.size() - 1)) { c.proc.apvts.getParameter (dragK == 0 ? "startHz" : "tailHz")->beginChangeGesture(); gestureOpen = true; }
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragType) return; const auto p = e.position.toDouble(); const auto g = gm(); auto d = c.proc.getEnv(); if (d.size() < 2) return;
        const int n = (int) d.size();
        if (dragType == 1 && dragK < n)
        {
            const int k = dragK;
            d[(size_t) k].ms = clampd (toMs (p.x - dragOx, g), k == 0 ? 0 : d[(size_t) k - 1].ms + 1, k < n - 1 ? d[(size_t) k + 1].ms - 1 : 100);
            const double hz = clampd (toHz (p.y - dragOy, g), 20, 4000);
            d[(size_t) k].hz = hz;
            if (k == 0 || k == n - 1) { auto* par = c.proc.apvts.getParameter (k == 0 ? "startHz" : "tailHz"); par->setValueNotifyingHost (par->convertTo0to1 ((float) hz)); }
            c.proc.setEnv (d);
        }
        else if (dragType == 2 && dragK + 1 < n)
        {
            auto& a = d[(size_t) dragK]; auto& b = d[(size_t) dragK + 1];
            const double u = (toMs (p.x, g) - a.ms) / (b.ms - a.ms), fyv = toHz (p.y, g), v = (std::log (fyv) - std::log (a.hz)) / (std::log (b.hz) - std::log (a.hz));
            if (u > 0.08 && u < 0.92 && std::isfinite (v) && v > 0.02 && v < 0.98 && std::abs (std::log (b.hz / a.hz)) > 0.05) { a.bend = clampd (std::log2 (std::log (v) / std::log (u)) / 2, -1, 1); c.proc.setEnv (d); }
        }
    }
    void mouseUp (const MouseEvent&) override
    {
        if (gestureOpen) { c.proc.apvts.getParameter (dragK == 0 ? "startHz" : "tailHz")->endChangeGesture(); gestureOpen = false; }
        dragType = 0; dragK = -1;
    }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        const auto p = e.position.toDouble(); const auto g = gm(); const auto h = hit (p);
        if (h.type == 1) { ensure(); auto d = c.proc.getEnv(); const int n = (int) d.size();
            if (h.k == 0) c.proc.setParam ("startHz", SPEC[id::startHz].def); else if (h.k == n - 1) c.proc.setParam ("tailHz", SPEC[id::tailHz].def);
            else { d.erase (d.begin() + h.k); c.proc.setEnv (d); }
            return; }
        if (p.x < g.l || p.x > g.r || p.y < g.t || p.y > g.b) return;
        ensure(); auto d = c.proc.getEnv(); if (d.size() >= 8) return;
        const double ms = clampd (toMs (p.x, g), 1, 99); size_t k = 1; while (k < d.size() && d[k].ms < ms) ++k;
        if (d[k - 1].ms > ms - 1 || (k < d.size() && d[k].ms < ms + 1)) return;
        d.insert (d.begin() + (long) k, { ms, clampd (toHz (p.y, g), 20, 4000), 0.0 }); c.proc.setEnv (d);
    }
    int dragType = 0, dragK = -1; double dragOx = 0, dragOy = 0; bool gestureOpen = false;
};

//==============================================================================
struct PianoView : public Component
{
    KickProcessor& proc; KickEditor::Content& content; int heldMouse = -1;
    PianoView (KickProcessor& p, KickEditor::Content& c) : proc (p), content (c) {}
    static bool isWhite (int k) { k %= 12; return k == 0 || k == 2 || k == 4 || k == 5 || k == 7 || k == 9 || k == 11; }
    static const char* labelFor (int off)
    {
        static const char* l[18] = { "A", "W", "S", "E", "D", "F", "T", "G", "Y", "H", "U", "J", "K", "O", "L", "P", ";", "'" };
        return off >= 0 && off < 18 ? l[off] : "";
    }
    int noteAt (Point<float> p) const
    {
        const float ww = getWidth() / 21.f; const int base = proc.baseMidi.load();
        int wi = 0; int hitNote = -1;
        for (int i = 0; i < 36; ++i)       // black keys first (on top)
        {
            const int m = base + i; if (isWhite (i)) { ++wi; continue; }
            const float bw = ww * 0.62f, x = wi * ww - bw / 2;
            if (p.x >= x && p.x < x + bw && p.y < getHeight() * 0.6f) hitNote = m;
        }
        if (hitNote >= 0) return hitNote;
        wi = 0;
        for (int i = 0; i < 36; ++i) { if (! isWhite (i)) continue; if (p.x >= wi * ww && p.x < (wi + 1) * ww) return base + i; ++wi; }
        return -1;
    }
    void paint (Graphics& g) override
    {
        const float ww = getWidth() / 21.f; const int base = proc.baseMidi.load();
        auto drawKey = [&] (int i, bool white, float x, float w)
        {
            const int m = base + i; const bool on = proc.keyState.isNoteOn (1, m);
            Rectangle<float> r (x, 0, w, white ? (float) getHeight() : getHeight() * 0.6f);
            if (white) { g.setColour (on ? col::read : Colour (0xffe9e9ee)); g.fillRoundedRectangle (r.withTrimmedTop (-4), 4.f); g.setColour (Colour (0xff14141a)); g.drawRoundedRectangle (r.reduced (0.5f).withTrimmedTop (-4), 4.f, 1.f);
                if (i % 12 == 0) { g.setColour (col::cap); g.fillRect (r.getX() + 1, r.getBottom() - 3, r.getWidth() - 2, 3.f); } }
            else { g.setColour (on ? col::selb : Colour (0xff17171b)); g.fillRoundedRectangle (r.withTrimmedTop (-4), 3.f); g.setColour (Colours::black); g.drawRoundedRectangle (r.withTrimmedTop (-4), 3.f, 1.f); }
            g.setColour (white ? Colour (0xff55555e) : Colour (0xff99aaaa)); g.setFont (mono (10.f, true)); g.drawText (labelFor (i), r.withTrimmedBottom (3), Justification::centredBottom, false);
            if (i % 12 == 0 && white) { g.setColour (Colour (0xff55555e)); g.setFont (mono (9.f)); g.drawText ("C" + String (m / 12 - 1), r.withTrimmedBottom (16), Justification::centredBottom, false); }
        };
        int wi = 0;
        for (int i = 0; i < 36; ++i) if (isWhite (i)) { drawKey (i, true, wi * ww, ww); ++wi; }
        wi = 0;
        for (int i = 0; i < 36; ++i) { if (isWhite (i)) { ++wi; continue; } const float bw = ww * 0.62f; drawKey (i, false, wi * ww - bw / 2, bw); }
    }
    void mouseDown (const MouseEvent& e) override { content.grabKeyboardFocus(); play (noteAt (e.position)); }
    void mouseDrag (const MouseEvent& e) override { const int n = noteAt (e.position); if (n != heldMouse) play (n); }
    void mouseUp (const MouseEvent&) override { release(); }
    void play (int n) { release(); if (n >= 0) { proc.keyState.noteOn (1, n, 0.8f); heldMouse = n; } }
    void release() { if (heldMouse >= 0) { proc.keyState.noteOff (1, heldMouse, 0.f); heldMouse = -1; } repaint(); }
};

//==============================================================================
KickEditor::Content::Content (KickEditor& o, KickProcessor& p) : owner (o), proc (p)
{
    setSize (kW, kH); setWantsKeyboardFocus (true);
    bpmBox.setInputRestrictions (3, "0123456789"); bpmBox.setJustification (Justification::centred); bpmBox.setFont (mono (12.f));
    bpmBox.setColour (TextEditor::backgroundColourId, col::display); bpmBox.setColour (TextEditor::outlineColourId, col::line); bpmBox.setColour (TextEditor::textColourId, col::text);
    bpmBox.setText (String ((int) p.bpm.load()), false);
    bpmBox.onTextChange = [this] { const int v = jlimit (60, 220, bpmBox.getText().getIntValue()); if (bpmBox.getText().isNotEmpty()) proc.bpm = v; if (cvWave) cvWave->repaint(); if (cvGap) cvGap->repaint(); };
    bpmBox.onFocusLost = [this] { bpmBox.setText (String ((int) proc.bpm.load()), false); };
    // settings
    { auto f = proc.presetDir().getParentDirectory().getChildFile ("settings.txt"); if (f.existsAsFile()) bubblesOn = ! f.loadFileAsString().contains ("bubbles=0"); }
    build();
    addChildComponent (tipBox); tipBox.setInterceptsMouseClicks (false, false);
    startTimerHz (30); refreshUi();
}

void KickEditor::Content::build()
{
    for (auto& it : kItems)
        if (clsIs (it, "knob"))
        {
            const int idx = specIndex (it.kid); if (idx < 0) continue;
            auto* k = knobs.add (new Knob (*this, idx, it.w < 60)); k->setBounds (it.x, it.y, it.w, it.h); k->tip = it.tip; addAndMakeVisible (k);
        }
    buildPresetBar(); buildPitch(); buildScopes(); buildLayers(); buildPlay(); buildGlueRender();
}

void KickEditor::Content::buildPresetBar()
{
    addBtn (byId ("pPrev"), BS::normal, String::fromUTF8 ("\xe2\x80\xb9"), [this] { stepPreset (-1); });
    nameBtn = addBtn (byId ("pName"), BS::pick, "", [this] { showPresetMenu(); }); nameBtn->textCol = col::read;
    addBtn (byId ("pNext"), BS::normal, String::fromUTF8 ("\xe2\x80\xba"), [this] { stepPreset (1); });
    addBtn (byId ("pSave"), BS::normal, "Save", [this] { doSave(); });
    addBtn (byId ("pRename"), BS::normal, "Rename", [this] { doRename(); });
    addBtn (byId ("pDel"), BS::normal, "Delete", [this] { doDelete(); });
    addBtn (byId ("pExp"), BS::normal, "Export", [this] { doExport(); });
    addBtn (byId ("pImp"), BS::normal, "Import", [this] { doImport(); });
    addBtn (byId ("bGuide"), BS::normal, "Guide", [this] { showGuide(); });
    addBtn (byId ("bSet"), BS::normal, "Settings", [this] { showSettings(); });
    addBtn (byId ("bAbout"), BS::normal, "About & Support", [this] { showAbout(); });
}

void KickEditor::Content::buildPitch()
{
    auto* pc = new PitchCanvas (*this); canvases.add (pc); cvPitch = pc;
    pc->setBounds (rect (byId ("cvPitch"))); pc->tip = ""; addAndMakeVisible (pc);
    pc->draw = [this] (Graphics& g, int w, int h)
    {
        const Params P = proc.snapshot(); const PitchCanvas::Geo gm { 44, (double) w - 46, 12, (double) h - 22 };
        auto FY = [&] (double f) { return PitchCanvas::fyOf (f, gm); }; auto FX = [&] (double ms) { return PitchCanvas::fxOf (ms, gm); };
        g.setColour (col::grid);
        for (double f : { 25, 50, 100, 200, 400, 800, 1600, 3200 }) { const float y = (float) FY (f); g.drawHorizontalLine ((int) y, (float) gm.l, (float) gm.r); txt (g, String ((int) f), (float) gm.l - 5, y + 3, col::cap, Justification::right); }
        txt (g, "Hz", 8, (float) gm.b + 12, col::cap, Justification::left);
        for (int ms = 0; ms <= 100; ms += 10) { const float x = (float) FX (ms); g.setColour (col::grid); g.drawVerticalLine ((int) x, (float) gm.t, (float) gm.b); txt (g, String (ms), x, (float) gm.b + 12, col::cap, Justification::centred); }
        txt (g, "ms", (float) gm.r + 4, (float) gm.b + 12, col::cap, Justification::left);
        g.setColour (col::body.withAlpha (0.08f)); g.fillRect (Rectangle<float> ((float) gm.l, (float) FY (300), (float) (gm.r - gm.l), (float) (FY (120) - FY (300))));
        g.setColour (col::sub.withAlpha (0.08f)); g.fillRect (Rectangle<float> ((float) gm.l, (float) FY (60), (float) (gm.r - gm.l), (float) (FY (40) - FY (60))));
        txt (g, "BODY 120-300", (float) gm.r - 4, (float) FY (300) + 10, col::body, Justification::right); txt (g, "SUB 40-60", (float) gm.r - 30, (float) FY (40) - 3, col::sub, Justification::right);
        const double tail = P[id::tailHz]; const float yt = (float) FY (tail);
        { Path dl; dl.startNewSubPath ((float) gm.l, yt); dl.lineTo ((float) gm.r, yt); Path st; const float dash[2] = { 5.f, 4.f }; PathStrokeType (1.f).createDashedStroke (st, dl, dash, 2); g.setColour (col::curve2); g.fillPath (st); }
        const auto nn = hz2note (tail); txt (g, nn.name + " " + String (tail, 1) + " Hz", (float) gm.l + 6, yt - 4, col::curve2, Justification::left);
        auto E = proc.getEnv(); const bool hasEnv = E.size() >= 2; if (! hasEnv) E = legacyEnv (P);
        Path cv;
        for (int i = 0; i <= 200; ++i)
        {
            const double ms = i / 2.0; const double f = hasEnv ? envValue (E, ms) : P[id::tailHz] + (P[id::startHz] - P[id::tailHz]) * std::exp (-3 * std::pow (ms / P[id::sweep], P[id::curve]));
            const float x = (float) FX (ms), y = (float) FY (clampd (f, 20, 4000)); if (i) cv.lineTo (x, y); else cv.startNewSubPath (x, y);
        }
        g.setColour (col::curve); g.strokePath (cv, PathStrokeType (2.f));
        // dots first, then labels with collision avoidance (dragged dot, first and last dot have priority)
        std::vector<Rectangle<float>> placed { Rectangle<float> ((float) gm.r - 90, (float) FY (300), 90, 14), Rectangle<float> ((float) gm.r - 90, (float) FY (40) - 14, 90, 14), Rectangle<float> ((float) gm.l + 4, yt - 16, 90, 14) }; std::vector<size_t> order;
        const int dragged = ((PitchCanvas*) cvPitch)->dragType == 1 ? ((PitchCanvas*) cvPitch)->dragK : -1;
        for (size_t k = 0; k < E.size(); ++k)
        {
            const float x = (float) FX (E[k].ms), y = (float) FY (clampd (E[k].hz, 20, 4000));
            g.setColour (col::ground); g.fillEllipse (x - 6, y - 6, 12, 12); g.setColour (dragged == (int) k ? col::read : col::curve); g.drawEllipse (x - 6, y - 6, 12, 12, 2.f);
        }
        if (dragged >= 0) order.push_back ((size_t) dragged);
        order.push_back (0); if (E.size() > 1) order.push_back (E.size() - 1);
        for (size_t k = 1; k + 1 < E.size(); ++k) order.push_back (k);
        std::vector<char> done (E.size(), 0);
        for (size_t k : order)
        {
            if (k >= E.size() || done[k]) continue; done[k] = 1;
            if (k > 0 && k + 1 == E.size() && std::abs (E[k].hz - tail) < 0.5 && dragged != (int) k) continue;   // the tail label already says it
            const float x = (float) FX (E[k].ms), y = (float) FY (clampd (E[k].hz, 20, 4000));
            const auto dn = hz2note (E[k].hz); const int cr = (int) std::lround (dn.cents);
            const String lab = String (E[k].hz, E[k].hz < 100 ? 1 : 0) + " Hz " + dn.name + (cr ? String (" ") + (cr > 0 ? "+" : "") + String (cr) : String());
            const String lab2 = String (E[k].ms, 1) + " ms";
            const bool right = x < gm.r - 120; const float tw = jmax (lab.length(), lab2.length()) * 6.4f;
            const float lx = right ? x + 10 : x - 10 - tw, ly = y + (k == 0 ? -6.f : -26.f);
            const Rectangle<float> box (lx, ly, tw, 24.f);
            bool hit = ! (dragged == (int) k);
            if (hit) { hit = false; for (auto& r : placed) if (r.intersects (box.expanded (2.f))) { hit = true; break; } }
            if (hit) continue;
            placed.push_back (box);
            txt (g, lab, right ? x + 10 : x - 10, y + (k == 0 ? 3 : -17), col::read, right ? Justification::left : Justification::right);
            if (k > 0 || E[k].ms > 0) txt (g, lab2, right ? x + 10 : x - 10, y + (k == 0 ? 4 : -5), col::cap, right ? Justification::left : Justification::right);
        }
    };
    envResetBtn = addBtn (byId ("envReset"), BS::mini, "Simple curve", [this] { proc.setEnv ({}); });
    auto* keyCap = byId ("keySel");
    selKey = addBtn (keyCap, BS::normal, "Free", [this]
    {
        PopupMenu m; m.addItem (1, "Free");
        for (int mi = 23; mi <= 39; ++mi) m.addItem (100 + mi, String (kNoteNames[mi % 12]) + String (mi / 12 - 1));
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (selKey), [this] (int r)
        { if (r >= 100) proc.setParam ("tailHz", 440.0 * std::pow (2.0, ((r - 100) - 69) / 12.0)); });
    });
    auto* kc = addBtn (byId ("keyChip"), BS::chip, "-", [this]
    { const double t = proc.getParam (id::tailHz); const int m = (int) std::lround (12 * std::log2 (t / 440.0) + 69); proc.setParam ("tailHz", 440.0 * std::pow (2.0, (m - 69) / 12.0)); });
    keyChipBtn = kc;
    selBass = addBtn (byId ("bassSel"), BS::normal, "Off", [this]
    {
        PopupMenu m; m.addItem (1, "Off"); for (int i = 0; i < 12; ++i) m.addItem (10 + i, kNoteNames[i]);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (selBass), [this] (int r) { if (r == 1) proc.bassRoot = -1; else if (r >= 10) proc.bassRoot = r - 10; refreshUi(); });
    });
    bassChip = addChip (byId ("bassChip"));
}

void KickEditor::Content::buildScopes()
{
    // spectrum
    cvSpec = addCanvas (byId ("cvSpec"), [this] (Graphics& g, int w, int h)
    {
        const double l = 34, r = w - 8, t = 10, b = h - 18, f0 = 20, f1 = 12000;
        auto FX = [&] (double f) { return l + std::log (f / f0) / std::log (f1 / f0) * (r - l); }; auto DY = [&] (double d) { return t + (-d) / 90.0 * (b - t); };
        g.setColour (col::grid);
        for (double d : { -80, -60, -40, -20, 0 }) { const float y = (float) DY (d); g.drawHorizontalLine ((int) y, (float) l, (float) r); txt (g, String ((int) d), (float) l - 4, y + 3, col::cap, Justification::right); }
        for (double f : { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 }) { const float x = (float) FX (f); g.setColour (col::grid); g.drawVerticalLine ((int) x, (float) t, (float) b); txt (g, f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), x, (float) b + 12, col::cap, Justification::centred); }
        txt (g, "dB", 6, (float) b + 12, col::cap, Justification::left);
        const int wpx = (int) (r - l);
        if (pub && pub->base && ! pub->base->mags.empty())
        {
            const auto cv = specCurve (pub->base->mags, pub->base->sr, wpx, f0, f1);
            Path fill, line; fill.startNewSubPath ((float) l, (float) b);
            for (int x = 0; x < wpx; ++x) { const float y = (float) clampd (DY (cv[(size_t) x]), t, b); fill.lineTo ((float) l + x, y); if (x) line.lineTo ((float) l + x, y); else line.startNewSubPath ((float) l + x, y); }
            fill.lineTo ((float) l + wpx, (float) b); fill.closeSubPath();
            g.setColour (col::curve.withAlpha (0.35f)); g.fillPath (fill); g.setColour (col::curve); g.strokePath (line, PathStrokeType (1.5f));
        }
        if (! refMags.empty())
        {
            const auto cv = specCurve (refMags, refSr, wpx, f0, f1); Path line;
            for (int x = 0; x < wpx; ++x) { const float y = (float) clampd (DY (cv[(size_t) x]), t, b); if (x) line.lineTo ((float) l + x, y); else line.startNewSubPath ((float) l + x, y); }
            g.setColour (Colour (0xffFFA600)); g.strokePath (line, PathStrokeType (1.5f));
        }
        if (refMsg.isNotEmpty()) txt (g, refMsg, (float) r - 4, (float) t + 10, Colour (0xffFFA600), Justification::right);
        const double tail = proc.getParam (id::tailHz); const float xt = (float) FX (tail);
        { Path dl; dl.startNewSubPath (xt, (float) t); dl.lineTo (xt, (float) b); Path st; const float dash[2] = { 4.f, 3.f }; PathStrokeType (1.f).createDashedStroke (st, dl, dash, 2); g.setColour (col::curve2); g.fillPath (st); }
        const auto nn = hz2note (tail); txt (g, nn.name + " " + String (tail, 1), xt + 4, (float) t + 10, col::curve2, Justification::left);
    });
    addBtn (byId ("refBtn"), BS::mini, "Reference", [this]
    {
        if (! refMags.empty()) { refMags.clear(); refMsg = {}; cvSpec->repaint(); return; }
        chooserHolder = std::make_unique<FileChooser> ("Pick a kick to compare with", File(), "*.wav;*.flac;*.aif;*.aiff;*.ogg;*.mp3");
        chooserHolder->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadReference (fc.getResult()); });
    })->tip = byId ("refBtn")->tip;
    refBtn = btns.getLast();

    // waveform
    cvWave = addCanvas (byId ("cvWave"), [this] (Graphics& g, int w, int h)
    {
        if (! pub || ! pub->base) return; const auto& buf = pub->base->pcm; const double sr = pub->base->sr; const Params P = proc.snapshot();
        const double l = 40, r = w - 10, t = 24, b = h - 20, mid = (t + b) / 2, lenMs = buf.size() / sr * 1000;
        const double xmax = std::min (400.0, std::max (160.0, std::ceil ((lenMs + 10) / 20) * 20));
        auto X = [&] (double ms) { return l + ms / xmax * (r - l); };
        g.setColour (col::grid);
        for (int ms = 0; ms <= (int) xmax; ms += 20) { g.drawVerticalLine ((int) X (ms), (float) t, (float) b); txt (g, String (ms), (float) X (ms), (float) b + 12, col::cap, Justification::centred); }
        txt (g, "ms", (float) r, (float) b + 12, col::cap, Justification::right);
        const std::pair<double, const char*> hl[5] = { { 1, "0 dB" }, { 0.5, "-6" }, { 0, "" }, { -0.5, "-6" }, { -1, "0 dB" } };
        for (auto& a : hl) { const float y = (float) (mid - a.first * (b - t) / 2); g.setColour (col::grid); g.drawHorizontalLine ((int) y, (float) l, (float) r); if (*a.second) txt (g, a.second, (float) l - 4, y + 3, col::cap, Justification::right); }
        const double ce = P[id::clickLen] * 1.5, c2 = std::max (45.0, ce);
        struct Seg { double a, b; Colour c; const char* n; } segs[3] = { { 0, ce, col::click, "CLICK" }, { ce, c2, col::body, "BODY" }, { c2, xmax, col::sub, "SUB" } };
        for (auto& s : segs) { if (s.b <= s.a) continue; g.setColour (s.c.withAlpha (0.9f)); g.fillRect ((float) X (s.a), (float) t - 12, (float) (X (s.b) - X (s.a)), 4.f); if (X (s.b) - X (s.a) > 34) txt (g, s.n, (float) X (s.a) + 3, (float) t - 14, s.c, Justification::left, 9.f); }
        const double spp = (xmax / 1000 * sr) / (r - l); g.setColour (col::curve.withAlpha (0.9f));
        for (int x = 0; x < (int) (r - l); ++x)
        {
            float mn = 0, mx = 0; const size_t a = (size_t) std::floor (x * spp), bb = std::min (buf.size(), (size_t) std::floor ((x + 1) * spp) + 1); if (a >= buf.size()) break;
            for (size_t i = a; i < bb; ++i) { mn = std::min (mn, buf[i]); mx = std::max (mx, buf[i]); }
            g.fillRect ((float) (l + x), (float) (mid - mx * (b - t) / 2), 1.f, (float) std::max (1.0, (mx - mn) * (b - t) / 2));
        }
        const double sx = 60000.0 / proc.bpm.load() / 4, tailEnd = P[id::hold] + P[id::decay];
        auto dashed = [&] (double x, Colour c) { Path dl; dl.startNewSubPath ((float) x, (float) t); dl.lineTo ((float) x, (float) b); Path st; const float dash[2] = { 4.f, 3.f }; PathStrokeType (1.5f).createDashedStroke (st, dl, dash, 2); g.setColour (c); g.fillPath (st); };
        dashed (X (sx), col::warn); dashed (X (tailEnd), col::curve2);
        txt (g, "16th " + String (sx, 1) + " ms", (float) X (sx) + 3, (float) b - 4, col::warn, Justification::left);
        txt (g, "end " + num (tailEnd, 0) + " ms", (float) X (tailEnd) + 3, (float) t + 10, col::curve2, Justification::left);
    });
    cvWave->onDragFn = [this] (const MouseEvent& e) { if (e.getDistanceFromDragStart() > 6 && ! dragStarted) { dragStarted = true; dragWavOut(); } };
    cvWave->onClickFn = [this] (const MouseEvent&) { dragStarted = false; };
    addBtn (byId ("expBtn"), BS::mini, "Export .wav", [this] { exportWav(); });

    // timing
    cvGap = addCanvas (byId ("cvGap"), [this] (Graphics& g, int w, int h)
    {
        const Params P = proc.snapshot(); const double bpm = proc.bpm.load(), l = 10, r = w - 10, beat = 60000.0 / bpm; auto Xp = [&] (double ms) { return l + ms / beat * (r - l); };
        const double y0 = 26, hh = h - 52;
        txt (g, "one beat = " + String (beat, 1) + " ms @ " + String ((int) bpm) + " BPM", (float) l, 12, col::cap, Justification::left);
        for (int i = 0; i <= 4; ++i)
        {
            const float x = (float) Xp (beat * i / 4); g.setColour (i % 4 == 0 ? col::cap : col::line); g.drawVerticalLine ((int) x, (float) (y0 - 4), (float) (y0 + hh + 4));
            if (i < 4) txt (g, i == 0 ? String ("kick") : "16th " + String (i), x + 3, (float) (y0 + hh + 14), i == 0 ? col::click : col::warn, Justification::left, 9.f);
        }
        const double te = std::min (beat, P[id::hold] + P[id::decay]);
        g.setColour (col::curve.withAlpha (0.55f)); g.fillRect ((float) l, (float) y0, (float) (Xp (te) - l), (float) hh);
        g.setColour (col::warn.withAlpha (0.25f)); g.fillRect ((float) Xp (beat / 4), (float) y0, (float) (Xp (beat) - Xp (beat / 4)), (float) hh);
        g.setColour (col::curve2); g.drawLine ((float) Xp (te), (float) (y0 - 4), (float) Xp (te), (float) (y0 + hh + 4), 2.f);
        txt (g, "bass zone", (float) Xp (beat / 4) + 4, (float) (y0 + 12), col::warn, Justification::left, 9.f);
    });
    // the three chips under the timing display have no ids: take them by position
    { std::vector<const LItem*> cs; for (auto& it : kItems) if (std::strcmp (it.tag, "span") == 0 && clsIs (it, "chip") && it.id[0] == 0 && it.y >= 480 && it.y <= 520 && it.x < 1000) cs.push_back (&it);
      if (cs.size() >= 3) { chipTail = addChip (cs[0]); chipPct = addChip (cs[1]); chipFirst = addChip (cs[2]); } }
    chipPk = addChip (byId ("rpk")); chipRms = addChip (byId ("rrms")); chipLufs = addChip (byId ("rlufs"));
    polBtn = addBtn (byId ("polBtn"), BS::normal, String::fromUTF8 ("\xc3\x98"), [this] { proc.setPol (proc.getPol() < 0 ? 1.0 : -1.0); refreshUi(); });
    alignBtn = addBtn (byId ("alignBtn"), BS::chip, "Align on", [this] { proc.setParam ("palign", proc.getParam (id::palign) > 0.5 ? 0.0 : 1.0); refreshUi(); });

    // meter
    cvMeter = addCanvas (byId ("cvMeter"), [this] (Graphics& g, int w, int h)
    {
        const double minDb = -48, maxDb = 3; const double x0 = 26, bw = w - x0 - 6;
        auto Y = [&] (double d) { return 4 + (1 - (clampd (d, minDb, maxDb) - minDb) / (maxDb - minDb)) * (h - 8); };
        for (double d : { 0, -6, -12, -18, -24, -36, -48 }) { const float y = (float) Y (d); g.setColour (col::line); g.drawHorizontalLine ((int) y, (float) x0 - 4, (float) x0); txt (g, String ((int) d), (float) x0 - 6, y + 3, col::cap, Justification::right, 9.f); }
        const double yv = Y (lvl), yb = Y (minDb); (void) yb;
        struct Z { double a, b; Colour c; } zones[3] = { { minDb, -12, col::ok }, { -12, 0, col::warn }, { 0, maxDb, col::clip } };
        for (auto& z : zones) { const double ya = Y (z.b), yb2 = Y (z.a), top = std::max (ya, yv); if (top < yb2) { g.setColour (z.c); g.fillRect ((float) x0, (float) top, (float) bw, (float) (yb2 - top)); } }
        g.setColour (Colour (0x30ffffff)); g.fillRect ((float) x0, (float) Y (0) - 0.5f, (float) bw, 1.f);
        g.setColour (col::text); g.fillRect ((float) x0, (float) Y (holdDb) - 1.f, (float) bw, 2.f);
    });
    cvMeter->onClickFn = [this] (const MouseEvent&) { holdDb = -99.f; proc.meterPeak = 0; };
    pkRead = new Btn (*this, BS::chip, "-inf"); pkRead->setBounds (1529, 76, 46, 17); pkRead->tip = "Highest level since the last reset. Click to reset it."; pkRead->onClick = [this] { holdDb = -99.f; }; addAndMakeVisible (pkRead); btns.add (pkRead);
}

void KickEditor::Content::buildLayers()
{
    std::vector<const LItem*> bs; for (auto& it : kItems) if (std::strcmp (it.tag, "button") == 0 && (clsIs (it, "ar") || clsIs (it, "pick") || clsIs (it, "ld") || clsIs (it, "pl"))) bs.push_back (&it);
    for (int l = 0; l < 3 && (int) bs.size() >= (l + 1) * 5; ++l)
    {
        const LItem* pv = bs[(size_t) l * 5]; const LItem* pk = bs[(size_t) l * 5 + 1]; const LItem* nx = bs[(size_t) l * 5 + 2]; const LItem* ld = bs[(size_t) l * 5 + 3]; const LItem* pl = bs[(size_t) l * 5 + 4];
        addBtn (pv, BS::normal, String::fromUTF8 ("\xe2\x80\xb9"), [this, l] { stepSource (l, -1); });
        pickBtn[l] = addBtn (pk, BS::pick, "Off", [this, l] { pickSource (l); });
        addBtn (nx, BS::normal, String::fromUTF8 ("\xe2\x80\xba"), [this, l] { stepSource (l, 1); });
        addBtn (ld, BS::normal, "Load", [this, l] { loadSample (l); });
        plBtn[l] = addBtn (pl, BS::normal, String::fromUTF8 ("\xc3\x98"), [this, l] { proc.setLayerPol (l, proc.getLayerPol (l) < 0 ? 1 : -1); refreshUi(); });
        cvLw[l] = addCanvas (byId (("lw" + String (l)).toRawUTF8()), [this, l] (Graphics& g, int w, int h)
        {
            g.setColour (col::grid); g.drawHorizontalLine (h / 2, 0.f, (float) w);
            txt (g, "100 ms", (float) w - 4, (float) h - 3, col::cap, Justification::right, 9.f);
            const int src = proc.getSrc (l);
            if (! src) { txt (g, "Off", 6, (float) h / 2 + 3, col::cap, Justification::left); return; }
            if (src == 7 && proc.ownName (l).isEmpty()) { txt (g, "Drop a sound here, or click Load", 6, (float) h / 2 + 3, col::cap, Justification::left); return; }
            const double sr = proc.sampleRateNow(); const int N = (int) std::floor (0.1 * sr);
            Params P = proc.snapshot();
            auto lookup = [this] (int layer, int s) -> const Sample* { return s == 7 ? ownView (layer) : proc.bank.get (s); };
            const auto L = renderLayer (P, l, sr, N, lookup); if (L.empty()) return;
            float pk = 0; for (int q = 0; q < N; ++q) pk = std::max (pk, std::abs (L[(size_t) q]));
            const float sc = pk > 0 ? 0.42f * h / pk : 0.f; g.setColour (col::curve.withAlpha (0.9f));
            for (int x = 0; x < w; ++x)
            {
                float mn = 0, mx = 0; const int a = (int) ((double) x / w * N), b = (int) ((double) (x + 1) / w * N) + 1;
                for (int q = a; q < b && q < N; ++q) { mn = std::min (mn, L[(size_t) q]); mx = std::max (mx, L[(size_t) q]); }
                g.fillRect ((float) x, h / 2.f - mx * sc, 1.f, std::max (1.f, (mx - mn) * sc));
            }
            txt (g, src == 7 ? proc.ownName (l) : srcName (src), 6, 10, col::read, Justification::left);
        });
        cvLw[l]->tip = "";
    }
    // order of sources for the arrow buttons, same as the folder tree
    order.clear(); order.push_back (0); for (int v = 1; v <= 6; ++v) order.push_back (v); order.push_back (7);
    StringArray groups; for (int i = 0; i < kBankCount; ++i) if (! groups.contains (kBankInfo[i].group)) groups.add (kBankInfo[i].group);
    for (auto& gp : groups)
    {
        StringArray subs; for (int i = 0; i < kBankCount; ++i) if (gp == kBankInfo[i].group && ! subs.contains (kBankInfo[i].sub)) subs.add (kBankInfo[i].sub);
        for (auto& sb : subs) for (int i = 0; i < kBankCount; ++i) if (gp == kBankInfo[i].group && sb == kBankInfo[i].sub) order.push_back (i + 8);
    }
}

const Sample* KickEditor::Content::ownView (int layer) { return proc.ownSampleView (layer); }

void KickEditor::Content::applySrc (int l, int v, bool gesture)
{
    proc.setSrc (l, v);
    if (gesture && v > 0) { static const auto lens = srcLens(); const char* ids[3] = { "l0len", "l1len", "l2len" }; proc.setParam (ids[l], (double) (v < (int) lens.size() ? lens[(size_t) v] : 80)); }
    refreshUi();
}
void KickEditor::Content::auditionSoon() { Timer::callAfterDelay (110, [sp = SafePointer<Component> (this), this] { if (sp != nullptr) proc.hitReq = true; }); }
void KickEditor::Content::stepSource (int l, int dir)
{
    std::vector<int> o; for (int v : order) if (v != 7 || proc.ownName (l).isNotEmpty()) o.push_back (v);
    int cur = proc.getSrc (l); size_t i = 0; for (size_t k = 0; k < o.size(); ++k) if (o[k] == cur) i = k;
    i = (size_t) (((int) i + dir + (int) o.size()) % (int) o.size()); applySrc (l, o[i], true); auditionSoon();
}
void KickEditor::Content::pickSource (int l)
{
    PopupMenu m; m.addItem (1, "Off", true, proc.getSrc (l) == 0);
    PopupMenu live; for (int v = 1; v <= 6; ++v) live.addItem (v + 1, srcName (v), true, proc.getSrc (l) == v); m.addSubMenu ("Live tops", live);
    if (proc.ownName (l).isNotEmpty()) m.addItem (8, "Your sample: " + proc.ownName (l), true, proc.getSrc (l) == 7);
    StringArray groups; for (int i = 0; i < kBankCount; ++i) if (! groups.contains (kBankInfo[i].group)) groups.add (kBankInfo[i].group);
    for (auto& gp : groups)
    {
        PopupMenu gm; StringArray subs; for (int i = 0; i < kBankCount; ++i) if (gp == kBankInfo[i].group && ! subs.contains (kBankInfo[i].sub)) subs.add (kBankInfo[i].sub);
        for (auto& sb : subs) { PopupMenu sm; for (int i = 0; i < kBankCount; ++i) if (gp == kBankInfo[i].group && sb == kBankInfo[i].sub) sm.addItem (i + 9, kBankInfo[i].name, true, proc.getSrc (l) == i + 8); gm.addSubMenu (sb, sm); }
        m.addSubMenu (gp, gm);
    }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (pickBtn[l]).withMaximumNumColumns (1), [this, l] (int r) { if (r > 0) { applySrc (l, r - 1, true); auditionSoon(); } });
}
void KickEditor::Content::loadSample (int l)
{
    chooserHolder = std::make_unique<FileChooser> ("Load a sound for layer " + String (l + 1), File(), "*.wav;*.flac;*.mp3;*.ogg;*.aif;*.aiff");
    chooserHolder->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, l] (const FileChooser& fc)
    {
        if (! fc.getResult().existsAsFile()) return; String err; proc.setOwnSample (l, fc.getResult(), err);
        if (err.isNotEmpty()) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Kick Kitten", err); else { proc.setParam (l == 0 ? "l0len" : l == 1 ? "l1len" : "l2len", 80.0); refreshUi(); auditionSoon(); }
    });
}

void KickEditor::Content::buildPlay()
{
    addBtn (byId ("oDn"), BS::normal, String::fromUTF8 ("Oct \xe2\x88\x92"), [this] { proc.baseMidi = jlimit (24, 60, proc.baseMidi.load() - 12); piano->repaint(); proc.requestRender(); });
    addBtn (byId ("oUp"), BS::normal, "+", [this] { proc.baseMidi = jlimit (24, 60, proc.baseMidi.load() + 12); piano->repaint(); proc.requestRender(); });
    kmS = addBtn (byId ("kmS"), BS::normal, "Same", [this] { proc.keyMode = 0; refreshUi(); });
    kmP = addBtn (byId ("kmP"), BS::normal, "Pitched", [this] { proc.keyMode = 1; proc.requestRender(); refreshUi(); });
    playedChip = addChip (byId ("played")); playedChip->text = "-";
    hitBtnP = addBtn (byId ("hitBtn"), BS::normal, "Hit", [this] { proc.hitReq = true; }); hitBtnP->on = true;
    loopBtn = addBtn (byId ("loopBtn"), BS::normal, "Loop", [this] { proc.loopOn = ! proc.loopOn.load(); refreshUi(); });
    if (auto* b = byId ("bpm")) { bpmBox.setBounds (rect (b)); addAndMakeVisible (bpmBox); bpmTip = b->tip; }
    bassBtn = addBtn (byId ("bassBtn"), BS::normal, "Bass test", [this] { proc.bassOn = ! proc.bassOn.load(); if (proc.bassOn.load() && ! proc.loopOn.load()) proc.loopOn = true; refreshUi(); });
    addBtn (byId ("zOut"), BS::normal, String::fromUTF8 ("\xe2\x88\x92"), [this] { owner.zoomBy (1.f / 1.1f); });
    zPct = addBtn (byId ("zPct"), BS::chip, "100%", [this] { owner.resetTo100(); });
    addBtn (byId ("zIn"), BS::normal, "+", [this] { owner.zoomBy (1.1f); });
    Rectangle<int> u; bool first = true;
    for (auto& it : kItems) if (clsIs (it, "wk") || clsIs (it, "bk")) { auto r = Rectangle<int> (it.x, it.y, it.w, it.h); u = first ? r : u.getUnion (r); first = false; }
    auto* pv = new PianoView (proc, *this); piano = pv; pv->setBounds (u); addAndMakeVisible (pv);
}

void KickEditor::Content::buildGlueRender()
{
    tabGlue = addBtn (byId ("tabGlue"), BS::normal, "GLUE & LIMITER", [this] { renderTab = 0; refreshUi(); });
    tabRender = addBtn (byId ("tabRender"), BS::normal, "RENDER", [this] { renderTab = 1; refreshUi(); });
    cvGlue = addCanvas (byId ("cvGlue"), [this] (Graphics& g, int w, int h)
    {
        if (! pub || ! pub->base) return; const auto& GL = pub->base->glue; const double l = 30, r = w - 6, wd = r - l; const int nb = GlueInfo::nb;
        const double t1 = 6, b1 = std::round (h * 0.52), t2 = b1 + 16, b2 = h - 16; auto X = [&] (double i) { return l + i / nb * wd; };
        auto Y1 = [&] (double d) { return t1 + clampd (-d, 0, 36) / 36 * (b1 - t1); }; auto Y2 = [&] (double d) { return t2 + clampd (-d, 0, 12) / 12 * (b2 - t2); };
        g.setColour (col::grid);
        for (double d : { 0, -12, -24 }) { const float y = (float) Y1 (d); g.drawHorizontalLine ((int) y, (float) l, (float) r); txt (g, String ((int) d), (float) l - 4, y + 3, col::cap, Justification::right); }
        for (double d : { 0, -3, -6, -9, -12 }) { const float y = (float) Y2 (d); g.drawHorizontalLine ((int) y, (float) l, (float) r); txt (g, String ((int) d), (float) l - 4, y + 3, col::cap, Justification::right); }
        for (int ms = 0; ms <= 150; ms += 50) { const float x = (float) X (ms); g.setColour (col::grid); g.drawVerticalLine ((int) x, (float) t2, (float) b2); txt (g, String (ms), x, (float) b2 + 11, col::cap, Justification::centred); }
        txt (g, "dB", 2, (float) t1 + 8, col::cap, Justification::left); txt (g, "GR", 2, (float) t2 - 5, col::body, Justification::left); txt (g, "ms", (float) l - 4, (float) b2 + 11, col::cap, Justification::right);
        Path fill, line; fill.startNewSubPath ((float) l, (float) Y1 (-36));
        for (int i = 0; i < nb; ++i) { const float x = (float) X (i), y = (float) Y1 (GL.lv[i] - GL.pk); fill.lineTo (x, y); if (i) line.lineTo (x, y); else line.startNewSubPath (x, y); }
        fill.lineTo ((float) r, (float) Y1 (-36)); fill.closeSubPath();
        g.setColour (col::curve.withAlpha (0.35f)); g.fillPath (fill); g.setColour (col::curve); g.strokePath (line, PathStrokeType (1.5f));
        const float yt = (float) Y1 (GL.thr - GL.pk); { Path dl; dl.startNewSubPath ((float) l, yt); dl.lineTo ((float) r, yt); Path st; const float dash[2] = { 4.f, 3.f }; PathStrokeType (1.f).createDashedStroke (st, dl, dash, 2); g.setColour (col::read); g.fillPath (st); }
        txt (g, "threshold", (float) r - 2, yt - 3, col::read, Justification::right);
        g.setColour (col::body.withAlpha (0.8f));
        for (int i = 0; i < nb; ++i) { const double hh = Y2 (GL.gr[i]) - t2; if (hh > 0.5) g.fillRect ((float) X (i), (float) t2, (float) std::max (1.0, wd / nb), (float) hh); }
    });
    chipGPk = addChip (byId ("gPk")); chipGGr = addChip (byId ("gGr"));
    limBtn = addBtn (byId ("limBtn"), BS::chip, "Limiter Off", [this] { proc.setParam ("lim", proc.getParam (id::lim) > 0.5 ? 0.0 : 1.0); refreshUi(); });
    // render tab
    renderSave = new Btn (*this, BS::normal, "Save 24-bit .wav"); renderSave->setBounds (1275, 360, 232, 30); renderSave->onClick = [this] { exportWav(); }; renderSave->tip = "Save the finished kick as a 24-bit mono .wav file."; addChildComponent (renderSave); btns.add (renderSave);
    renderCopy = new Btn (*this, BS::normal, "Copy sound as text"); renderCopy->setBounds (1275, 398, 232, 30); renderCopy->onClick = [this] { doExport(); }; renderCopy->tip = "Copy this sound as text you can paste into another Kick Kitten."; addChildComponent (renderCopy); btns.add (renderCopy);
    for (auto* b : { tabGlue, tabRender }) b->setVisible (true);
    for (auto* k : knobs) { const String id (SPEC[k->idx].id); if (id == "sGlue" || id == "gThr" || id == "gRat" || id == "gAtk" || id == "gRel" || id == "limG") glueGroup.push_back (k); }
    for (auto* c : { (Component*) cvGlue, (Component*) chipGPk, (Component*) chipGGr, (Component*) limBtn }) glueGroup.push_back (c);
}

//==============================================================================
void KickEditor::Content::paintWood (Graphics& g, Rectangle<int> r, bool left)
{
    uint32_t s = left ? 7u : 19u; auto rnd = [&] { s = s * 1664525u + 1013904223u; return (double) s / 4294967296.0; };
    const int w = r.getWidth(), h = r.getHeight();
    g.setColour (Colour (0xff3d2210)); g.fillRect (r);
    for (int x = 0; x < w; ++x)
    {
        const double a = rnd(); g.setColour (Colour::fromFloatRGBA (123 / 255.f, 74 / 255.f, 38 / 255.f, (float) (0.4 * a))); const double w1 = 1 + std::floor (rnd() * 2); g.fillRect (Rectangle<float> ((float) (r.getX() + x), (float) r.getY(), (float) w1, (float) h));
        const double b = rnd(); g.setColour (Colour::fromFloatRGBA (90 / 255.f, 52 / 255.f, 25 / 255.f, (float) (0.75 * b))); g.fillRect (Rectangle<float> ((float) (r.getX() + x) + 0.5f, (float) r.getY(), 1.f, (float) h));
        if (rnd() < 0.25) { g.setColour (Colour::fromFloatRGBA (20 / 255.f, 10 / 255.f, 4 / 255.f, 0.35f)); g.fillRect (Rectangle<float> ((float) (r.getX() + x), (float) r.getY() + (float) (rnd() * h * 0.2), 1.f, (float) h)); }
    }
    g.setColour (Colours::black); g.fillRect (Rectangle<float> (left ? (float) r.getRight() - 1.5f : (float) r.getX(), (float) r.getY(), 1.5f, (float) h));
}

void KickEditor::Content::paint (Graphics& g)
{
    g.fillAll (col::ground);
    paintWood (g, { 0, 0, 52, kH }, true); paintWood (g, { kW - 52, 0, 52, kH }, false);
    for (auto& it : kItems)
    {
        const Rectangle<int> r (it.x, it.y, it.w, it.h); const String tag (it.tag);
        if (tag == "section") { g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 6.f); g.setColour (col::line); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.f, 1.f); }
    }
    // layer slots, keyboard panel, preview rig (not sections in the mockup DOM, so listed here)
    { const Rectangle<int> slots[3] = { { 60, 646, 501, 138 }, { 569, 646, 501, 138 }, { 1079, 646, 501, 138 } };
      for (int i = 0; i < 3; ++i) { g.setColour (col::panel); g.fillRoundedRectangle (slots[i].toFloat(), 6.f); g.setColour (col::line); g.drawRoundedRectangle (slots[i].toFloat().reduced (0.5f), 6.f, 1.f);
          g.setColour (col::text); g.setFont (sans (12.f, true, 0.12f)); g.drawText (String ("LAYER ") + (char) ('A' + i), slots[i].getX() + 7, 654, 70, 16, Justification::centredLeft, false); }
      const Rectangle<int> keys (60, 792, 1520, 84), rig (60, 884, 1520, 34);
      g.setColour (col::panel); g.fillRoundedRectangle (keys.toFloat(), 6.f); g.setColour (col::line); g.drawRoundedRectangle (keys.toFloat().reduced (0.5f), 6.f, 1.f);
      g.drawRoundedRectangle (rig.toFloat().reduced (0.5f), 6.f, 1.f);
      g.setColour (col::cap); g.setFont (sans (10.f, false, 0.14f)); g.drawText ("PREVIEW RIG", 71, 894, 90, 14, Justification::centredLeft, false);
      g.setFont (sans (11.f, false, 0.08f)); g.drawText ("BPM", 297, 894, 30, 15, Justification::centredLeft, false); g.drawText ("ZOOM", 1426, 894, 40, 15, Justification::centredLeft, false);
      g.setColour (col::dim); g.setFont (sans (11.f, false, 0.1f)); g.drawText ("POLARITY", 830, 567, 63, 15, Justification::centredLeft, false); g.drawText ("LEVELS", 912, 556, 47, 15, Justification::centredLeft, false); }
    // header
    if (auto* hd = byId ("hdr"))
    {
        const Rectangle<int> r (hd->x, hd->y, hd->w, hd->h);
        static const Path head = Drawable::parseSVGPath ("M9 6 L25 18 Q32 15.5 39 18 L55 6 L58 33 Q58 55 32 60 Q6 55 6 33 Z");
        g.setColour (col::text); g.fillPath (head, AffineTransform::scale (30.f / 64.f).translated ((float) r.getX() + 2, (float) r.getY() + 2));
        g.setColour (col::ground);
        Path e1; e1.addPath (Drawable::parseSVGPath ("M12 28 Q22 26 30 36 Q18 37 12 28 Z")); e1.addPath (Drawable::parseSVGPath ("M52 28 Q42 26 34 36 Q46 37 52 28 Z")); e1.addPath (Drawable::parseSVGPath ("M29.5 45 H34.5 L32 49 Z"));
        g.fillPath (e1, AffineTransform::scale (30.f / 64.f).translated ((float) r.getX() + 2, (float) r.getY() + 2));
        g.setColour (col::text); g.setFont (sans (22.f, true, 0.06f)); g.drawText ("KICK KITTEN", r.getX() + 42, r.getY(), 170, r.getHeight(), Justification::centredLeft, false);
        g.setColour (col::cap); g.setFont (sans (11.f, false, 0.14f)); g.drawText ("STUDIO 87", r.getX() + 190, r.getY() + 4, 100, r.getHeight() - 8, Justification::centredLeft, false);
    }
    for (auto& it : kItems)
    {
        const Rectangle<int> r (it.x, it.y, it.w, it.h); const String tag (it.tag), text (it.text);
        if (tag == "h2")
        {
            String title = text, hint;
            if (text.startsWith ("PITCH")) title = "PITCH ENVELOPE"; else if (text.startsWith ("SPECTRUM")) title = "SPECTRUM"; else if (text.startsWith ("WAVEFORM")) title = "WAVEFORM & LAYERS";
            else if (text.startsWith ("OUTPUT")) title = "OUTPUT & TIMING"; else if (text.startsWith ("SAUCE")) title = "SAUCE"; else if (text.startsWith ("OUT")) title = "OUT";
            hint = hintFor (title);
            if (title.startsWith ("WAVEFORM") && pub && pub->base) hint = "length " + num (pub->base->pcm.size() / pub->base->sr * 1000.0, 0) + " ms at " + String (pub->base->sr / 1000.0, 1) + " kHz";
            if (title == "SAUCE")
            {
                g.setColour (col::text); g.setFont (sans (13.f, true, 0.14f)); g.drawText (title, r.withHeight (16), Justification::topLeft, false);
                g.setColour (col::cap); g.setFont (sans (12.f, false, 0.03f)); g.drawFittedText (hint, r.withTrimmedTop (20), Justification::topLeft, 4);
            }
            else
            {
                g.setColour (col::text); g.setFont (sans (13.f, true, 0.14f)); g.drawText (title, r.withHeight (18), Justification::centredLeft, false);
                if (hint.isNotEmpty()) { const float tw = GlyphArrangement::getStringWidth (sans (13.f, true, 0.14f), title); g.setColour (col::cap); g.setFont (sans (12.f, false, 0.06f)); g.drawText (hint, r.getX() + (int) tw + 8, r.getY(), r.getWidth() - (int) tw - 100, 18, Justification::centredLeft, false); }
            }
        }
        else if (tag == "i")
        {
            if (renderTab == 1 && (text == "GLUE" || text == "TIMING" || text == "LIMITER")) continue;
            g.setColour (col::dim); g.setFont (sans (11.f, false, 0.1f)); g.drawText (text, r.expanded (4, 0), Justification::centredLeft, false);
        }
        else if (tag == "div" && clsIs (it, "kl") && (text == "KEY" || text == "BASS ROOT"))
        { g.setColour (col::dim); g.setFont (sans (11.f, false, 0.1f)); g.drawText (text, r.expanded (6, 0), Justification::centred, false); }
    }
    if (renderTab == 1)
    {
        g.setColour (col::text); g.setFont (sans (13.f, true, 0.14f)); g.drawText ("RENDER", 1284, 100, 220, 18, Justification::centredLeft, false);
        g.setColour (col::cap); g.setFont (sans (12.f, false, 0.03f)); g.drawFittedText ("The whole kick, ready to save. Drag the waveform on the left into your DAW, or save it here as a 24-bit mono .wav.", Rectangle<int> (1284, 122, 214, 80), Justification::topLeft, 5);
        if (pub && pub->base)
        {
            g.setColour (col::text); g.setFont (mono (12.f));
            int y = 214; auto line = [&] (const String& s) { g.drawText (s, 1284, y, 220, 18, Justification::centredLeft, false); y += 22; };
            line ("Length " + num (pub->base->pcm.size() / pub->base->sr * 1000.0, 0) + " ms"); line ("Sample rate " + String (pub->base->sr / 1000.0, 1) + " kHz");
            line ("True peak " + dbStr (pub->base->tp > 0 ? 20 * std::log10 (pub->base->tp) : -99.0)); line ("RMS " + dbStr (pub->base->rms)); line ("LUFS " + String (pub->base->lufs, 1));
        }
    }
}

void KickEditor::Content::timerCallback()
{
    auto np = proc.published();
    if (np && (np != pub)) { pub = np; if (np->gen != lastGen) { lastGen = np->gen; refreshUi(); for (auto* c : canvases) c->repaint(); } }
    for (auto* k : knobs) k->tick();
    // meter: fast attack, smooth fall; peak hold
    const float pk = proc.meterL.exchange (0.f); const float db = pk > 1e-5f ? 20.f * std::log10 (pk) : -99.f;
    lvl = db > lvl ? db : jmax (db, lvl - 1.8f);
    if (db > holdDb) { holdDb = db; holdT = 45; } else if (holdT > 0) --holdT; else holdDb = jmax (-99.f, holdDb - 0.6f);
    if (cvMeter) cvMeter->repaint();
    if (pkRead) { const String t = holdDb < -90.f ? String::fromUTF8 ("-inf") : String (holdDb, 1); if (pkRead->text != t) { pkRead->text = t; pkRead->textCol = holdDb >= 0 ? col::clip : holdDb > -12 ? col::warn : col::ok; pkRead->repaint(); } }
    if (piano) piano->repaint();
    if (proc.flash.exchange (0)) { const int n = proc.lastNote.load(); if (playedChip) { if (n >= 0 && proc.keyMode.load() == 1) { const double f = 440.0 * std::pow (2.0, (n - 69) / 12.0); playedChip->text = hz2note (f).name + " " + String (f, 1) + " Hz"; } else playedChip->text = "kick"; playedChip->repaint(); } }
    if (nameBtn) { const String t = proc.presetName + (proc.presetEdited ? " *" : ""); if (nameBtn->text != t) { nameBtn->text = t; nameBtn->repaint(); } }
    if (tipTarget && tipDelay > 0 && --tipDelay == 0)
    {
        tipBox.set (tipTarget->tip); auto b = getLocalArea (tipTarget, tipTarget->getLocalBounds());
        int x = jlimit (4, kW - tipBox.getWidth() - 4, b.getX()), y = b.getBottom() + 6; if (y + tipBox.getHeight() > kH - 4) y = b.getY() - tipBox.getHeight() - 6;
        tipBox.setTopLeftPosition (x, jmax (4, y)); tipBox.setVisible (true); tipBox.toFront (false);
    }
    if (zPct) { const String z = String (owner.zoomPercent()) + "%"; if (zPct->text != z) { zPct->text = z; zPct->repaint(); } }
}

void KickEditor::Content::refreshUi()
{
    const Params P = proc.snapshot(); const double bpm = proc.bpm.load(), sx = 60000.0 / bpm / 4, te = P[id::hold] + P[id::decay];
    if (nameBtn) nameBtn->text = proc.presetName + (proc.presetEdited ? " *" : "");
    if (chipTail) { chipTail->text = "Tail ends " + num (te, 0) + " ms"; chipPct->text = num (te / sx * 100, 0) + "% of a 16th (" + String (sx, 1) + " ms)";
        float first = 0; if (pub && pub->base) { float pk = 0; for (float v : pub->base->pcm) pk = std::max (pk, std::abs (v)); for (size_t i = 0; i < std::min<size_t> (pub->base->pcm.size(), 2000); ++i) if (std::abs (pub->base->pcm[i]) > 0.01f * pk) { first = pub->base->pcm[i]; break; } }
        chipFirst->text = String ("1st cycle ") + (first >= 0 ? String::fromUTF8 ("\xe2\x96\xb2 +") : String::fromUTF8 ("\xe2\x96\xbc -")); }
    auto dbCol = [] (double d) { return d >= 0 ? col::clip : d > -12 ? col::warn : col::ok; };
    if (pub && pub->base && chipPk)
    {
        const double td = pub->base->tp > 0 ? 20 * std::log10 (pub->base->tp) : -99.0;
        chipPk->text = "TP " + String (td, 1) + " dB"; chipPk->colour = dbCol (td); chipRms->text = "RMS " + String (pub->base->rms, 1) + " dB"; chipRms->colour = dbCol (pub->base->rms); chipLufs->text = "LUFS " + String (pub->base->lufs, 1);
        const auto& gl = pub->base->glue; chipGPk->text = "Peak " + String (gl.pk, 1) + " dB"; chipGGr->text = gl.on ? "GR " + String (gl.mg, 1) + " dB" : String ("GR off");
    }
    const auto nn = hz2note (P[id::tailHz]); const int cr = (int) std::lround (nn.cents);
    if (keyChipBtn) { keyChipBtn->text = nn.name + " " + (cr >= 0 ? "+" : "") + String (cr) + " ct"; keyChipBtn->repaint(); }
    { String key = "Free"; for (int mi = 23; mi <= 39; ++mi) if (std::abs (440.0 * std::pow (2.0, (mi - 69) / 12.0) - P[id::tailHz]) < 0.05) key = String (kNoteNames[mi % 12]) + String (mi / 12 - 1); if (selKey) selKey->text = key + String::fromUTF8 (" \xe2\x96\xbe"); }
    { const int b = proc.bassRoot.load(); if (selBass) selBass->text = (b < 0 ? String ("Off") : String (kNoteNames[b])) + String::fromUTF8 (" \xe2\x96\xbe");
      if (bassChip) { if (b < 0) { bassChip->text = "-"; bassChip->colour = col::read; } else { const int m = (int) std::lround (12 * std::log2 (P[id::tailHz] / 440.0) + 69), d = (((m - b) % 12) + 12) % 12;
          if (d == 0) { bassChip->text = "on root"; bassChip->colour = col::ok; } else if (d == 7) { bassChip->text = "on fifth"; bassChip->colour = col::ok; } else if (d == 5) { bassChip->text = "on fourth"; bassChip->colour = col::warn; } else { bassChip->text = String (d > 6 ? d - 12 : d) + " st off"; bassChip->colour = col::clip; } } } }
    if (polBtn) polBtn->on = proc.getPol() < 0;
    if (alignBtn) { alignBtn->on = P[id::palign] > 0.5; alignBtn->text = alignBtn->on ? "Align on" : "Align off"; }
    if (limBtn) { limBtn->on = P[id::lim] > 0.5; limBtn->text = limBtn->on ? "Limiter On" : "Limiter Off"; }
    if (kmS) { kmS->on = proc.keyMode.load() == 0; kmP->on = proc.keyMode.load() == 1; }
    if (loopBtn) { loopBtn->on = proc.loopOn.load(); bassBtn->on = proc.bassOn.load(); }
    for (int l = 0; l < 3; ++l) if (pickBtn[l])
    { const int s = proc.getSrc (l); pickBtn[l]->text = s == 7 ? "Sample: " + proc.ownName (l) : srcName (s); plBtn[l]->on = proc.getLayerPol (l) < 0; }
    if (envResetBtn) envResetBtn->setVisible (proc.getEnv().size() >= 2);
    for (auto* c : glueGroup) c->setVisible (renderTab == 0);
    if (renderSave) { renderSave->setVisible (renderTab == 1); renderCopy->setVisible (renderTab == 1); }
    if (tabGlue) { tabGlue->on = renderTab == 0; tabRender->on = renderTab == 1; }
    for (auto* c : chips) c->repaint();
    for (auto* b : btns) b->repaint();
    repaint();
}

//==============================================================================
// presets
namespace
{
struct PEntry { String name; bool factory; int index; File file; };
std::vector<PEntry> presetList (KickProcessor& p)
{
    std::vector<PEntry> v; for (int i = 0; i < kFactoryCount; ++i) v.push_back ({ kFactory[i].name, true, i, {} });
    auto files = p.presetDir().findChildFiles (File::findFiles, false, "*.kkpreset"); files.sort();
    for (auto& f : files) v.push_back ({ f.getFileNameWithoutExtension(), false, -1, f });
    return v;
}
void loadEntry (KickProcessor& p, const PEntry& e)
{
    if (e.factory) { p.loadFactory (e.index); return; }
    if (auto xml = parseXML (e.file)) { p.loadStateTree (ValueTree::fromXml (*xml), false); p.presetName = e.name; p.factoryIndex = -1; }
}
String cleanName (String s) { return s.removeCharacters ("\\/:*?\"<>|").trim().substring (0, 60); }
}

void KickEditor::Content::stepPreset (int dir)
{
    auto list = presetList (proc); if (list.empty()) return;
    int cur = -1; for (size_t i = 0; i < list.size(); ++i) if ((proc.factoryIndex >= 0 && list[i].factory && list[i].index == proc.factoryIndex) || (proc.factoryIndex < 0 && ! list[i].factory && list[i].name == proc.presetName)) cur = (int) i;
    const int n = (int) list.size(); cur = ((cur < 0 ? 0 : cur) + dir + n) % n;
    loadEntry (proc, list[(size_t) cur]); refreshUi(); auditionSoon();
}
void KickEditor::Content::showPresetMenu()
{
    auto list = presetList (proc); PopupMenu m; PopupMenu tags;
    m.addSectionHeader ("Factory");
    for (auto& e : list) if (e.factory) m.addItem (1000 + e.index, e.name, true, proc.factoryIndex == e.index);
    bool any = false; for (auto& e : list) if (! e.factory) { if (! any) { m.addSeparator(); m.addSectionHeader ("Yours"); any = true; } m.addItem (5000 + (int) (&e - list.data()), e.name, true, proc.factoryIndex < 0 && proc.presetName == e.name); }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (nameBtn).withMaximumNumColumns (1), [this, list] (int r)
    {
        if (r >= 5000 && r - 5000 < (int) list.size()) { loadEntry (proc, list[(size_t) (r - 5000)]); refreshUi(); auditionSoon(); }
        else if (r >= 1000 && r < 1000 + kFactoryCount) { proc.loadFactory (r - 1000); refreshUi(); auditionSoon(); }
    });
}
void KickEditor::Content::askText (const String& title, const String& msg, const String& initial, std::function<void (String)> done)
{
    dlg = std::make_unique<AlertWindow> (title, msg, MessageBoxIconType::NoIcon); dlg->addTextEditor ("name", initial); dlg->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); dlg->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    dlg->enterModalState (true, ModalCallbackFunction::create ([this, done] (int r) { const String t = dlg ? dlg->getTextEditorContents ("name") : String(); dlg.reset(); if (r == 1) done (t); }), false);
}
void KickEditor::Content::writePreset (const String& name)
{
    auto t = proc.currentTree(); t.setProperty ("presetName", name, nullptr); t.setProperty ("factoryIndex", -1, nullptr);
    if (auto xml = t.createXml()) xml->writeTo (proc.presetDir().getChildFile (name + ".kkpreset"));
    proc.presetName = name; proc.factoryIndex = -1; proc.presetEdited = false; refreshUi();
}
void KickEditor::Content::doSave()
{
    askText ("Save sound", "Name for your preset. Saving a factory sound makes your own copy.", proc.factoryIndex >= 0 ? proc.presetName + " copy" : proc.presetName, [this] (String n)
    {
        n = cleanName (n); if (n.isEmpty()) return; const auto f = proc.presetDir().getChildFile (n + ".kkpreset");
        if (f.existsAsFile())
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Overwrite?", "A preset called \"" + n + "\" already exists.", "Overwrite", "Cancel", this, ModalCallbackFunction::create ([this, n] (int r) { if (r == 1) writePreset (n); }));
        else writePreset (n);
    });
}
void KickEditor::Content::doRename()
{
    if (proc.factoryIndex >= 0) { AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Rename", "Factory sounds can not be renamed. Use Save to make your own copy."); return; }
    askText ("Rename sound", "New name:", proc.presetName, [this] (String n)
    {
        n = cleanName (n); if (n.isEmpty() || n == proc.presetName) return; const auto from = proc.presetDir().getChildFile (proc.presetName + ".kkpreset"), to = proc.presetDir().getChildFile (n + ".kkpreset");
        if (to.existsAsFile()) { AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Rename", "That name is already used."); return; }
        if (from.existsAsFile()) from.deleteFile(); writePreset (n);
    });
}
void KickEditor::Content::doDelete()
{
    if (proc.factoryIndex >= 0) { AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Delete", "Factory sounds stay."); return; }
    AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Delete", "Delete \"" + proc.presetName + "\"?", "Delete", "Cancel", this, ModalCallbackFunction::create ([this] (int r)
    { if (r == 1) { proc.presetDir().getChildFile (proc.presetName + ".kkpreset").deleteFile(); proc.loadFactory (0); refreshUi(); } }));
}
void KickEditor::Content::doExport()
{
    auto t = proc.currentTree(); if (auto xml = t.createXml()) { SystemClipboard::copyTextToClipboard (xml->toString (XmlElement::TextFormat().singleLine())); AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Export", "This sound is on your clipboard as text. Paste it anywhere to share it, and use Import to load one."); }
}
void KickEditor::Content::doImport()
{
    if (auto xml = parseXML (SystemClipboard::getTextFromClipboard())) if (xml->hasTagName ("STATE")) { proc.loadStateTree (ValueTree::fromXml (*xml), false); if (proc.presetName.isEmpty()) proc.presetName = "Imported"; refreshUi(); auditionSoon(); return; }
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Import", "The clipboard does not hold a Kick Kitten sound.");
}
void KickEditor::Content::showGuide()
{
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::NoIcon, "Quick guide",
        "Drag a knob up or down. Shift+drag is fine. Double click resets it.\n\n"
        "Pitch envelope: drag the dots; drag the line to bend it; double click empty space adds a dot, double click a dot removes it.\n\n"
        "Keyboard: A W S E D F T G Y H U J K O L P ; ' play notes, Z and X move the octave, Space hits the kick, left and right arrows change preset.\n\n"
        "Pitched keys retune the whole kick to the note. Same keeps the kick as you set it.\n\n"
        "Drag the waveform into your DAW to drop the kick as a .wav, or use Export .wav.");
}
void KickEditor::Content::showSettings()
{
    PopupMenu m; m.addItem (1, "Info bubbles", true, bubblesOn); m.addItem (2, "Reset window size (100%)");
    m.showMenuAsync (PopupMenu::Options().withParentComponent (nullptr), [this] (int r)
    {
        if (r == 1) { bubblesOn = ! bubblesOn; hideTip(); proc.presetDir().getParentDirectory().getChildFile ("settings.txt").replaceWithText (String ("bubbles=") + (bubblesOn ? "1" : "0") + "\n"); }
        else if (r == 2) owner.resetTo100();
    });
}
void KickEditor::Content::showAbout()
{
    auto* w = new AlertWindow ("Kick Kitten " + String (KK_VERSION), "A free and open source trance kick maker by Studio 87.\nLicence: AGPL-3.0-or-later. It collects nothing and makes no network calls.\n\nYour presets live in:\n" + proc.presetDir().getFullPathName(), MessageBoxIconType::NoIcon);
    w->addButton ("Support on Ko-fi", 2); w->addButton ("Source code", 3); w->addButton ("Close", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([] (int r) { if (r == 2) URL ("https://ko-fi.com/studio87").launchInDefaultBrowser(); else if (r == 3) URL ("https://github.com/SpeakingColors/KickKitten").launchInDefaultBrowser(); }), true);
}

//==============================================================================
// wav export, drag to DAW, reference
namespace
{
String wavNameFor (const String& preset, double hz)
{
    String n = preset.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 #-").trim().replaceCharacter (' ', '_'); if (n.isEmpty()) n = "Kick";
    return "KickKitten_" + n + "_" + hz2note (hz).name.replace ("#", "s") + ".wav";
}
bool writeWav (const File& f, const std::vector<float>& pcm, double sr)
{
    f.deleteFile(); std::unique_ptr<FileOutputStream> os (f.createOutputStream()); if (! os) return false;
    WavAudioFormat wav; std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 24, {}, 0)); if (! w) return false; os.release();
    AudioBuffer<float> b (1, (int) pcm.size()); b.copyFrom (0, 0, pcm.data(), (int) pcm.size()); return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
}
void KickEditor::Content::exportWav()
{
    if (! pub || ! pub->base) return; auto keep = pub;
    chooserHolder = std::make_unique<FileChooser> ("Save the kick as a .wav", File::getSpecialLocation (File::userMusicDirectory).getChildFile (wavNameFor (proc.presetName, proc.getParam (id::tailHz))), "*.wav");
    chooserHolder->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting, [this, keep] (const FileChooser& fc)
    { auto f = fc.getResult(); if (f == File()) return; if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav"); if (! writeWav (f, keep->base->pcm, keep->base->sr)) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Export", "Could not write that file."); });
}
void KickEditor::Content::dragWavOut()
{
    if (! pub || ! pub->base) return;
    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("Kick Kitten").getChildFile (wavNameFor (proc.presetName, proc.getParam (id::tailHz)));
    f.getParentDirectory().createDirectory(); if (writeWav (f, pub->base->pcm, pub->base->sr)) DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragStarted = false; });
}
void KickEditor::Content::loadReference (const File& f)
{
    AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<AudioFormatReader> rd (fm.createReaderFor (f)); if (! rd) { refMsg = "could not read"; cvSpec->repaint(); return; }
    const int n = (int) std::min<int64> (rd->lengthInSamples, (int64) (rd->sampleRate * 1.5)); AudioBuffer<float> b ((int) rd->numChannels, n); rd->read (&b, 0, n, 0, true, true);
    std::vector<float> m ((size_t) n); float pk = 1e-9f; for (int i = 0; i < n; ++i) { float a = 0; for (int c = 0; c < b.getNumChannels(); ++c) a += b.getSample (c, i); m[(size_t) i] = a / (float) b.getNumChannels(); pk = jmax (pk, std::abs (m[(size_t) i])); }
    size_t on = 0; for (size_t i = 0; i < m.size(); ++i) if (std::abs (m[i]) > 0.1f * pk) { on = i; break; }
    on = on > 32 ? on - 32 : 0; std::vector<float> t (m.begin() + (long) on, m.end()); for (auto& v : t) v *= 0.9f / pk;
    constexpr int NF = 16384; dsp::FFT fft (14); std::vector<float> d ((size_t) NF * 2, 0.f); double ws = 0; const size_t nn = std::min (t.size(), (size_t) NF);
    for (int i = 0; i < NF; ++i) { const double w = 0.5 * (1 + std::cos (MathConstants<double>::pi * i / NF)); ws += w; if ((size_t) i < nn) d[(size_t) i] = (float) (t[(size_t) i] * w); }
    fft.performFrequencyOnlyForwardTransform (d.data()); refMags.resize (NF / 2); for (int k = 0; k < NF / 2; ++k) refMags[(size_t) k] = (float) (2.0 * d[(size_t) k] / ws);
    refSr = rd->sampleRate; refName = f.getFileNameWithoutExtension(); refMsg = "reference: " + refName; if (refBtn) refBtn->text = "Clear"; cvSpec->repaint();
}

//==============================================================================
// keyboard
bool KickEditor::Content::keyPressed (const KeyPress& k)
{
    const juce_wchar c = CharacterFunctions::toUpperCase (k.getTextCharacter());
    if (k == KeyPress::spaceKey) { proc.hitReq = true; return true; }
    if (k == KeyPress::leftKey) { stepPreset (-1); return true; } if (k == KeyPress::rightKey) { stepPreset (1); return true; }
    if (c == 'Z') { proc.baseMidi = jlimit (24, 60, proc.baseMidi.load() - 12); proc.requestRender(); return true; }
    if (c == 'X') { proc.baseMidi = jlimit (24, 60, proc.baseMidi.load() + 12); proc.requestRender(); return true; }
    return keyOffset (c) >= 0;
}
int KickEditor::Content::keyOffset (juce_wchar c)
{
    static const juce_wchar keys[18] = { 'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K', 'O', 'L', 'P', ';', '\'' };
    for (int i = 0; i < 18; ++i) if (keys[i] == c) return i; return -1;
}
bool KickEditor::Content::keyStateChanged (bool)
{
    static const juce_wchar keys[18] = { 'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K', 'O', 'L', 'P', ';', '\'' };
    bool used = false;
    for (int i = 0; i < 18; ++i)
    {
        const bool dn = KeyPress::isKeyCurrentlyDown ((int) keys[i]); const int m = proc.baseMidi.load() + i;
        if (dn && ! (qDown & (1u << i))) { qDown |= 1u << i; proc.keyState.noteOn (1, m, 0.8f); used = true; }
        else if (! dn && (qDown & (1u << i))) { qDown &= ~(1u << i); proc.keyState.noteOff (1, m, 0.f); used = true; }
    }
    return used;
}
void KickEditor::Content::mouseDown (const MouseEvent&) { grabKeyboardFocus(); }

//==============================================================================
KickEditor::KickEditor (KickProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    content = std::make_unique<Content> (*this, p); addAndMakeVisible (*content);
    constrainer.setFixedAspectRatio ((double) kW / kH); constrainer.setSizeLimits (700, (int) (700.0 * kH / kW), 2600, (int) (2600.0 * kH / kW));
    setConstrainer (&constrainer);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer); border->setBorderThickness (BorderSize<int> (10)); addAndMakeVisible (*border);
    corner = std::make_unique<ResizableCornerComponent> (this, &constrainer); addAndMakeVisible (*corner);
    setResizable (true, false);
    const int w = proc.editorWidth > 0 ? jlimit (700, 2600, proc.editorWidth) : kDefaultW;
    setSize (w, (int) std::lround (w * (double) kH / kW));
    content->grabKeyboardFocus();
}
KickEditor::~KickEditor() { content.reset(); }
void KickEditor::timerCallback() {}
void KickEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setTransform (AffineTransform::scale (s)); content->setBounds (0, 0, kW, kH);
    if (border) { border->setBounds (getLocalBounds()); border->toFront (false); }
    if (corner) { corner->setBounds (getWidth() - 24, getHeight() - 24, 24, 24); corner->toFront (false); }
    proc.editorWidth = getWidth();
}
void KickEditor::resetTo100() { setSize (kDefaultW, (int) std::lround (kDefaultW * (double) kH / kW)); }
void KickEditor::zoomBy (float f) { const int w = jlimit (700, 2600, (int) std::lround (getWidth() * f)); setSize (w, (int) std::lround (w * (double) kH / kW)); }
int KickEditor::zoomPercent() const { return (int) std::lround (100.0 * getWidth() / kDefaultW); }
