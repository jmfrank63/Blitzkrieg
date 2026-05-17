#include "StdAfx.h"
#include "..\main\\gamestats.h"
#include "..\main\\gamedb.h"
#include "UIMessages.h"
#include "UIObjectiveScreen.h"
#include "UIScreen.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIObjectiveScreen::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	
	saver.Add( "ShowAllObjectives", &bShowAllObjectives );
	
	if ( saver.IsReading() )
	{
		//�������������� pSB
		pSB = checked_cast<IUIShortcutBar *> ( GetChildByID(10) );
		//		NI_ASSERT_T( pSB != 0, "Can't find ShortcutBar with ID 10" );
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIObjectiveScreen::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMultipleWindow*>(this) );
	
	saver.Add( 2, &bShowAllObjectives );
	
	if ( saver.IsReading() )
	{
		//�������������� pSB
		CPtr<IUIElement> pElement;
		saver.Add( 3, &pElement );
		pSB = checked_cast<IUIShortcutBar *> ( pElement.GetPtr() );
		
		//		pSB = checked_cast<IUIShortcutBar *> ( GetChildByID(10) );
		NI_ASSERT_T( pSB != 0, "Can't find ShortcutBar with ID 10" );
	}
	else
	{
		CPtr<IUIElement> pElement = pSB;
		saver.Add( 3, &pElement );
	}
	
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIObjectiveScreen::ShowWindow( int _nCmdShow )
{
	if ( bool(_nCmdShow) == IsVisible() )
		return;
	CSimpleWindow::ShowWindow( _nCmdShow );
	
	if ( !_nCmdShow )
		return;
	
	NI_ASSERT_T( pSB != 0, "Can't find objective" );
	pSB->Clear();
	
	std::string szMissionName = GetGlobalVar( "Mission.Current.Name" );
	const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
	//	NI_ASSERT_T( pStats != 0, NStr::Format( "Invalid mission %s stats", szMissionName ) );
	if ( !pStats )
	{
		pSB->InitialUpdate();
		return;
	}
	
	//������� Objectives
	ITextManager *pTM = GetSingleton<ITextManager>();
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();

	CPtr<IDataStream> pActiveStream = pStorage->OpenStream( "ui\\common\\active.xml", STREAM_ACCESS_READ );
	CPtr<IDataTree> pActiveDT = CreateDataTreeSaver( pActiveStream, IDataTree::READ );
	CPtr<IDataStream> pDoneStream = pStorage->OpenStream( "ui\\common\\done.xml", STREAM_ACCESS_READ );
	CPtr<IDataTree> pDoneDT = CreateDataTreeSaver( pDoneStream, IDataTree::READ );
	CPtr<IDataStream> pFailedStream = pStorage->OpenStream( "ui\\common\\failed.xml", STREAM_ACCESS_READ );
	CPtr<IDataTree> pFailedDT = CreateDataTreeSaver( pFailedStream, IDataTree::READ );
	
	int nFirstActiveObjective = -1;

	for ( int i=0; i<pStats->objectives.size(); i++ )
	{
		int nObjectiveState = 0;
/*
		if ( !bShowAllObjectives )
		{
			int nDefault = -1;
			if ( !pStats->objectives[i].bSecret )
				nDefault = 0;
			
			std::string szObjName = NStr::Format( "temp.%s.%s%d", szMissionName.c_str(), "objective", i );
			
			nObjectiveState = GetGlobalVar( szObjName.c_str(), nDefault );
			if ( nObjectiveState == -1 )
				continue;		//objective �� �����
		}
*/

		int nDefault = -1;
		if ( !pStats->objectives[i].bSecret )
			nDefault = 0;
		
		std::string szObjName = NStr::Format( "temp.%s.%s%d", szMissionName.c_str(), "objective", i );
		
		nObjectiveState = GetGlobalVar( szObjName.c_str(), nDefault );
		if ( nObjectiveState == -1 )
			continue;		//objective �� �����
		
		//Add bar
		IUIDialog *pDialog = checked_cast<IUIDialog *> ( pSB->AddBar() );
		CPtr<IUIElement> pElement = pDialog->GetChildByID( 11 );
		if ( pElement->IsVisible() )
		{
			CVec2 vPos;
			CVec2 vSize;
			pElement->GetWindowPlacement( &vPos, &vSize, 0 );
			
			std::string szFlagName;
			switch ( nObjectiveState )
			{
			case 0:
				pElement->operator &( *pActiveDT );
				if ( -1 == nFirstActiveObjective )
				{
					nFirstActiveObjective = pSB->GetNumberOfBars() - 1;
				}
				break;
			case 1:
				pElement->operator &( *pDoneDT );
				break;
			case 2:
				pElement->operator &( *pFailedDT );
				break;
			default:
				NI_ASSERT_T( 0, NStr::Format("Unknown flag %d", nObjectiveState) );
			}
			pElement->SetWindowPlacement( &vPos, &vSize );
		}

		CPtr<IText> p1 = pTM->GetDialog( pStats->objectives[i].szHeader.c_str() );
		if ( p1 == 0 )
			p1 = pTM->GetString( "objective" );
		pDialog->SetWindowText( 0, p1->GetString() );
		pDialog->SetWindowText( 1, p1->GetString() );
		pDialog->SetWindowID( i );

		//Add text item
		CPtr<IText> p2 = pTM->GetDialog( pStats->objectives[i].szDescriptionText.c_str() );
		NI_ASSERT_TF( p2 != 0, NStr::Format( "There is no file %s", pStats->objectives[i].szDescriptionText ), continue );		//�� ����� ������
		pSB->AddTextItem( p2->GetString() );
	}
	pSB->InitialUpdate();

	if ( -1 != nFirstActiveObjective )
	{
		pSB->SetSelectionItem( nFirstActiveObjective, -1 );
		pSB->SetBarExpandState( nFirstActiveObjective, true, false );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
