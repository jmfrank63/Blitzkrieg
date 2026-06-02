#include "StdAfx.h"

#include "Icon.h"
int SIconDesc::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pIcon );
	saver.Add( 2, &nID );
	saver.Add( 3, &nPriority );
	saver.Add( 4, &placement );
	saver.Add( 5, &vAddValue );
	saver.Add( 6, &vAddStep );
	return 0;
}
void RepositionIconsLocal( CIconsList &icons, DWORD placement, const CTRect<float> &rcRect, const float _fAddZ )
{
	CIconsList locals;
	for ( CIconsList::iterator it = icons.begin(); it != icons.end(); ++it )
	{
		if ( (it->placement & placement) == placement )
			locals.push_back( *it );
	}
	if ( locals.empty() )
		return;
	locals.sort( SIconDescPriorityLessFunctional() );
	const float fAddZ = rcRect.top + _fAddZ;
	CVec3 vPos = VNULL3;
	CVec3 vStep = VNULL3;
	CVec3 vStepSign = CVec3( 1, 1, 1 );
	CVec3 vAdd = VNULL3;
	switch ( placement & (ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_RIGHT) ) 
	{
		case ICON_ALIGNMENT_LEFT:
			vPos.x = vPos.y = rcRect.left;
			break;
		case ICON_ALIGNMENT_HCENTER:
			vPos.x = vPos.y = ( rcRect.right + rcRect.left ) / 2.0f;
			break;
		case ICON_ALIGNMENT_RIGHT:
			vPos.x = rcRect.right;
			vPos.y = rcRect.right;
			vStepSign.x = vStepSign.y = -1;
			break;
		default:
			NI_ASSERT_T( false, "unknown horizontal placement" )
	}
	switch ( placement & (ICON_ALIGNMENT_TOP | ICON_ALIGNMENT_VCENTER | ICON_ALIGNMENT_BOTTOM) ) 
	{
		case ICON_ALIGNMENT_TOP:
			vPos.z = fAddZ;
			vStepSign.z = 1;
			break;
		case ICON_ALIGNMENT_VCENTER:
			vPos.z = ( rcRect.top + rcRect.bottom ) / 2.0f;
			break;
		case ICON_ALIGNMENT_BOTTOM:
			vPos.z = rcRect.bottom;
			break;
		default:
			NI_ASSERT_T( false, "unknown vertical placement" )
	}
	switch ( placement & (ICON_PLACEMENT_VERTICAL | ICON_PLACEMENT_HORIZONTAL) ) 
	{
		case ICON_PLACEMENT_VERTICAL:
			vStepSign.x = vStepSign.y = 0;
			vStep.z = 1;
			vAdd.z = 1;
			break;
		case ICON_PLACEMENT_HORIZONTAL:
			vStepSign.z = 0;
			vStep.x = vStep.y = 1.0f / FP_SQRT_2;
			vAdd.x = vAdd.y = 1;
			break;
		default:
			NI_ASSERT_T( false, "unknown direction" )
	}
	NI_ASSERT_T( vStep != VNULL3, NStr::Format("Can't reposition icons - placement 0x%x still not realized", placement) );
	for ( CIconsList::iterator it = locals.begin(); it != locals.end(); ++it )
	{
		vPos += it->vAddStep;
		it->pIcon->SetPosition( vPos + it->vAddValue );
		const CVec2 vSize = it->pIcon->GetSize();
		vPos.x += vStepSign.x * ( vStep.x * vSize.x + vAdd.x );
		vPos.y += vStepSign.y * ( vStep.y * vSize.x + vAdd.y );
		vPos.z += vStepSign.z * ( vStep.z * vSize.y + vAdd.z );
	}
}
