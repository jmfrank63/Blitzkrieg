#ifndef BLITZKRIEG_PORTABLE_COMDEF_H
#define BLITZKRIEG_PORTABLE_COMDEF_H
#include <string>
typedef wchar_t *BSTR;
struct VARIANT {
    unsigned short vt = 0;
    void *byref = nullptr;
    unsigned char bVal = 0;
    short iVal = 0;
    int intVal = 0;
    long lVal = 0;
    float fltVal = 0;
    double dblVal = 0;
    BSTR bstrVal = nullptr;
};
struct variant_t : VARIANT {
    variant_t() = default;
    variant_t(float value) { vt = 4; fltVal = value; }
    variant_t(double value) { vt = 5; dblVal = value; }
    variant_t(const variant_t &) = default;
    variant_t &operator=(const VARIANT &other) { static_cast<VARIANT &>(*this) = other; return *this; }
    operator float() const { return vt == VT_R8 ? (float)dblVal : fltVal; }
};
#define VT_EMPTY 0
#define VT_NULL 1
#define VT_I2 2
#define VT_I4 3
#define VT_R4 4
#define VT_R8 5
#define VT_CY 6
#define VT_DATE 7
#define VT_BSTR 8
#define VT_BOOL 11
#define VT_UI1 17
#define V_BOOL(v) ((v)->intVal)
struct bstr_t {
    std::wstring value;
    bstr_t(const char *text) : value() { while (*text) value.push_back((wchar_t)(unsigned char)*text++); }
    operator BSTR() { return const_cast<BSTR>(value.c_str()); }
};
class _com_error {
public:
    explicit _com_error(long) {}
    const char *ErrorMessage() const { return "COM unavailable on Linux"; }
};
typedef bstr_t _bstr_t;
#endif
