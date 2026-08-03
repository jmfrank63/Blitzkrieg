#ifndef BLITZKRIEG_PORTABLE_COMDEF_H
#define BLITZKRIEG_PORTABLE_COMDEF_H
class _com_error {
public:
    explicit _com_error(long) {}
    const char *ErrorMessage() const { return "COM unavailable on Linux"; }
};
#endif
