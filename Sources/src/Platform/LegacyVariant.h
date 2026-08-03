#ifndef BLITZKRIEG_PLATFORM_LEGACY_VARIANT_H
#define BLITZKRIEG_PLATFORM_LEGACY_VARIANT_H

#if defined(_WIN32) || defined(_WIN64)
#include <comutil.h>
#else
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>

using VARTYPE = std::uint16_t;
using VARIANT_BOOL = std::int16_t;
using SCODE = std::int32_t;
using OLECHAR = wchar_t;
using BSTR = OLECHAR *;

constexpr VARTYPE VT_EMPTY = 0;
constexpr VARTYPE VT_NULL = 1;
constexpr VARTYPE VT_I2 = 2;
constexpr VARTYPE VT_I4 = 3;
constexpr VARTYPE VT_R4 = 4;
constexpr VARTYPE VT_R8 = 5;
constexpr VARTYPE VT_CY = 6;
constexpr VARTYPE VT_DATE = 7;
constexpr VARTYPE VT_BSTR = 8;
constexpr VARTYPE VT_ERROR = 10;
constexpr VARTYPE VT_BOOL = 11;
constexpr VARTYPE VT_VARIANT = 12;
constexpr VARTYPE VT_UNKNOWN = 13;
constexpr VARTYPE VT_DISPATCH = 9;
constexpr VARTYPE VT_UI1 = 17;
constexpr VARTYPE VT_UI8 = 21;
constexpr VARTYPE VT_INT = 22;
#define V_BOOL(v) ((v)->boolVal)
constexpr VARTYPE VT_BYREF = 0x4000;
constexpr VARIANT_BOOL VARIANT_FALSE = 0;
constexpr VARIANT_BOOL VARIANT_TRUE = static_cast<VARIANT_BOOL>(-1);

struct tagCY { std::int32_t Lo; std::int32_t Hi; };

inline BSTR SysAllocString(const OLECHAR *value)
{
    if (!value) return nullptr;
    const std::size_t length = std::wcslen(value);
    BSTR result = new OLECHAR[length + 1];
    std::wmemcpy(result, value, length);
    result[length] = L'\0';
    return result;
}
inline BSTR SysAllocStringLen(const OLECHAR *value, unsigned int length)
{
    BSTR result = new OLECHAR[length + 1];
    if (value && length) std::wmemcpy(result, value, length);
    result[length] = L'\0';
    return result;
}
inline void SysFreeString(BSTR value) { delete[] value; }
inline unsigned int SysStringLen(BSTR value) { return value ? static_cast<unsigned int>(std::wcslen(value)) : 0; }
inline unsigned int SysStringByteLen(BSTR value) { return SysStringLen(value) * sizeof(OLECHAR); }

class variant_t {
public:
    VARTYPE vt;
    union {
        unsigned char bVal;
        short iVal;
        int lVal;
        int intVal;
        unsigned int uintVal;
        float fltVal;
        double dblVal;
        double date;
        std::uint64_t ulVal;
        VARIANT_BOOL boolVal;
        SCODE scode;
        tagCY cyVal;
        BSTR bstrVal;
        void *byref;
        void *punkVal;
        void *pdispVal;
    };

    variant_t() : vt(VT_EMPTY), ulVal(0) {}
    variant_t(const variant_t &other) : vt(VT_EMPTY), ulVal(0) { *this = other; }
    variant_t(variant_t &&other) noexcept : vt(other.vt), ulVal(other.ulVal) {
        if (vt == VT_BSTR) other.bstrVal = nullptr;
        other.vt = VT_EMPTY;
    }
    explicit variant_t(bool value) : vt(VT_BOOL), boolVal(value ? VARIANT_TRUE : VARIANT_FALSE) {}
    explicit variant_t(unsigned char value) : vt(VT_UI1), bVal(value) {}
    explicit variant_t(short value) : vt(VT_I2), iVal(value) {}
    explicit variant_t(int value) : vt(VT_I4), lVal(value) {}
    explicit variant_t(long value) : vt(VT_I4), lVal(static_cast<int>(value)) {}
    explicit variant_t(std::uint64_t value) : vt(VT_UI8), ulVal(value) {}
    variant_t(float value) : vt(VT_R4), fltVal(value) {}
    variant_t(double value) : vt(VT_R8), dblVal(value) {}
    explicit variant_t(const char *value) : vt(VT_BSTR), bstrVal(nullptr) {
        if (!value) { bstrVal = SysAllocString(L""); return; }
        std::wstring converted;
        while (*value) converted.push_back(static_cast<unsigned char>(*value++));
        bstrVal = SysAllocString(converted.c_str());
    }
    explicit variant_t(const std::string &value) : variant_t(value.c_str()) {}
    explicit variant_t(const wchar_t *value) : vt(VT_BSTR), bstrVal(SysAllocString(value ? value : L"")) {}
    explicit variant_t(const std::wstring &value) : variant_t(value.c_str()) {}
    ~variant_t() { clear(); }

    variant_t &operator=(const variant_t &other) {
        if (this == &other) return *this;
        clear(); vt = other.vt;
        if (vt == VT_BSTR) bstrVal = SysAllocString(other.bstrVal); else std::memcpy(&ulVal, &other.ulVal, sizeof(ulVal));
        return *this;
    }
    variant_t &operator=(variant_t &&other) noexcept {
        if (this == &other) return *this;
        clear(); vt = other.vt; std::memcpy(&ulVal, &other.ulVal, sizeof(ulVal));
        if (vt == VT_BSTR) other.bstrVal = nullptr;
        other.vt = VT_EMPTY; return *this;
    }
    operator bool() const { return vt == VT_BOOL ? boolVal != VARIANT_FALSE : lVal != 0; }
    operator short() const { return vt == VT_I2 ? iVal : static_cast<short>(lVal); }
    operator int() const { return vt == VT_I2 ? iVal : (vt == VT_UI1 ? bVal : lVal); }
    operator long() const { return static_cast<long>(static_cast<int>(*this)); }
    operator float() const { return vt == VT_R4 ? fltVal : static_cast<float>(dblVal); }
    operator double() const { return vt == VT_R8 ? dblVal : static_cast<double>(fltVal); }
    void clear() { if (vt == VT_BSTR) SysFreeString(bstrVal); vt = VT_EMPTY; ulVal = 0; }
    bool operator==(const variant_t &other) const {
        if (vt != other.vt) return false;
        if (vt == VT_BSTR) return SysStringLen(bstrVal) == SysStringLen(other.bstrVal) && std::wcscmp(bstrVal ? bstrVal : L"", other.bstrVal ? other.bstrVal : L"") == 0;
        return std::memcmp(&ulVal, &other.ulVal, sizeof(ulVal)) == 0;
    }
};

using VARIANT = variant_t;

class bstr_t {
    std::wstring value_;
    mutable std::string narrow_;
public:
    bstr_t() = default;
    bstr_t(const char *value) { if (value) while (*value) value_.push_back(static_cast<unsigned char>(*value++)); }
    bstr_t(const std::string &value) : bstr_t(value.c_str()) {}
    bstr_t(const wchar_t *value) : value_(value ? value : L"") {}
    bstr_t(const std::wstring &value) : value_(value) {}
    bstr_t(BSTR value) : value_(value ? value : L"") {}
    bstr_t(const variant_t &value) {
        if (value.vt == VT_BSTR && value.bstrVal) value_ = value.bstrVal;
        else if (value.vt == VT_BOOL) value_ = value.boolVal ? L"-1" : L"0";
        else if (value.vt == VT_I4) value_ = std::to_wstring(value.lVal);
        else if (value.vt == VT_R8) value_ = std::to_wstring(value.dblVal);
    }
    operator const wchar_t *() const { return value_.c_str(); }
    operator BSTR() const { return const_cast<BSTR>(value_.c_str()); }
    operator const char *() const { narrow_.clear(); for (wchar_t c : value_) narrow_.push_back(static_cast<char>(c)); narrow_.push_back('\0'); return narrow_.c_str(); }
    std::size_t length() const { return value_.size(); }
};
using _bstr_t = bstr_t;
#endif

#endif
