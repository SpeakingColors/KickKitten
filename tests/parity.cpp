// Renders every factory preset with the C++ engine and compares with the mockup's JS renders.
#include "../src/engine/Bank.h"
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <iterator>
using namespace kk;
static std::vector<char> readAll(const std::string& f) { std::ifstream s(f, std::ios::binary); return { std::istreambuf_iterator<char>(s), {} }; }
int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    Bank bank; auto bb = readAll(root + "/assets/bank.bin");
    if (! bank.load(bb.data(), bb.size())) { std::printf("bank load failed\n"); return 2; }
    std::printf("bank: %zu entries\n", bank.items.size());
    std::ifstream idx(root + "/tests/parity/index.txt"); double sr; idx >> sr; std::string line; std::getline(idx, line);
    int fails = 0;
    while (std::getline(idx, line))
    {
        if (line.empty()) continue;
        const int n = std::stoi(line.substr(0, line.find(' '))); const std::string name = line.substr(line.find(' ') + 1);
        Params p; std::ifstream pf(root + "/tests/parity/p" + std::to_string(n) + ".txt"); std::string k;
        while (pf >> k)
        {
            if (k == "env") { std::string v; pf >> v; if (v != "-") { std::stringstream ss(v); std::string d; while (std::getline(ss, d, ';')) { EnvDot e; std::sscanf(d.c_str(), "%lf,%lf,%lf", &e.ms, &e.hz, &e.bend); p.env.push_back(e); } } continue; }
            double v; pf >> v;
            if (k == "pol") p.pol = v;
            else if (k.size() == 5 && k[0] == 'l' && k.substr(2) == "src") p.src[k[1] - '0'] = (int) v;
            else if (k.size() == 5 && k[0] == 'l' && k.substr(2) == "pol") p.lpol[k[1] - '0'] = (int) v;
            else { const int i = specIndex(k.c_str()); if (i >= 0) p.v[(size_t) i] = v; else std::printf("unknown key %s\n", k.c_str()); }
        }
        auto out = renderKick(p, sr, [&] (int, int src) { return bank.get(src); });
        auto rb = readAll(root + "/tests/parity/r" + std::to_string(n) + ".f32");
        const size_t nr = rb.size() / 4; const float* ref = (const float*) rb.data();
        double err = 0, pk = 1e-9, e2 = 0, r2 = 0;
        const size_t m = std::min(nr, out.size());
        for (size_t i = 0; i < m; ++i) { const double d = out[i] - ref[i]; err = std::max(err, std::abs(d)); pk = std::max(pk, (double) std::abs(ref[i])); e2 += d * d; r2 += (double) ref[i] * ref[i]; }
        const double snr = 10 * std::log10((r2 + 1e-20) / (e2 + 1e-20));
        const bool ok = nr == out.size() && snr > 60;
        std::printf("%-26s len js %zu c++ %zu  maxerr %.2e (peak %.3f)  SNR %.1f dB  %s\n", name.c_str(), nr, out.size(), err, pk, snr, ok ? "ok" : "FAIL");
        if (! ok) ++fails;
    }
    std::printf("%s\n", fails ? "PARITY FAILED" : "PARITY OK");
    return fails ? 1 : 0;
}
