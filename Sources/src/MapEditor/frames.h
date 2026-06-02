#ifndef __FRAMES_H__
#define __FRAMES_H__

#include <Mmsystem.h>
#include "..\Misc\FileUtils.h"
#include "MiniMapDialog.h"

using std::string;
using std::vector;

class CTemplateEditorFrame;
class CMiniMapDialog;
class CGameWnd;


/**
inline DWORD COLORREF2GFXColor( COLORREF color )
{
	return 0xff000000 | ( (color >> 16) & 0xff) | ( color & 0x0000ff00 ) | ( (color & 0xff) << 16 );
}
/**/


struct TEFConsts
{
	static const int THUMBNAILTILE_WIDTH;
	static const int THUMBNAILTILE_HEIGHT;

	static const int THUMBNAILTILEWITHTEXT_SPACE_X;
	static const int THUMBNAILTILEWITHTEXT_SPACE_Y;

	static  int THUMBNAILTILE_SPACE_X;
	static  int THUMBNAILTILE_SPACE_Y;
};

class CFrameManager
{
public:
	enum EFrameType
	{
		E_UNKNOWN_FRAME,
		E_GUI_FRAME,
		E_ANIMATION_FRAME,
		E_SPRITE_FRAME,
	  E_TEMPLATE_FRAME,
		E_EFFECT_FRAME,
		E_OBJECT_FRAME,
		E_MESH_FRAME,
		E_WEAPON_FRAME,
		E_BUILDING_FRAME,
		E_TILESET_FRAME,
		E_ROAD_FRAME,
		E_FENCE_FRAME,
		E_PARTICLE_FRAME,
		E_TRENCH_FRAME,
		E_SQUAD_FRAME,
		E_MINE_FRAME,
		E_BRIDGE_FRAME,
	};

private:
	EFrameType activeFrameType;
	CWnd *pActiveFrame;
	CGameWnd *pGameWnd;

	CTemplateEditorFrame *pTemplateEditorFrame;
	CMiniMapDialog *pwndMiniMapDialog;	

public:
	CFrameManager();
	~CFrameManager() {}
	
	CWnd *GetActiveWnd() { return pActiveFrame; }
	void SetGameWnd( CGameWnd *pWnd ) { pGameWnd = pWnd; }
	void SetTemplateEditorFrame( CTemplateEditorFrame * pFrame) { pTemplateEditorFrame = pFrame;}
	void SetMiniMapWindow( CMiniMapDialog *_pwndMiniMapDialog ) { pwndMiniMapDialog = _pwndMiniMapDialog;}

	EFrameType GetActiveFrameType() { return activeFrameType; }
	void SetActiveFrame( CWnd *pNewActiveFrame );
	CWnd *GetFrameWindow( int nID );

	CGameWnd* GetGameWnd() { return pGameWnd; }
	CTemplateEditorFrame* GetTemplateEditorFrame() { return pTemplateEditorFrame; }
	CMiniMapDialog* GetMiniMapWindow() { return pwndMiniMapDialog; }
};

extern CFrameManager g_frameManager;

#endif		//__FRAMES_H__
