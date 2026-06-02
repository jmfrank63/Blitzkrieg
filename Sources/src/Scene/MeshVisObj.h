#ifndef __MESHVISOBJ_H__
#define __MESHVISOBJ_H__
#pragma ONCE
#include "ObjVisObj.h"
template <class TEffector>
struct SEffector
{
	int nID;															// effector ID
	int nPart;														// bodypart to attach this effector to
	CPtr<TEffector> pEffector;						// effector itself
	SEffector() : nID( -1 ), nPart( -1 ) {  }
	SEffector( int _nID, TEffector *_pEffector ) : nID( _nID ), nPart( -1 ), pEffector( _pEffector ) {  }
	SEffector( int _nID, TEffector *_pEffector, int _nPart ) : nID( _nID ), pEffector( _pEffector ), nPart( _nPart ) {  }
	const SHMatrix& GetMatrix() const { return pEffector->GetMatrix(); }
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &nID );
		saver.Add( 2, &nPart );
		saver.Add( 3, &pEffector );
		return 0;
	}
};
typedef SEffector<ISceneMatrixEffector> SMatrixEffector;
class CMeshVisObj : public CTObjVisObj< CTRefCount<IMeshVisObj> >
{
	OBJECT_SERVICE_METHODS( CMeshVisObj );
	DECLARE_SERIALIZE;
	CPtr<IGFXMesh> pMesh;
	CPtr<IMeshAnimation> pAnim;						// animation system
	CPtr<IGFXTexture> pTexture;						// texture
	SHMatrix matPlacement, matPlacement1;	// placement and placement with effectors
	CQuat quat;														// rotation
	CVec3 vScale;													// scale
	bool bHasScale;												// is scale != 1?
	SGFXMaterial material;								// materail properties
	typedef std::list<SMatrixEffector> CEffectorsList;
	CEffectorsList effectors;
	CPtr<ISceneMaterialEffector> pMaterialEffector;
	virtual void RepositionIcons();
	void RepositionIconsLocal( DWORD placement );
	virtual ~CMeshVisObj() {  }
public:
	CMeshVisObj()
		: quat( QNULL ), vScale( 1, 1, 1 ), bHasScale( false ), pMaterialEffector( 0 )
	{
		Identity( &matPlacement );
		Zero( material );
		material.vAmbient = CVec4( 1, 1, 1, 1 );
		material.vDiffuse = CVec4( 1, 1, 1, 1 );
	}
	bool Init( IGFXMesh *_pMesh, IMeshAnimation *_pAnim, IGFXTexture *_pTexture ) { SetMeshAnim( _pMesh, _pAnim ); SetTexture( _pTexture ); return true; }
	void SetMeshAnim( IGFXMesh *_pMesh, IMeshAnimation *_pAnim ) { pMesh = _pMesh; pAnim = _pAnim; effectors.clear(); dwLastUpdateTime = 0; }
	void SetTexture( IGFXTexture *_pTexture ) { pTexture = _pTexture; }
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	virtual void STDCALL SetScale( float sx, float sy, float sz )
	{
		vScale.Set( sx, sy, sz );
		bHasScale = (sx != 1.0f) || (sy != 1.0f) || (sz != 1.0f);
	}
	virtual void STDCALL SetDirection( const int _nDirection )
	{
		SetDir( _nDirection );
		quat.FromAngleAxis( ToRadian( float( GetDir() ) / 65536.0f * 360.0f ), 0, 0, 1 );
	}
	virtual void STDCALL SetPosition( const CVec3 &_pos ) { SetPos( _pos ); }
	virtual void STDCALL SetPlacement( const CVec3 &pos, const int nDir ) { SetPosition( pos ); SetDirection( nDir ); }
	virtual void STDCALL SetOpacity( BYTE opacity ) { material.vDiffuse.a = float( opacity ) / 255.0f; }
	virtual void STDCALL SetColor( DWORD color )
	{
		material.vDiffuse.Set( material.vDiffuse.a,
			                     float( (color >> 16) & 0xff ) / 255.0f,
			                     float( (color >> 8) & 0xff ) / 255.0f,
													 float( (color) & 0xff ) / 255.0f );
	}
	virtual void STDCALL SetSpecular( DWORD color ) 
	{  
		material.vSpecular.Set( 1.0f,
			                     float( (color >> 16) & 0xff ) / 255.0f,
			                     float( (color >> 8) & 0xff ) / 255.0f,
													 float( (color) & 0xff ) / 255.0f );
	}
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift );
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const RECT &rect );
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual bool STDCALL DrawBB( IGFX *pGFX );
	virtual bool STDCALL DrawShadow( IGFX *pGFX, const SHMatrix *pMatShadow, const CVec3 &vSunDir );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	virtual void STDCALL SetAnimation( const int nAnim ) { pAnim->SetAnimation( nAnim ); }
	virtual IAnimation* STDCALL GetAnimation() { return pAnim; }
	virtual void STDCALL SetAnim( interface IAnimation *_pAnim ) { pAnim = dynamic_cast<IMeshAnimation*>( _pAnim ); }
	virtual IGFXMesh* STDCALL GetMesh() const { return pMesh; }
	virtual IGFXTexture* STDCALL GetTexture() const { return pTexture; }
	virtual const SHMatrix& STDCALL GetPlacement() const { return matPlacement; }
	virtual const SHMatrix& STDCALL GetPlacement1() const { return matPlacement1; }
	virtual const SHMatrix STDCALL GetBasePlacement();
	virtual const SHMatrix* STDCALL GetMatrices() { return pAnim->GetMatrices( matPlacement1 ); }
	virtual const SHMatrix* STDCALL GetExtMatrices( const SHMatrix &matExternal );
	virtual DWORD STDCALL CheckForViewVolume( const SPlane *pViewVolumePlanes );
	virtual void STDCALL AddEffector( int nID, ISceneMatrixEffector *pEffector, int nPart = -1 );
	virtual void STDCALL RemoveEffector( int nID, int nPart = -1 );
	virtual void STDCALL AddMaterialEffector( ISceneMaterialEffector *pEffector );
	virtual void STDCALL RemoveMaterialEffector();
};
#endif // __MESHVISOBJ_H__
