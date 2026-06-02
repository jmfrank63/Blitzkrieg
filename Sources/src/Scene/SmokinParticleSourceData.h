#ifndef __SMOKIN_PARTICLESOURCEDATA_H__
#define __SMIKIN_PARTICLESOURCEDATA_H__
#pragma ONCE
#include "Track.h"
struct SSmokinParticleSourceData : public CTRefCount<ISharedResource>
{
	OBJECT_SERVICE_METHODS( SSmokinParticleSourceData );
	SHARED_RESOURCE_METHODS( nRefData, "ComplexParticleSource.Data" );
public:
	SSmokinParticleSourceData();

	bool bComplexParticleSource;					//тип источника, если true, complex particle source
	int nLifeTime;												// время жизни всего источника
	float fGravity;												// параметр гравитации (никакого отношения к физике не имеет)
	CVec3 vWind;                          // ветер
	CVec3 vDirection;                     // направление источника
	CTrack trackGenerateArea;							// размер области из которой вылетают партиклы 
	CTrack trackDensity;									// к-во партиклов рожденных в ед. времени 
	CTrack trackBeginSpeed;								// начальная скорость частицы при  вылете 
	CTrack trackBeginSpeedRandomizer;     // ее рандомизатор
	CTrack trackBeginAngleRandomizer;     // рандомизатор угла вылета
	CTrack trackSpeed;                    // коэфф. скорости
	CTrack trackSpeedRnd;                 // рандомизатор коэфф. скорости
	CTrack trackWeight;										// масса партикла (никакого отношения к физической массе не имеет)
	int nAreaType;                        // тип области для генерации партиклов
	float fRadialWind;                    // сила радиального ветра
	std::string szParticleEffectName;     // название разбрасываемого эффекта
	CTrack trackIntegralMass;             // первообразная от g*m(t), умноженная на коэффициент скорости (без его рандома)
	float fDensityCoeff;                  // коэффициент на плотность, берется из сеттингов
	int nUpdateStep;                      // количество миллисекунд между update'ами

	virtual void STDCALL SwapData( ISharedResource *pResource );
	virtual void STDCALL ClearInternalContainer() {  }
	virtual bool STDCALL Load( const bool bPreLoad = false );
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL InitIntegrals();
};
#endif 
