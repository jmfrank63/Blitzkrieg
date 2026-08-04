#ifndef __UIINTERNAL_H__
#define __UIINTERNAL_H__
#include "../Main/TextSystem.h"
#include "../SFX/SFX.h"
#include "MaskSystem.h"
enum EUIWindowSubState
{
	E_NORMAL_STATE				= 0,
	E_HIGHLIGHTED_STATE		= 1,
	E_PUSHED_STATE				= 2,			//�� ������ ��� ID!
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

	CPtr<IGFXTexture> pTexture;						// ������� ��� - ��������
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
		CTRect<float> rc;			//������ �� ��������, ���� �������� ��������������
		CVec2 vSize;					//������ ������ �����
		CTRect<float> mapa;		//���������� ���������� ��� ������ �����
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
	std::string szPushSound;						// ���� ��� ������� ����� ������� ����� �� �������
	std::string szClickSound;						// ���� ��� ������� ����� ������ � ������� �������� (�������� � �������� ������)

	CUIWindowSubState subStates[4];			// ����������, ������������, ������������ � disabled
	std::string szKey;									// ���� ��� ��������� �������, �� ����� ����� ����� �������� ����� � ������
	CPtr<IGFXText> pGfxText;						// ��� ����������� ������

	std::string szToolKey;							// ���� ��� �������� ������� �� ��������� ��������
	CPtr<IText> pToolText;							// ����� ������������ � �������� �������

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
