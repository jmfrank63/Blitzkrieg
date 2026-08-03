#include <cstdint>

#ifndef RUNTIME_HEADER_INDEX
#error RUNTIME_HEADER_INDEX must select one runtime StdAfx header
#endif

#if RUNTIME_HEADER_INDEX == 0
#include "AILogic/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 1
#include "Anim/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 2
#include "Common/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 3
#include "Formats/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 4
#include "Game/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 5
#include "GameTT/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 6
#include "Image/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 7
#include "Input/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 8
#include "Main/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 9
#include "Misc/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 10
#include "Net/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 11
#include "RandomMapGen/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 12
#include "Scene/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 13
#include "SFX/StdAfx.h"
#elif RUNTIME_HEADER_INDEX == 14
#include "UI/StdAfx.h"
#else
#error Unknown runtime header index
#endif

static_assert(sizeof(QWORD) == 8);
static_assert(sizeof(int64) == 8);

int main() { return 0; }
