#pragma once
// Kick Kitten engine parameters. The table is generated from the mockup (tools/gen_spec.py).
#include <array>
#include <vector>
#include <cstdint>
namespace kk
{
namespace id { enum { 
#include "SpecEnum.inc"
}; }
struct SpecInfo { const char* id; const char* label; double mn, mx; const char* unit; int dec; double def; bool lg; const char* tip; };
extern const SpecInfo SPEC[id::NPARAMS];
int specIndex(const char* name);   // -1 when unknown

struct EnvDot { double ms, hz, bend; };

struct Params
{
    std::array<double, id::NPARAMS> v;
    int src[3] { 0, 0, 0 };        // layer source: 0 off, 1..6 live tops, 7 own sample, 8+ bank
    int lpol[3] { 1, 1, 1 };       // layer polarity
    double pol = 1.0;              // global polarity
    std::vector<EnvDot> env;       // pitch dots; empty = legacy curve from Start, Sweep, Curve
    Params() { for (int i = 0; i < id::NPARAMS; ++i) v[(size_t) i] = SPEC[i].def; }
    double operator[] (int i) const { return v[(size_t) i]; }
    double& operator[] (int i) { return v[(size_t) i]; }
};
}
