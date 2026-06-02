#ifndef __MESHANIMATION_H__
#define __MESHANIMATION_H__
#include "Effectors.h"
struct SMeshSkeletonData : public ISharedResource
{
	OBJECT_COMPLETE_METHODS( SMeshSkeletonData );
	DECLARE_SERIALIZE;
	SHARED_RESOURCE_METHODS( nRefData.a, "Mesh.Skeleton" );
public:
	struct SNodeData
	{
		std::string szName;									// node name
		SHMatrix matBase;										// base placement
		int nIndex;													// node index
		CVec3 vAxis;												// animation axis
		BYTE animType;											// rotation (1) or translation (2)
		std::vector<int> children;					// children indices
	};
	typedef std::vector<SNodeData> CNodesList;
	CNodesList nodes;											// all nodes
	int nTopNode;													// top node index
	std::vector<int> locators;						// locator indices;
	virtual void STDCALL SwapData( ISharedResource *pResource )
	{
		SMeshSkeletonData *pRes = dynamic_cast<SMeshSkeletonData*>( pResource );
		NI_ASSERT_TF( pRes != 0, NStr::Format("shared resource is not a \"%s\"", typeid(*this).name()), return );
		std::swap( nodes, pRes->nodes );
		std::swap( nTopNode, pRes->nTopNode );
		std::swap( locators, pRes->locators );
	}
	virtual void STDCALL ClearInternalContainer() {  }
	virtual bool STDCALL Load( const bool bPreLoad = false );
	void CreateFrom( const SSkeletonFormat &skeleton );
	const SNodeData& GetNode( int nIndex ) const { return nodes[nIndex]; }
	const SNodeData& GetTopNode() const { return nodes[nTopNode]; }

	const int GetNumNodes() const { return nodes.size(); }
	const SNodeData* GetNodes() const { return &( nodes[0] ); }

	const int GetNumLocators() const { return locators.size(); }
	const int* GetLocatorIndices() const { return &( locators[0] ); }
};
struct SProcAnimNode
{
	DWORD startTime;											// absolute start time
	DWORD endTime;												// absolute end time
	float fValue;													// absolute value to reac at 'endTime'
	float fAtom;													// элементарное приращение
	int operator&( IStructureSaver &ss );
	SProcAnimNode( DWORD _startTime = 0, DWORD _endTime = 1, float _fValue = 0 )
		: startTime( _startTime ), endTime( _endTime ), fValue( _fValue ), fAtom( 0 ) {  }
	void CalcTransAtom( float fCurrVal ) { fAtom = ( fValue - fCurrVal ) / float( endTime - startTime ); }
	void CalcRotAtom( float fCurrVal )
	{ 
		float fAngle = fValue - fCurrVal;
		if ( fabs(fAngle) > PI )
			fAngle = Sign(fAngle) * ( fabs(fAngle) - FP_2PI );
		fAtom = fAngle / float( endTime - startTime ); 
	}
	float GetAtom() const { return fAtom; }
	void CutoffFrom( float fCurrValue, DWORD lastTime, DWORD currTime )
	{
		const DWORD stime = Max( lastTime, startTime );
		fValue = fCurrValue + (fValue - fCurrValue) * float(currTime - stime) / float(endTime - stime);
		endTime = currTime;
	}
};
struct SMeshAnimNodeData
{
	CVec3 vPos;
	CQuat qRot;
	void Interpolate( const SMeshAnimNodeData &n1, const SMeshAnimNodeData &n2, float fCoeff )
	{
		vPos.Interpolate( n1.vPos, n2.vPos, fCoeff );
		qRot.Interpolate( n1.qRot, n2.qRot, fCoeff );
	}
};
typedef CArray2D<SMeshAnimNodeData> SMeshAnimData;
struct SSkeletonNode
{
	typedef std::list<SProcAnimNode> CProcAnimNodesList;
	typedef std::vector<SSkeletonNode> CChildrenList;
	int nIndex;														// skeleton node index
	int nAnimType;
	DWORD lastTime;												// last time, animation was updated
	float fValue;													// value, which corresponds to the 'lastTime'
	CProcAnimNodesList procNodes;					// all procedural animation nodes
	CChildrenList children;								// children list
	int operator&( IStructureSaver &ss );
	void CalcAtom( SProcAnimNode *pNode ) const
	{
		if ( nAnimType == 1 )
			pNode->CalcRotAtom( fValue );
		else
			pNode->CalcTransAtom( fValue );
	}
	void RollTo( DWORD time )
	{
		for ( CProcAnimNodesList::iterator it = procNodes.begin(); it != procNodes.end(); )
		{
			if ( it->startTime < time )
			{
				if ( it->endTime <= time )			// если время этого нода прошло совсем
				{
					fValue = it->fValue;
					lastTime = it->endTime;
					it = procNodes.erase( it );
					if ( it != procNodes.end() )
						CalcAtom( &(*it) );
				}
				else														// если мы находимся где-то внутри этого node
				{
					fValue += it->GetAtom() * ( time - Max(lastTime, it->startTime) );
					lastTime = time;
					break;
				}
			}
			else
			{
				lastTime = time;
				break;
			}
		}
	}
	void SkipFrom( DWORD time )
	{
		for ( CProcAnimNodesList::iterator it = procNodes.begin(); it != procNodes.end(); ++it )
		{
			if ( it->startTime <= time )
			{
				if ( it->endTime >= time )
				{
					it->CutoffFrom( fValue, lastTime, time );
					CalcAtom( &(*it) );
				}
				else
				{
					procNodes.erase( it, procNodes.end() );
					break;
				}
			}
			else
			{
				procNodes.erase( it, procNodes.end() );
				break;
			}
		}
	}
	void CutProceduralAnimation( const NTimer::STime &time )
	{
		RollTo( time );
		SkipFrom( time );
	}
	void CalcNode( DWORD time, CMatrixStack<32> &mstack, SHMatrix *pMatrix, SMatrixEffectorsList *pEffectors, 
		const SMeshSkeletonData *pSkeleton, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff );
	SSkeletonNode()
	{
		nIndex = 0;
		lastTime = 0;
		fValue = 0;
	}
	void GetMatrix( DWORD time, CMatrixStack<32> &mstack, SHMatrix *pMatrices, SMatrixEffectorsList *pEffectors, 
		              const SMeshSkeletonData *pSkeleton, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff );
	void GetMatrixNoRecurse( DWORD time, CMatrixStack<32> &mstack, SHMatrix *pMatrix, SMatrixEffectorsList *pEffectors, 
													 const SMeshSkeletonData *pSkeleton, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff );
	void AddProcedural( DWORD currTime, DWORD _startTime, DWORD _endTime, float _fValue )
	{
		RollTo( Min(currTime, _startTime) );
		SkipFrom( _startTime );
		SProcAnimNode node( _startTime, _endTime, _fValue );
		if ( procNodes.empty() )
			CalcAtom( &node );
		procNodes.push_back( node );
	}
	int GetNumNodes() const
	{
		int nNumNodes = 1;
		for ( CChildrenList::const_iterator it = children.begin(); it != children.end(); ++it )
			nNumNodes += it->GetNumNodes();
		return nNumNodes;
	}
};
class CMeshSkeleton : public IRefCount
{
	OBJECT_NORMAL_METHODS( CMeshSkeleton );
	DECLARE_SERIALIZE;

	SSkeletonNode topnode;								// top node
	std::vector<SSkeletonNode*> nodes;		// all nodes vector (for the fast indexed access)
	CPtr<SMeshSkeletonData> pSkeleton;		// base skeleton info
public:
	void Init( SMeshSkeletonData *_pSkeleton );
	int GetNumNodes() const { return pSkeleton->nodes.size(); }
	void GetMatrices( DWORD time, CMatrixStack<32> &mstack, SHMatrix *pMatrices, 
		                SMatrixEffectorsList *pEffectors, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff );
	void GetBaseMatrix( DWORD time, CMatrixStack<32> &mstack, SHMatrix *pMatrix, 
											SMatrixEffectorsList *pEffectors, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff );
	int GetTopNodeIndex() const { return topnode.nIndex; }
	void AddProceduralNode( int nNodeIdx, DWORD currTime, DWORD startTime, DWORD endTime, float fValue )
	{
		nodes[nNodeIdx]->AddProcedural( currTime, startTime, endTime, fValue );
	}
	void CutProceduralAnimation( const NTimer::STime &time, const int nModelPart )
	{
		if ( nModelPart == -1 ) 
		{
			for ( std::vector<SSkeletonNode*>::iterator it = nodes.begin(); it != nodes.end(); ++it )
				(*it)->CutProceduralAnimation( time );
		}
		else if ( (nModelPart >= 0) && (nModelPart < nodes.size()) ) 
			nodes[nModelPart]->CutProceduralAnimation( time );
	}
	int GetNumLocators() const { return pSkeleton->locators.size(); }
	const int* GetAllLocatorIndices() const { return &( pSkeleton->locators[0] ); }
	void GetAllLocatorNames( const char **ppBuffer, int nBufferSize ) const
	{
		for ( std::vector<int>::const_iterator it = pSkeleton->locators.begin(); it != pSkeleton->locators.end(); ++it )
		{
			NI_ASSERT_TF( nBufferSize != 0, "Wrong buffer size was passed to function", return );
			*ppBuffer = pSkeleton->nodes[*it].szName.c_str();
			++ppBuffer;
			--nBufferSize;
		}
	}
	void GetAllNodeNames( const char **ppBuffer, int nBufferSize ) const
	{
		for ( SMeshSkeletonData::CNodesList::const_iterator it = pSkeleton->nodes.begin(); it != pSkeleton->nodes.end(); ++it )
		{
			NI_ASSERT_TF( nBufferSize != 0, "Wrong buffer size was passed to function", return );
			*ppBuffer = it->szName.c_str();
			++ppBuffer;
			--nBufferSize;
		}
	}
};
struct SMeshAnimationData : public ISharedResource
{
	OBJECT_NORMAL_METHODS( SMeshAnimationData );
	SHARED_RESOURCE_METHODS( nRefData.a, "Mesh.Animation" );
public:
	typedef std::vector<SMeshAnimData> CAnimationsList;
	CAnimationsList animations;
public:
	bool HasAnimation( const int nAnim ) const { return nAnim < animations.size(); }
	const SMeshAnimData* GetAnimation( const int nAnim )
	{
		if ( !HasAnimation(nAnim) ) 
			return 0;
		else
		{
			const SMeshAnimData *pAnim = &( animations[nAnim] );
			return pAnim->IsEmpty() ? 0 : pAnim;
		}
	}
	virtual int STDCALL operator&( IStructureSaver &ss ) { return 0; }
	virtual void STDCALL SwapData( ISharedResource *pResource )
	{
		SMeshAnimationData *pRes = dynamic_cast<SMeshAnimationData*>( pResource );
		NI_ASSERT_TF( pRes != 0, NStr::Format("shared resource is not a \"%s\"", typeid(*this).name()), return );
		std::swap( animations, pRes->animations );
	}
	virtual void STDCALL ClearInternalContainer() {  }
	virtual bool STDCALL Load( const bool bPreLoad = false );
};
class CMeshAnimation : public IMeshAnimation, public IMeshAnimationEdit
{
	OBJECT_COMPLETE_METHODS( CMeshAnimation );
	DECLARE_SERIALIZE;
	CPtr<SMeshAnimationData> animations;	// shared animations data
	const SMeshAnimData *pAnimation;			// current animation shortcut
	int nCurrAnim;												// current animation index
	std::vector<SHMatrix> matrices;				// mesh animation matrices...
	DWORD dwStartTime;										// animation start time
	DWORD dwCurrTime;											// current time
	float fSpeedCoeff;										// animation speed coeff
	std::vector<SMatrixEffectorsList> effectors;
	CPtr<CMeshSkeleton> pSkeleton;				// skeleton
	bool ReAcquireAnimation()
	{
		if ( (animations != 0) && (nCurrAnim != -1) )
		{
			if ( pAnimation == 0 ) 
			{
				pAnimation = animations->GetAnimation( nCurrAnim );
				if ( pAnimation == 0 )
					nCurrAnim = -1;
			}
			return pAnimation != 0;
		}
		else
			return false;
	}
	const NTimer::STime GetTimeDiff() const { return MINT( fSpeedCoeff*(dwCurrTime - dwStartTime) ); }
public:
	CMeshAnimation() : pAnimation( 0 ), nCurrAnim( -1 ), dwStartTime( 0 ), fSpeedCoeff( 1 ) {  }
	void Update( const NTimer::STime &time );
	virtual void STDCALL Visit( IAnimVisitor *pVisitor );
	virtual void STDCALL SetTime( DWORD time ) { dwCurrTime = time; }
	virtual void STDCALL SetStartTime( DWORD time ) { dwStartTime = time; }
	virtual void STDCALL SetAnimSpeedCoeff( float fCoeff ) { fSpeedCoeff = fCoeff; }
	virtual bool STDCALL SetAnimation( const int nAnim ) 
	{ 
		if ( nCurrAnim == nAnim ) 
			return ReAcquireAnimation();
		nCurrAnim = nAnim; 
		pAnimation = 0; 
		const bool bRetVal = ReAcquireAnimation(); 
		if ( bRetVal ) 
			fSpeedCoeff = 1;
		return bRetVal;
	}
	virtual int STDCALL GetAnimation() const { return nCurrAnim; };
	virtual int STDCALL GetLengthOf( const int nAnim ) { return 1; }
	virtual int STDCALL GetNumNodes() const { return matrices.size(); }
	virtual const SHMatrix* STDCALL GetMatrices( const SHMatrix &matBase );
	virtual const SHMatrix* STDCALL GetCurrMatrices() const { return &( matrices[0] ); }
	virtual void STDCALL GetBaseMatrix( const SHMatrix &matBase, SHMatrix * pResult );
	virtual void STDCALL AddProceduralNode( int nNodeIdx, DWORD currTime, DWORD startTime, DWORD endTime, float fValue );
	virtual void STDCALL CutProceduralAnimation( const NTimer::STime &time, const int nModelPart = -1 );
	virtual void STDCALL AddEffector( IMatrixEffector *pEffector, int nID, int nPart );
	virtual void STDCALL RemoveEffector( int nID, int nPart );
	virtual IMatrixEffector* STDCALL GetEffector( int nID, int nPart );
	virtual void STDCALL GetAllNodeNames( const char **ppBuffer, int nBufferSize ) const { pSkeleton->GetAllNodeNames(ppBuffer, nBufferSize); }
	virtual int STDCALL GetNumLocators() const { return pSkeleton->GetNumLocators(); }
	virtual const int* STDCALL GetAllLocatorIndices() const { return pSkeleton->GetAllLocatorIndices(); }
	virtual void STDCALL GetAllLocatorNames( const char **ppBuffer, int nBufferSize ) const { pSkeleton->GetAllLocatorNames( ppBuffer, nBufferSize ); }
	void Init( CMeshSkeleton *_pSkeleton, SMeshAnimationData *_animations ) 
	{
		pSkeleton = _pSkeleton;
		animations = _animations;
		matrices.resize( pSkeleton->GetNumNodes() );
		effectors.resize( pSkeleton->GetNumNodes() );
	}
};
#endif // __MESHANIMATION_H__
