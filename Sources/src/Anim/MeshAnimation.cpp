#include "StdAfx.h"

#include "MeshAnimation.h"
bool SMeshSkeletonData::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	SSkeletonFormat skeleton;
	saver.Add( 1, &skeleton );
	CreateFrom( skeleton );
	return true;
}
int SMeshSkeletonData::operator&( IStructureSaver &ss )
{
	return 0;
}
int SProcAnimNode::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &startTime );
	saver.Add( 3, &endTime );
	saver.Add( 4, &fValue );
	saver.Add( 5, &fAtom );
	return 0;
}
int SSkeletonNode::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &nIndex );
	saver.Add( 3, &nAnimType );
	saver.Add( 4, &lastTime );
	saver.Add( 5, &fValue );
	saver.Add( 6, &procNodes );
	saver.Add( 7, &children );
	return 0;
}
bool SMeshAnimationData::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	std::vector<SAnimationFormat> animations;
	saver.Add( 3, &animations );
	this->animations.resize( animations.size() );
	for ( int i = 0; i != animations.size(); ++i )
	{
		SMeshAnimData &data = this->animations[i];
		data.SetSizes( animations[i].nodes.GetSizeX(), animations[i].nodes.GetSizeY() );
		memcpy( data.GetBuffer(), animations[i].nodes.GetBuffer(), animations[i].nodes.GetSizeX()*animations[i].nodes.GetSizeY()*sizeof(SAnimNodeFormat) );
	}
	return true;
}
void CreateNodeFrom( SMeshSkeletonData::SNodeData *pNode, const SSkeletonFormat::SNodeFormat *pSrc )
{
	NI_ASSERT_SLOW_TF( pSrc != 0, "Can't create node from NULL source", return );
	pNode->szName = pSrc->szName;
	pNode->matBase.Set( pSrc->bone, CQuat(pSrc->quat) );
	pNode->nIndex = pSrc->nIndex;
	pNode->vAxis = pSrc->constraint.axis;
	pNode->animType = pSrc->constraint.type;
	pNode->children.resize( pSrc->children.size() );
	for ( int i=0; i<pSrc->children.size(); ++i )
		pNode->children[i] = pSrc->children[i];
}
void SMeshSkeletonData::CreateFrom( const SSkeletonFormat &skeleton )
{
	nodes.resize( skeleton.GetNumNodes() );
	for ( int i=0; i<nodes.size(); ++i )
		CreateNodeFrom( &( nodes[i] ), skeleton.GetNode(i) );
	nTopNode = skeleton.nTopNode;
	locators = skeleton.locators;
}
SSkeletonNode* GetNode( SSkeletonNode *pNode, int nIndex )
{
	if ( pNode->nIndex == nIndex )
		return pNode;
	for ( SSkeletonNode::CChildrenList::iterator it = pNode->children.begin(); it != pNode->children.end(); ++it )
	{
		SSkeletonNode *pNewNode = GetNode( &(*it), nIndex );
		if ( pNewNode != 0 )
			return pNewNode;
	}
	return 0;
}
void RetrieveAllNodes( std::vector<SSkeletonNode*> &nodes, SSkeletonNode *pTopNode, int nNumNodes )
{
	nodes.resize( nNumNodes );
	for ( int i=0; i<nNumNodes; ++i )
		nodes[i] = ::GetNode( pTopNode, i );
}
void CreateNodeFrom( SSkeletonNode *pNode, int nNodeIndex, const SMeshSkeletonData::SNodeData *nodes )
{
	const SMeshSkeletonData::SNodeData &node = nodes[nNodeIndex];
	pNode->nIndex = nNodeIndex;
	pNode->nAnimType = nodes[nNodeIndex].animType;
	pNode->lastTime = 0;
	pNode->fValue = 0;
	pNode->procNodes.clear();
	pNode->children.resize( node.children.size() );
	for ( int i=0; i<node.children.size(); ++i )
		::CreateNodeFrom( &(pNode->children[i]), node.children[i], nodes );
}
void CMeshSkeleton::Init( SMeshSkeletonData *_pSkeleton )
{
	pSkeleton = _pSkeleton;
	::CreateNodeFrom( &topnode, pSkeleton->nTopNode, &( pSkeleton->nodes[0] ) );
	RetrieveAllNodes( nodes, &topnode, pSkeleton->nodes.size() );
}
static CMatrixStack<32> localmstack;
void SSkeletonNode::CalcNode( DWORD time, CMatrixStack<32> &globalmstack, SHMatrix *pMatrix, SMatrixEffectorsList *pEffectors, 
															const SMeshSkeletonData *pSkeleton, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff )
{
	const SMeshSkeletonData::SNodeData &node = pSkeleton->GetNode( nIndex );
	CMatrixStack<32> &mstack = pEffectors[nIndex].empty() ? globalmstack : localmstack;
	if ( pAnimation == 0 )
		mstack.Push43( node.matBase );
	else
	{
		const SMeshAnimData &animation = *pAnimation;

		const int nAnimNodeIndex = timeDiff / fAnimTimeStep;
		const float fCoeff = float( timeDiff - nAnimNodeIndex*fAnimTimeStep ) / fAnimTimeStep;
		SMeshAnimNodeData localnode;
		if ( nAnimNodeIndex >= animation.GetSizeX() - 1 )
			localnode = animation[node.nIndex][animation.GetSizeX() - 1];
		else
		{
			const SMeshAnimNodeData &prevNode = animation[node.nIndex][nAnimNodeIndex];
			const SMeshAnimNodeData &nextNode = animation[node.nIndex][nAnimNodeIndex + 1];
			localnode.Interpolate( prevNode, nextNode, fCoeff );
		}
		mstack.Push( localnode.vPos, localnode.qRot );
	}
	RollTo( time );
	if ( fValue != 0 )
	{
		if ( node.animType == 1 )
			mstack.Push( CQuat(fValue, node.vAxis) );
		else
			mstack.Push( node.vAxis * fValue );
	}
	if ( !pEffectors[nIndex].empty() )
	{
		for ( SMatrixEffectorsList::const_iterator it = pEffectors[nIndex].begin(); it != pEffectors[nIndex].end(); ++it )
			localmstack.Push( it->GetMatrix() );
		globalmstack.Push( localmstack() );
		localmstack.Clear();
	}
	*pMatrix = globalmstack();
}
void SSkeletonNode::GetMatrix( DWORD time, CMatrixStack<32> &globalmstack, SHMatrix *pMatrices, SMatrixEffectorsList *pEffectors,
															 const SMeshSkeletonData *pSkeleton, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff )
{
	CalcNode( time, globalmstack, &(pMatrices[nIndex]), pEffectors, pSkeleton, pAnimation, timeDiff );
	for ( CChildrenList::iterator it = children.begin(); it != children.end(); ++it )
		it->GetMatrix( time, globalmstack, pMatrices, pEffectors, pSkeleton, pAnimation, timeDiff );
	if ( pEffectors[nIndex].empty() ) 
		globalmstack.Pop( (fValue != 0) + 1 );
	else
		globalmstack.Pop();
	lastTime = time;
}
void SSkeletonNode::GetMatrixNoRecurse( DWORD time, CMatrixStack<32> &globalmstack, SHMatrix *pMatrix, SMatrixEffectorsList *pEffectors, 
																				const SMeshSkeletonData *pSkeleton, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff )
{
	CalcNode( time, globalmstack, pMatrix, pEffectors, pSkeleton, pAnimation, timeDiff );
	if ( pEffectors[nIndex].empty() ) 
		globalmstack.Pop( (fValue != 0) + 1 );
	else
		globalmstack.Pop();
}
static CMatrixStack<32> mstack;
void CMeshSkeleton::GetMatrices( DWORD time, CMatrixStack<32> &mstack, SHMatrix *pMatrices, 
																 SMatrixEffectorsList *pEffectors, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff )
{
	topnode.GetMatrix( time, mstack, pMatrices, pEffectors, pSkeleton, pAnimation, timeDiff );
}
void CMeshSkeleton::GetBaseMatrix( DWORD time, CMatrixStack<32> &mstack, SHMatrix *pMatrix, 
																	 SMatrixEffectorsList *pEffectors, const SMeshAnimData *pAnimation, const NTimer::STime &timeDiff )
{
	topnode.GetMatrixNoRecurse( time, mstack, pMatrix, pEffectors, pSkeleton, pAnimation, timeDiff );
}
int CMeshSkeleton::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &topnode );
	saver.Add( 3, &pSkeleton );
	if ( saver.IsReading() )
	{
		nodes.clear();
		RetrieveAllNodes( nodes, &topnode, topnode.GetNumNodes() );
	}
	return 0;
}
void CMeshAnimation::Update( const NTimer::STime &time )
{
	for ( std::vector<SMatrixEffectorsList>::iterator it = effectors.begin(); it != effectors.end(); ++it )
	{
		for ( SMatrixEffectorsList::iterator effector = it->begin(); effector != it->end(); )
		{
			if ( effector->pEffector->Update(time) == false )
				effector = it->erase( effector );
			else
				++effector;
		}
	}
}
const SHMatrix* CMeshAnimation::GetMatrices( const SHMatrix &matBase ) 
{ 
	ReAcquireAnimation();
	Update( dwCurrTime );
	mstack.Set( matBase );
	pSkeleton->GetMatrices( dwCurrTime, mstack, &(matrices[0]), &(effectors[0]), pAnimation, GetTimeDiff() );
	mstack.Clear();
	return &( matrices[0] ); 
}
void CMeshAnimation::GetBaseMatrix( const SHMatrix &matBase, SHMatrix * pResult )
{
	ReAcquireAnimation();
	Update( dwCurrTime );
	mstack.Set( matBase );
	pSkeleton->GetBaseMatrix( dwCurrTime, mstack, pResult, &(effectors[0]), pAnimation, GetTimeDiff() );
	mstack.Clear();
}
void CMeshAnimation::AddProceduralNode( int nNodeIdx, DWORD currTime, DWORD startTime, DWORD endTime, float fValue )
{
	if ( nNodeIdx != -1 )
		pSkeleton->AddProceduralNode( nNodeIdx, currTime, startTime, endTime, fValue );
}
void CMeshAnimation::CutProceduralAnimation( const NTimer::STime &time, const int nModelPart )
{
	pSkeleton->CutProceduralAnimation( time, nModelPart );
}
void CMeshAnimation::AddEffector( IMatrixEffector *pEffector, int nID, int nPart )
{
	if ( pEffector == 0 )
		return;
	RemoveEffector( nID, nPart );
	if ( nPart >= 0 )
		effectors[nPart].push_back( SMatrixEffectorDesc(pEffector, nID) );
	else if ( nPart == -2 )								// top node
		AddEffector( pEffector, nID, pSkeleton->GetTopNodeIndex() );
	else
	{
		NI_ASSERT_T( false, NStr::Format("Unknown part index %d", nPart) );
	}
}
void CMeshAnimation::RemoveEffector( int nID, int nPart )
{
	if ( (nID != -1) && (nPart >= 0) )
	{
		for ( SMatrixEffectorsList::iterator it = effectors[nPart].begin(); it != effectors[nPart].end(); ++it )
		{
			if ( it->nID == nID )
			{
				effectors[nPart].erase( it );
				return;
			}
		}
	}
	else if ( nPart == -2 )
		RemoveEffector( nID, pSkeleton->GetTopNodeIndex() );
	else if ( nPart == -1 )
	{
		for ( std::vector<SMatrixEffectorsList>::iterator it = effectors.begin(); it != effectors.end(); ++it )
			it->clear();
	}
	else
	{
		NI_ASSERT_T( false, NStr::Format("Unknown part index %d", nPart) );
	}
}
IMatrixEffector* CMeshAnimation::GetEffector( int nID, int nPart )
{
	if ( nPart >= 0 )
	{
		for ( SMatrixEffectorsList::iterator it = effectors[nPart].begin(); it != effectors[nPart].end(); ++it )
		{
			if ( it->nID == nID )
				return it->pEffector;
		}
		return 0;
	}
	else if ( nPart == -2 )
		return GetEffector( nID, pSkeleton->GetTopNodeIndex() );
	else
	{
		NI_ASSERT_T( false, NStr::Format("Unknown part index %d", nPart) );
	}
	return 0;
}
int CMeshAnimation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nCurrAnim );
	saver.Add( 2, &dwStartTime );
	saver.Add( 3, &dwCurrTime );
	saver.Add( 4, &fSpeedCoeff );
	saver.Add( 5, &matrices );
	saver.Add( 6, &pSkeleton );
	saver.Add( 7, &animations );
	saver.Add( 8, &effectors );
	if ( saver.IsReading() )
		pAnimation = 0;
	return 0;
}
void CMeshAnimation::Visit( IAnimVisitor *pVisitor )
{
}
