#include "StdAfx.h"

#include "../Main/RPGStats.h"
#include "RMG_FieldObjectPropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CRMGFieldObjectPropertiesDialog::VIS_TYPES_COUNT = 4;
const int CRMGFieldObjectPropertiesDialog::GAME_TYPES_COUNT = 18;
const int CRMGFieldObjectPropertiesDialog::SQUAD_TYPES_COUNT = 9;
const int CRMGFieldObjectPropertiesDialog::BUILDING_TYPES_COUNT = 4;

const char* CRMGFieldObjectPropertiesDialog::VIS_TYPES[] =
{
	"Not specified",
	"Sprite",
	"Mesh",
	"Visual effect",
};

const char* CRMGFieldObjectPropertiesDialog::GAME_TYPES[] =
{
	"Not specified",
	"Dynamic unit with AI behavior",
	"Static building, can be boarded by soldiers, etc",
	"Pillboxes, ground fortifications, etc",
	"Entrenchment segment, can be boarded by soldiers, etc",
	"Tank pit",
	"Bridge segment, can pass through it",
	"Mine (anti-infantry or anti-tank)",
	"Simple object",
	"Fence element",
	"Terrain object",
	"Effect (boom, smoke, etc.)",
	"Projectile effect",
	"Shadow",
	"Icon object (bar, text, picture, etc.)",
	"Squad (a number of soldiers)",
	"Flash after the shot",
	"Flag",
};

const char* CRMGFieldObjectPropertiesDialog::SQUAD_TYPES[] =
{
	"Riflemans",
	"Infantry",
	"Submachinegunners",
	"Machinegunners",
	"AT Team",
	"Mortars crew",
	"Snipers",
	"Guns crew",
	"Engineers", 
};

const char* CRMGFieldObjectPropertiesDialog::BUILDING_TYPES[] =
{
	"Building",
	"Main resource storage",
	"Additional resource storage",
	"Pillbox",
};

CRMGFieldObjectPropertiesDialog::CRMGFieldObjectPropertiesDialog( CWnd* pParent )
	: CResizeDialog( CRMGFieldObjectPropertiesDialog::IDD, pParent ), bDisableEditWeight( false ), hIcon( 0 )
{
	m_szName = _T("");
	m_szPath = _T("");
	m_szStats = _T("");
	m_szVisType = _T("");
	m_szGameType = _T("");
	m_szWeight = _T("");

	SetControlStyle( IDC_RMG_CF_OS_OP_STATS_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_OS_OP_STATS_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_RMG_CF_OS_OP_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_RMG_CF_OS_OP_ICON, ANCHORE_LEFT_TOP );
	
	SetControlStyle( IDC_RMG_CF_OS_OP_NAME_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_OS_OP_NAME_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_OS_OP_WEIGHT_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_OS_OP_WEIGHT_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_OS_OP_DELIMITER_01, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_OS_OP_PATH_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_OS_OP_PATH_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_OS_OP_VIS_TYPE_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_OS_OP_VIS_TYPE_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CF_OS_OP_GAME_TYPE_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_RMG_CF_OS_OP_GAME_TYPE_LABEL_RIGHT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDOK, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDCANCEL, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CRMGFieldObjectPropertiesDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_RMG_CF_OS_OP_ICON, m_Icon);
	DDX_Text(pDX, IDC_RMG_CF_OS_OP_NAME_LABEL_RIGHT, m_szName);
	DDX_Text(pDX, IDC_RMG_CF_OS_OP_PATH_LABEL_RIGHT, m_szPath);
	DDX_Text(pDX, IDC_RMG_CF_OS_OP_STATS_LABEL_RIGHT, m_szStats);
	DDX_Text(pDX, IDC_RMG_CF_OS_OP_VIS_TYPE_LABEL_RIGHT, m_szVisType);
	DDX_Text(pDX, IDC_RMG_CF_OS_OP_GAME_TYPE_LABEL_RIGHT, m_szGameType);
	DDX_Text(pDX, IDC_RMG_CF_OS_OP_WEIGHT_EDIT, m_szWeight);
}

BEGIN_MESSAGE_MAP(CRMGFieldObjectPropertiesDialog, CResizeDialog)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CRMGFieldObjectPropertiesDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( bDisableEditWeight )
	{
		if ( CWnd *pWnd = GetDlgItem( IDC_RMG_CF_OS_OP_WEIGHT_LABEL ) )
		{
			pWnd->ShowWindow( SW_HIDE );
			pWnd->EnableWindow( false );
		}
		if ( CWnd *pWnd = GetDlgItem( IDC_RMG_CF_OS_OP_WEIGHT_EDIT ) )
		{
			pWnd->ShowWindow( SW_HIDE );
			pWnd->EnableWindow( false );
		}
		if ( CWnd *pWnd = GetDlgItem( IDCANCEL ) )
		{
			pWnd->ShowWindow( SW_HIDE );
		}
		if ( CWnd *pWnd = GetDlgItem( IDOK ) )
		{
			pWnd->SetWindowText( "&Close" );

			CRect buttonRect;
			pWnd->GetWindowRect( &buttonRect );
			ScreenToClient( &buttonRect );

			CRect clientRect;
			GetClientRect( &clientRect );
			pWnd->MoveWindow( ( clientRect.Width() - buttonRect.Width() ) / 2,
												buttonRect.top,
												buttonRect.Width(),
												buttonRect.Height() );
		}
		UpdateControlPositions();
	}
	
	if ( hIcon != 0 )
	{
		m_Icon.SetIcon( hIcon );
	}
	else
	{
		m_Icon.ShowWindow( SW_HIDE );
		m_Icon.EnableWindow( false );
	}

	return TRUE;
}

void CRMGFieldObjectPropertiesDialog::OnDestroy() 
{
	CResizeDialog::OnDestroy();
	
	if ( hIcon != 0 )
	{
		::DestroyIcon( hIcon );
		hIcon = 0;
	}
}

std::string CRMGFieldObjectPropertiesDialog::GetObjectStats( IObjectsDB *pODB, const SGDBObjectDesc *pObjectDesc )
{
	std::string szStats;
	switch ( pObjectDesc->eGameType )
	{
		case SGVOGT_UNIT:
		{
			CGDBPtr<SMechUnitRPGStats> pStats = NGDB::GetRPGStats<SMechUnitRPGStats>( pODB, pObjectDesc );
			if ( pStats )
			{
				float fDamage = 0.0f;
				int nPiersing = 0;
				int nGunners = 0;
				if ( pStats->pPrimaryGun && pStats->pPrimaryGun->pWeapon && !pStats->pPrimaryGun->pWeapon->shells.empty() ) 
				{
					const SWeaponRPGStats::SShell &shell = pStats->pPrimaryGun->pWeapon->shells[0];
					fDamage = shell.fDamagePower;
					nPiersing = shell.nPiercing;
				}
				if ( !pStats->vGunners.empty() )
				{
					nGunners = pStats->vGunners[0].size();
				}
				szStats = NStr::Format( "HP: %.0f, Armor(f,s,r,t): %d, %d, %d, %d, Gun(d,p): %.0f, %d, Crew: %d, Passengers: %d",
																pStats->GetMapHP(),
																pStats->GetArmor( RPG_FRONT ),
																( pStats->GetArmor( RPG_LEFT ) + pStats->GetArmor( RPG_RIGHT ) ) / 2,
																pStats->GetArmor( RPG_BACK ),
																pStats->GetArmor( RPG_TOP ),
																fDamage,
																nPiersing,
																nGunners,
																pStats->nPassangers );
			}
			break;
		}
		case SGVOGT_BUILDING:
		case SGVOGT_FORTIFICATION:
		{
			CGDBPtr<SBuildingRPGStats> pStats = NGDB::GetRPGStats<SBuildingRPGStats>( pODB, pObjectDesc );
			if ( pStats )
			{
				int nType = pStats->eType;
				if ( ( nType < 0 ) || ( nType >= CRMGFieldObjectPropertiesDialog::BUILDING_TYPES_COUNT ) )
				{
					nType = 0;
				}
				szStats = NStr::Format( "HP: %.0f, Type: %s, Entrances: %d, Unit slots: %d",
																pStats->GetMapHP(),
																CRMGFieldObjectPropertiesDialog::BUILDING_TYPES[nType],
																pStats->entrances.size(),
																pStats->nRestSlots + pStats->nMedicalSlots );
			}
			break;
		}
		case SGVOGT_SQUAD:
		{
			CGDBPtr<SSquadRPGStats> pStats = NGDB::GetRPGStats<SSquadRPGStats>( pODB, pObjectDesc );
			if ( pStats )
			{
				int nType = pStats->type;
				if ( ( nType < 0 ) || ( nType >= CRMGFieldObjectPropertiesDialog::SQUAD_TYPES_COUNT ) )
				{
					nType = 0;
				}
				szStats = NStr::Format( "Type: %s, Units: %d, Fomations: %d",
																CRMGFieldObjectPropertiesDialog::SQUAD_TYPES[nType],
																pStats->memberNames.size(),
																pStats->formations.size() );
			}
			break;
		}
	}
	return szStats;
}
