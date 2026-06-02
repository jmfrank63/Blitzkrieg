
#if !defined(AFX_SCENEVISITOR_H__CC0E1C35_E89D_4F88_9097_8E7A4330C6C2__INCLUDED_)
#define AFX_SCENEVISITOR_H__CC0E1C35_E89D_4F88_9097_8E7A4330C6C2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

interface ISceneVisitor  
{
	virtual void STDCALL VisitBoldLine( CVec3 *corners, float fWidth, DWORD color ) = 0;
	virtual void STDCALL VisitUIRects( interface IGFXTexture *pTexture, const int nShadingEffect, struct SGFXRect2 *rects, const int nNumRects ){  }
	virtual void STDCALL VisitUIText( interface IGFXText *pText, const CTRect<float> &rcRect, const int nY, const DWORD dwColor, const DWORD dwFlags ){  }
};

#endif // !defined(AFX_SCENEVISITOR_H__CC0E1C35_E89D_4F88_9097_8E7A4330C6C2__INCLUDED_)
