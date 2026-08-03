#include "StdAfx.h"
#include <io.h>
#include "..\Common\LegacyUiCompat.h"

#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\Main\rpgstats.h"
#include "..\Formats\fmtMesh.h"

#include "editor.h"
#include "PropView.h"
#include "TreeItem.h"
#include "TrenchTreeItem.h"
#include "TrenchFrm.h"
#include "TrenchView.h"
#include "GameWnd.h"
#include "frames.h"
#include "RefDlg.h"

static const int THUMB_LIST_WIDTH = 145;
static char BASED_CODE szTrenchComposeFilter[] = "Trench Compose Project Files (*.trc)|*.trc||";



IMPLEMENT_DYNCREATE(CTrenchFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CTrenchFrame, CParentFrame)
	ON_WM_CREATE()
END_MESSAGE_MAP()


CTrenchFrame::CTrenchFrame()
{
	szComposerName = "Trench Editor";
	szExtension = "*.trc";
	szComposerSaveName = "Trench_Composer_Project";
	nTreeRootItemID = E_TRENCH_ROOT_ITEM;
	nFrameType = CFrameManager::E_TRENCH_FRAME;
	pWndView = new CTrenchView;
	szAddDir = "units\\technics\\common\\entrenchment\\";
	
	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB1555;
}

CTrenchFrame::~CTrenchFrame()
{
}

int CTrenchFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CParentFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}

	GenerateProjectName();
	return 0;
}


void CTrenchFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( nCommand );
	
	if ( nCommand == SW_SHOW )
	{
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );
		pCamera->Update();
		
		IGFX *pGFX = GetSingleton<IGFX>();
		pGFX->SetViewTransform( pCamera->GetPlacement() );
	}
}

void CTrenchFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}


void CTrenchFrame::SpecificClearBeforeBatchMode()
{
	freeIndexes.clear();
	freeIndexes.push_back( 0 );
}

void CTrenchFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, 0x80808080 );
	pGFX->BeginScene();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );

	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	pCamera->Update();
  pSG->Draw( pCamera );

	pGFX->EndScene();
	pGFX->Flip();
}

struct SMySegment
{
	int nIndex;
	SEntrenchmentRPGStats::SSegmentRPGStats segment;
};
inline bool operator<( const SMySegment &m1, const SMySegment &m2 ) { return m1.nIndex < m2.nIndex; }

void CTrenchFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( !pDT->IsReading() );
	NI_ASSERT( pszProjectName != 0 );
	
	SEntrenchmentRPGStats rpgStats;
	CTrenchCommonPropsItem *pCommonProps = static_cast<CTrenchCommonPropsItem *>( pRootItem->GetChildItem( E_TRENCH_COMMON_PROPS_ITEM ) );
	rpgStats.szKeyName = pCommonProps->GetTrenchName();
	rpgStats.fMaxHP = pCommonProps->GetTrenchHealth();
	
	CTreeItem *pDefencesItem = pRootItem->GetChildItem( E_TRENCH_DEFENCES_ITEM );
	for ( int i=0; i<6; i++ )
	{
		CTrenchDefencePropsItem *pDefProps = static_cast<CTrenchDefencePropsItem *> ( pDefencesItem->GetChildItem( E_TRENCH_DEFENCE_PROPS_ITEM, i ) );
		int nIndex = 0;
		if ( string( "Left" ) == pDefProps->GetItemName() )
			nIndex = RPG_LEFT;
		else if ( string( "Right" ) == pDefProps->GetItemName() )
			nIndex = RPG_RIGHT;
		else if ( string( "Top" ) == pDefProps->GetItemName() )
			nIndex = RPG_TOP;
		else if ( string( "Bottom" ) == pDefProps->GetItemName() )
			nIndex = RPG_BOTTOM;
		else if ( string( "Front" ) == pDefProps->GetItemName() )
			nIndex = RPG_FRONT;
		else if ( string( "Back" ) == pDefProps->GetItemName() )
			nIndex = RPG_BACK;
		
		rpgStats.defences[ nIndex ].nArmorMin = pDefProps->GetMinArmor();
		rpgStats.defences[ nIndex ].nArmorMax = pDefProps->GetMaxArmor();
		rpgStats.defences[ nIndex ].fSilhouette = pCommonProps->GetTrenchCover();
	}

	std::list<SMySegment> segmentsToSort;		//��������������� ������, ���� ������� ��� ��������

	for ( int nTrenchIndex=0; nTrenchIndex<4; nTrenchIndex++ )
	{
		CTreeItem *pTrenchParts = pRootItem->GetChildItem( E_TRENCH_SOURCES_ITEM, nTrenchIndex );
		for ( CTreeItem::CTreeItemList::const_iterator it=pTrenchParts->GetBegin(); it!=pTrenchParts->GetEnd(); ++it )
		{
			CTrenchSourcePropsItem *pTrenchProps = static_cast<CTrenchSourcePropsItem *> ( it->GetPtr() );
			SMySegment my;
			my.nIndex = pTrenchProps->nTrenchIndex;

			string szFullName;
			{
				string szRel = pTrenchProps->GetFileName();
				if ( IsRelatedPath( szRel.c_str() ) )
					MakeFullPath( GetDirectory( pszProjectName ).c_str(), szRel.c_str(), szFullName );
				else
					szFullName = szRel;

				string szTempModFile = theApp.GetEditorTempDir();
				szTempModFile += "1.mod";
				if ( !CopyFile( szFullName.c_str(), szTempModFile.c_str(), FALSE ) )
				{
					CString szErr;
					szErr.Format( "Error while saving project: Cannot copy file %s", szFullName.c_str() );
					AfxMessageBox( szErr );
					continue;
				}
/*
				string szTgaFile = GetDirectory( szFullName.c_str() );
				szTgaFile += "1.tga";
				string szTempTgaFile = theApp.GetEditorTempDir();
				szTempTgaFile += "1.tga";
				if ( !CopyFile( szTgaFile.c_str(), szTempTgaFile.c_str(), FALSE ) )
				{
					CString szErr = "Error while saving project: Cannot copy file: ";
					szErr += szFullName;
					AfxMessageBox( szErr );
					continue;
				}
*/
			}


			{
				string szTemp = pTrenchProps->GetFileName();
				int nPos = szTemp.rfind( '\\' );
				if ( nPos != string::npos )
					szTemp = szTemp.substr( nPos+1 );
				szTemp = szTemp.substr( 0, szTemp.rfind( '.' ) );
				my.segment.szModel = szTemp;
			}

			{
				SAABBFormat aabb;											// axis-aligned bounding box
				
				CPtr<IDataStream> pStream = OpenFileStream( szFullName.c_str(), STREAM_ACCESS_READ );
				NI_ASSERT( pStream != 0 );
				if ( pStream == 0 )
				{
					CString szErr;
					szErr.Format( "Error saving RPG : Cannot open file %s", szFullName.c_str() );
					AfxMessageBox( szErr );
					return;
				}
					
				CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
				CSaverAccessor saver = pSaver;
				saver.Add( 4, &aabb );

				my.segment.vAABBCenter.x = aabb.vCenter.x;
				my.segment.vAABBCenter.y = aabb.vCenter.y;
				my.segment.vAABBHalfSize.x = aabb.vHalfSize.x;
				my.segment.vAABBHalfSize.y = aabb.vHalfSize.y;
				my.segment.vAABBHalfSize.z = aabb.vHalfSize.z;
			}

			{
				string szTempModFile = theApp.GetEditorTempResourceDir();
				szTempModFile += "\\1";
				CPtr<IObjVisObj> pReadyObject;
				IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
				pReadyObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( szTempModFile.c_str(), 0, SGVOT_MESH ) );
				if ( !pReadyObject )
				{
					CString szErr;
					szErr.Format( "Error saving RPG : Cannot create model %s", szTempModFile.c_str() );
					AfxMessageBox( szErr );
					continue;
				}

				IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pReadyObject->GetAnimation() );
				IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );

				int nNumLocators = pMeshAnimEdit->GetNumLocators();
				std::vector<const char*> locatorNamesVector;
				if ( nNumLocators > 0 )
				{
					locatorNamesVector.resize( nNumLocators );
					pMeshAnimEdit->GetAllLocatorNames( &(locatorNamesVector[0]), nNumLocators );
				}

				int nNumNodes = pMeshAnim->GetNumNodes();
				std::vector<const char*> allNamesVector;
				if ( nNumNodes > 0 )
				{
					allNamesVector.resize( nNumNodes );
					pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
				}
				
/*
				if ( nNumLocators != 0 && nTrenchIndex != 0 )
				{
					CString szErr;
					szErr.Format( "Error: trench file %s\nhas fire place, and it should be in the trench with ambrasure folder", szFullName.c_str() );
					AfxMessageBox( szErr );
					continue;
				}
				if ( nNumLocators == 0 && nTrenchIndex == 0 )
				{
					CString szErr;
					szErr.Format( "Error: trench file %s\nhasn't fire place, and it should not be in the trench with ambrasure folder", szFullName.c_str() );
					AfxMessageBox( szErr );
					continue;
				}
*/

				for ( int i=0; i<nNumLocators; i++ )
				{
					int nFireLocatorIndex = 0;
					for ( ; nFireLocatorIndex<nNumNodes; nFireLocatorIndex++ )
					{
						if ( allNamesVector[nFireLocatorIndex] == locatorNamesVector[i] )
							break;
					}
					
					pReadyObject->SetPosition( CVec3(0, 0, 0) );
					pReadyObject->SetDirection( 0 );
					
					IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pReadyObject->GetAnimation() );
					const SHMatrix *pMatrix = pMeshAnim->GetMatrices( MONE );
					SHMatrix fireMatrix = pMatrix[ nFireLocatorIndex ];
					CVec3 vFireTrans = fireMatrix.GetTrans3();
					my.segment.fireplaces.push_back( CVec2( vFireTrans.x, vFireTrans.y ) );
				}
			}

			SEntrenchmentRPGStats::EEntrenchSegmType nType = SEntrenchmentRPGStats::EST_FIREPLACE;
			bool bErr = false;
			if ( nTrenchIndex == 0 )
			{
				nType = SEntrenchmentRPGStats::EST_FIREPLACE;
				rpgStats.fireplaces.push_back( my.nIndex );
			}
			else if ( nTrenchIndex == 1 )
			{
				nType = SEntrenchmentRPGStats::EST_LINE;
				rpgStats.lines.push_back( my.nIndex );
			}
			else if ( nTrenchIndex == 2 )
			{
				nType = SEntrenchmentRPGStats::EST_TERMINATOR;
				rpgStats.terminators.push_back( my.nIndex );
			}
			else if ( nTrenchIndex == 3 )
			{
				nType = SEntrenchmentRPGStats::EST_ARC;
				rpgStats.arcs.push_back( my.nIndex );
			}
			else
			{
				bErr = true;
				NI_ASSERT( 0 );			//unknown trench folder
			}

			my.segment.fCoverage = pTrenchProps->GetCoverage();
			my.segment.eType = nType;
			if ( !bErr )
				segmentsToSort.push_back( my );
		}
	}

	segmentsToSort.sort();
	int nPrev = -1;
	for ( std::list<SMySegment>::iterator it=segmentsToSort.begin(); it!=segmentsToSort.end(); ++it )
	{
		if ( it->nIndex != nPrev + 1 )
		{
			for ( int i=nPrev+1; i<it->nIndex; i++ )
			{
				SEntrenchmentRPGStats::SSegmentRPGStats segment;
				segment.fCoverage = 0.0f;
				segment.vFirePlace = VNULL2;
				segment.vAABBCenter = VNULL2;
				segment.vAABBHalfSize = VNULL3;
				segment.eType = SEntrenchmentRPGStats::EST_LINE;			//0
				rpgStats.segments.push_back( segment );
			}
		}
		rpgStats.segments.push_back( it->segment );
		nPrev = it->nIndex;
	}

	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CTrenchFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	IScene *pSG = GetSingleton<IScene>();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	
	SEntrenchmentRPGStats rpgStats;
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	
	CTrenchCommonPropsItem *pCommonProps = static_cast<CTrenchCommonPropsItem *> ( pRootItem->GetChildItem( E_TRENCH_COMMON_PROPS_ITEM ) );
	pCommonProps->SetTrenchName( rpgStats.szKeyName.c_str() );
	pCommonProps->SetTrenchHealth( rpgStats.fMaxHP );
	
	CTreeItem *pDefencesItem = pRootItem->GetChildItem( E_TRENCH_DEFENCES_ITEM );
	for ( int i=0; i<6; i++ )
	{
		CTrenchDefencePropsItem *pDefProps = static_cast<CTrenchDefencePropsItem *> ( pDefencesItem->GetChildItem( E_TRENCH_DEFENCE_PROPS_ITEM, i ) );
		int nIndex = 0;
		if ( string( "Left" ) == pDefProps->GetItemName() )
			nIndex = RPG_LEFT;
		else if ( string( "Right" ) == pDefProps->GetItemName() )
			nIndex = RPG_RIGHT;
		else if ( string( "Top" ) == pDefProps->GetItemName() )
			nIndex = RPG_TOP;
		else if ( string( "Bottom" ) == pDefProps->GetItemName() )
			nIndex = RPG_BOTTOM;
		else if ( string( "Front" ) == pDefProps->GetItemName() )
			nIndex = RPG_FRONT;
		else if ( string( "Back" ) == pDefProps->GetItemName() )
			nIndex = RPG_BACK;
		
		pDefProps->SetMinArmor( rpgStats.defences[nIndex].nArmorMin );
		pDefProps->SetMaxArmor( rpgStats.defences[nIndex].nArmorMax );
	}

	int nIndex = 0;
	std::set<int> indexSet;
	for ( int nTrenchIndex=0; nTrenchIndex<4; nTrenchIndex++ )
	{
		CTreeItem *pTrenchParts = pRootItem->GetChildItem( E_TRENCH_SOURCES_ITEM, nTrenchIndex );
		for ( CTreeItem::CTreeItemList::const_iterator it=pTrenchParts->GetBegin(); it!=pTrenchParts->GetEnd(); ++it )
		{
			CTrenchSourcePropsItem *pTrenchProps = static_cast<CTrenchSourcePropsItem *> ( it->GetPtr() );
			if ( pTrenchProps->nTrenchIndex == -1 )
				pTrenchProps->nTrenchIndex = nIndex;
			indexSet.insert( pTrenchProps->nTrenchIndex );
			nIndex++;
		}
	}
	
	freeIndexes.clear();
	int nPrev = -1;
	for ( std::set<int>::iterator it=indexSet.begin(); it!=indexSet.end(); ++it )
	{
		if ( *it != nPrev + 1 )				//���� ���� ������ �������
		{
			for ( int i=nPrev+1; i!=*it; i++ )
				freeIndexes.push_back( i );
		}
		nPrev = *it;
	}
	freeIndexes.push_back( nPrev + 1 );			//��� ����� ��������� ������
}

void CTrenchFrame::RemoveTrenchIndex( int nIndex )
{
	NI_ASSERT( nIndex != -1 );
	for ( std::list<int>::iterator it=freeIndexes.begin(); it!=freeIndexes.end(); ++it )
	{
		if ( nIndex < *it )
		{
			freeIndexes.insert( it, nIndex );
			return;
		}
	}
}

int CTrenchFrame::GetFreeTrenchIndex()
{
	int nRes = -1;
	if ( freeIndexes.size() == 1 )
	{
		nRes = freeIndexes.back()++;
	}
	else
	{
		nRes = freeIndexes.front();
		freeIndexes.pop_front();
	}

	return nRes;
}

bool CTrenchFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	SaveRPGStats( pDT, pRootItem, pszProjectName );

	int nCount = 0;
	for ( int nTrenchIndex=0; nTrenchIndex<4; nTrenchIndex++ )
	{
		CTreeItem *pTrenchParts = pRootItem->GetChildItem( E_TRENCH_SOURCES_ITEM, nTrenchIndex );
		for ( CTreeItem::CTreeItemList::const_iterator it=pTrenchParts->GetBegin(); it!=pTrenchParts->GetEnd(); ++it )
		{
			CTrenchSourcePropsItem *pTrenchProps = static_cast<CTrenchSourcePropsItem *> ( it->GetPtr() );
			string szFullName;
			string szRel = pTrenchProps->GetFileName();
			if ( IsRelatedPath( szRel.c_str() ) )
				MakeFullPath( GetDirectory( pszProjectName ).c_str(), szRel.c_str(), szFullName );
			else
				szFullName = szRel;

			string szResult = GetDirectory( pszResultFileName );
			int nPos = szRel.rfind( '\\' );
			if ( nPos != string::npos )
				szResult += szRel.substr( nPos+1 );
			else
				szResult += szRel.c_str();
			MyCopyFile( szFullName.c_str(), szResult.c_str() );

			if ( nCount == 0 )
			{
				string szTGA = GetDirectory( szFullName.c_str() );
				szTGA += "1.tga";
				string szResult = GetDirectory( pszResultFileName );
				szResult += "1";
				ConvertAndSaveImage( szTGA.c_str(), szResult.c_str() );

				szTGA = GetDirectory( szFullName.c_str() );
				szTGA += "1w.tga";
				szResult = GetDirectory( pszResultFileName );
				szResult += "1w";
				ConvertAndSaveImage( szTGA.c_str(), szResult.c_str() );

				szTGA = GetDirectory( szFullName.c_str() );
				szTGA += "1a.tga";
				szResult = GetDirectory( pszResultFileName );
				szResult += "1a";
				ConvertAndSaveImage( szTGA.c_str(), szResult.c_str() );
			}

			nCount++;
		}
	}

	return true;
}

FILETIME CTrenchFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	FILETIME maxTime, currentTime;
	maxTime.dwHighDateTime = 0;
	maxTime.dwLowDateTime = 0;

	int nCount = 0;
	
	for ( int nTrenchIndex=0; nTrenchIndex<4; nTrenchIndex++ )
	{
		CTreeItem *pTrenchParts = pRootItem->GetChildItem( E_TRENCH_SOURCES_ITEM, nTrenchIndex );
		for ( CTreeItem::CTreeItemList::const_iterator it=pTrenchParts->GetBegin(); it!=pTrenchParts->GetEnd(); ++it )
		{
			CTrenchSourcePropsItem *pTrenchProps = static_cast<CTrenchSourcePropsItem *> ( it->GetPtr() );
			
			string szFullName;
			string szRel = pTrenchProps->GetFileName();
			if ( IsRelatedPath( szRel.c_str() ) )
				MakeFullPath( GetDirectory( pszProjectName ).c_str(), szRel.c_str(), szFullName );
			else
				szFullName = szRel;
			
			if ( nCount == 0 )
			{
				string szTGA = GetDirectory( szFullName.c_str() );
				szTGA += "1.tga";
				currentTime = GetFileChangeTime( szTGA.c_str() );
				if ( currentTime > maxTime )
					maxTime = currentTime;
			}
			
			currentTime = GetFileChangeTime( szFullName.c_str() );
			if ( currentTime > maxTime )
				maxTime = currentTime;
			nCount++;
		}
	}
	
	return maxTime;
}

FILETIME CTrenchFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime, currentTime;
	string szDestDir = GetDirectory( pszResultFileName );
	minTime.dwLowDateTime = -1;
	minTime.dwHighDateTime = -1;

	{
		string szTGA = szDestDir;
		szTGA += "1.tga";
		minTime = GetFileChangeTime( szTGA.c_str() );
	}
	
	for ( int nTrenchIndex=0; nTrenchIndex<4; nTrenchIndex++ )
	{
		CTreeItem *pTrenchParts = pRootItem->GetChildItem( E_TRENCH_SOURCES_ITEM, nTrenchIndex );
		for ( CTreeItem::CTreeItemList::const_iterator it=pTrenchParts->GetBegin(); it!=pTrenchParts->GetEnd(); ++it )
		{
			CTrenchSourcePropsItem *pTrenchProps = static_cast<CTrenchSourcePropsItem *> ( it->GetPtr() );
			string szShortName = pTrenchProps->GetFileName();
			int nPos = szShortName.rfind( '\\' );
			if ( nPos != string::npos )
				szShortName = szShortName.substr( nPos+1 );

			string szFullName = szDestDir;
			szFullName += szShortName;
			currentTime = GetFileChangeTime( szFullName.c_str() );
			if ( currentTime < minTime )
				minTime = currentTime;
		}
	}
	
	return minTime;
}
