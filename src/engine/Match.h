#pragma once
// Reference match: port of featOf / featDist / setRef (tail detection) / matchRef in the mockup.
#include "KickEngine.h"
#include <atomic>
namespace kk
{
struct RefFeat { std::vector<float> sp, en; };
struct RefInfo { RefFeat feat; double tail = 55.0; };

constexpr int kNF = 16384;
// magnitude spectrum of the first kNF samples (half-cosine taper), same as spectrumDb() in the mockup
std::vector<float> spectrumMags(const std::vector<float>& b);
RefFeat featOf(const std::vector<float>& b, double sr);
double featDist(const RefFeat& a, const RefFeat& b);
// Prepares a reference from raw mono samples: trims to the onset, normalises, finds its tail pitch. false when silent.
bool makeRef(const std::vector<float>& raw, double sr, RefInfo& out);
// Hill-climbs the parameters in the mockup's list so the rendered kick gets closer to the reference.
// progress gets 0..100; stop is polled. Returns the final distance.
double matchToRef(Params& p, double sr, const SampleLookup& lookup, const RefInfo& ref, const std::function<void(int)>& progress, const std::atomic<bool>* stop);
}
