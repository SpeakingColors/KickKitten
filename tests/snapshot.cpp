// Renders the real editor offscreen to a PNG (for screenshots and checks): snapshot out.png [presetIndex] [width]
#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"
#include <string>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    KickProcessor p; p.setPlayConfigDetails (0, 2, 44100.0, 512); p.prepareToPlay (44100.0, 512);
    if (argc > 2) p.loadFactory (std::atoi (argv[2]));
    for (int i = 0; i < 100; ++i) { juce::MessageManager::getInstance()->runDispatchLoopUntil (50); if (p.published() && p.published()->base) break; }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    const bool rt = argc > 4 && std::string (argv[4]).find ("render") != std::string::npos; const juce::File ref (argc > 5 ? argv[5] : "");
    const int w = argc > 3 ? std::atoi (argv[3]) : 1640; ed->setSize (w, (int) std::lround (w * 946.0 / 1640.0));
    if (rt || ref != juce::File()) static_cast<KickEditor*> (ed.get())->showForTest (rt, ref);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argc > 1 ? argv[1] : "snap.png"); out.deleteFile(); juce::FileOutputStream os (out); juce::PNGImageFormat png; png.writeImageToStream (img, os);
    ed.reset(); return 0;
}
