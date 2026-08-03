#include "StdAfx.h"
#include "editor.h"
#include "VectorObjectsState.h"
#include "frames.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

void CVectorObjectsState::Enter()
{
	CInputMultiState::Enter();
}
