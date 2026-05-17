#ifndef __VISOBJBUILDER_H__
#define __VISOBJBUILDER_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Formats\fmtEffect.h"
#include "..\Scene\ParticleManager.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CVisObjBuilder : public IVisObjBuilder
{
	OBJECT_NORMAL_METHODS( CVisObjBuilder );
	//
	CPtr<IGFX> pGFX;
	CPtr<ITextureManager> pTM;
	CPtr<IMeshManager> pMM;
	CPtr<IAnimationManager> pAM;
	CPtr<IDataStorage> pStorage;
	CPtr<IParticleManager> pPM;
	//
	typedef std::unordered_map<std::string, SEffectDesc> CEffectDescMap;
	CEffectDescMap effectDescs;
	//
	class CSpriteVisObj* CreateSpriteVisObj( const std::string &szName );
	class CFlashVisObj* CreateFlashVisObj( const std::string &szName );
	class CMeshVisObj* CreateMeshVisObj( const char *pszModelName, const char *pszTextureName );
	class CEffectVisObj* CreateEffectVisObj( const std::string &szName );
public:
	virtual bool STDCALL Init( ISingleton *pSingleton );
	//
	virtual IVisObj* STDCALL BuildObject( const char *pszModelName, const char *pszTextureName, EObjVisType type );
	virtual ISceneObject* STDCALL BuildSceneObject( const char *pszName, ESceneObjectType eType, int nSubtype = -1 );
	virtual const char* STDCALL GetEffectSound( const std::string &szName );
	virtual bool STDCALL ChangeObject( IVisObj *pObj, const char *pszModelName, const char *pszTextureName, EObjVisType type );
	virtual bool STDCALL ChangeSceneObject( ISceneObject *pObj, const char *pszName, ESceneObjectType eType, int nSubtype = -1 );
	virtual void STDCALL Clear() { effectDescs.clear(); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __VISOBJBUILDER_H__
