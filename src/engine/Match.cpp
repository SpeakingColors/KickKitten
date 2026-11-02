#include "Match.h"
#include <cmath>
#include <algorithm>
#include <complex>
static const double kPi = 3.14159265358979323846;
namespace kk
{
namespace
{
void fftInPlace(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / (double) len; const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1, 0);
            for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; }
        }
    }
}
const std::vector<double>& refQ() { static std::vector<double> q; if (q.empty()) for (int i = 0; i < 32; ++i) q.push_back(30.0 * std::pow(9000.0 / 30.0, i / 31.0)); return q; }
const int REFT[10] = { 5, 15, 30, 50, 80, 120, 170, 220, 270, 320 };
double clampd(double v, double a, double b) { return std::max(a, std::min(b, v)); }
}

std::vector<float> spectrumMags(const std::vector<float>& b)
{
    std::vector<std::complex<double>> a((size_t) kNF); double ws = 0; const size_t n = std::min(b.size(), (size_t) kNF);
    for (int i = 0; i < kNF; ++i) { const double w = 0.5 * (1 + std::cos(kPi * i / kNF)); ws += w; a[(size_t) i] = (size_t) i < n ? (double) b[(size_t) i] * w : 0.0; }
    fftInPlace(a);
    std::vector<float> m((size_t) kNF / 2); for (int k = 0; k < kNF / 2; ++k) m[(size_t) k] = (float) (2.0 * std::abs(a[(size_t) k]) / ws);
    return m;
}

RefFeat featOf(const std::vector<float>& b, double sr)
{
    const auto m = spectrumMags(b); const double df = sr / kNF; std::vector<double> sp; double mx = 1e-12;
    for (double f : refQ())
    {
        const int ka = std::max(1, (int) std::floor(f * 0.89 / df)); const int kb = std::max(ka + 1, (int) std::ceil(f * 1.12 / df)); double e = 0;
        for (int k = ka; k < kb && k < kNF / 2; ++k) e += (double) m[(size_t) k] * m[(size_t) k];
        sp.push_back(e / (kb - ka)); mx = std::max(mx, sp.back());
    }
    RefFeat r; for (double e : sp) r.sp.push_back((float) (10 * std::log10(e / mx + 1e-12)));
    const int w = 2 * (int) std::floor(0.006 * sr); std::vector<double> en; double em = 1e-9;
    for (int t : REFT)
    {
        const int c = (int) std::floor(t / 1000.0 * sr); double a = 0; int n = 0;
        for (int i = std::max(0, c - w / 2); i < std::min((int) b.size(), c + w / 2); ++i) { a += std::fabs(b[(size_t) i]); ++n; }
        en.push_back(n ? a / n : 0); em = std::max(em, en.back());
    }
    for (double e : en) r.en.push_back((float) (20 * std::log10(e / em + 1e-6)));
    return r;
}

double featDist(const RefFeat& a, const RefFeat& b)
{
    double d = 0; for (size_t i = 0; i < a.sp.size(); ++i) { const double e = std::max(-60.f, a.sp[i]) - std::max(-60.f, b.sp[i]); d += e * e / (double) a.sp.size(); }
    double e2 = 0; for (size_t i = 0; i < a.en.size(); ++i) { const double e = std::max(-50.f, a.en[i]) - std::max(-50.f, b.en[i]); e2 += e * e / (double) a.en.size(); }
    return d + 0.6 * e2;
}

bool makeRef(const std::vector<float>& raw, double sr, RefInfo& out)
{
    float pk = 0; for (float v : raw) pk = std::max(pk, std::fabs(v)); if (pk < 1e-6f) return false;
    size_t s0 = 0; while (s0 < raw.size() && std::fabs(raw[s0]) < 0.05f * pk) ++s0;
    const size_t back = (size_t) std::floor(0.0005 * sr); s0 = s0 > back ? s0 - back : 0;
    const size_t len = std::min(raw.size() - s0, (size_t) std::floor(0.5 * sr));
    std::vector<float> x(raw.begin() + (long) s0, raw.begin() + (long) (s0 + len)); for (auto& v : x) v /= pk;
    const auto mags = spectrumMags(x);
    double tail = 0;
    {
        std::vector<double> lp(x.begin(), x.end());
        for (int pass = 0; pass < 4; ++pass) { const double a = std::exp(-2 * kPi * 160 / sr); double y = 0; for (auto& v : lp) { y = (1 - a) * v + a * y; v = y; } }
        const size_t i0 = (size_t) std::floor(0.12 * sr), i1 = std::min(lp.size() - 1, (size_t) std::floor(0.24 * sr)); std::vector<double> zc;
        for (size_t i = i0 + 1; i < i1; ++i) if (lp[i - 1] <= 0 && lp[i] > 0) zc.push_back((double) (i - 1) + (-lp[i - 1]) / (lp[i] - lp[i - 1]));
        if (zc.size() > 2) tail = (double) (zc.size() - 1) / ((zc.back() - zc.front()) / sr);
    }
    if (!(tail > 25 && tail < 120))
    {
        const double df = sr / kNF; int bi = 0; double bv = 0;
        for (int k = (int) std::floor(30 / df); k < (int) std::floor(110 / df); ++k) if (mags[(size_t) k] > bv) { bv = mags[(size_t) k]; bi = k; }
        tail = bi * df;
    }
    out.feat = featOf(x, sr); out.tail = tail; return true;
}

double matchToRef(Params& P, double sr, const SampleLookup& lookup, const RefInfo& ref, const std::function<void(int)>& progress, const std::atomic<bool>* stop)
{
    struct K { int id; bool lg; };
    const K KS[] = { { id::decay, 1 }, { id::ampShape, 0 }, { id::hold, 0 }, { id::swell, 0 }, { id::eMud, 0 }, { id::eChest, 0 }, { id::eShine, 0 }, { id::sThump, 0 },
                     { id::sPunch, 0 }, { id::sCrack, 0 }, { id::clickLvl, 0 }, { id::clickFreq, 1 }, { id::clickLen, 1 } };
    const int nk = (int) (sizeof KS / sizeof KS[0]);
    P[id::tailHz] = clampd(ref.tail, SPEC[id::tailHz].mn, SPEC[id::tailHz].mx);
    auto cost = [&](const Params& q) { return featDist(featOf(renderKick(q, sr, lookup, nullptr), sr), ref.feat); };
    Params cur = P; double best = cost(cur); int n = 0; const int tot = nk * 2 * 4;
    for (int round = 0; round < 4; ++round)
    {
        const double step = 0.12 / std::pow(2.0, round);
        for (const auto& k : KS)
        {
            const auto& sp = SPEC[k.id];
            for (int dir : { 1, -1 })
            {
                if (stop && stop->load()) return best;
                double v = k.lg ? cur[k.id] * std::exp(dir * step * std::log(sp.mx / std::max(sp.mn, 1e-3)) * 0.35) : cur[k.id] + dir * step * (sp.mx - sp.mn);
                v = clampd(v, sp.mn, sp.mx); ++n;
                if (progress) progress((int) std::lround(100.0 * n / tot));
                if (v == cur[k.id]) continue;
                Params t = cur; t[k.id] = v; const double c = cost(t);
                if (c < best) { best = c; cur = t; }
            }
        }
    }
    P = cur; return best;
}
}
