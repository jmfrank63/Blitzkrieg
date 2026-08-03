#include "StdAfx.h"
#include "editor.h"
#include "frames.h"
#include "../GFX/GFX.H"
#include "../Image/Image.h"
#include "../Scene/Terrain.h"
#include "TabTerrainAltitudesDialog.h"
#include "TemplateEditorFrame1.h"
#include "../RandomMapGen/RMG_Types.h"
#include "../RandomMapGen/PNoise.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CShadeEditorWnd::vID[] = 
{
	IDC_SHADE_BRUSH_SIZE_LABEL,					//0
	IDC_SHADE_BRUSH_SIZE,								//1
	IDC_SHADE_BRUSH_SIZE_LABEL_LEFT,		//2
	IDC_SHADE_BRUSH_SIZE_LABEL_RIGHT,		//3
	IDC_SHADE_LEVEL_LABEL,							//4
	IDC_SHADE_LEVEL_TO_0,								//5
	IDC_SHADE_LEVEL_TO_1,								//6
	IDC_SHADE_LEVEL_TO_2,								//7
	IDC_SHADE_LEVEL_TO_3,								//8
	IDC_SHADE_BRUSH_SIZE_LABEL_BOTTOM,	//9
	IDC_LEVEL_LABEL,										//10
	IDC_SHADE_HEIGHT_LABEL_LEFT,				//11
	IDC_SHADE_HEIGHT,										//12
	IDC_SHADE_HEIGHT_LABEL_RIGHT,				//13
	IDC_SHADE_LEVEL_RATIO_LABEL_LEFT,		//14
	IDC_SHADE_LEVEL_RATIO,							//15
	IDC_SHADE_LEVEL_RATIO_LABEL_RIGHT,	//16
	IDC_SHADE_TYPE0,										//17
	IDC_SHADE_TYPE1,										//18
	IDC_SHADE_TYPE2,										//19
	IDC_SHADE_TYPE3,										//20
	IDC_SHADE_TYPE4,										//21
	IDC_SHADE_GRANULARITY_LABEL,				//22
	IDC_SHADE_GRANULARITY,							//23
	IDC_SHADE_MAX_LABEL_LEFT,						//24
	IDC_SHADE_MAX,											//25
	IDC_SHADE_MAX_LABEL_RIGHT,					//26
	IDC_SHADE_MIN_LABEL_LEFT,						//27
	IDC_SHADE_MIN,											//28
	IDC_SHADE_MIN_LABEL_RIGHT,					//29
	IDC_SHADE_GENERATE_BUTTON,					//30
	IDC_SHADE_UPDATE_BUTTON,						//31
	IDC_SHADE_ZERO_BUTTON,							//32
	IDC_SHADE_DELIMITER_00,							//33
	IDC_SHADE_DELIMITER_01,							//34
	IDC_SHADE_DELIMITER_02,							//35
	IDOK,																//36
	IDCANCEL,														//37
};

CShadeEditorWnd::CShadeEditorWnd( CWnd* pParent )
	: CResizeDialog(CShadeEditorWnd::IDD, pParent),
		m_tickCount( 0xFFffFFff ), m_refreshRate( 100 ), isSetEditCtrlValue( true ) 
{

	SetControlStyle( IDC_SHADE_BRUSH_SIZE_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_BRUSH_SIZE, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_BRUSH_SIZE_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_BRUSH_SIZE_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SHADE_LEVEL_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_LEVEL_TO_0, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_LEVEL_TO_1, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_LEVEL_TO_2, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SHADE_LEVEL_TO_3, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SHADE_BRUSH_SIZE_LABEL_BOTTOM, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_LEVEL_LABEL, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_SHADE_HEIGHT_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_HEIGHT, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_HEIGHT_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SHADE_LEVEL_RATIO_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_LEVEL_RATIO, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_LEVEL_RATIO_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SHADE_TYPE0, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_TYPE1, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_TYPE2, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_TYPE3, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_TYPE4, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_GRANULARITY_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_GRANULARITY, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_MAX_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_MAX, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_MAX_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SHADE_MIN_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_SHADE_MIN, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_MIN_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_SHADE_GENERATE_BUTTON, ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SHADE_UPDATE_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SHADE_ZERO_BUTTON, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 1.0f / 3.0f, 1.0f );
	SetControlStyle( IDC_SHADE_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_DELIMITER_01, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_SHADE_DELIMITER_02, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDOK, ANCHORE_LEFT_TOP );
	SetControlStyle( IDCANCEL, ANCHORE_LEFT_TOP );

}

void CShadeEditorWnd::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP( CShadeEditorWnd, CResizeDialog )
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SHADE_BRUSH_SIZE, OnReleasedcaptureShadeBrushSize)
	ON_EN_CHANGE(IDC_SHADE_HEIGHT, OnChangeShadeHeight)
	ON_EN_CHANGE(IDC_SHADE_LEVEL_RATIO, OnChangeShadeLevelratio)
	ON_BN_CLICKED(IDC_SHADE_TYPE0, OnShadeType0)
	ON_BN_CLICKED(IDC_SHADE_TYPE1, OnShadeType1)
	ON_BN_CLICKED(IDC_SHADE_TYPE2, OnShadeType2)
	ON_BN_CLICKED(IDC_SHADE_TYPE3, OnShadeType3)
	ON_BN_CLICKED(IDC_SHADE_TYPE4, OnShadeType4)
	ON_EN_CHANGE(IDC_SHADE_MAX, OnChangeShadeMax)
	ON_EN_CHANGE(IDC_SHADE_MIN, OnChangeShadeMin)
	ON_BN_CLICKED(IDC_SHADE_GENERATE_BUTTON, OnShadeGenerateButton)
	ON_BN_CLICKED(IDC_SHADE_ZERO_BUTTON, OnShadeZeroButton)
	ON_EN_CHANGE(IDC_SHADE_GRANULARITY, OnChangeShadeGranularity)
	ON_BN_CLICKED(IDC_SHADE_LEVEL_TO_0, OnShadeLevelTo0)
	ON_BN_CLICKED(IDC_SHADE_LEVEL_TO_1, OnShadeLevelTo1)
	ON_BN_CLICKED(IDC_SHADE_LEVEL_TO_2, OnShadeLevelTo2)
	ON_BN_CLICKED(IDC_SHADE_LEVEL_TO_3, OnShadeLevelTo3)
	ON_BN_CLICKED(IDC_SHADE_UPDATE_BUTTON, OnShadeUpdateButton)
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CShadeEditorWnd::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.nParameters.size() < 3 )
	{
		resizeDialogOptions.nParameters.resize( 3 );
		resizeDialogOptions.nParameters[0] = 3;
		resizeDialogOptions.nParameters[1] = LEVEL_TO_2;
		resizeDialogOptions.nParameters[2] = TG_FBM;
	}

	if ( resizeDialogOptions.fParameters.size() < 5 )
	{
		resizeDialogOptions.fParameters.resize( 5 );
		resizeDialogOptions.fParameters[0] = 1.0f;
		resizeDialogOptions.fParameters[1] = 0.03f;
		resizeDialogOptions.fParameters[2] = -3.0f;
		resizeDialogOptions.fParameters[3] = 3.0f;
		resizeDialogOptions.fParameters[4] = 0.3f;
	}

	CSliderCtrl* pSliderCtrl = static_cast<CSliderCtrl*>( GetDlgItem( IDC_SHADE_BRUSH_SIZE ) );
	if ( pSliderCtrl )
	{
		pSliderCtrl->SetRange( 2, 16, true );
		pSliderCtrl->SetPos( resizeDialogOptions.nParameters[0] );
		pSliderCtrl->SetLineSize( 1 );
		pSliderCtrl->SetPageSize( 1 );
	}

	CreateCurrentPattern();

	isSetEditCtrlValue = true;
	SetDlgItemText( IDC_SHADE_HEIGHT, NStr::Format( "%.2f", resizeDialogOptions.fParameters[0] ) );
	SetDlgItemText( IDC_SHADE_LEVELRATIO, NStr::Format( "%.2f", resizeDialogOptions.fParameters[1] * 100.0f ) );
	SetDlgItemText( IDC_SHADE_MIN, NStr::Format( "%.2f", resizeDialogOptions.fParameters[2] ) );
	SetDlgItemText( IDC_SHADE_MAX, NStr::Format( "%.2f", resizeDialogOptions.fParameters[3] ) );
	SetDlgItemText( IDC_SHADE_GRANULARITY, NStr::Format( "%.2f", resizeDialogOptions.fParameters[4] ) );
	isSetEditCtrlValue = false;

	UpdateTerrainGenButtons();
	UpdateLevelToButtons();
	UpdateControls();
	return TRUE;
}

void CShadeEditorWnd::CreateCurrentPattern()
{
	std::string stdFileName = "editor\\profile.tga";

	CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( stdFileName.c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_T( pImageStream != 0, NStr::Format("Can't open image file \"%s\"", stdFileName.c_str() ) );

  IImageProcessor *pIP = GetImageProcessor();
  CPtr<IImage> pImage = pIP->LoadImage( pImageStream );
	
	SVAGradient gradient;
	gradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, resizeDialogOptions.fParameters[0] ) );

	CSliderCtrl* pSliderCtrl = static_cast<CSliderCtrl*>( GetDlgItem( IDC_SHADE_BRUSH_SIZE ) );
	if ( pSliderCtrl )
	{
		resizeDialogOptions.nParameters[0] = pSliderCtrl->GetPos();
		m_currentPattern.CreateFromGradient( gradient , resizeDialogOptions.nParameters[0] * 2 );
		m_currentLevelPattern.CreateValue( 1.0f, resizeDialogOptions.nParameters[0] * 2 );
		m_currentUndoLevelPattern.CreateValue( 1.0f, resizeDialogOptions.nParameters[0] * 2 );
	}
	SetDlgItemText( IDC_BRUSH_SIZE_LABEL, NStr::Format( "Brush size: %d", m_currentPattern.heights.GetSizeX() ) );
}

void CShadeEditorWnd::OnReleasedcaptureShadeBrushSize(NMHDR* pNMHDR, LRESULT* pResult) 
{
	*pResult = 0;
}

void CShadeEditorWnd::OnChangeShadeHeight() 
{
	if ( !isSetEditCtrlValue )
	{
		CString szValue;
		GetDlgItemText( IDC_SHADE_HEIGHT, szValue );
		float _fMaxHeight = resizeDialogOptions.fParameters[0];
		if ( ( sscanf( szValue, "%g", &( resizeDialogOptions.fParameters[0] ) ) < 1 ) || ( resizeDialogOptions.fParameters[0] < FP_EPSILON ) )
		{
			resizeDialogOptions.fParameters[0] = _fMaxHeight;
			isSetEditCtrlValue = true;
			isSetEditCtrlValue = false;
		}
		else 
		{
			CreateCurrentPattern();
		}
	}
}

void CShadeEditorWnd::OnChangeShadeLevelratio() 
{
	if ( !isSetEditCtrlValue )
	{
		CString szValue;
		GetDlgItemText( IDC_SHADE_LEVELRATIO, szValue );
		float _fLevelRatio = resizeDialogOptions.fParameters[1];
		if ( ( sscanf( szValue, "%g", &( resizeDialogOptions.fParameters[1] ) ) < 1 ) || ( resizeDialogOptions.fParameters[1] < ( FP_EPSILON * 100.0f ) ) )
		{
			resizeDialogOptions.fParameters[1] = _fLevelRatio;
			isSetEditCtrlValue = true;
			isSetEditCtrlValue = false;
		}
		else
		{
			resizeDialogOptions.fParameters[1] = resizeDialogOptions.fParameters[1] / 100.0f;
		}
	}
}

void CShadeEditorWnd::OnShadeType0() 
{
	resizeDialogOptions.nParameters[2] = TG_FBM;
	UpdateTerrainGenButtons();
}

void CShadeEditorWnd::OnShadeType1() 
{
	resizeDialogOptions.nParameters[2] = TG_MULTI;
	UpdateTerrainGenButtons();
}

void CShadeEditorWnd::OnShadeType2() 
{
	resizeDialogOptions.nParameters[2] = TG_HETERO;
	UpdateTerrainGenButtons();
}

void CShadeEditorWnd::OnShadeType3() 
{
	resizeDialogOptions.nParameters[2] = TG_HYBRID;
	UpdateTerrainGenButtons();
}

void CShadeEditorWnd::OnShadeType4() 
{
	resizeDialogOptions.nParameters[2] = TG_RIDGED;
	UpdateTerrainGenButtons();
}

void CShadeEditorWnd::OnChangeShadeMax() 
{
	if ( !isSetEditCtrlValue )
	{
		CString szValue;
		GetDlgItemText( IDC_SHADE_MAX, szValue );
		float _max = resizeDialogOptions.fParameters[3];
		if ( ( sscanf( szValue, "%g", &( resizeDialogOptions.fParameters[3] ) ) < 1 ) || ( ( resizeDialogOptions.fParameters[3] - resizeDialogOptions.fParameters[2] ) <= FP_EPSILON ) )
		{
			resizeDialogOptions.fParameters[3] = _max;
			isSetEditCtrlValue = true;
			isSetEditCtrlValue = false;
		}
	}
}

void CShadeEditorWnd::OnChangeShadeMin() 
{
	if ( !isSetEditCtrlValue )
	{
		CString szValue;
		GetDlgItemText( IDC_SHADE_MIN, szValue );
		float _min = resizeDialogOptions.fParameters[2];
		if ( ( sscanf( szValue, "%g", &( resizeDialogOptions.fParameters[2] ) ) < 1 ) || ( ( resizeDialogOptions.fParameters[3] - resizeDialogOptions.fParameters[2] ) <= FP_EPSILON ) )
		{
			resizeDialogOptions.fParameters[2] = _min;
			isSetEditCtrlValue = true;
			isSetEditCtrlValue = false;
		}
	}
}

void CShadeEditorWnd::OnShadeGenerateButton() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
		if ( pFrame )
		{
			if ( IScene *pScene = GetSingleton<IScene>() )
			{
				if ( ITerrain *pTerrain = pScene->GetTerrain() )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					if ( MessageBox( "Do you really want to generate heights?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
					{
						ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
						STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

						NPerlinNoise::Init();
						CHField hfield( rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() );

						SfBmValues fBmValue = CHField::fBmDefVals[resizeDialogOptions.nParameters[2]];
						fBmValue.featSize = resizeDialogOptions.fParameters[4];
						hfield.Generate( fBmValue );
						
						CTPoint<float> currentRange( 0.0f, 0.0f );
						float fCurrentRange = hfield.AltitudeRange( &( currentRange.min ), &( currentRange.max ) );
						
						for ( int nXIndex = 0; nXIndex < rTerrainInfo.altitudes.GetSizeX(); ++nXIndex )
						{
							for ( int nYIndex = 0; nYIndex < rTerrainInfo.altitudes.GetSizeY(); ++nYIndex )
							{
								rTerrainInfo.altitudes[nYIndex][nXIndex].fHeight = ( ( hfield.H( nXIndex, nYIndex ) - currentRange.min ) * ( resizeDialogOptions.fParameters[3] - resizeDialogOptions.fParameters[2] ) * fWorldCellSize / fCurrentRange ) + ( resizeDialogOptions.fParameters[2] * fWorldCellSize );
							}
						}
						pFrame->OnButtonUpdate();
						if ( g_frameManager.GetMiniMapWindow() )
						{
							g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( true );
						}
					}
				}	
			}
		}
	}
}

void CShadeEditorWnd::OnShadeZeroButton() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
		if ( pFrame )
		{
			if ( IScene *pScene = GetSingleton<IScene>() )
			{
				if ( ITerrain *pTerrain = pScene->GetTerrain() )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					if ( MessageBox( "Do you really want to zero all heights?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
					{
						ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
						STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
						rTerrainInfo.altitudes.SetZero();
						pFrame->OnButtonUpdate();
						if ( g_frameManager.GetMiniMapWindow() )
						{
							g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( true );
						}
					}
				}	
			}
		}
	}
}

void CShadeEditorWnd::UpdateTerrainGenButtons()
{
	int type = IDC_SHADE_TYPE0;
	switch( resizeDialogOptions.nParameters[2] )
	{
		case TG_FBM:
		{
			type = IDC_SHADE_TYPE0;
			break;
		}
		case TG_MULTI:
		{
			type = IDC_SHADE_TYPE1;
			break;
		}
		case TG_HETERO:
		{
			type = IDC_SHADE_TYPE2;
			break;
		}
		case TG_HYBRID:
		{
			type = IDC_SHADE_TYPE3;
			break;
		}
		default:
		{
			type = IDC_SHADE_TYPE4;
			break;
		}
	}
	CheckRadioButton( IDC_SHADE_TYPE0, IDC_SHADE_TYPE4, type );
}

void CShadeEditorWnd::UpdateLevelToButtons()
{
	int type = IDC_SHADE_LEVEL_TO_0;
	switch( resizeDialogOptions.nParameters[1] )
	{
		case LEVEL_TO_0:
		{
			type = IDC_SHADE_LEVEL_TO_0;
			break;
		}
		case LEVEL_TO_1:
		{
			type = IDC_SHADE_LEVEL_TO_1;
			break;
		}
		case LEVEL_TO_2:
		{
			type = IDC_SHADE_LEVEL_TO_2;
			break;
		}
		default:
		{
			type = IDC_SHADE_LEVEL_TO_3;
			break;
		}
	}
	CheckRadioButton( IDC_SHADE_LEVEL_TO_0, IDC_SHADE_LEVEL_TO_3, type );
}

void CShadeEditorWnd::OnChangeShadeGranularity() 
{
	if ( !isSetEditCtrlValue )
	{
		CString szValue;
		GetDlgItemText( IDC_SHADE_GRANULARITY, szValue );
		float _fGranularity = resizeDialogOptions.fParameters[4];
		if ( ( sscanf( szValue, "%g", &( resizeDialogOptions.fParameters[4] ) ) < 1 ) || ( resizeDialogOptions.fParameters[4] < FP_EPSILON ) )
		{
			resizeDialogOptions.fParameters[4] = _fGranularity;
			isSetEditCtrlValue = true;
			isSetEditCtrlValue = false;
		}
		else 
		{
			CreateCurrentPattern();
		}
	}
}

void CShadeEditorWnd::OnShadeLevelTo0() 
{
	resizeDialogOptions.nParameters[1] = LEVEL_TO_0;
	UpdateLevelToButtons();
}

void CShadeEditorWnd::OnShadeLevelTo1() 
{
	resizeDialogOptions.nParameters[1] = LEVEL_TO_1;
	UpdateLevelToButtons();
}

void CShadeEditorWnd::OnShadeLevelTo2() 
{
	resizeDialogOptions.nParameters[1] = LEVEL_TO_2;
	UpdateLevelToButtons();
}

void CShadeEditorWnd::OnShadeLevelTo3() 
{
	resizeDialogOptions.nParameters[1] = LEVEL_TO_3;
	UpdateLevelToButtons();
}

void CShadeEditorWnd::OnShadeUpdateButton() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
		if ( pFrame )
		{
			if ( IScene *pScene = GetSingleton<IScene>() )
			{
				if ( ITerrain *pTerrain = pScene->GetTerrain() )
				{
					pFrame->OnButtonUpdate();
					if ( g_frameManager.GetMiniMapWindow() )
					{
						g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( true );
					}
				}
			}
		}
	}
}

void CShadeEditorWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	CResizeDialog ::OnHScroll(nSBCode, nPos, pScrollBar);
	CreateCurrentPattern();
}

void CShadeEditorWnd::UpdateControls()
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
	
	if ( CWnd *pWnd = GetDlgItem( IDC_SHADE_GENERATE_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_SHADE_UPDATE_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled );
	}
	if ( CWnd *pWnd = GetDlgItem( IDC_SHADE_ZERO_BUTTON ) )
	{
		pWnd->EnableWindow( bEnabled );
	}
}

void CShadeEditorWnd::OnDestroy() 
{
	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog ::OnDestroy();
}
