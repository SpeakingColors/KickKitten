// Match test: make a reference from known settings, start from other settings, check the distance drops and the settings move toward the reference.
#include "../src/engine/Match.h"
#include <cstdio>
#include <cmath>
using namespace kk;
int main()
{
    const double sr = 44100; SampleLookup none = [](int, int) -> const Sample* { return nullptr; };
    Params target; target[id::decay] = 380; target[id::eMud] = 40; target[id::sThump] = 80; target[id::clickLvl] = 20; target[id::tailHz] = 52;
    const auto ref = renderKick(target, sr, none, nullptr);
    RefInfo ri; if (!makeRef(ref, sr, ri)) { std::puts("FAIL makeRef"); return 1; }
    std::printf("ref tail %.2f Hz (target 52)\n", ri.tail);
    Params start; const double d0 = featDist(featOf(renderKick(start, sr, none, nullptr), sr), ri.feat);
    Params m = start; int last = -1; const double d1 = matchToRef(m, sr, none, ri, [&](int p) { last = p; }, nullptr);
    std::printf("distance %.2f -> %.2f, progress end %d\n", d0, d1, last);
    std::printf("decay %.0f -> %.0f (target 380), tail %.1f\n", start[id::decay], m[id::decay], m[id::tailHz]);
    const bool ok = d1 < d0 && std::fabs(m[id::tailHz] - 52) < 4 && last >= 95;
    std::puts(ok ? "OK" : "FAIL"); return ok ? 0 : 1;
}
