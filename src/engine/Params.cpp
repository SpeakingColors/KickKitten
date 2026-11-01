#include "Params.h"
#include <cstring>
namespace kk
{
const SpecInfo SPEC[id::NPARAMS] = {
#include "SpecTable.inc"
};
int specIndex(const char* name)
{
    for (int i = 0; i < id::NPARAMS; ++i) if (std::strcmp(SPEC[i].id, name) == 0) return i;
    return -1;
}
}
