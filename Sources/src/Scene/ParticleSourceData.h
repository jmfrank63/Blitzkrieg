#ifndef __PARTICLESOURCEDATA_H__
#define __PARTICLESOURCEDATA_H__
#pragma ONCE
#include "PFX.h"
#include "Track.h"
struct SExtendedParticle: public SSimpleParticle
{
	NTimer::STime birthTime;              // время рождения
	NTimer::STime deathTime;              // время смерти
	float fSpin;                          // угловая скорость
	CVec3 vSpeed;                         // просто скорость
	CVec3 vWind;                          // ветер для частицы
	float fOpacity;                       // начальная прозрачность
	STrackContext contextSpeed;						// контексты для расчета интегралов
	STrackContext contextZSpeed; 
	STrackContext contextSpin;
};
enum
{
	PSA_TYPE_SQUARE = 0,
	PSA_TYPE_DISK   = 1,
	PSA_TYPE_CIRCLE = 2
};
struct SParticleSourceData : public CTRefCount<ISharedResource>
{
	OBJECT_SERVICE_METHODS( SParticleSourceData );
	DECLARE_SERIALIZE;
	SHARED_RESOURCE_METHODS( nRefData, "ParticleSource.Data" );
public:
	SParticleSourceData();

	bool bComplexParticleSource;					//тип источника, если true, complex particle source
	int nLifeTime;												// время жизни всего источника
	float fGravity;												// параметр гравитации (никакого отношения к физике не имеет)
	int nTextureDX;												// сколько кадров по X (для анимированной текстуры)
	int nTextureDY;												// сколько кадров по Y (для анимированной текстуры)
	std::string szTextureName;            // название текстуры
	CVec3 vWind;                          // ветер
	CVec3 vDirection;                     // направление источника
	int nAreaType;                        // тип области для генерации партиклов
	float fRadialWind;                    // сила радиального ветра
	CTrack trackGenerateArea;							// размер области из которой вылетают партиклы 
	CTrack trackDensity;									// к-во партиклов рожденных в ед. времени 
	CTrack trackBeginSpeed;								// начальная скорость частицы при  вылете 
	CTrack trackBeginSpeedRandomizer;     // ее рандомизатор
	CTrack trackBeginAngleRandomizer;			// рандомизатор угла вылета (в радианах, от 0 до PI)
	CTrack trackLife;											// сколько живет партикл после генерации
	CTrack trackLifeRandomizer;           // рандомизатор предыдущего (от 0 до 1)
	CTrack trackGenerateSpin;							// начальная угловая скорость при вылете 
	CTrack trackGenerateSpinRandomizer;   // рандомизатор предыдущего (пусть задается, но пока не юзается)
	CTrack trackGenerateOpacity;					// начальная прозрачность при вылете ( 0 - 255 ) 
	CTrack trackSpin;                     // коэфф. угловой скорости (0-1)
	CTrack trackSpeed;                    // коэфф. скорости (0-1)
	CTrack trackSpeedRnd;                 // его рандомизатор (0-1)
	CTrack trackWeight;										// масса партикла (никакого отношения к физической массе не имеет)
	CTrack trackTextureFrame;							// frame in texture [0..1]
	CTrack trackSize;											// размер частици (0-1)
	CTrack trackOpacity;									// коэффициент на прозрачность (0-1)
	CTrack trackIntegralMass;             // первообразная от g*m(t), умноженная на коэффициент скорости (без его рандома) (сохранять не надо, рассчитывается по ходу дела)
	float fDensityCoeff;                  // коэффициент на плотность, берется из сеттингов
	virtual void STDCALL Init();
	virtual void STDCALL InitIntegrals();
	virtual void STDCALL SwapData( ISharedResource *pResource );
	virtual void STDCALL ClearInternalContainer() {  }
	virtual bool STDCALL Load( const bool bPreLoad = false );
	virtual int STDCALL operator&( IDataTree &ss );
};
#endif // __PARTICLESOURCEDATA_H__
