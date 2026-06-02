
#if !defined(AFX_KEYBASEDDATA_H__A200BF93_5018_4053_AF5A_78E9878C0636__INCLUDED_)
#define AFX_KEYBASEDDATA_H__A200BF93_5018_4053_AF5A_78E9878C0636__INCLUDED_

#pragma ONCE

#include "SParticleKey.h"
struct SParticleSetup
{
	CTrack	trackSpin;												// коофицент на угловую скорость   ( 1 - 0 ) ( время 0 - 100 )
	CTrack	trackGenerateSpin;								// ( угловая скорость при вылете ) ( 0 - 2 )
	CTrack	trackLife;												// сколько живет партикл после генерации ( 10 - 1000 ) ( время 0 - 100 )
	CTrack	trackDensity;											// к-во партиклов рожденных в  ед. времени ( 0 -  0.1 ) ( время 0 - 100 )
	CVec3		vWind;														// ветер ( 0,0,0 )
	CTrack	trackWeight;										  // масса партикла ( 0 - 10 ) ( по умолчанию 1 ) ( время 0 - 100 )
	CTrack	trackSpeed;											  // коофицент скорости ( 1 - 0 ) ( время 0 - 100 ) ( время 0 - 100 )
	CTrack	trackGenerateArea;							  // размер области из которой вылетают партиклы ( 1 - 100 ) ( время 0 - 100 )
	CVec3		vDirection;												// направление ( 0,0,1 )
	int 		nGenerateAngel;										// 0 - max , 100000 ~ min ( угол разворота )
	CTrack	trackBeginSpeed;									// начальная сскрость частици при  вылете ( 0 - 2 ) ( по умолч  0.20 )
	CVec3		vPosition;
	CTrack 	trackGenerateAngle;								// угол разворота 0 - max , 100000 ~ min - ( время от 0 - 100 )
	CTrack 	trackSize;												// размер частици ( 1 -  200 ) ( время 0 - 100 )
	int			nTextureDX;												// разрешение дял анимационной текстуры - 1
	int			nTextureDY;												// - 1
	float		fGravity;													// g	- "просто g" :)			- ( 0.0001)
	std::string szTextureName;								//
	CTrack 	trackGenerateOpacity;							// прозрачность при генерации	 ( 0 - 255 ) ( время 0 - 100 )
	CTrack 	trackOpacity;											// коофицент на прозрачность	( 0 - 1 ) ( время 0 - 100 )
	int			nLifeTime;												// время жизни
	CTrack	trackGenerateSpinRand;						// размер rand() на вращение(в процентах )  ( 0 - 100 )
	CTrack  trackTextureFrame;								// frame in texture [0..1]
	int operator&( IStructureSaver &ss );
	int operator&( IDataTree &ss );
};
class CKeyBasedData  : public ISharedResource
{
	OBJECT_COMPLETE_METHODS( CKeyBasedData );
	SHARED_RESOURCE_METHODS( nRefData.a );
public:
	SParticleSetup keyData;
	virtual void STDCALL SwapData( ISharedResource *pResource )
	{
		CKeyBasedData *pRes = dynamic_cast<CKeyBasedData*>( pResource );
		NI_ASSERT_TF( pRes != 0, "shared resource is not a CKeyBasedData", return );
		std::swap( keyData, pRes->keyData );
	}
	virtual void STDCALL ClearInternalContainer() {  }
	virtual int STDCALL operator&( IDataTree &ss );
};
#endif // !defined(AFX_KEYBASEDDATA_H__A200BF93_5018_4053_AF5A_78E9878C0636__INCLUDED_)
