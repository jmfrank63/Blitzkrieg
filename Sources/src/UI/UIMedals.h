#ifndef __UI_MEDALS_H__
#define __UI_MEDALS_H__
#include "UIBasic.h"
#include "UISlider.h"
class CUIMedals : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	CUIScrollBar *pScrollBar;				//инициализируется во время загрузки и используется для ускорения доступа к компонентам
	int nSpace;											//отступ текста слева от контрола и справа от скроллбара
	int nVTextSpace;								//отступ текста сверху от медали
	int nHSubSpace;									//отступ одной медали от другой по горизонтали
	int nVSubSpace;									//отступ одной медали от другой по вертикали
	int nNextPosX, nNextPosY;				//позиция следующей медали
	int nMedalsCount;								//число медалей
	std::vector< CTRect<float> > medalMaps;		//для сохранения текстурных координат медалек
	
public:
	CUIMedals() : nSpace( 4 ), nHSubSpace( 10 ), nVSubSpace( 10 ), nMedalsCount( 0 ),
		nNextPosX( 0 ), nNextPosY( 0 ), nVTextSpace( 5 ) {}
	~CUIMedals() {}
	
	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );
	
	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	
	virtual void STDCALL ShowWindow( int _nCmdShow );
	
	void ClearMedals();
	void AddMedal( IGFXTexture *pTexture, const CTRect<float> &mapImageRect, const WORD *pszMedalsName );
	void UpdateMedals();
	
private:
	void ComputeHPositions();					//вычисляется один раз при добавлении медалек, потом скроллирование только по вертикали
	void UpdatePositions();						//вызывается при обновлении позиции ScrollBar
	void UpdateScrollbar();						//вызывается после добавления всех objectives для обновления ScrollBar
	void InitMaps();									//для инициализации текстурных координат
};
class CUIMedalsBridge : public IUIContainer, public CUIMedals
{
	OBJECT_NORMAL_METHODS( CUIMedalsBridge );
	DECLARE_SUPER( CUIMedals );
	DEFINE_UICONTAINER_BRIDGE;
};
#endif		//__UI_MEDALS_H__
