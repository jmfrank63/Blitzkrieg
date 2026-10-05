// The effect exporter: CEffectFrame::SaveRPGStats (Sources/src/editor/
// EffectFrm.cpp:170-250), line for line. An SEffectDesc is filled from the
// tree and written as the "effect" chunk through the engine's own operator&,
// so the file is what the game's effect manager reads. Only the sound, the
// Animations and the Function Particles are exported, as in MFC: its tree
// kept Meshes, Maya Particles and Lights commented out, so the port has no
// such items to write. The file root is <effect>, as MFC named it for an
// effect export (ParentFrame.cpp:1073) and as every shipped effect has it.
//
// Each Function Particle child names a source below effects\particles\. MFC
// opened that xml from the editor's data folder to read KeyData/
// ComplexParticleSource, which says whether the entry is a plain or a smokin
// particle. A source it could not open raised a message box and the entry
// was skipped, which in a batch export would silently write an effect
// without it; the port refuses the export naming the file instead. The
// source is looked up in the export root's data folder first and in the
// staging folder second, since a project exported in the same run may have
// just written it.
#include "StdAfx.h"

#include "effect_export.h"

#include "../stats_export.h"
#include "../tree_item_types.h"
#include "../../../Formats/fmtEffect.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

const char kEffectAddDir[] = "effects\\effects\\";

// SSourceType of EffectFrm.cpp:156: which of the two particle structs a
// KeyData chunk holds.
struct SSourceType
{
	bool bComplexParticleSource = false;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "ComplexParticleSource", &bComplexParticleSource );
		return 0;
	}
};

CVec3 Position( const CTreeItem &item, int nFirst )
{
	return CVec3( ValueFloat( item, nFirst ), ValueFloat( item, nFirst + 1 ), ValueFloat( item, nFirst + 2 ) );
}

// Reads KeyData/ComplexParticleSource of the particle source szName
// (effects\particles\<name>), or false when no file can be opened.
bool ReadSourceType( const std::string &szName, const SExportContext &context, SSourceType &sourceType, std::string &szTried )
{
	const std::string szRelative = ToSlashes( szName ) + ".xml";
	const std::string szRoots[] = { context.szDataRoot, context.szStagingRoot };
	for ( const std::string &szRoot : szRoots )
	{
		if ( szRoot.empty() )
			continue;
		const std::filesystem::path file = FoldedFile( ( std::filesystem::path( szRoot ) / szRelative ).string() );
		if ( szTried.empty() )
			szTried = file.string();
		CPtr<IDataStorage> pStorage = OpenStorage( ( file.parent_path().string() + "/" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
		CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( file.filename().string().c_str(), STREAM_ACCESS_READ ) : 0;
		if ( pStream == 0 )
			continue;
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
		if ( pDT == 0 )
			continue;
		CTreeAccessor tree = pDT;
		tree.Add( "KeyData", &sourceType );
		return true;
	}
	return false;
}

}

bool ExportEffect( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_EFFECT_ROOT_ITEM, "effect", outcome );
	if ( !pProject )
		return false;
	const CTreeItem *pCommon = RequireChild( *pProject->root, ETIT_EFFECT_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	const CTreeItem *pAnims = pCommon != nullptr ? RequireChild( *pProject->root, ETIT_EFFECT_ANIMATIONS_ITEM, 0, "Animations", outcome ) : nullptr;
	const CTreeItem *pFuncParticles = pAnims != nullptr ? RequireChild( *pProject->root, ETIT_EFFECT_FUNC_PARTICLES_ITEM, 0, "Function Particles", outcome ) : nullptr;
	if ( pFuncParticles == nullptr )
		return false;

	SEffectDesc effDesc;
	effDesc.szSound = ValueStr( *pCommon, 1 );

	for ( const auto &pChild : pAnims->GetChildren() )
	{
		SSpriteEffectDesc spriteEffect;
		spriteEffect.szPath = std::string( "Effects\\sprites\\" ) + pChild->GetDisplayName();
		spriteEffect.nStart = ValueInt( *pChild, 0 );
		spriteEffect.nRepeat = ValueInt( *pChild, 4 );
		spriteEffect.vPos = Position( *pChild, 1 );
		effDesc.sprites.push_back( spriteEffect );
	}

	for ( const auto &pChild : pFuncParticles->GetChildren() )
	{
		const std::string szName = std::string( "Effects\\particles\\" ) + pChild->GetDisplayName();
		SSourceType sourceType;
		std::string szTried;
		if ( !ReadSourceType( szName, context, sourceType, szTried ) )
		{
			outcome.szError = "the effect's function particle \"" + pChild->GetDisplayName() + "\" has no source file: cannot open " + szTried;
			return false;
		}
		if ( sourceType.bComplexParticleSource )
		{
			SSmokinParticleEffectDesc complexParticleEffect;
			complexParticleEffect.szPath = szName;
			complexParticleEffect.nStart = ValueInt( *pChild, 0 );
			complexParticleEffect.nDuration = ValueInt( *pChild, 1 );
			complexParticleEffect.vPos = Position( *pChild, 2 );
			complexParticleEffect.fScale = ValueFloat( *pChild, 5 );
			effDesc.smokinParticles.push_back( complexParticleEffect );
		}
		else
		{
			SParticleEffectDesc particleEffect;
			particleEffect.szPath = szName;
			particleEffect.nStart = ValueInt( *pChild, 0 );
			particleEffect.nDuration = ValueInt( *pChild, 1 );
			particleEffect.vPos = Position( *pChild, 2 );
			particleEffect.fScale = ValueFloat( *pChild, 5 );
			effDesc.particles.push_back( particleEffect );
		}
	}

	const std::string szFile = StatsFileName( project, context, kEffectAddDir, true );
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "effect", &effDesc );
	}, outcome, "effect" ) )
		return false;
	// The effect the preview plays: the file without its extension.
	std::string szObject = szFile;
	const std::string::size_type nDot = szObject.find_last_of( '.' );
	if ( nDot != std::string::npos && nDot > szObject.find_last_of( '\\' ) + 1 )
		szObject.resize( nDot );
	outcome.szObjectName = szObject;
	return true;
}

}
