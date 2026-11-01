#pragma once
#include "PluginProcessor.h"

// The Kick Kitten window. Layout, colours and wording follow docs/mockups/KickKitten-v1.html (generated table in
// ui/LayoutTable.inc). It is drawn in a fixed 1640 x 946 space and scaled to the window size, aspect ratio locked.
class KickEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit KickEditor (KickProcessor&);
    ~KickEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff121214)); }
    struct Content;
    void resetTo100();
    void zoomBy (float f);
    int zoomPercent() const;
private:
    void timerCallback() override;
    KickProcessor& proc;
    std::unique_ptr<Content> content;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> border;
    std::unique_ptr<juce::ResizableCornerComponent> corner;
    static constexpr int kDefaultW = 1400;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickEditor)
};
