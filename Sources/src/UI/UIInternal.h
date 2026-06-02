#ifndef __UIINTERNAL_H__
#define __UIINTERNAL_H__
#include "..\Main\TextSystem.h"
#include "..\sfx\sfx.h"
#include "MaskSystem.h"
enum EUIWindowSubState
{
	E_NORMAL_STATE				= 0,
	E_HIGHLIGHTED_STATE		= 1,
	E_PUSHED_STATE				= 2,			//Не менять эти ID!
	E_DISABLED_STATE			= 3,

	UI_ESS_FORCE_DWORD	= 0x7fffffff
};
struct SWindowSubRect
{
	CTRect<float> rc;
	CTRect<float> mapa;
};
struct CUIWindowSubState
{
private:
	DECLARE_SERIALIZE;
	CObj<IManipulator> pManipulator;

public:
	std::vector<SWindowSubRect> subRects;

	CPtr<IGFXTexture> pTexture;						// внешний вид - текстура
	CPtr<IUIMask> pMask;
	DWORD color;
	DWORD specular;
	DWORD textColor;

	CUIWindowSubState() : color( 0xffffffff ), specular( 0xff000000 ), textColor( 0 ) {}
	int operator&( IDataTree &ss );

	IManipulator *GetManipulator();

	void CopyInternals( CUIWindowSubState *pSS ) const;
private:
/*
	struct SWindowTileRect
	{
		CTRect<float> rc;			//размер на контроле, куда тайлятся прямоугольники
		CVec2 vSize;					//размер одного тайла
		CTRect<float> mapa;		//текстурные координаты для одного тайла
		int operator&( IDataTree &ss );
	};
	std::vector<SWindowTileRect> tileRects;
*/

	void SaveTextureAndSubRects( CTreeAccessor *pFile );
	void LoadTextureAndSubRects( CTreeAccessor *pFile );
	void LoadTileRects( CTreeAccessor *pFile );
};
struct CWindowState
{
private:
	DECLARE_SERIALIZE;
	CObj<IManipulator> pManipulator;

	void InitDependentInfo();
public:
	std::string szPushSound;						// звук при нажатии левой кнопкой мышки на контрол
	std::string szClickSound;						// звук при нажатии клике мышкой в области контрола (нажалась и отжалась внутри)

	CUIWindowSubState subStates[4];			// нормальный, подсвеченный, придавленный и disabled
	std::string szKey;									// ключ для текстовой системы, по этому ключу можно получить текст в окошке
	CPtr<IGFXText> pGfxText;						// для отображения текста

	std::string szToolKey;							// ключ для загрузки тултипа из текстовых ресурсов
	CPtr<IText> pToolText;							// текст отображаемый в качестве тултипа

	int operator&( IDataTree &ss );
	CWindowState() {}
	IManipulator *GetManipulator();

	void CopyInternals( CWindowState * pS ) const;
};
void LoadTileRectangles( CTreeAccessor *pFile, std::vector<SWindowSubRect> &subRects, DTChunkID sName, IGFXTexture *pTexture );
void SaveTextureAndMap( CTreeAccessor *pFile, IGFXTexture *pTexture, DTChunkID tName, const CTRect<float> &maps, DTChunkID mName );
void LoadTextureAndMap( CTreeAccessor *pFile, CPtr<IGFXTexture> *ppTexture, DTChunkID tName, CTRect<float> *pMaps, DTChunkID mName );
void SaveSound( CTreeAccessor *pFile, ISound *pSound, DTChunkID sName );
void LoadSound( CTreeAccessor *pFile, CPtr<ISound> *ppSound, DTChunkID sName );

#endif // __UIINTERNAL_H__
