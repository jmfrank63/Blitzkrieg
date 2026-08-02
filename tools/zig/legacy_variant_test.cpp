#include "Platform/LegacyVariant.h"
#include <cassert>
#include <cwchar>
#include <utility>

int main()
{
    variant_t empty;
    assert(empty.vt == VT_EMPTY);
    variant_t integer(42);
    assert(integer.vt == VT_I4 && integer.lVal == 42);
    variant_t copied(integer);
    variant_t moved(std::move(copied));
    assert(moved == integer);

    variant_t text(L"portable variant");
    variant_t deep_copy = text;
    assert(text.vt == VT_BSTR && deep_copy.vt == VT_BSTR);
    assert(text.bstrVal != deep_copy.bstrVal);
    assert(SysStringLen(deep_copy.bstrVal) == 16);
    assert(bstr_t(deep_copy).length() == 16);

    variant_t assigned;
    assigned = text;
    text.bstrVal[0] = L'P';
    assert(std::wcscmp(assigned.bstrVal, L"portable variant") == 0);

    variant_t boolean(true);
    assert(boolean.vt == VT_BOOL && boolean.boolVal == VARIANT_TRUE);
    variant_t real(3.5);
    assert(real.vt == VT_R8 && real.dblVal == 3.5);

    BSTR raw = SysAllocString(L"round trip");
    assert(SysStringByteLen(raw) == SysStringLen(raw) * sizeof(OLECHAR));
    SysFreeString(raw);
    return 0;
}
