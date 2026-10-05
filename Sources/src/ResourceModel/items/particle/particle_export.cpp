// The particle exporter: CParticleFrame::ExportFrameData / SaveRPGStats
// (Sources/src/editor/ParticleFrm.cpp:386-439, 500-504) with FillRPGStats
// (274) and FillRPGStats2 (338), line for line. The engine's
// SParticleSourceData or SSmokinParticleSourceData is filled from the tree and
// written as the "KeyData" chunk through the engine's own operator&, so the
// file is what the game's particle manager reads. A particle is a stats-only
// kind: its texture is a shipped one the source names, so nothing here
// composes graphics and bStatsOnly changes nothing.
//
// MFC wrote the one (0, 0) key per track while bNewProjectJustCreated was
// set, a frame state for a project that had never been edited; the bridge
// exports saved projects only, so the tree is always used (as the weapon).
//
// MFC ended each Fill with InitIntegrals, which fills the runtime
// integrals the files do not store (and asserts on an empty track); it
// changes nothing that is written, so the port leaves it out.
#include "StdAfx.h"

#include "particle_export.h"

#include "../stats_export.h"
#include "../tree_item_types.h"
#include "../../key_frame_tree_item.h"
#include "../../../Scene/ParticleSourceData.h"
#include "../../../Scene/SmokinParticleSourceData.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kParticleAddDir[] = "effects\\particles\\";
// The angle track's degrees to radians, as ParticleFrm.cpp:263 computed them.
const float kAngleToRadian = 2.0f * 3.14159265358979f / 360.0f;

// CopyFramesListToKeyTrack (ParticleFrm.cpp:239): the x of a frame is a
// fraction of the source's life, a key's time is in thousandths of it. A
// track with one frame gets a second key at 1000 with the same value, so
// the engine's interpolation has an interval.
void CopyFramesListToKeyTrack( CTrack &track, const CFramesList &framesList, float fMulty = 1.0f )
{
	int i = 0;
	for ( const auto &frame : framesList )
	{
		track.AddKey( frame.first * 1000, frame.second * fMulty );
		++i;
	}
	if ( i == 1 )
		track.AddKey( 1000, framesList.front().second * fMulty );
}

// CopyFramesListToAngleTrack (ParticleFrm.cpp:257): the angle track is edited
// in degrees and stored in radians.
void CopyFramesListToAngleTrack( CTrack &track, const CFramesList &framesList )
{
	int i = 0;
	float fValue = 0;
	for ( const auto &frame : framesList )
	{
		fValue = frame.second * kAngleToRadian;
		track.AddKey( frame.first * 1000, fValue );
		++i;
	}
	if ( i == 1 )
		track.AddKey( 1000, fValue );
}

// The curve item of the given type under parent, or null with the export
// failed naming the track.
const CKeyFrameTreeItem *RequireCurve( const CTreeItem &parent, int nType, const char *pszTrack, SExportOutcome &outcome )
{
	const CTreeItem *pItem = RequireChild( parent, nType, 0, pszTrack, outcome );
	if ( pItem == nullptr )
		return nullptr;
	const CKeyFrameTreeItem *pCurve = dynamic_cast<const CKeyFrameTreeItem *>( pItem );
	if ( pCurve == nullptr && outcome.szError.empty() )
		outcome.szError = std::string( "the particle track \"" ) + pszTrack + "\" is not a key frame item";
	return pCurve;
}

// One track filled from its curve item; false with the track named.
bool FillTrack( CTrack &track, const CTreeItem &parent, int nType, const char *pszTrack, SExportOutcome &outcome, bool bAngle = false )
{
	const CKeyFrameTreeItem *pCurve = RequireCurve( parent, nType, pszTrack, outcome );
	if ( pCurve == nullptr )
		return false;
	if ( bAngle )
		CopyFramesListToAngleTrack( track, pCurve->framesList );
	else
		CopyFramesListToKeyTrack( track, pCurve->framesList );
	return true;
}

CVec3 Vector( const CTreeItem &item, int nFirst )
{
	return CVec3( ValueFloat( item, nFirst ), ValueFloat( item, nFirst + 1 ), ValueFloat( item, nFirst + 2 ) );
}

// CParticleCommonPropsItem::GetAreaType: the combo's text, either case.
int AreaTypeOf( const CTreeItem &commonProps )
{
	const std::string szVal = ValueStr( commonProps, 14 );
	if ( szVal == "disk" || szVal == "Disk" )
		return PSA_TYPE_DISK;
	if ( szVal == "circle" || szVal == "Circle" )
		return PSA_TYPE_CIRCLE;
	return PSA_TYPE_SQUARE;
}

const CTreeItem *ComplexSourceItem( const CTreeItem &root )
{
	return ChildItem( root, ETIT_PARTICLE_COMPLEX_SOURCE_ITEM );
}

// CParticleFrame::FillRPGStats, the part both structs share.
template <class TData>
bool FillCommon( TData &particleSetup, const CTreeItem &root, SExportOutcome &outcome )
{
	const CTreeItem *pCommonProps = RequireChild( root, ETIT_PARTICLE_COMMON_PROPS_ITEM, 0, "Basic info", outcome );
	if ( pCommonProps == nullptr )
		return false;
	particleSetup.fGravity = ValueFloat( *pCommonProps, 3 );
	particleSetup.nLifeTime = ValueInt( *pCommonProps, 1 );
	particleSetup.vDirection = Vector( *pCommonProps, 7 );
	particleSetup.vWind = Vector( *pCommonProps, 10 );
	particleSetup.fRadialWind = ValueFloat( *pCommonProps, 13 );
	particleSetup.nAreaType = AreaTypeOf( *pCommonProps );
	return true;
}

// CParticleFrame::FillRPGStats: the 17 tracks of a plain source.
bool FillRPGStats( SParticleSourceData &particleSetup, const CTreeItem &root, SExportOutcome &outcome )
{
	if ( !FillCommon( particleSetup, root, outcome ) )
		return false;
	const CTreeItem *pSourceProps = RequireChild( root, ETIT_PARTICLE_SOURCE_PROP_ITEMS, 0, "Particle source props", outcome );
	if ( pSourceProps == nullptr )
		return false;
	particleSetup.szTextureName = ValueStr( *pSourceProps, 0 );
	particleSetup.nTextureDX = ValueInt( *pSourceProps, 1 );
	particleSetup.nTextureDY = ValueInt( *pSourceProps, 2 );
	if ( !FillTrack( particleSetup.trackBeginSpeed, *pSourceProps, ETIT_PARTICLE_GENERATE_SPEED_ITEM, "Speed", outcome )
	  || !FillTrack( particleSetup.trackBeginSpeedRandomizer, *pSourceProps, ETIT_PARTICLE_RAND_SPEED_ITEM, "Random speed", outcome )
	  || !FillTrack( particleSetup.trackGenerateArea, *pSourceProps, ETIT_PARTICLE_GENERATE_AREA_ITEM, "Area", outcome )
	  || !FillTrack( particleSetup.trackBeginAngleRandomizer, *pSourceProps, ETIT_PARTICLE_GENERATE_ANGLE_ITEM, "Angle", outcome, true )
	  || !FillTrack( particleSetup.trackDensity, *pSourceProps, ETIT_PARTICLE_GENERATE_DENSITY_ITEM, "Density", outcome )
	  || !FillTrack( particleSetup.trackLife, *pSourceProps, ETIT_PARTICLE_GENERATE_LIFE_ITEM, "Life", outcome )
	  || !FillTrack( particleSetup.trackLifeRandomizer, *pSourceProps, ETIT_PARTICLE_RAND_LIFE_ITEM, "Random life", outcome )
	  || !FillTrack( particleSetup.trackGenerateSpin, *pSourceProps, ETIT_PARTICLE_GENERATE_SPIN_ITEM, "Spin", outcome )
	  || !FillTrack( particleSetup.trackGenerateSpinRandomizer, *pSourceProps, ETIT_PARTICLE_GENERATE_RANDOM_SPIN_ITEM, "Random spin", outcome )
	  || !FillTrack( particleSetup.trackGenerateOpacity, *pSourceProps, ETIT_PARTICLE_GENERATE_OPACITY_ITEM, "Opacity", outcome ) )
		return false;

	const CTreeItem *pParticleProps = RequireChild( root, ETIT_PARTICLE_PROP_ITEMS, 0, "Particle props", outcome );
	if ( pParticleProps == nullptr )
		return false;
	return FillTrack( particleSetup.trackSpin, *pParticleProps, ETIT_PARTICLE_SPIN_ITEM, "Spin", outcome )
	    && FillTrack( particleSetup.trackWeight, *pParticleProps, ETIT_PARTICLE_WEIGHT_ITEM, "Weight", outcome )
	    && FillTrack( particleSetup.trackSpeed, *pParticleProps, ETIT_PARTICLE_SPEED_ITEM, "Speed", outcome )
	    && FillTrack( particleSetup.trackSpeedRnd, *pParticleProps, ETIT_PARTICLE_C_RANDOM_SPEED_ITEM, "Random speed", outcome )
	    && FillTrack( particleSetup.trackSize, *pParticleProps, ETIT_PARTICLE_SIZE_ITEM, "Size", outcome )
	    && FillTrack( particleSetup.trackOpacity, *pParticleProps, ETIT_PARTICLE_OPACITY_ITEM, "Opacity", outcome )
	    && FillTrack( particleSetup.trackTextureFrame, *pParticleProps, ETIT_PARTICLE_TEXTURE_FRAME_ITEM, "Texture frame", outcome );
}

// CParticleFrame::FillRPGStats2: the eight tracks of a complex source.
bool FillRPGStats2( SSmokinParticleSourceData &particleSetup, const CTreeItem &root, SExportOutcome &outcome )
{
	if ( !FillCommon( particleSetup, root, outcome ) )
		return false;
	const CTreeItem *pComplexSource = RequireChild( root, ETIT_PARTICLE_COMPLEX_SOURCE_ITEM, 0, "Particle complex source props", outcome );
	if ( pComplexSource == nullptr )
		return false;
	particleSetup.szParticleEffectName = ValueStr( *pComplexSource, 0 );
	if ( !FillTrack( particleSetup.trackBeginSpeed, *pComplexSource, ETIT_PARTICLE_GENERATE_SPEED_ITEM, "Speed", outcome )
	  || !FillTrack( particleSetup.trackBeginSpeedRandomizer, *pComplexSource, ETIT_PARTICLE_RAND_SPEED_ITEM, "Random speed", outcome )
	  || !FillTrack( particleSetup.trackGenerateArea, *pComplexSource, ETIT_PARTICLE_GENERATE_AREA_ITEM, "Area", outcome )
	  || !FillTrack( particleSetup.trackBeginAngleRandomizer, *pComplexSource, ETIT_PARTICLE_GENERATE_ANGLE_ITEM, "Angle", outcome, true )
	  || !FillTrack( particleSetup.trackDensity, *pComplexSource, ETIT_PARTICLE_GENERATE_DENSITY_ITEM, "Density", outcome ) )
		return false;

	const CTreeItem *pComplexProps = RequireChild( root, ETIT_PARTICLE_COMPLEX_ITEM, 0, "Particle complex props", outcome );
	if ( pComplexProps == nullptr )
		return false;
	return FillTrack( particleSetup.trackWeight, *pComplexProps, ETIT_PARTICLE_WEIGHT_ITEM, "Weight", outcome )
	    && FillTrack( particleSetup.trackSpeed, *pComplexProps, ETIT_PARTICLE_SPEED_ITEM, "Speed", outcome )
	    && FillTrack( particleSetup.trackSpeedRnd, *pComplexProps, ETIT_PARTICLE_C_RANDOM_SPEED_ITEM, "Random speed", outcome );
}

// A curve item's frames from the keys the file lists for its chunk. A chunk
// the file lacks leaves the item's default key. The angle track is stored in
// radians and shown in degrees.
void FramesFromRaw( CTreeItem &parent, int nType, const SParticleRawFile &raw, const char *pszChunk, bool bAngle = false )
{
	const auto found = raw.tracks.find( pszChunk );
	if ( found == raw.tracks.end() || found->second.empty() )
		return;
	for ( const auto &pChild : parent.GetChildren() )
	{
		if ( pChild->GetItemType() != nType )
			continue;
		CKeyFrameTreeItem *pCurve = dynamic_cast<CKeyFrameTreeItem *>( pChild.get() );
		if ( pCurve == nullptr )
			return;
		pCurve->framesList.clear();
		for ( const auto &key : found->second )
			pCurve->framesList.push_back( std::make_pair( key.first / 1000.0f, bAngle ? key.second / kAngleToRadian : key.second ) );
		return;
	}
}

CTreeItem *MutableChild( CTreeItem &parent, int nType )
{
	for ( const auto &pChild : parent.GetChildren() )
		if ( pChild->GetItemType() == nType )
			return pChild.get();
	return nullptr;
}

void SetSlot( CTreeItem *pItem, std::size_t nSlot, const CVariant &value )
{
	if ( pItem != nullptr && nSlot < pItem->MutableValues().size() )
		pItem->MutableValues()[nSlot].value = value;
}

const char *AreaTypeName( int nAreaType )
{
	switch ( nAreaType )
	{
		case PSA_TYPE_DISK:   return "disk";
		case PSA_TYPE_CIRCLE: return "circle";
		default:              return "square";
	}
}

// The basic info both structs fill (CParticleCommonPropsItem's Set* and the
// slots they write).
template <class TData>
void CommonToTree( const TData &stats, CTreeItem &root, const std::string &szName )
{
	CTreeItem *pCommon = MutableChild( root, ETIT_PARTICLE_COMMON_PROPS_ITEM );
	SetSlot( pCommon, 0, CVariant( szName ) );
	SetSlot( pCommon, 1, CVariant( int( stats.nLifeTime ) ) );
	SetSlot( pCommon, 3, CVariant( float( stats.fGravity ) ) );
	SetSlot( pCommon, 7, CVariant( float( stats.vDirection.x ) ) );
	SetSlot( pCommon, 8, CVariant( float( stats.vDirection.y ) ) );
	SetSlot( pCommon, 9, CVariant( float( stats.vDirection.z ) ) );
	SetSlot( pCommon, 10, CVariant( float( stats.vWind.x ) ) );
	SetSlot( pCommon, 11, CVariant( float( stats.vWind.y ) ) );
	SetSlot( pCommon, 12, CVariant( float( stats.vWind.z ) ) );
	SetSlot( pCommon, 13, CVariant( float( stats.fRadialWind ) ) );
	SetSlot( pCommon, 14, CVariant( std::string( AreaTypeName( stats.nAreaType ) ) ) );
}

}

bool IsComplexSource( const CTreeItem &root )
{
	const CTreeItem *pComplexSource = ComplexSourceItem( root );
	return pComplexSource != nullptr && !ValueStr( *pComplexSource, 0 ).empty();
}

bool ExportParticle( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_PARTICLE_ROOT_ITEM, "particle", outcome );
	if ( !pProject )
		return false;
	const bool bComplexSource = IsComplexSource( *pProject->root );

	// The two structs are shared resources: on the heap, as the engine reads them.
	CPtr<SParticleSourceData> pSimple;
	CPtr<SSmokinParticleSourceData> pComplex;
	if ( bComplexSource )
	{
		pComplex = new SSmokinParticleSourceData();
		if ( !FillRPGStats2( *pComplex, *pProject->root, outcome ) )
			return false;
	}
	else
	{
		pSimple = new SParticleSourceData();
		if ( !FillRPGStats( *pSimple, *pProject->root, outcome ) )
			return false;
	}
	const std::string szFile = StatsFileName( project, context, kParticleAddDir, true );
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		if ( bComplexSource )
			tree.Add( "KeyData", pComplex.GetPtr() );
		else
			tree.Add( "KeyData", pSimple.GetPtr() );
	}, outcome ) )
		return false;
	// The source the preview wraps in an effect: the file without its extension.
	std::string szName = szFile;
	const std::string::size_type nDot = szName.find_last_of( '.' );
	if ( nDot != std::string::npos && nDot > szName.find_last_of( '\\' ) + 1 )
		szName.resize( nDot );
	outcome.szObjectName = szName;
	return true;
}

namespace
{

// One raw key as the file spells it, for SParticleRawFile.
struct SRawKey
{
	float fValue = 0, fTime = 0;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "value", &fValue );
		saver.Add( "time", &fTime );
		return 0;
	}
};

struct SRawTrack
{
	std::vector<SRawKey> keys;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "keys", &keys );
		return 0;
	}
};

const char *const kRawChunks[] = { "lifeTime", "GenerateArea", "Density", "BeginSpeed", "BeginSpeedRandomizer", "GenerateAngel",
	"ParticleLifeTimeRandomizer", "GenerateSpin", "GenerateSpinRnd", "GenerateOpacity", "Spin", "Wight", "TextureFrame", "Size",
	"Opacity", "Speed", "SpeedRnd",
	// the complex source's own names for two of them
	"BeginAngleRandomizer", "Weight" };

}

int SParticleRawFile::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	for ( const char *pszChunk : kRawChunks )
	{
		SRawTrack track;
		saver.Add( pszChunk, &track );
		if ( track.keys.empty() )
			continue;
		auto &keys = tracks[pszChunk];
		for ( const SRawKey &key : track.keys )
			keys.push_back( std::make_pair( key.fTime, key.fValue ) );
	}
	CVec3 vDirection( 0, 0, 0 );
	saver.Add( "Direction", &vDirection );
	this->vDirection[0] = vDirection.x;
	this->vDirection[1] = vDirection.y;
	this->vDirection[2] = vDirection.z;
	bFoundDirection = true;
	return 0;
}

void ParticleStatsToTree( const SParticleSourceData &stats, const SParticleRawFile &raw, CTreeItem &root, const std::string &szName )
{
	CommonToTree( stats, root, szName );
	if ( CTreeItem *pCommon = MutableChild( root, ETIT_PARTICLE_COMMON_PROPS_ITEM ) )
	{
		SetSlot( pCommon, 7, CVariant( raw.vDirection[0] ) );
		SetSlot( pCommon, 8, CVariant( raw.vDirection[1] ) );
		SetSlot( pCommon, 9, CVariant( raw.vDirection[2] ) );
	}
	CTreeItem *pSourceProps = MutableChild( root, ETIT_PARTICLE_SOURCE_PROP_ITEMS );
	if ( pSourceProps != nullptr )
	{
		SetSlot( pSourceProps, 0, CVariant( stats.szTextureName ) );
		SetSlot( pSourceProps, 1, CVariant( int( stats.nTextureDX ) ) );
		SetSlot( pSourceProps, 2, CVariant( int( stats.nTextureDY ) ) );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_SPEED_ITEM, raw, "BeginSpeed" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_RAND_SPEED_ITEM, raw, "BeginSpeedRandomizer" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_AREA_ITEM, raw, "GenerateArea" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_ANGLE_ITEM, raw, "GenerateAngel", true );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_DENSITY_ITEM, raw, "Density" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_LIFE_ITEM, raw, "lifeTime" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_RAND_LIFE_ITEM, raw, "ParticleLifeTimeRandomizer" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_SPIN_ITEM, raw, "GenerateSpin" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_RANDOM_SPIN_ITEM, raw, "GenerateSpinRnd" );
		FramesFromRaw( *pSourceProps, ETIT_PARTICLE_GENERATE_OPACITY_ITEM, raw, "GenerateOpacity" );
	}
	CTreeItem *pParticleProps = MutableChild( root, ETIT_PARTICLE_PROP_ITEMS );
	if ( pParticleProps != nullptr )
	{
		FramesFromRaw( *pParticleProps, ETIT_PARTICLE_SPIN_ITEM, raw, "Spin" );
		FramesFromRaw( *pParticleProps, ETIT_PARTICLE_WEIGHT_ITEM, raw, "Wight" );
		FramesFromRaw( *pParticleProps, ETIT_PARTICLE_SPEED_ITEM, raw, "Speed" );
		FramesFromRaw( *pParticleProps, ETIT_PARTICLE_C_RANDOM_SPEED_ITEM, raw, "SpeedRnd" );
		FramesFromRaw( *pParticleProps, ETIT_PARTICLE_SIZE_ITEM, raw, "Size" );
		FramesFromRaw( *pParticleProps, ETIT_PARTICLE_OPACITY_ITEM, raw, "Opacity" );
		FramesFromRaw( *pParticleProps, ETIT_PARTICLE_TEXTURE_FRAME_ITEM, raw, "TextureFrame" );
	}
}

void ParticleStatsToTree( const SSmokinParticleSourceData &stats, const SParticleRawFile &raw, CTreeItem &root, const std::string &szName )
{
	CommonToTree( stats, root, szName );
	if ( CTreeItem *pCommon = MutableChild( root, ETIT_PARTICLE_COMMON_PROPS_ITEM ) )
	{
		SetSlot( pCommon, 7, CVariant( raw.vDirection[0] ) );
		SetSlot( pCommon, 8, CVariant( raw.vDirection[1] ) );
		SetSlot( pCommon, 9, CVariant( raw.vDirection[2] ) );
	}
	CTreeItem *pComplexSource = MutableChild( root, ETIT_PARTICLE_COMPLEX_SOURCE_ITEM );
	if ( pComplexSource != nullptr )
	{
		SetSlot( pComplexSource, 0, CVariant( stats.szParticleEffectName ) );
		FramesFromRaw( *pComplexSource, ETIT_PARTICLE_GENERATE_SPEED_ITEM, raw, "BeginSpeed" );
		FramesFromRaw( *pComplexSource, ETIT_PARTICLE_RAND_SPEED_ITEM, raw, "BeginSpeedRandomizer" );
		FramesFromRaw( *pComplexSource, ETIT_PARTICLE_GENERATE_AREA_ITEM, raw, "GenerateArea" );
		FramesFromRaw( *pComplexSource, ETIT_PARTICLE_GENERATE_ANGLE_ITEM, raw, "BeginAngleRandomizer", true );
		FramesFromRaw( *pComplexSource, ETIT_PARTICLE_GENERATE_DENSITY_ITEM, raw, "Density" );
	}
	CTreeItem *pComplexProps = MutableChild( root, ETIT_PARTICLE_COMPLEX_ITEM );
	if ( pComplexProps != nullptr )
	{
		FramesFromRaw( *pComplexProps, ETIT_PARTICLE_WEIGHT_ITEM, raw, "Weight" );
		FramesFromRaw( *pComplexProps, ETIT_PARTICLE_SPEED_ITEM, raw, "Speed" );
		FramesFromRaw( *pComplexProps, ETIT_PARTICLE_C_RANDOM_SPEED_ITEM, raw, "SpeedRnd" );
	}
}

}
