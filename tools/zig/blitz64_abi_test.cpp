#include <cstdint>

#include "blitz64.h"

int main()
{
    union
    {
        float value;
        std::uint32_t bits;
    } nan = { 0.0f };

    nan.bits = 0x7fc01234u;
    return bk_f32_bits_c(nan.value) == nan.bits ? 0 : 1;
}
