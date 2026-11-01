#include "KickEngine.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace kk
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
inline double clampd(double v, double a, double b) { return std::min(b, std::max(a, v)); }
inline double sgn(double v) { return v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0); }

struct Bq { double b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
enum class BqType { hp, lp, bp };
Bq bqMake(BqType type, double f, double fs)
{
    const double q = type == BqType::bp ? 1.2 : 0.7071, w = 2 * kPi * f / fs, c = std::cos(w), sn = std::sin(w), al = sn / (2 * q);
    double b0, b1, b2;
    if (type == BqType::hp) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
    else if (type == BqType::lp) { b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; }
    else { b0 = al; b1 = 0; b2 = -al; }
    const double a0 = 1 + al;
    Bq f2; f2.b0 = b0 / a0; f2.b1 = b1 / a0; f2.b2 = b2 / a0; f2.a1 = -2 * c / a0; f2.a2 = (1 - al) / a0;
    return f2;
}
inline double bqRun(Bq& f, double v)
{
    const double y = f.b0 * v + f.b1 * f.x1 + f.b2 * f.x2 - f.a1 * f.y1 - f.a2 * f.y2;
    f.x2 = f.x1; f.x1 = v; f.y2 = f.y1; f.y1 = std::isfinite(y) ? y : 0.0;
    return f.y1;
}

double envF(const std::vector<EnvDot>& e, double t)
{
    if (t <= e[0].ms) return e[0].hz;
    for (size_t i = 0; i + 1 < e.size(); ++i)
    {
        const auto& a = e[i]; const auto& b = e[i + 1];
        if (t < b.ms) { const double u = (t - a.ms) / (b.ms - a.ms), k = std::pow(2.0, 2 * a.bend); return a.hz * std::pow(b.hz / a.hz, std::pow(u, k)); }
    }
    return e.back().hz;
}

std::vector<float> genLayer(const Params& p, int i, double fs, int N, const SampleLookup& lookup)
{
    static const char* kLvl[3] = { "l0lvl", "l1lvl", "l2lvl" };
    const int src = p.src[i];
    if (!src) return {};
    const Sample* sm = nullptr;
    if (src >= 7) { sm = lookup ? lookup(i, src) : nullptr; if (!sm || sm->data.empty()) return {}; }
    const int base = specIndex(kLvl[i]);              // lvl, pitch, start, len, delay, hp, lp are consecutive in the table
    const double lvl = p[base], pitch = p[base + 1], startMs = p[base + 2], lenMs = p[base + 3], delayMs = p[base + 4], hpHz = p[base + 5], lpHz = p[base + 6];
    const double ratio = std::pow(2.0, pitch / 12), lenS = lenMs / 1000, tau = lenS / 4.6;
    const double g = lvl / 100 * (p.lpol[i] < 0 ? -1.0 : 1.0);
    const int i0 = (int) std::floor(delayMs / 1000 * fs);
    std::vector<float> out((size_t) N, 0.f);
    uint32_t seed = (uint32_t) (777 + i * 131);
    const bool hasHp = hpHz > 21, hasLp = lpHz < 19500;
    Bq hpF = bqMake(BqType::hp, std::min(hpHz, fs * 0.45), fs), lpF = bqMake(BqType::lp, std::min(lpHz, fs * 0.45), fs);
    Bq nf {}; bool hasNf = false;
    double y1 = 0, x1 = 0, ph = 0;
    if (src == 1) { nf = bqMake(BqType::bp, std::min(3500 * ratio, fs * 0.45), fs); hasNf = true; }
    else if (src == 5) { nf = bqMake(BqType::hp, std::min(7000 * ratio, fs * 0.45), fs); hasNf = true; }
    const double hpA = src == 3 ? std::exp(-2 * kPi * std::min(1500 * ratio, fs * 0.4) / fs) : 0.0, startS = startMs / 1000;
    for (int j = i0; j < N; ++j)
    {
        const double t = (j - i0) / fs; double v = 0;
        if (sm)
        {
            const double pos = (startS + t * ratio) * sm->sr; const long ii = (long) std::floor(pos);
            if (ii >= (long) sm->data.size() - 1 || t >= lenS) break;
            const double fr = pos - (double) ii;
            v = sm->data[(size_t) ii] * (1 - fr) + sm->data[(size_t) ii + 1] * fr;
            const double fd = std::min(0.005, lenS * 0.3), rem = lenS - t;
            if (rem < fd) v *= rem / fd;
        }
        else
        {
            if (t > lenS * 2) break;
            const double e = std::exp(-t / tau);
            if (src == 1 || src == 3 || src == 5)
            {
                seed = seed * 1664525u + 1013904223u;
                const double n = seed / 2147483648.0 - 1;
                if (src == 3) { const double hv = hpA * (y1 + n - x1); x1 = n; y1 = hv; v = hv * e * (1 + 1.5 * std::exp(-t / (tau * 0.15))) / 2.5 * 1.6; }
                else v = bqRun(nf, n) * e * (src == 1 ? 1.1 : 1.2);
            }
            else if (src == 2) { ph += 2 * kPi * std::min(2400 * ratio, fs * 0.4) / fs; v = std::sin(ph) * std::exp(-t / (tau * 0.5)); }
            else if (src == 4) { const double f = 700 * ratio + 7300 * ratio * std::exp(-t / (lenS / 6)); ph += 2 * kPi * std::min(f, fs * 0.4) / fs; v = std::sin(ph) * e; }
            else if (src == 6) { ph += 2 * kPi * std::min(1500 * ratio, fs * 0.4) / fs; v = std::sin(ph) * e; }
        }
        (void) hasNf;
        if (hasHp) v = bqRun(hpF, v);
        if (hasLp) v = bqRun(lpF, v);
        const double y = v * g, a = std::abs(y);
        out[(size_t) j] = (float) (a < 0.5 ? y : sgn(y) * (0.5 + 0.5 * std::tanh((a - 0.5) / 0.5)));
    }
    return out;
}

void peqRun(std::vector<float>& x, double fs, double f, double q, double gdb)
{
    if (std::abs(gdb) < 0.05) return;
    const double A = std::pow(10.0, gdb / 40), w = 2 * kPi * f / fs, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al / A;
    const double b0 = (1 + al * A) / a0, b1 = -2 * c / a0, b2 = (1 - al * A) / a0, a1 = -2 * c / a0, a2 = (1 - al / A) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (auto& s : x) { const double v = s, y = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y; s = (float) y; }
}
void shelfRun(std::vector<float>& x, double fs, double f, double gdb)
{
    if (std::abs(gdb) < 0.05) return;
    const double A = std::pow(10.0, gdb / 40), w = 2 * kPi * f / fs, c = std::cos(w), s = std::sin(w), al = s / 2 * std::sqrt(2.0), sq = 2 * std::sqrt(A) * al;
    const double a0 = (A + 1) - (A - 1) * c + sq, b0 = A * ((A + 1) + (A - 1) * c + sq) / a0, b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0, b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
    const double a1 = 2 * ((A - 1) - (A + 1) * c) / a0, a2 = ((A + 1) - (A - 1) * c - sq) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (auto& e : x) { const double v = e, y = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = v; y2 = y1; y1 = y; e = (float) y; }
}
void toneFx(std::vector<float>& x, double fs, const Params& p)
{
    peqRun(x, fs, 250, 0.9, -12 * p[id::eMud] / 100); peqRun(x, fs, 80, 0.8, 8 * p[id::eChest] / 100); shelfRun(x, fs, 2500, 15 * p[id::eShine] / 100);
}
double pkOf(const std::vector<float>& a) { double m = 1e-9; for (float f : a) { const double v = std::abs((double) f); if (v > m) m = v; } return m; }

void glueFx(std::vector<float>& x, double fs, const Params& p, GlueInfo* gi)
{
    const int N = (int) x.size();
    const double w = p[id::sGlue] / 100, pk0 = pkOf(x), thr = pk0 * std::pow(10.0, p[id::gThr] / 20);
    const double ca = std::exp(-1 / (p[id::gAtk] / 1000 * fs)), cr = std::exp(-1 / (p[id::gRel] / 1000 * fs)), ex = 1 - 1 / p[id::gRat];
    const int nb = GlueInfo::nb, bin = std::max(1, (int) std::floor(fs / 1000));
    std::vector<float> lv((size_t) nb, 1e-6f), gr((size_t) nb, 1.f), y((size_t) N);
    double env = 0;
    for (int i = 0; i < N; ++i)
    {
        const double a = std::abs((double) x[(size_t) i]);
        env = a > env ? ca * env + (1 - ca) * a : cr * env + (1 - cr) * a;
        const double g = env > thr ? std::pow(thr / env, ex) : 1.0, ge = 1 - w + w * g;
        y[(size_t) i] = (float) (x[(size_t) i] * ge);
        const int b = i / bin;
        if (b < nb) { if (a > lv[(size_t) b]) lv[(size_t) b] = (float) a; if (ge < gr[(size_t) b]) gr[(size_t) b] = (float) ge; }
    }
    if (w > 0) { const double sc = pk0 / pkOf(y); for (int i = 0; i < N; ++i) x[(size_t) i] = (float) (y[(size_t) i] * sc); }
    if (gi)
    {
        auto db = [] (double v) { return 20 * std::log10(std::max(v, 1e-6)); };
        double mg = 0;
        for (int b = 0; b < nb; ++b) { gi->gr[b] = (float) db(gr[(size_t) b]); gi->lv[b] = (float) db(lv[(size_t) b]); mg = std::min(mg, (double) gi->gr[b]); }
        gi->thr = (float) db(thr); gi->pk = (float) db(pk0); gi->mg = (float) (w > 0 ? mg : 0); gi->on = w > 0;
    }
}
void limiterFx(std::vector<float>& x, double fs, const Params& p)
{
    if (p[id::lim] == 0) return;
    const int N = (int) x.size();
    const double G = std::pow(10.0, p[id::limG] / 20), ceil = pkOf(x);
    const int L = std::max(2, (int) std::lround(0.0015 * fs));
    const double cr = std::exp(-1 / (8.0 / 1000 * fs));
    std::vector<float> r((size_t) N), gm((size_t) N), gs((size_t) N);
    for (int i = 0; i < N; ++i) { const double a = std::abs((double) x[(size_t) i]) * G; r[(size_t) i] = (float) (a > ceil ? ceil / a : 1.0); }
    for (int i = 0; i < N; ++i) { float m = 1.f; const int e = std::min(N, i + L); for (int j = i; j < e; ++j) if (r[(size_t) j] < m) m = r[(size_t) j]; gm[(size_t) i] = m; }
    double prev = 1;
    for (int i = 0; i < N; ++i) { const double v = gm[(size_t) i]; prev = v < prev ? v : prev + (v - prev) * (1 - cr); gs[(size_t) i] = (float) prev; }
    std::vector<double> q((size_t) N + 1, 0.0);
    for (int i = 0; i < N; ++i) q[(size_t) i + 1] = q[(size_t) i] + gs[(size_t) i];
    for (int i = 0; i < N; ++i)
    {
        const int s = std::max(0, i - L + 1);
        const double g = (q[(size_t) i + 1] - q[(size_t) s] + (L - (i + 1 - s)) * 1.0) / L;
        x[(size_t) i] = (float) (x[(size_t) i] * G * std::min(g, (double) r[(size_t) i]));
    }
}
void lp4(std::vector<float>& a, double f, double fs)
{
    Bq q1 = bqMake(BqType::lp, f, fs), q2 = bqMake(BqType::lp, f, fs);
    for (auto& s : a) s = (float) bqRun(q2, bqRun(q1, s));
    Bq q3 = bqMake(BqType::lp, f, fs), q4 = bqMake(BqType::lp, f, fs);
    for (size_t i = a.size(); i-- > 0;) a[i] = (float) bqRun(q4, bqRun(q3, a[i]));
}
void satBand(std::vector<float>& a, double d)
{
    const double pk = pkOf(a), td = std::tanh(d);
    for (auto& s : a) s = (float) (std::tanh(d * s / pk) * pk / td);
}
void sauceFx(std::vector<float>& x, double fs, const Params& p)
{
    const double A = p[id::sauce] / 100;
    if (A <= 0) return;
    const int N = (int) x.size();
    auto k = [&] (int n) { return p[n] / 100 * A; };
    const double kP = k(id::sPunch), kT = k(id::sThump), kB = k(id::sBloom), kC = k(id::sCrack), kL = k(id::sLoud);
    const double pk0 = pkOf(x);
    std::vector<float> low = x; lp4(low, 110, fs);
    std::vector<float> rest((size_t) N);
    for (int i = 0; i < N; ++i) rest[(size_t) i] = x[(size_t) i] - low[(size_t) i];
    std::vector<float> mid = rest; lp4(mid, 800, fs);
    std::vector<float> high((size_t) N);
    for (int i = 0; i < N; ++i) high[(size_t) i] = rest[(size_t) i] - mid[(size_t) i];
    std::vector<float> sm = mid, sl = low, sh = high;
    satBand(sm, 7); satBand(sl, 2.0); satBand(sh, 4);
    std::vector<float> y((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double t = i / fs, u = (t - 0.06) / 0.04;
        const double bl = 1 + kB * 1.6 * std::exp(-u * u);
        const double lo = (low[(size_t) i] * (1 - kT) + sl[(size_t) i] * kT * 1.25) * (1 + 0.35 * kT) * bl;
        const double mi = mid[(size_t) i] * (1 - kP) + sm[(size_t) i] * kP * 1.12;
        const double hi = (high[(size_t) i] * (1 - kC) + sh[(size_t) i] * kC) * (1 + kC * (0.8 + 2.4 * std::exp(-t / 0.006)));
        y[(size_t) i] = (float) (lo + mi + hi);
    }
    if (kL > 0)
    {
        const double gain = std::pow(10.0, 9 * kL / 20), kn = 0.5 * pk0, cl = 0.95 * pk0;
        for (auto& s : y) { const double v = s * gain, a = std::abs(v); s = (float) (a < kn ? v : sgn(v) * (kn + (cl - kn) * std::tanh((a - kn) / (cl - kn)))); }
    }
    const double sc = pk0 / pkOf(y);
    for (int i = 0; i < N; ++i) x[(size_t) i] = (float) (y[(size_t) i] * sc);
}

struct TpTable
{
    static constexpr int T = 16; double h[3][2 * T];
    TpTable() { for (int ph = 1; ph < 4; ++ph) { int n = 0; for (int k = -T + 1; k <= T; ++k) { const double d = k - ph / 4.0, s = std::abs(d) < 1e-9 ? 1.0 : std::sin(kPi * d) / (kPi * d), w = 0.5 + 0.5 * std::cos(kPi * d / T); h[ph - 1][n++] = s * w; } } }
};
}

double truePeak(const std::vector<float>& b)
{
    static const TpTable tp;
    double m = 0; const int n = (int) b.size();
    for (float f : b) m = std::max(m, std::abs((double) f));
    for (int i = 0; i < n; ++i)
        for (int ph = 0; ph < 3; ++ph)
        {
            double a = 0;
            for (int k = 0; k < 2 * TpTable::T; ++k) { const int idx = i + k - TpTable::T + 1; if (idx >= 0 && idx < n) a += tp.h[ph][k] * b[(size_t) idx]; }
            m = std::max(m, std::abs(a));
        }
    return m;
}

double lufs400(const std::vector<float>& b, double sr)
{
    struct St { double b[3]; double a1, a2; };
    auto kb = [&] (double f0, double Q, double G, bool hp) -> St
    {
        const double K = std::tan(kPi * f0 / sr); const double a0 = 1 + K / Q + K * K;
        if (hp) return { { 1, -2, 1 }, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0 };
        const double Vh = std::pow(10.0, G / 20), Vb = std::pow(Vh, 0.4996667741545416);
        return { { (Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0 }, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0 };
    };
    const St st[2] = { kb(1681.974450955533, 0.7071752369554196, 3.999843853973347, false), kb(38.13547087602444, 0.5003270373238773, 0, true) };
    std::vector<double> y(b.begin(), b.end());
    for (const auto& f : st) { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; for (auto& s : y) { const double x0 = s, o = f.b[0] * x0 + f.b[1] * x1 + f.b[2] * x2 - f.a1 * y1 - f.a2 * y2; x2 = x1; x1 = x0; y2 = y1; y1 = o; s = o; } }
    const size_t W = std::min(y.size(), (size_t) std::floor(0.4 * sr));
    if (W == 0) return -120.0;
    double s = 0, best;
    for (size_t i = 0; i < W; ++i) s += y[i] * y[i];
    best = s;
    for (size_t i = W; i < y.size(); ++i) { s += y[i] * y[i] - y[i - W] * y[i - W]; if (s > best) best = s; }
    return -0.691 + 10 * std::log10(best / (double) W + 1e-12);
}

std::vector<float> renderKick(const Params& p, double sr, const SampleLookup& lookup, GlueInfo* glue)
{
    constexpr int OS = 4;
    const double fs = sr * OS, hold = p[id::hold] / 1000, dec = p[id::decay] / 1000, len = hold + dec + 0.025;
    const int N = (int) std::ceil(len * fs);
    std::vector<float> x((size_t) N, 0.f);
    const double w0 = 2 * kPi * p[id::clickFreq] / fs, al = std::sin(w0) / (2 * 1.2), cw = std::cos(w0);
    const double b0 = al / (1 + al), b2 = -al / (1 + al), a1 = -2 * cw / (1 + al), a2 = (1 - al) / (1 + al);
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0, phi = 0; uint32_t seed = 12345;
    const double swl = p[id::swell] / 1000, shp = p[id::ampShape] != 0 ? p[id::ampShape] : 1.0, tauC = (p[id::clickLen] / 1000) / 4.6, sw = p[id::sweep] / 1000, twoPi = 2 * kPi;
    const double bl = p[id::body] / 100, sl = p[id::sub] / 100, cl = p[id::clickLvl] / 100 * 1.1;
    const bool useEnv = p.env.size() >= 2;
    for (int i = 0; i < N; ++i)
    {
        const double t = i / fs;
        const double f = useEnv ? envF(p.env, t * 1000) : p[id::tailHz] + (p[id::startHz] - p[id::tailHz]) * std::exp(-3 * std::pow(t / sw, p[id::curve]));
        phi += twoPi * f / fs;
        const double amp = t < hold ? 1.0 : std::pow(10.0, -3 * std::pow((t - hold) / dec, shp));
        const double swg = swl > 0 && t < swl ? std::pow(10.0, -1.2 * (1 - t / swl)) : 1.0;
        const double tone = std::sin(phi) * (bl * std::exp(-t / 0.035) + sl) * amp * swg;
        seed = seed * 1664525u + 1013904223u;
        const double n = seed / 2147483648.0 - 1;
        const double bp = b0 * n + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = n; y2 = y1; y1 = bp;
        const double click = t < tauC * 8 ? bp * std::exp(-t / tauC) : 0.0;
        x[(size_t) i] = (float) (0.5 * (tone + cl * click));
    }
    const bool pal = p[id::palign] != 0; const int na = std::min(N, (int) std::floor(0.004 * fs));
    for (int i = 0; i < 3; ++i)
    {
        auto L = genLayer(p, i, fs, N, lookup);
        if (!L.empty())
        {
            double sg = 1;
            if (pal) { double d = 0; for (int q = 0; q < na; ++q) d += (double) x[(size_t) q] * L[(size_t) q]; if (d < 0) sg = -1; }
            for (int q = 0; q < N; ++q) x[(size_t) q] = (float) (x[(size_t) q] + sg * L[(size_t) q]);
        }
    }
    { const int fl = (int) std::floor(0.01 * fs); for (int q = std::max(0, N - fl); q < N; ++q) x[(size_t) q] = (float) (x[(size_t) q] * (double) (N - q) / fl); }
    const double dr = p[id::drive] / 100, tr = p[id::trans] / 100, fz = p[id::fuzz] / 100;
    if (dr > 0 || tr > 0 || fz > 0)
    {
        const double gd = 1 + 7 * std::pow(dr, 1.5), gt = 1 + 4 * tr, gf = 1 + 12 * fz, bf = 0.15 * fz;
        auto sf = [] (double u) { return u / std::sqrt(1 + u * u); };
        const double sb = sf(bf);
        const int m = std::min(N, (int) std::floor(0.12 * fs)); double ri = 0, ro = 0;
        std::vector<float> y((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            double v = x[(size_t) i];
            if (dr > 0) v = std::tanh(gd * v) / std::tanh(gd * 0.7);
            if (tr > 0) { const double u = v + 0.5 * tr * v * v; v = std::tanh(gt * u) / std::tanh(gt * 0.7); }
            if (fz > 0) v = (sf(gf * v + bf) - sb) / sf(gf * 0.7);
            y[(size_t) i] = (float) v;
            if (i < m) { ri += (double) x[(size_t) i] * x[(size_t) i]; ro += (double) y[(size_t) i] * y[(size_t) i]; }
        }
        const double mk = ro > 1e-12 ? clampd(std::sqrt(ri / ro), 0.4, 2.5) : 1.0;
        for (int i = 0; i < N; ++i) x[(size_t) i] = (float) (y[(size_t) i] * mk);
    }
    sauceFx(x, fs, p);
    glueFx(x, fs, p, glue);
    toneFx(x, fs, p);
    limiterFx(x, fs, p);
    {
        const double c = p[id::clip] / 100, gc = 1 + 3 * c, tg = std::tanh(gc), R = 1 - twoPi * 8 / fs;
        double px = 0, py = 0;
        for (int i = 0; i < N; ++i)
        {
            double v = x[(size_t) i]; if (c > 0) v = std::tanh(gc * v) / tg;
            const double o = v - px + R * py; px = v; py = o; x[(size_t) i] = (float) o;
        }
    }
    const int M = 48; std::vector<double> h((size_t) (2 * M + 1)); const double fc = 0.45 * sr / fs; double hs = 0;
    for (int k = 0; k <= 2 * M; ++k)
    {
        const int d = k - M; const double s = d == 0 ? 2 * fc : std::sin(2 * kPi * fc * d) / (kPi * d);
        const double wv = 0.42 - 0.5 * std::cos(2 * kPi * k / (2 * M)) + 0.08 * std::cos(4 * kPi * k / (2 * M));
        h[(size_t) k] = s * wv; hs += h[(size_t) k];
    }
    for (auto& v : h) v /= hs;
    const int No = N / OS; std::vector<float> out((size_t) No);
    const double g = std::pow(10.0, p[id::out] / 20) * p.pol;
    for (int j = 0; j < No; ++j)
    {
        double a = 0; const int c0 = j * OS - M;
        for (int k = 0; k <= 2 * M; ++k) { const int idx = c0 + k; if (idx >= 0 && idx < N) a += h[(size_t) k] * x[(size_t) idx]; }
        a *= g; const double ab = std::abs(a);
        if (ab > 0.8) a = sgn(a) * (0.8 + 0.2 * std::tanh((ab - 0.8) / 0.2));
        out[(size_t) j] = (float) a;
    }
    {
        const double cl2 = std::pow(10.0, p[id::ceil] / 20), tp = truePeak(out);
        if (tp > cl2) { const double sc = cl2 / tp; for (auto& v : out) v = (float) (v * sc); }
    }
    return out;
}
}
