#include "StdAfx.h"

#include "EffectVisObj.h"
int CEffectVisObj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &sprites );
	saver.Add( 2, &particles );
	saver.Add( 3, &dwStartTime );
	saver.Add( 4, &dwDuration );
	saver.Add( 5, &vPos );
	saver.Add( 6, &nDirection );
	saver.Add( 7, &selectionState );
	saver.Add( 9, &szSoundName );
	saver.Add( 10, &bStopped );
	saver.Add( 11, &bSuspended );
	return 0;
}
void CEffectVisObj::Stop() 
{ 
	bStopped = true; 
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		it->pObj->Stop();
	}
}
void CEffectVisObj::SetSuspendedState( bool bState ) 
{ 
	bSuspended = bState; 
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		it->pObj->SetSuspendedState( bState );
	}
}
bool CEffectVisObj::Update( const NTimer::STime &time, bool bForced )
{
	if ( time < dwStartTime )
		return true;
	DWORD dt = time - dwStartTime;
	if ( dt > dwDuration )
		return false;
	for ( std::vector<SSpriteEffect>::iterator it = sprites.begin(); it != sprites.end(); ++it )
	{
		if ( (dt >= it->dwStart) && (dt < it->dwEnd) )
		{
			it->pObj->SetPosition( vPos + it->vRelPos );
			it->pObj->Update( dt );
			it->bActive = true;
		}
		else
			it->bActive = false;
	}
	bool bHasParticles = false;
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		if ( (dt >= it->dwStart) && (dt < it->dwEnd) )
		{
			it->pObj->SetPos( vPos + it->vRelPos );
			it->pObj->Update( dt );
			it->bActive = !( it->pObj->IsFinished() );
		}
		else
			it->bActive = false;
		bHasParticles = bHasParticles || it->bActive;
	}
	if ( bStopped && !bHasParticles ) 
		return false;
	return true;
}
void CEffectVisObj::AddSpriteEffect( ISpriteVisObj *pObj, DWORD dwStart, int nRepeat, const CVec3 &vPos )
{
	pObj->SetAnimation( 0 );
	pObj->GetAnimation()->SetStartTime( dwStart );
	sprites.push_back( SSpriteEffect() );
	SSpriteEffect &effect = sprites.back();
	effect.pObj = pObj;
	effect.dwStart = dwStart;
	effect.dwEnd = dwStart + nRepeat*pObj->GetAnimation()->GetLengthOf( 0 );
	effect.vPos = vPos;
	effect.vRelPos = vPos;
	effect.bActive = false;
	
	dwDuration = Max( dwDuration, effect.dwEnd );
}
void CEffectVisObj::AddParticleEffect( IParticleSource *pObj, DWORD dwStart, int nDuration, const CVec3 &vPos )
{
	particles.push_back( SParticleEffect() );
	SParticleEffect &effect = particles.back();
	effect.pObj = pObj;
	effect.dwStart = dwStart;
	effect.dwEnd = dwStart + nDuration;
	effect.vPos = vPos;
	effect.vRelPos = vPos;
	effect.bActive = false;
	pObj->SetStartTime( dwStart );
	if ( bStopped )
		pObj->Stop();

	dwDuration = Max( dwDuration, effect.dwEnd );
}
void CEffectVisObj::CalibrateDuration( const NTimer::STime &timeDuration )
{
	const float fCoeff = float(timeDuration) / float(dwDuration);
	for ( std::vector<SSpriteEffect>::iterator it = sprites.begin(); it != sprites.end(); ++it )
	{
		if( IAnimation *pAnim = it->pObj->GetAnimation() )
			pAnim->SetAnimSpeedCoeff( 1.0f / fCoeff );
		it->dwStart = it->dwStart * fCoeff;
		it->dwEnd = it->dwEnd * fCoeff;
	}
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		it->dwStart = it->dwStart * fCoeff;
		it->dwEnd = it->dwEnd * fCoeff;
	}
	dwDuration = dwDuration * fCoeff;
}
bool CEffectVisObj::Draw( IGFX *pGFX )
{
	for ( std::vector<SSpriteEffect>::iterator it = sprites.begin(); it != sprites.end(); ++it )
	{
		if ( it->bActive )
			it->pObj->Draw( pGFX );
	}
	return true;
}
void CEffectVisObj::Visit( ISceneVisitor *pVisitor, int nType )
{
	if ( !bSuspended )
	{
		for ( std::vector<SSpriteEffect>::iterator it = sprites.begin(); it != sprites.end(); ++it )
		{
			if ( it->bActive )
				it->pObj->Visit( pVisitor, SGVOGT_EFFECT );
		}
	}
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		if ( it->bActive )
			pVisitor->VisitParticles( it->pObj );
	}
}
void CEffectVisObj::GetSpriteEffects( const SSpriteInfo ***ppEffects, int *pnNumEffects, bool bAll )
{
	const SSpriteInfo **pEffects = *ppEffects = GetTempBuffer<const SSpriteInfo*>( sprites.size() );
	*pnNumEffects = 0;
	for ( std::vector<SSpriteEffect>::iterator it = sprites.begin(); it != sprites.end(); ++it )
	{
		if ( it->bActive || bAll )
			pEffects[(*pnNumEffects)++] = it->pObj->GetSpriteInfo();
	}
}
void CEffectVisObj::GetParticleEffects( IParticleSource ***ppEffects, int *pnNumEffects, bool bAll )
{
	IParticleSource **pEffects = *ppEffects = GetTempBuffer<IParticleSource*>( sprites.size() );
	*pnNumEffects = 0;
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		if ( it->bActive || bAll )
			pEffects[(*pnNumEffects)++] = it->pObj;
	}
}
void CEffectVisObj::SetDirection( const int nDir ) 
{ 
	nDirection = nDir; 
	for ( std::vector<SSpriteEffect>::iterator it = sprites.begin(); it != sprites.end(); ++it )
		it->pObj->SetDirection( nDir );
}
void CEffectVisObj::SetEffectDirection( const SHMatrix &matrix )
{
	CVec3 vDir;
	matrix.RotateVector( &vDir, V3_AXIS_Z );
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		matrix.RotateVector( &it->vRelPos, it->vPos );
		it->pObj->SetDirection( matrix );
	}
}
void CEffectVisObj::SetScale( const float fScale )
{
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
		it->pObj->SetScale( fScale );
}
bool CEffectVisObj::IsFinished( const NTimer::STime &time )
{ 
	if ( time - dwStartTime > dwDuration )
		return true; 
	bool bHasParticles = false;
	for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
	{
		bHasParticles = bHasParticles || !( it->pObj->IsFinished() );
	}
	return bStopped && !bHasParticles;
}
