#include "streamio_c.h"

#include <cassert>

int main()
{
    void *first = bk_streamio_temp_buffer(64, 0);
    void *second = bk_streamio_temp_buffer(64, 0);
    assert(first != nullptr);
    assert(second != nullptr);
    assert(bk_streamio_temp_buffer(1, 10) == nullptr);
    return 0;
}
