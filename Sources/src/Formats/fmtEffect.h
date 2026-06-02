#ifndef __FMTEFFECT_H__
#define __FMTEFFECT_H__
#pragma ONCE
struct SSpriteEffectDesc
{
	std::string szPath;										// effect path name
	int nStart;														// start time (relative to parent effect) in msec.
	int nRepeat;													// repeat counter
	CVec3 vPos;														// relative position

	int operator&( IDataTree &ss );
};
struct SParticleEffectDesc
{
	std::string szPath;										// effect path name
	int nStart;														// effect start time
	int nDuration;												// effect duration
	CVec3 vPos;														// relative position
	float fScale;                         // לאסרעאב

	int operator&( IDataTree &ss );
};
struct SSmokinParticleEffectDesc
{
	std::string szPath;										// effect path name
	int nStart;														// effect start time
	int nDuration;												// effect duration
	CVec3 vPos;														// relative position
	float fScale;                         // לאסרעאב

	int operator&( IDataTree &ss );
};
struct SEffectDesc
{
	typedef std::vector<SSpriteEffectDesc> CSpritesList;
	typedef std::vector<SParticleEffectDesc> CParticlesList;
	typedef std::vector<SSmokinParticleEffectDesc> CSmokinParticlesList;
	CSpritesList sprites;									// sprite effects
	CParticlesList particles;							// particle effects
	CSmokinParticlesList smokinParticles;	// smokin particle effects
	std::string szSound;									// sound effect

	int operator&( IDataTree &ss );
};
#endif // __FMTEFFECT_H__
