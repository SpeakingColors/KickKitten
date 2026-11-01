#pragma once
// Kick Kitten engine: offline render of one kick, straight port of renderKick() in the mockup.
#include "Params.h"
#include <vector>
#include <functional>
namespace kk
{
struct Sample { std::vector<float> data; double sr = 44100.0; };

// Gives the layer a sample for source ids 7 (own sample slot) and 8+ (bank). Returns nullptr when missing.
using SampleLookup = std::function<const Sample*(int layer, int srcId)>;

struct GlueInfo { static constexpr int nb = 150; float lv[nb]; float gr[nb]; float thr = 0, pk = 0, mg = 0; bool on = false; };

std::vector<float> renderKick(const Params& p, double sr, const SampleLookup& lookup, GlueInfo* glue = nullptr);
// pitch envelope value in Hz at t milliseconds
double envValue(const std::vector<EnvDot>& e, double ms);
// one layer on its own at rate fs (for the layer displays); empty when off
std::vector<float> renderLayer(const Params& p, int layer, double fs, int N, const SampleLookup& lookup);
double truePeak(const std::vector<float>& b);
double lufs400(const std::vector<float>& b, double sr);
}
