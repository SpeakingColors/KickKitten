#pragma once
// Sound bank: assets/bank.bin = per entry { uint32 sr, uint32 n, float32[n] } (little endian), plus bank.json for names.
#include "KickEngine.h"
#include <cstring>
namespace kk
{
struct Bank
{
    std::vector<Sample> items;
    bool load(const void* data, size_t size)
    {
        items.clear();
        const unsigned char* p = (const unsigned char*) data; size_t pos = 0;
        while (pos + 8 <= size)
        {
            uint32_t sr, n; std::memcpy(&sr, p + pos, 4); std::memcpy(&n, p + pos + 4, 4); pos += 8;
            if (pos + (size_t) n * 4 > size) return false;
            Sample s; s.sr = sr; s.data.resize(n); std::memcpy(s.data.data(), p + pos, (size_t) n * 4); pos += (size_t) n * 4;
            items.push_back(std::move(s));
        }
        return pos == size && ! items.empty();
    }
    const Sample* get(int srcId) const { const int i = srcId - 8; return i >= 0 && i < (int) items.size() ? &items[(size_t) i] : nullptr; }
};
}
