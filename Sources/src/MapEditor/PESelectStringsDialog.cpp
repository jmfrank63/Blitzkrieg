#include "StdAfx.h"
#include "editor.h"
#include "PESelectStringsDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CPESelectStringsDialog::vID[] = 
{
	IDC_SS_STRINGS_LIST,	//0
	IDOK,									//1
	IDCANCEL,							//2
};

CPESelectStringsDialog::CPESelectStringsDialog( CWnd* pParent )
	: CResizeDialog( CPESelectStringsDialog::IDD, pParent ), pAvailiableStrings( 0 ), pSelectedStrings( 0 )
{

	SetControlStyle( vID[0], ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( vID[1], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[2], ANCHORE_RIGHT_BOTTOM );
}

void CPESelectStringsDialog::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control( pDX, vID[0], stringList );
}

BEGIN_MESSAGE_MAP( CPESelectStringsDialog, CResizeDialog )
END_MESSAGE_MAP()

BOOL CPESelectStringsDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	SetWindowText( szDialogName.c_str() );
	CreateList();
	
	return TRUE;
}

void CPESelectStringsDialog::CreateList()
{
	stringList.ResetContent();
	if ( pAvailiableStrings && pSelectedStrings )
	{
		for( std::list<std::string>::const_iterator stringIterator = pAvailiableStrings->begin(); stringIterator != pAvailiableStrings->end(); ++stringIterator )
		{
			int nItem = stringList.AddString( ( *stringIterator ).c_str() );
			if( nItem >= 0 )
			{
				for ( std::vector<std::string>::const_iterator innerStringIterator = pSelectedStrings->begin(); innerStringIterator != pSelectedStrings->end(); ++innerStringIterator )
				{
					std::string szToCompare00 = ( *stringIterator );
					NStr::ToLower( szToCompare00 );
					std::string szToCompare01 = ( *innerStringIterator );
					NStr::ToLower( szToCompare01 );
					if ( szToCompare00.compare( szToCompare01 ) == 0 )
					{
						stringList.SetCheck( nItem, true );
						break;
					}
				}
			}
		}
	}
}

void CPESelectStringsDialog::OnOK() 
{
	if ( pSelectedStrings )
	{
		pSelectedStrings->clear();
		for ( int nItem = 0; nItem != stringList.GetCount(); ++nItem )
		{
			if (  stringList.GetCheck( nItem ) == 1 )
			{
				CString szString;
				stringList.GetText( nItem, szString );
				pSelectedStrings->push_back( std::string( szString ) );
			}
		}
	}
	CResizeDialog::OnOK();
}
