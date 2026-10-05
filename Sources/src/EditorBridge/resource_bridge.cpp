// The resource editor's C ABI implementation. Every entry point is wrapped in
// the shared Guarded template (guarded.h) so no exception crosses into Zig.
// T01 stubs them to BK_EDITOR_OK; later tasks (T02..T06) replace each body
// with real behaviour against NResourceModel. This file lives in the same
// static library as bridge.cpp (addEditorBridge in build.zig), so a
// resource-editor executable links one archive and gets both ABIs.
#include "StdAfx.h"
#include "resource_bridge.h"
#include "session.h"
#include "bridge_session.h"
#include "guarded.h"

extern "C" {

/* ---- Projects --------------------------------------------------------- */

BkEditorStatus BkResNew( BkResSession *pSession, BkResKind )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResOpen( BkResSession *pSession, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSave( BkResSession *pSession, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResClose( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResKindOf( BkResSession *pSession, BkResKind *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		*pOut = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResLock( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResLockOwner( BkResSession *pSession, char *pOut, int nCapacity )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut == 0 || nCapacity <= 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		pOut[0] = 0;
		return BK_EDITOR_OK;
	} );
}

/* ---- Tree ------------------------------------------------------------- */

BkEditorStatus BkResNodes( BkResSession *pSession, BkResNodeRecord *, int, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount != 0 )
			*pnCount = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResProps( BkResSession *pSession, int, BkResPropRecord *, int, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount != 0 )
			*pnCount = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetProp( BkResSession *pSession, int, int, const char * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResInsertNode( BkResSession *pSession, int, int, int, int *pnOutID )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnOutID != 0 )
			*pnOutID = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResDeleteNode( BkResSession *pSession, int, unsigned char *, int, int *pnSize )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnSize != 0 )
			*pnSize = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResRestoreNode( BkResSession *pSession, const unsigned char *, int, int, int, int *pnOutID )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnOutID != 0 )
			*pnOutID = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResMoveNode( BkResSession *pSession, int, int, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		return BK_EDITOR_OK;
	} );
}

/* ---- References ------------------------------------------------------- */

BkEditorStatus BkResRefList( BkResSession *pSession, int nType, BkResReferenceEntry *, int, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( nType < 0 || nType > 19 )
			return BK_EDITOR_BAD_ARGUMENT;
		if ( pnCount != 0 )
			*pnCount = 0;
		return BK_EDITOR_OK;
	} );
}

/* ---- Geometry --------------------------------------------------------- */

/* All get/set pairs are stubbed. T04 fills them with the real NResourceModel
   reads. Each returns BK_EDITOR_OK and zeroed outputs for now, which is
   enough for the smoke tier below to link and run. */
BkEditorStatus BkResGetPassabilityCells( BkResSession *pSession, int, unsigned char *, int, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnW != 0 ) *pnW = 0;
		if ( pnH != 0 ) *pnH = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetPassabilityCells( BkResSession *pSession, int, const unsigned char *, int, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetLockedTiles( BkResSession *pSession, int, unsigned char *, int, int *pnW, int *pnH )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnW != 0 ) *pnW = 0;
		if ( pnH != 0 ) *pnH = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetLockedTiles( BkResSession *pSession, int, const unsigned char *, int, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetTransparencyLines( BkResSession *pSession, int, BkResPoint2 *, int, int *pnCount )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pnCount != 0 ) *pnCount = 0;
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetTransparencyLines( BkResSession *pSession, int, const BkResPoint2 *, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetZeroPoint( BkResSession *pSession, int, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut != 0 ) { pOut->x = 0; pOut->y = 0; }
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetZeroPoint( BkResSession *pSession, int, const BkResPoint2 * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResGetEntrance( BkResSession *pSession, int, BkResPoint2 *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut != 0 ) { pOut->x = 0; pOut->y = 0; }
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResSetEntrance( BkResSession *pSession, int, const BkResPoint2 * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

#define BKRES_GET_AIMED_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, BkResAimedPoint *, int, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		if ( pnCount != 0 ) *pnCount = 0; \
		return BK_EDITOR_OK; \
	} ); \
}
#define BKRES_SET_AIMED_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResAimedPoint *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } ); \
}
BKRES_GET_AIMED_STUB( BkResGetShootPoints )
BKRES_SET_AIMED_STUB( BkResSetShootPoints )
BKRES_GET_AIMED_STUB( BkResGetFirePoints )
BKRES_SET_AIMED_STUB( BkResSetFirePoints )
BKRES_GET_AIMED_STUB( BkResGetSmokePoints )
BKRES_SET_AIMED_STUB( BkResSetSmokePoints )
BKRES_GET_AIMED_STUB( BkResGetDirectedExplosionPoints )
BKRES_SET_AIMED_STUB( BkResSetDirectedExplosionPoints )
#undef BKRES_GET_AIMED_STUB
#undef BKRES_SET_AIMED_STUB

#define BKRES_GET_POINT2_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, BkResPoint2 *, int, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		if ( pnCount != 0 ) *pnCount = 0; \
		return BK_EDITOR_OK; \
	} ); \
}
#define BKRES_SET_POINT2_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResPoint2 *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } ); \
}
BKRES_GET_POINT2_STUB( BkResGetFormationPositions )
BKRES_SET_POINT2_STUB( BkResSetFormationPositions )
BKRES_GET_POINT2_STUB( BkResGetMissionObjectives )
BKRES_SET_POINT2_STUB( BkResSetMissionObjectives )
BKRES_GET_POINT2_STUB( BkResGetChapterCrosses )
BKRES_SET_POINT2_STUB( BkResSetChapterCrosses )
BKRES_GET_POINT2_STUB( BkResGetCampaignCrosses )
BKRES_SET_POINT2_STUB( BkResSetCampaignCrosses )
#undef BKRES_GET_POINT2_STUB
#undef BKRES_SET_POINT2_STUB

#define BKRES_GET_VEC3_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, BkResVec3 *, int, int *pnCount ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus \
	{ \
		if ( pnCount != 0 ) *pnCount = 0; \
		return BK_EDITOR_OK; \
	} ); \
}
#define BKRES_SET_VEC3_STUB( fname ) \
BkEditorStatus fname( BkResSession *pSession, int, const BkResVec3 *, int ) \
{ \
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } ); \
}
BKRES_GET_VEC3_STUB( BkResGetBridgeSpanMarks )
BKRES_SET_VEC3_STUB( BkResSetBridgeSpanMarks )
BKRES_GET_VEC3_STUB( BkResGetParticleKeyframes )
BKRES_SET_VEC3_STUB( BkResSetParticleKeyframes )
BKRES_GET_VEC3_STUB( BkResGetEffectKeyframes )
BKRES_SET_VEC3_STUB( BkResSetEffectKeyframes )
#undef BKRES_GET_VEC3_STUB
#undef BKRES_SET_VEC3_STUB

/* ---- Export ----------------------------------------------------------- */

static void ClearReport( BkResExportReport *pReport )
{
	if ( pReport == 0 )
		return;
	pReport->written = 0;
	pReport->skipped = 0;
	pReport->warning_count = 0;
}

BkEditorStatus BkResExport( BkResSession *pSession, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResExportStatsOnly( BkResSession *pSession, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResBatch( BkResSession *pSession, int, const char *, const char *, int, BkResExportReport *pReport )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		ClearReport( pReport );
		return BK_EDITOR_OK;
	} );
}

/* ---- MOD -------------------------------------------------------------- */

BkEditorStatus BkResModSettingsGet( BkResSession *pSession, BkResModSettings *pOut )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pOut != 0 )
		{
			pOut->name[0] = 0;
			pOut->version[0] = 0;
			pOut->bake_compressed = 0;
			pOut->bake_packed = 0;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkResModSettingsSet( BkResSession *pSession, const BkResModSettings * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPackMod( BkResSession *pSession, const char * )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

/* ---- Preview --------------------------------------------------------- */

BkEditorStatus BkResPreviewBegin( BkResSession *pSession, BkResKind )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewShow( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewStop( BkResSession *pSession )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewPlayback( BkResSession *pSession, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

BkEditorStatus BkResPreviewCamera( BkResSession *pSession, float, float, int )
{
	return Guarded( pSession, [=]() -> BkEditorStatus { return BK_EDITOR_OK; } );
}

/* ---- Import ----------------------------------------------------------- */

BkEditorStatus BkResImportFromGame( BkResSession *pSession, BkResKind, const char *pszPath )
{
	return Guarded( pSession, [=]() -> BkEditorStatus
	{
		if ( pszPath == 0 )
			return BK_EDITOR_BAD_ARGUMENT;
		return BK_EDITOR_OK;
	} );
}

}
