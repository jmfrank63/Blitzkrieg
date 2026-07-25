// Minimal stand-in for the MFC afxres.h the original VC6 project pulled in.
// Game.rc only needs the standard resource definitions plus IDC_STATIC.
#ifndef __AFXRES_H__
#define __AFXRES_H__
#include <winres.h>
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif
#endif // __AFXRES_H__
