#if !defined(__FMT__VSO__H__)
#define __FMT__VSO__H__

#pragma ONCE

#include "..\Image\Image.h"
struct SVectorStripeObjectPoint
{
	CVec3 vPos;														// point position
	CVec3 vNorm;													// normale at this point
	float fRadius;												// curvature radius
	float fWidth;													// width at this point
	bool	bKeyPoint;											// key point of the sampling
	float fOpacity;												// прозрачность ( 0..1 ) только дл€ key point

	SVectorStripeObjectPoint()
		: vPos( VNULL3 ), vNorm( VNULL3 ), fRadius( 0.0f ), fWidth( 0.0f ), bKeyPoint( false ), fOpacity( 1.0f ) {}

	int operator&( IDataTree &ss );
};

struct SVectorStripeObjectDesc
{
	enum EType
	{
		TYPE_UNKNOUN	= 0,
		TYPE_RIVER		= 1,
		TYPE_ROAD			= 2,
		TYPE_RAILROAD	= 3,
	};
	
	struct SLayer
	{
		BYTE opacityCenter;									// прозрачность в центре потока
		BYTE opacityBorder;									// прозрачность по кра€м
		float fStreamSpeed;									// условна€ скорость потока
		float fTextureStep;									// шаг текстурировани€ по тайлам
		int nNumCells;											// ширина потока в €чейках (в тайлах)
		bool bAnimated;											// animated layer
		std::string szTexture;							// текстура потока (или директори€, если это анимированна€ текстура)
		float fDisturbance;									// mesh disturbance
		float fRelWidth;										// relative width

		SLayer()
			: opacityCenter( 0xff ), opacityBorder( 0x80 ), fStreamSpeed( 0.1f ), fTextureStep( 0.1f ),	fDisturbance( 0.3f ), fRelWidth( 1 ), nNumCells( 4 ), bAnimated( false ) {}
		
		int operator&( IDataTree &ss );
		int operator&( IStructureSaver &ss );
	};
	
	int	eType;														// type
	int nPriority;												// priority
	float fPassability;										// passability
	DWORD dwAIClasses;										// AI классы, которые не могут ходить по этой дороге

	enum ESoilParams
	{ 
		ESP_TRACE = 0x01,
		ESP_DUST	= 0x10
	};
	BYTE cSoilParams;											// параметры почвы - следы, пыль и т.д.
	
	SLayer bottom;												// bottom central layer
	std::vector<SLayer> bottomBorders;		// bottom layer border parts
	std::vector<SLayer> layers;						// additional layers
	SColor miniMapCenterColor;						// цвет обьекта на минимапе ( центральна€ часть )
	SColor miniMapBorderColor;						// цвет обьекта на минимапе ( край )
	
	std::string szAmbientSound;
	
	SVectorStripeObjectDesc() 
		: eType( TYPE_UNKNOUN ), nPriority( 0 ), miniMapCenterColor( 0x00000000 ), miniMapBorderColor( 0x00000000 ),
			fPassability( 1.0f ), dwAIClasses( 0 ), cSoilParams( 0 ) { }
	
	virtual int operator&( IDataTree &ss );
	virtual int operator&( IStructureSaver &ss );
};

struct SVectorStripeObject : SVectorStripeObjectDesc
{
	std::string szDescName;								// complete path to descriptor

	std::vector<SVectorStripeObjectPoint> points;	// points
	std::vector<CVec3> controlpoints;			// control polyline points

	int nID;															// ID


	virtual int operator&( IDataTree &ss );
	virtual int operator&( IStructureSaver &ss );
};

typedef std::vector<SVectorStripeObject> TVSOList;
#endif //#if !defined(__FMT__VSO__H__)
