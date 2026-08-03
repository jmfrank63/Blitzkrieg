#include "StdAfx.h"
#include "editor.h"
#include "TabToolsDialog.h"
#include "frames.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"
#include "MapToolState.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CTabToolsDialog::vID[] = 
{
	IDC_TOOLS_DAMAGE_RADIO_BUTTON,			//0
	IDC_TOOLS_DAMAGE_LABEL_LEFT,				//1
	IDC_TOOLS_DAMAGE_EDIT,							//2
	IDC_TOOLS_DAMAGE_SPIN,							//3
	IDC_TOOLS_DELIMITER_00,							//4
	IDC_TOOLS_SCRIPT_AREA_RADIO_BUTTON,	//5
	IDC_TOOLS_SA_RECT_RADIO_BUTTON,			//6
	IDC_TOOLS_SA_CIRCLE_RADIO_BUTTON,		//7
	IDC_TOOLS_SA_DELETE_BUTTON,					//8
	IDC_TOOLS_SA_LIST,									//9
	IDC_TOOLS_DAMAGE_LABEL_RIGHT,				//10
};


CTabToolsDialog::CTabToolsDialog( CWnd* pParent )
	: CResizeDialog( CTabToolsDialog::IDD, pParent ), bCreateControls( false )
{
	m_mode = 0;
	m_drawType = 0;
	
	SetControlStyle( IDC_TOOLS_DAMAGE_RADIO_BUTTON, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_TOOLS_DAMAGE_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_TOOLS_DAMAGE_EDIT, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_TOOLS_DAMAGE_SPIN, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_TOOLS_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_TOOLS_SCRIPT_AREA_RADIO_BUTTON, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_TOOLS_SA_RECT_RADIO_BUTTON, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_TOOLS_SA_CIRCLE_RADIO_BUTTON, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_TOOLS_SA_DELETE_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_TOOLS_SA_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( IDC_TOOLS_DAMAGE_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
}

void CTabToolsDialog::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange(pDX); 
	DDX_Control(pDX, IDC_TOOLS_SA_LIST, m_areas);
	DDX_Text(pDX, IDC_TOOLS_DAMAGE_EDIT, m_hp);
	DDX_Radio(pDX, IDC_TOOLS_DAMAGE_RADIO_BUTTON, m_mode);
	DDX_Radio(pDX, IDC_TOOLS_SA_RECT_RADIO_BUTTON, m_drawType);
}

BEGIN_MESSAGE_MAP(CTabToolsDialog, CResizeDialog)
	ON_EN_CHANGE(IDC_TOOLS_DAMAGE_EDIT, OnChangeEdit1)
	ON_BN_CLICKED(IDC_TOOLS_DAMAGE_RADIO_BUTTON, OnRadioChanged0)
	ON_BN_CLICKED(IDC_TOOLS_SCRIPT_AREA_RADIO_BUTTON, OnRadioChanged1)
	ON_BN_CLICKED(IDC_TOOLS_SA_RECT_RADIO_BUTTON, OnRadio1Changed0)
	ON_BN_CLICKED(IDC_TOOLS_SA_CIRCLE_RADIO_BUTTON, OnRadio1Changed1)
	ON_BN_CLICKED(IDC_TOOLS_SA_DELETE_BUTTON, OnButtonDelArea)
	ON_LBN_SELCHANGE(IDC_TOOLS_SA_LIST, OnSelchangeToolsSaList)
	ON_WM_CREATE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CTabToolsDialog::OnInitDialog() 
{
	bCreateControls = true;
	CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.nParameters.size() < 3 )
	{	
		resizeDialogOptions.nParameters.resize( 3 );
		resizeDialogOptions.nParameters[0] = 10;
		resizeDialogOptions.nParameters[1] = m_mode;
		resizeDialogOptions.nParameters[2] = m_drawType;
	}

	m_hp = NStr::Format( "%d", resizeDialogOptions.nParameters[0] );
	m_mode = resizeDialogOptions.nParameters[1];
	m_drawType = resizeDialogOptions.nParameters[2];

	UpdateData( false );

	UpdateControls();
	bCreateControls = false;
	return TRUE;
}

void CTabToolsDialog::OnChangeEdit1() 
{
	UpdateData();
	sscanf( m_hp, "%d", &( resizeDialogOptions.nParameters[0] ) );
	UpdateControls();
}

void CTabToolsDialog::OnRadioChanged0() 
{
	m_mode = toolStateConsts::nRepair;
	resizeDialogOptions.nParameters[1] = m_mode;
	UpdateControls();
}

void CTabToolsDialog::OnRadioChanged1() 
{
	m_mode = toolStateConsts::nArea;
	resizeDialogOptions.nParameters[1] = m_mode;
	UpdateControls();
}

void CTabToolsDialog::OnRadio1Changed0() 
{
	m_drawType = toolStateConsts::nRectType;
	resizeDialogOptions.nParameters[2] = m_drawType;
	UpdateControls();
}

void CTabToolsDialog::OnRadio1Changed1() 
{
	m_drawType = toolStateConsts::nCircleType;
	resizeDialogOptions.nParameters[2] = m_drawType;
	UpdateControls();
}

void CTabToolsDialog::OnButtonDelArea() 
{
	CTemplateEditorFrame* pFrame = g_frameManager.GetTemplateEditorFrame();
	if ( pFrame )
	{
		int i = m_areas.GetCurSel();
		if( i != -1 )	
		{
			i = m_areas.GetItemData( i );
			pFrame->m_scriptAreas.erase( pFrame->m_scriptAreas.begin() + i );
			pFrame->CalculateAreas();
			pFrame->InvalidateRect( 0 );
			pFrame->RedrawWindow();
			pFrame->SetMapModified();
			UpdateControls();
		}
	}
}

void CTabToolsDialog::OnSelchangeToolsSaList() 
{
	UpdateControls();

	if ( !bCreateControls )
	{
		CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
		ICamera *pCamera = GetSingleton<ICamera>();
		IScene *pScene = GetSingleton<IScene>();
		ITerrain *pTerrain = pScene ? pScene->GetTerrain() : 0;

		if ( pFrame && pCamera && pTerrain )
		{
			int i = m_areas.GetCurSel();
			if( i != -1 )	
			{
				i = m_areas.GetItemData( i );
				CVec2 vPos = pFrame->m_scriptAreas[i].center;

				const CVec3 center3 = pFrame->GetScreenCenter();
				const CVec3 camera3 = pCamera->GetAnchor();
				vPos += CVec2( ( camera3.x - center3.x ), ( camera3.y - center3.y ) );
				
				pFrame->NormalizeCamera( &( CVec3( vPos, 0.0f ) ) );
				pFrame->inputStates.Update();
				pFrame->RedrawWindow();
			}
		}
	}
}

void CTabToolsDialog::UpdateControls()
{
	bool bEnabled = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnabled = true;
			}
		}
	}
	
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_DAMAGE_RADIO_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_DAMAGE_EDIT ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_mode == toolStateConsts::nRepair ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_DAMAGE_SPIN ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_mode == toolStateConsts::nRepair ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_SCRIPT_AREA_RADIO_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_SA_RECT_RADIO_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_mode != toolStateConsts::nRepair ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_SA_CIRCLE_RADIO_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_mode != toolStateConsts::nRepair ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_SA_LIST ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_mode != toolStateConsts::nRepair ) );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_TOOLS_SA_DELETE_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled && ( m_mode != toolStateConsts::nRepair ) && ( m_areas.GetCurSel() != -1 ) );
	}
}

void CTabToolsDialog::OnDestroy() 
{
	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog::OnDestroy();
}
