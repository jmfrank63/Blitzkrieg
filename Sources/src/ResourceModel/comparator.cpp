#include "StdAfx.h"

#include "comparator.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

#include "../Main/iMain.h"
#include "../Main/RPGStats.h"
#include "../Main/GameStats.h"
#include "../Formats/fmtEffect.h"
#include "../Formats/fmtTerrain.h"
#include "../Formats/fmtVSO.h"
#include "../Scene/ParticleSourceData.h"
#include "../Scene/SmokinParticleSourceData.h"
#include "../Scene/PFX.h"
#include "xml.h"

namespace NResourceModel
{

namespace
{

const SExportKindInfo kKinds[] = {
	{ EExportKind::MECH_UNIT,    "SMechUnitRPGStats",       "ReadRPGStats<SMechUnitRPGStats>",      "RPG", "base" },
	{ EExportKind::INFANTRY,     "SInfantryRPGStats",       "ReadRPGStats<SInfantryRPGStats>",      "RPG", "base" },
	{ EExportKind::WEAPON,       "SWeaponRPGStats",         "GetAddStats<SWeaponRPGStats>",         "RPG", "base" },
	{ EExportKind::SQUAD,        "SSquadRPGStats",          "ReadRPGStats<SSquadRPGStats>",         "RPG", "base" },
	{ EExportKind::MINE,         "SMineRPGStats",           "ReadRPGStats<SMineRPGStats>",          "RPG", "base" },
	{ EExportKind::ENTRENCHMENT, "SEntrenchmentRPGStats",   "ReadRPGStats<SEntrenchmentRPGStats>",  "RPG", "base" },
	{ EExportKind::OBJECT,       "SObjectRPGStats",         "ReadRPGStats<SObjectRPGStats>",        "desc", "base" },
	{ EExportKind::FENCE,        "SFenceRPGStats",          "ReadRPGStats<SFenceRPGStats>",         "RPG", "base" },
	{ EExportKind::BUILDING,     "SBuildingRPGStats",       "ReadRPGStats<SBuildingRPGStats>",      "desc", "base" },
	{ EExportKind::BRIDGE,       "SBridgeRPGStats",         "ReadRPGStats<SBridgeRPGStats>",        "RPG", "base" },
	{ EExportKind::PARTICLE,     "SParticleSourceData",     "SParticleSourceData::Load",            "KeyData", "base" },
	{ EExportKind::EFFECT,       "SEffectDesc",             "fmtEffect SEffectDesc::operator&",     "effect", "effect" },
	{ EExportKind::TILESET,      "STilesetDesc",            "fmtTerrain STilesetDesc::operator&",   "tileset", "base" },
	{ EExportKind::CROSSET,      "SCrossetDesc",            "fmtTerrain SCrossetDesc::operator&",   "crosset", "base" },
	{ EExportKind::VSO,          "SVectorStripeObjectDesc", "fmtVSO SVectorStripeObjectDesc::operator&", "VSODescription", "base" },
	{ EExportKind::MISSION,      "SMissionStats",           "GetGameStats<SMissionStats>",          "RPG", "base" },
	{ EExportKind::CHAPTER,      "SChapterStats",           "GetGameStats<SChapterStats>",          "RPG", "base" },
	{ EExportKind::CAMPAIGN,     "SCampaignStats",          "GetGameStats<SCampaignStats>",         "RPG", "base" },
	{ EExportKind::MEDAL,        "SMedalStats",             "GetGameStats<SMedalStats>",            "RPG", "base" },
};

// A chunk on the way down: its name, and for a container the item the
// reader is on. Paths join them with '/', a container item as item[i], which
// is how EnumerateFile names the same node in the file.
struct SFrame
{
	std::string szName;
	bool bContainer = false;
	bool bIndexed = false;
	int nItem = -1;
	int nItems = 0;
	std::map<std::string, int> asked;
};

std::string PathOf( const std::vector<SFrame> &frames )
{
	std::string szPath;
	for ( const SFrame &frame : frames )
	{
		if ( !szPath.empty() )
			szPath += '/';
		szPath += frame.szName;
		if ( frame.bContainer && frame.nItem >= 0 )
			szPath += "/item[" + std::to_string( frame.nItem ) + "]";
	}
	return szPath;
}

std::string Join( const std::string &szPath, const char *pszChunk )
{
	return szPath.empty() ? std::string( pszChunk ) : szPath + "/" + pszChunk;
}

// Sits in front of the engine's CDataTreeXML while the struct reads, passes
// every call through unchanged and notes each chunk the reader found. The
// struct reads exactly what it would read without it.
//
// A name the reader asks for again under the same parent is noted as name#n,
// as EnumerateNode names the n-th element of that name: the struct's own
// writer wrote it n times (SBuildingRPGStats writes AmbientSound twice), so
// the file's repeats are accounted for. A container the reader enters but
// never indexes is noted in skipped: CTreeAccessor::Do2DArray reads nothing of
// an empty 2D array, whose one item only carries size_x and size_y.
class CVisitTree : public CTRefCount<IDataTree>
{
	CPtr<IDataTree> pTree;
	std::vector<SFrame> frames;
	std::map<std::string, int> rootAsked;

	std::string Ask( DTChunkID idChunk )
	{
		std::map<std::string, int> &asked = frames.empty() ? rootAsked : frames.back().asked;
		const int nTimes = ++asked[idChunk];
		return nTimes == 1 ? std::string( idChunk ) : std::string( idChunk ) + "#" + std::to_string( nTimes );
	}
public:
	std::vector<std::string> visited;
	std::vector<std::string> skipped;

	explicit CVisitTree( IDataTree *_pTree ) : pTree( _pTree ) {}

	virtual bool STDCALL IsReading() const { return true; }
	virtual int STDCALL StartChunk( DTChunkID idChunk )
	{
		const int nResult = pTree->StartChunk( idChunk );
		if ( nResult == 1 )
		{
			SFrame frame;
			frame.szName = Ask( idChunk );
			frames.push_back( frame );
			visited.push_back( PathOf( frames ) );
		}
		return nResult;
	}
	virtual void STDCALL FinishChunk()
	{
		pTree->FinishChunk();
		if ( !frames.empty() )
			frames.pop_back();
	}
	virtual int STDCALL GetChunkSize() { return pTree->GetChunkSize(); }
	virtual bool STDCALL RawData( void *pData, int nSize ) { return pTree->RawData( pData, nSize ); }
	virtual bool STDCALL StringData( char *pData ) { return pTree->StringData( pData ); }
	virtual bool STDCALL StringData( WORD *pData ) { return pTree->StringData( pData ); }
	virtual bool STDCALL DataChunk( DTChunkID idChunk, int *pData )
	{
		const bool bFound = pTree->DataChunk( idChunk, pData );
		if ( bFound )
			visited.push_back( Join( PathOf( frames ), Ask( idChunk ).c_str() ) );
		return bFound;
	}
	virtual bool STDCALL DataChunk( DTChunkID idChunk, double *pData )
	{
		const bool bFound = pTree->DataChunk( idChunk, pData );
		if ( bFound )
			visited.push_back( Join( PathOf( frames ), Ask( idChunk ).c_str() ) );
		return bFound;
	}
	virtual int STDCALL CountChunks( DTChunkID idChunk ) { return pTree->CountChunks( idChunk ); }
	virtual bool STDCALL SetChunkCounter( int nCount )
	{
		const bool bFound = pTree->SetChunkCounter( nCount );
		if ( !frames.empty() )
		{
			frames.back().nItem = nCount;
			frames.back().bIndexed = true;
			frames.back().asked.clear();
			if ( bFound )
				visited.push_back( PathOf( frames ) );
		}
		return bFound;
	}
	virtual int STDCALL StartContainerChunk( DTChunkID idChunk )
	{
		const int nResult = pTree->StartContainerChunk( idChunk );
		if ( nResult != 0 )
		{
			SFrame frame;
			frame.szName = idChunk[0] == '\0' ? "data" : Ask( idChunk );
			frame.bContainer = true;
			frames.push_back( frame );
			visited.push_back( PathOf( frames ) );
		}
		return nResult;
	}
	virtual void STDCALL FinishContainerChunk()
	{
		pTree->FinishContainerChunk();
		if ( frames.empty() )
			return;
		if ( frames.back().bContainer && !frames.back().bIndexed )
			skipped.push_back( PathOf( frames ) );
		frames.pop_back();
	}
};

std::string FloatText( double fValue )
{
	char buffer[96];
	const float fNarrow = static_cast<float>( fValue );
	if ( static_cast<double>( fNarrow ) == fValue )
	{
		DWORD dwBits = 0;
		std::memcpy( &dwBits, &fNarrow, sizeof( dwBits ) );
		std::snprintf( buffer, sizeof( buffer ), "%.9g (float 0x%08x)", fValue, static_cast<unsigned>( dwBits ) );
	}
	else
	{
		unsigned long long nBits = 0;
		std::memcpy( &nBits, &fValue, sizeof( nBits ) );
		std::snprintf( buffer, sizeof( buffer ), "%.17g (double 0x%016llx)", fValue, nBits );
	}
	return buffer;
}

// The struct as read, written back through its own operator& into a list of
// (path, value) pairs. Values are text, but floats carry their bits, so two
// values are equal exactly when the struct fields are.
class CRecordTree : public CTRefCount<IDataTree>
{
	std::vector<SFrame> frames;
public:
	std::vector<std::pair<std::string, std::string>> fields;

	virtual bool STDCALL IsReading() const { return false; }
	virtual int STDCALL StartChunk( DTChunkID idChunk )
	{
		if ( idChunk[0] == '\0' )
			return -1;
		SFrame frame;
		frame.szName = idChunk;
		frames.push_back( frame );
		return 1;
	}
	virtual void STDCALL FinishChunk()
	{
		if ( !frames.empty() )
			frames.pop_back();
	}
	virtual int STDCALL GetChunkSize() { return 0; }
	virtual bool STDCALL RawData( void *pData, int nSize )
	{
		static const char hex[] = "0123456789abcdef";
		std::string szValue = "raw ";
		const unsigned char *p = static_cast<const unsigned char *>( pData );
		for ( int i = 0; i < nSize; ++i )
		{
			szValue += hex[p[i] >> 4];
			szValue += hex[p[i] & 15];
		}
		fields.push_back( { PathOf( frames ), szValue } );
		return true;
	}
	virtual bool STDCALL StringData( char *pData )
	{
		fields.push_back( { PathOf( frames ), std::string( "\"" ) + pData + "\"" } );
		return true;
	}
	virtual bool STDCALL StringData( WORD *pData )
	{
		std::string szValue = "L\"";
		for ( const WORD *p = pData; *p != 0; ++p )
		{
			if ( *p < 0x80 )
				szValue += static_cast<char>( *p );
			else
			{
				char buffer[8];
				std::snprintf( buffer, sizeof( buffer ), "\\u%04x", static_cast<unsigned>( *p ) );
				szValue += buffer;
			}
		}
		fields.push_back( { PathOf( frames ), szValue + "\"" } );
		return true;
	}
	virtual bool STDCALL DataChunk( DTChunkID idChunk, int *pData )
	{
		fields.push_back( { Join( PathOf( frames ), idChunk ), std::to_string( *pData ) } );
		return true;
	}
	virtual bool STDCALL DataChunk( DTChunkID idChunk, double *pData )
	{
		fields.push_back( { Join( PathOf( frames ), idChunk ), FloatText( *pData ) } );
		return true;
	}
	virtual int STDCALL CountChunks( DTChunkID idChunk ) { return 0; }
	virtual bool STDCALL SetChunkCounter( int nCount )
	{
		if ( !frames.empty() )
		{
			frames.back().nItem = nCount;
			if ( nCount + 1 > frames.back().nItems )
				frames.back().nItems = nCount + 1;
		}
		return true;
	}
	virtual int STDCALL StartContainerChunk( DTChunkID idChunk )
	{
		SFrame frame;
		frame.szName = idChunk[0] == '\0' ? "data" : idChunk;
		frame.bContainer = true;
		frames.push_back( frame );
		return 1;
	}
	virtual void STDCALL FinishContainerChunk()
	{
		if ( frames.empty() )
			return;
		// The item count is a field too: an empty container and a missing
		// one read the same, a shorter one does not.
		SFrame frame = frames.back();
		frame.nItem = -1;
		frames.back() = frame;
		fields.push_back( { PathOf( frames ) + "#count", std::to_string( frame.nItems ) } );
		frames.pop_back();
	}
};

// Every node of the file under <base>, named the way the visit tree names
// what the reader found: attributes and child elements alike as parent/name
// (CDataTreeXML::GetTextNode takes either), container items as item[i] in
// document order, and a repeated name after the first as name#n, which no
// reader can ask for.
void EnumerateNode( const NResourceXml::Node &node, const std::string &szPath, std::vector<std::string> &out )
{
	for ( const auto &attr : node.attrs )
		out.push_back( Join( szPath, attr.first.c_str() ) );
	std::map<std::string, int> seen;
	for ( const NResourceXml::Node &child : node.children )
	{
		if ( child.kind != NResourceXml::Node::Element )
			continue;
		const int nIndex = seen[child.name]++;
		std::string szName = child.name;
		if ( child.name == "item" )
			szName = "item[" + std::to_string( nIndex ) + "]";
		else if ( nIndex > 0 )
			szName += "#" + std::to_string( nIndex + 1 );
		const std::string szChild = Join( szPath, szName.c_str() );
		out.push_back( szChild );
		EnumerateNode( child, szChild, out );
	}
}

bool ReadFileBytes( const std::string &szPath, std::string *pBytes )
{
	std::ifstream file( szPath.c_str(), std::ios::binary );
	if ( !file )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

IDataStream *OpenForRead( const std::string &szFile )
{
	const std::string::size_type nCut = szFile.find_last_of( "/\\" );
	const std::string szDir = nCut == std::string::npos ? std::string( "./" ) : szFile.substr( 0, nCut + 1 );
	const std::string szName = nCut == std::string::npos ? szFile : szFile.substr( nCut + 1 );
	CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( pStorage == 0 )
		return 0;
	return pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
}

// SSourceType of ParticleFrm.cpp: which of the two particle structs a
// KeyData chunk holds.
struct SParticleKind
{
	bool bComplexParticleSource = false;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "ComplexParticleSource", &bComplexParticleSource );
		return 0;
	}
};

template <class TYPE>
void ReadInto( CVisitTree *pVisit, const char *pszRoot, SExportRead *pRead )
{
	TYPE stats;
	{
		CTreeAccessor tree = pVisit;
		tree.Add( pszRoot, &stats );
	}
	CPtr<CRecordTree> pRecord = new CRecordTree();
	{
		CTreeAccessor tree = static_cast<IDataTree *>( pRecord );
		tree.Add( pszRoot, &stats );
	}
	pRead->fields = pRecord->fields;
}

// The same with the struct on the heap, for the reference-counted shared
// resources, which must not live on the stack.
template <class TYPE>
void ReadIntoShared( CVisitTree *pVisit, const char *pszRoot, SExportRead *pRead )
{
	CPtr<TYPE> pStats = new TYPE();
	{
		CTreeAccessor tree = pVisit;
		tree.Add( pszRoot, pStats.GetPtr() );
	}
	CPtr<CRecordTree> pRecord = new CRecordTree();
	{
		CTreeAccessor tree = static_cast<IDataTree *>( pRecord );
		tree.Add( pszRoot, pStats.GetPtr() );
	}
	pRead->fields = pRecord->fields;
}

// SSmokinParticleSourceData::operator& ends its read with InitIntegrals,
// which asks the particle manager for the referenced key-based source only to
// cache its optimal update time (nUpdateStep). That value is runtime state the
// struct never writes, so it takes no part in the comparison. The game
// registers the Scene module's manager, which needs textures; a data-only
// host registers this one instead, whose source answers that one question.
class CReaderParticleSource : public CTRefCount<IParticleSource>
{
public:
	IGFXTexture* STDCALL GetTexture() const { return 0; }
	const int STDCALL GetNumParticles() const { return 0; }
	void STDCALL FillParticleBuffer( SSimpleParticle * ) const {}
	const CVec3 STDCALL GetPos() const { return CVec3( 0, 0, 0 ); }
	void STDCALL SetPos( const CVec3 & ) {}
	const CVec3 STDCALL GetDirection() const { return CVec3( 0, 0, 1 ); }
	void STDCALL SetDirection( const SHMatrix & ) {}
	void STDCALL SetScale( float ) {}
	void STDCALL Update( const NTimer::STime & ) {}
	void STDCALL SetStartTime( const NTimer::STime & ) {}
	const NTimer::STime STDCALL GetStartTime() const { return 0; }
	const NTimer::STime STDCALL GetEffectLifeTime() const { return 0; }
	bool STDCALL IsFinished() const { return true; }
	float STDCALL GetArea() const { return 0; }
	void STDCALL Stop() {}
	int STDCALL GetOptimalUpdateTime() const { return 0; }
	void STDCALL SetSuspendedState( bool ) {}
};

class CReaderParticleManager : public CTRefCount<IParticleManager>
{
public:
	bool STDCALL Init() { return true; }
	void STDCALL SetSerialMode( ESharedDataSerialMode ) {}
	void STDCALL SetShareMode( ESharedDataSharingMode ) {}
	void STDCALL Clear( const EClearMode, const int, const int ) {}
	IParticleSource* STDCALL GetKeyBasedSource( const char * ) { return new CReaderParticleSource(); }
	IParticleSource* STDCALL GetSmokinParticleSource( const char * ) { return new CReaderParticleSource(); }
	void STDCALL SetQuality( const float ) {}
};

// Nodes the shipped Data carries that no current reader reads: an older
// exporter wrote them, and its successor and the struct moved on. Each was
// found by comparing every shipped file with itself (test-resource-model-
// comparator). They are not unknown fields, but are not ignored either: a
// stale node on one side only is a difference. A path ending in '/' covers
// the node's descendants only; item[*] is any container item.
struct SStaleField
{
	EExportKind kind;
	const char *pszPath;
	const char *pszWhy;
};
const SStaleField kStaleFields[] = {
	{ EExportKind::OBJECT, "desc/EffectExplosion/",
	  "an older SStaticObjectRPGStats wrote the effect as a struct (Effect, Sound, MinDist, MaxDist); the struct now reads the element's text" },
	{ EExportKind::OBJECT, "desc/EffectDeath/", "as desc/EffectExplosion/" },
	{ EExportKind::EFFECT, "effect/sound/",
	  "an older effect export wrote the sound as a struct (Name, MinDist, MaxDist), as in Effects/shell.xml; SEffectDesc reads the element's text" },
	{ EExportKind::PARTICLE, "KeyData/Position", "an older particle export; SParticleSourceData reads no position" },
	{ EExportKind::PARTICLE, "KeyData/GenerateSpinRand", "the older name of GenerateSpinRnd, which SParticleSourceData reads" },
	{ EExportKind::MECH_UNIT, "RPG/Acks", "acknowledgements inline, before SUnitBaseRPGStats read them by reference (AcksRef)" },
	{ EExportKind::MISSION, "RPG/BGImage", "a 2002 mission export (ScenarioMissions/german/africa) from before SMissionStats dropped it" },
	{ EExportKind::MISSION, "RPG/Script", "as RPG/BGImage" },
};

// A node path with every container index replaced by item[*].
std::string GenericPath( const std::string &szNode )
{
	std::string szGeneric;
	for ( size_t i = 0; i < szNode.size(); )
	{
		if ( szNode.compare( i, 5, "item[" ) == 0 && szNode.find( ']', i ) != std::string::npos )
		{
			szGeneric += "item[*]";
			i = szNode.find( ']', i ) + 1;
		}
		else
			szGeneric += szNode[i++];
	}
	return szGeneric;
}

// Whether szPath (as GenericPath made it) is the node szPattern names: the
// node itself and what is below it, or, for a pattern ending in '/', only
// what is below it.
bool PathMatches( const std::string &szGeneric, const std::string &szPath )
{
	if ( szPath.back() == '/' )
		return szGeneric.compare( 0, szPath.size(), szPath ) == 0 && szGeneric.size() > szPath.size();
	return szGeneric == szPath || szGeneric.compare( 0, szPath.size() + 1, szPath + "/" ) == 0;
}

bool IsStale( EExportKind kind, const std::string &szNode )
{
	const std::string szGeneric = GenericPath( szNode );
	for ( const SStaleField &stale : kStaleFields )
		if ( stale.kind == kind && PathMatches( szGeneric, stale.pszPath ) )
			return true;
	return false;
}

bool UnderSkipped( const std::vector<std::string> &skipped, const std::string &szNode )
{
	for ( const std::string &szContainer : skipped )
		if ( szNode.compare( 0, szContainer.size() + 6, szContainer + "/item[" ) == 0 )
			return true;
	return false;
}

void Unique( std::vector<std::string> &values )
{
	std::set<std::string> seen;
	std::vector<std::string> out;
	for ( const std::string &value : values )
		if ( seen.insert( value ).second )
			out.push_back( value );
	values.swap( out );
}

SCompareResult Unreadable( const std::string &szWhat )
{
	SCompareResult result;
	result.status = ECompareStatus::UNREADABLE;
	result.messages.push_back( szWhat );
	return result;
}

}

const SExportKindInfo &GetExportKindInfo( EExportKind kind )
{
	for ( const SExportKindInfo &info : kKinds )
		if ( info.kind == kind )
			return info;
	return kKinds[0];
}

std::vector<EExportKind> AllExportKinds()
{
	std::vector<EExportKind> kinds;
	for ( const SExportKindInfo &info : kKinds )
		kinds.push_back( info.kind );
	return kinds;
}

const char *CompareStatusName( ECompareStatus status )
{
	switch ( status )
	{
		case ECompareStatus::EQUAL:            return "EQUAL";
		case ECompareStatus::DIFFERENT:        return "DIFFERENT";
		case ECompareStatus::UNKNOWN_FIELD:    return "UNKNOWN_FIELD";
		case ECompareStatus::UNREADABLE:       return "UNREADABLE";
	}
	return "?";
}

bool StartEngineReaders( std::string *pszError )
{
	NMain::EnsureGlobalHooks();
	if ( GetSLS() == 0 || GetSingletonGlobal() == 0 )
	{
		if ( pszError )
			*pszError = "the engine's StreamIO module did not load beside the executable";
		return false;
	}
	if ( GetSingleton<IParticleManager>() == 0 )
		RegisterSingleton( IParticleManager::tidTypeID, new CReaderParticleManager() );
	return true;
}

SExportRead ReadExport( EExportKind kind, const std::string &szFile )
{
	SExportRead read;
	std::string szBytes;
	if ( !ReadFileBytes( szFile, &szBytes ) )
	{
		read.szError = "cannot open " + szFile;
		return read;
	}
	NResourceXml::Document doc;
	std::string szParseError;
	if ( !NResourceXml::Parse( szBytes, doc, szParseError ) )
	{
		read.szError = "not XML: " + szFile + ": " + szParseError;
		return read;
	}
	const SExportKindInfo &info = GetExportKindInfo( kind );
	// The shipped editor's batch export (CParentFrame::ExportSingleFile) opens every kind's tree as <base>,
	// an effect included, while its File menu export and the shipped effect data use <effect>. Both name the
	// same <effect> chunk, so a golden made by batch mode is read under the root it has.
	const bool bBatchEffectRoot = kind == EExportKind::EFFECT && doc.root.name == "base" && NResourceXml::FindChild( doc.root, "effect" ) != 0;
	if ( doc.root.name != info.pszBase && !bBatchEffectRoot )
	{
		read.szError = "root element is <" + doc.root.name + ">, " + info.pszReader + " opens <" + info.pszBase + ">: " + szFile;
		return read;
	}
	if ( !NResourceXml::FindChild( doc.root, info.pszRoot ) )
	{
		read.szError = std::string( "no <" ) + info.pszRoot + "> under <" + info.pszBase + ">, which " + info.pszReader + " reads: " + szFile;
		return read;
	}

	CPtr<IDataStream> pStream = OpenForRead( szFile );
	if ( pStream == 0 )
	{
		read.szError = "the engine's storage cannot open " + szFile;
		return read;
	}
	CPtr<IDataTree> pTree = CreateDataTreeSaver( pStream, IDataTree::READ, doc.root.name.c_str() );
	if ( pTree == 0 )
	{
		read.szError = "the engine's CDataTreeXML cannot parse " + szFile;
		return read;
	}
	CPtr<CVisitTree> pVisit = new CVisitTree( pTree );
	read.szVariant = info.pszName;
	switch ( kind )
	{
		case EExportKind::MECH_UNIT:    ReadInto<SMechUnitRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::INFANTRY:     ReadInto<SInfantryRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::WEAPON:       ReadInto<SWeaponRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::SQUAD:        ReadInto<SSquadRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::MINE:         ReadInto<SMineRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::ENTRENCHMENT: ReadInto<SEntrenchmentRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::OBJECT:       ReadInto<SObjectRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::FENCE:        ReadInto<SFenceRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::BUILDING:     ReadInto<SBuildingRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::BRIDGE:       ReadInto<SBridgeRPGStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::EFFECT:       ReadInto<SEffectDesc>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::TILESET:      ReadInto<STilesetDesc>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::CROSSET:      ReadInto<SCrossetDesc>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::VSO:          ReadInto<SVectorStripeObjectDesc>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::MISSION:      ReadInto<SMissionStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::CHAPTER:      ReadInto<SChapterStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::CAMPAIGN:     ReadInto<SCampaignStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::MEDAL:        ReadInto<SMedalStats>( pVisit, info.pszRoot, &read ); break;
		case EExportKind::PARTICLE:
		{
			// The particle editor's own test (CParticleFrame::LoadRPGStats):
			// KeyData/ComplexParticleSource picks the struct.
			SParticleKind particleKind;
			{
				CTreeAccessor tree = pTree.GetPtr();
				tree.Add( info.pszRoot, &particleKind );
			}
			if ( particleKind.bComplexParticleSource )
			{
				read.szVariant = "SSmokinParticleSourceData";
				ReadIntoShared<SSmokinParticleSourceData>( pVisit, info.pszRoot, &read );
			}
			else
				ReadIntoShared<SParticleSourceData>( pVisit, info.pszRoot, &read );
			break;
		}
	}
	read.present = pVisit->visited;
	Unique( read.present );

	// MFC's export starts every file with <History>, the transaction log
	// CParentFrame::SaveTransactions writes: who exported what, when. No game
	// reader opens it and its dates differ on every export, so it is neither
	// an unknown field nor compared.
	NResourceXml::Node gameData = doc.root;
	gameData.attrs.clear();
	gameData.children.clear();
	for ( const NResourceXml::Node &child : doc.root.children )
		if ( !( child.kind == NResourceXml::Node::Element && child.name == "History" ) )
			gameData.children.push_back( child );
	gameData.attrs = doc.root.attrs;
	std::vector<std::string> nodes;
	EnumerateNode( gameData, "", nodes );
	const std::set<std::string> visited( read.present.begin(), read.present.end() );
	for ( const std::string &szNode : nodes )
	{
		if ( visited.find( szNode ) != visited.end() || UnderSkipped( pVisit->skipped, szNode ) )
			continue;
		if ( IsStale( kind, szNode ) )
			read.stale.push_back( szNode );
		else
			read.unknown.push_back( szNode );
	}
	read.bReadable = true;
	return read;
}

SCompareResult CompareStats( EExportKind kind, const std::string &szPortFile, const std::string &szGoldenFile )
{
	const SExportRead port = ReadExport( kind, szPortFile );
	if ( !port.bReadable )
		return Unreadable( "port: " + port.szError );
	const SExportRead golden = ReadExport( kind, szGoldenFile );
	if ( !golden.bReadable )
		return Unreadable( "golden: " + golden.szError );

	SCompareResult result;
	for ( const std::string &szNode : port.unknown )
		result.messages.push_back( "UNKNOWN FIELD port " + szNode + ": " + GetExportKindInfo( kind ).pszReader + " does not read it" );
	for ( const std::string &szNode : golden.unknown )
		result.messages.push_back( "UNKNOWN FIELD golden " + szNode + ": " + GetExportKindInfo( kind ).pszReader + " does not read it" );
	if ( !result.messages.empty() )
	{
		// Loud: a field nobody reads is either port drift the game cannot see or
		// an MFC field this comparator was never taught; both stop the run.
		for ( const std::string &szMessage : result.messages )
			std::fprintf( stderr, "%s\n", szMessage.c_str() );
		result.status = ECompareStatus::UNKNOWN_FIELD;
	}
	if ( port.szVariant != golden.szVariant )
		result.messages.push_back( "struct: port reads as " + port.szVariant + ", golden as " + golden.szVariant );

	result.nStale = static_cast<int>( port.stale.size() );
	const std::set<std::string> portStale( port.stale.begin(), port.stale.end() ), goldenStale( golden.stale.begin(), golden.stale.end() );
	for ( const std::string &szNode : golden.stale )
		if ( portStale.find( szNode ) == portStale.end() )
			result.messages.push_back( "stale field " + szNode + ": in the golden, not in the port export" );
	for ( const std::string &szNode : port.stale )
		if ( goldenStale.find( szNode ) == goldenStale.end() )
			result.messages.push_back( "stale field " + szNode + ": in the port export, not in the golden" );

	const std::set<std::string> portPresent( port.present.begin(), port.present.end() );
	const std::set<std::string> goldenPresent( golden.present.begin(), golden.present.end() );
	for ( const std::string &szPath : golden.present )
		if ( portPresent.find( szPath ) == portPresent.end() )
			result.messages.push_back( "dropped field " + szPath + ": in the golden, not in the port export" );
	for ( const std::string &szPath : port.present )
		if ( goldenPresent.find( szPath ) == goldenPresent.end() )
			result.messages.push_back( "extra field " + szPath + ": in the port export, not in the golden" );

	std::map<std::string, std::string> goldenValues( golden.fields.begin(), golden.fields.end() );
	std::set<std::string> compared;
	for ( const auto &field : port.fields )
	{
		if ( !compared.insert( field.first ).second )
			continue;
		const auto pos = goldenValues.find( field.first );
		if ( pos == goldenValues.end() )
			result.messages.push_back( "field " + field.first + ": port " + field.second + ", golden has no such field" );
		else if ( pos->second != field.second )
			result.messages.push_back( "field " + field.first + ": port " + field.second + ", golden " + pos->second );
	}
	for ( const auto &field : golden.fields )
		if ( compared.insert( field.first ).second )
			result.messages.push_back( "field " + field.first + ": golden " + field.second + ", port has no such field" );
	result.nFieldsCompared = static_cast<int>( compared.size() );
	if ( result.status == ECompareStatus::EQUAL && !result.messages.empty() )
		result.status = ECompareStatus::DIFFERENT;
	return result;
}

// What MFC's own import and export lose, field by field, between a shipped
// runtime stats file and the same stats after BkResImportFromGame and
// BkResExport (D-13). Each is a limit of the MFC frame the port ports line
// for line, not of the port: the project tree has no place for the field, so
// the export writes the struct's default. The path is the one a "field"
// message names; a path ending in '/' covers what is below it. Nothing is
// listed here for a field the tree does hold.
struct SRoundTripLoss
{
	EExportKind kind;
	const char *pszPath;
	const char *pszWhy;
};
const SRoundTripLoss kRoundTripLosses[] = {
	{ EExportKind::PARTICLE, "KeyData/Position", "an older particle export wrote a position the struct never reads and no project item holds, so an imported source comes back without it" },
	{ EExportKind::PARTICLE, "KeyData/Position/", "an older particle export wrote a position the struct never reads and no project item holds, so an imported source comes back without it" },
	{ EExportKind::PARTICLE, "KeyData/GenerateSpinRand", "an older particle export spelled GenerateSpinRnd this way; the struct reads only the new name and the project holds that one" },
	{ EExportKind::PARTICLE, "KeyData/GenerateSpinRand/", "an older particle export spelled GenerateSpinRnd this way; the struct reads only the new name and the project holds that one" },
	{ EExportKind::PARTICLE, "KeyData/BeginSpeedRandomizer", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/BeginSpeedRandomizer/", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/SpeedRnd", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/SpeedRnd/", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/ParticleLifeTimeRandomizer", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/ParticleLifeTimeRandomizer/", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/GenerateSpinRnd", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/GenerateSpinRnd/", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/TextureFrame", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/TextureFrame/", "the file omits this track and SParticleSourceData::Init gives the struct a zero track; the project holds the same zero as the curve\'s default key, so the export writes the track the file left out" },
	{ EExportKind::PARTICLE, "KeyData/AreaType", "an older particle export omitted this attribute and the struct reads its default (0); the project holds that value and the export writes the attribute" },
	{ EExportKind::PARTICLE, "KeyData/RadialWind", "an older particle export omitted this attribute and the struct reads its default (0); the project holds that value and the export writes the attribute" },
	{ EExportKind::PARTICLE, "KeyData/ComplexParticleSource", "an older particle export omitted this attribute and the struct reads its default (0); the project holds that value and the export writes the attribute" },
	{ EExportKind::ENTRENCHMENT, "RPG/Segments", "CTrenchFrame::LoadRPGStats (TrenchFrm.cpp:363) never read the segments back: a segment is a model file the project points at, which a runtime stats file does not say" },
	{ EExportKind::ENTRENCHMENT, "RPG/Segments#count", "CTrenchFrame::LoadRPGStats (TrenchFrm.cpp:363) never read the segments back: a segment is a model file the project points at, which a runtime stats file does not say" },
	{ EExportKind::ENTRENCHMENT, "RPG/Lines", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::ENTRENCHMENT, "RPG/Lines#count", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::ENTRENCHMENT, "RPG/FirePlaces", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::ENTRENCHMENT, "RPG/FirePlaces#count", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::ENTRENCHMENT, "RPG/Terminators", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::ENTRENCHMENT, "RPG/Terminators#count", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::ENTRENCHMENT, "RPG/Arcs", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::ENTRENCHMENT, "RPG/Arcs#count", "derived from the segments the import cannot read back (see RPG/Segments)" },
	{ EExportKind::SQUAD, "RPG/Formations/item[*]/Order/item[*]/Pos", "CSquadFrame::SaveRPGStats rebuilds each slot from the zero point with float arithmetic, so an imported slot differs from the shipped one by a few 1e-5" },
	{ EExportKind::SQUAD, "RPG/Formations/item[*]/Order/item[*]/Dir", "CSquadFrame::SaveRPGStats rebuilds each slot from the zero point with float arithmetic, so an imported slot differs from the shipped one by a few 1e-6" },
	{ EExportKind::FENCE, "RPG/Stats/item[*]/Origin", "a segment's origin is the sprite position minus its grid's corner, and the project stores the sprite position with six digits, so an imported origin differs from the shipped one by up to 5e-4" },
	{ EExportKind::FENCE, "RPG/Stats/item[*]/VisOrigin", "a segment's origin is the sprite position minus its grid's corner, and the project stores the sprite position with six digits, so an imported origin differs from the shipped one by up to 5e-4" },
	{ EExportKind::INFANTRY, "RPG/Commands", "CUnitActionsItem keeps only the actions of the editor's Actions list, so a shipped unit whose Commands bits lie beyond it comes back with fewer" },
	{ EExportKind::INFANTRY, "RPG/Commands/Size", "CUnitActionsItem keeps only the actions of the editor's Actions list, so a shipped unit whose Commands bits lie beyond it comes back with fewer" },
	{ EExportKind::INFANTRY, "RPG/Commands/BitArray", "CUnitActionsItem keeps only the actions of the editor's Actions list, so a shipped unit whose Commands bits lie beyond it comes back with fewer" },
	{ EExportKind::INFANTRY, "RPG/Commands/BitArray#count", "CUnitActionsItem keeps only the actions of the editor's Actions list, so a shipped unit whose Commands bits lie beyond it comes back with fewer" },
	{ EExportKind::INFANTRY, "RPG/AnimDescs/item[*]/data/item[*]/Length", "CUnitAnimationItem::FillRPGStats computes Length from the frame count and frame time; a stats-only import has no frames, so it is 0" },
	{ EExportKind::INFANTRY, "RPG/RotateSpeed", "FillRPGStats writes a constant (fRotateSpeed = 0)" },
	{ EExportKind::INFANTRY, "RPG/Priority", "FillRPGStats writes a constant (nPriority = 0)" },
	{ EExportKind::INFANTRY, "RPG/UninstallRotate", "FillRPGStats writes a constant (nUninstallRotate = 0)" },
	{ EExportKind::INFANTRY, "RPG/UninstallTransport", "FillRPGStats writes a constant (nUninstallTransport = 0)" },
	{ EExportKind::INFANTRY, "RPG/AnimDescs/item[*]/data/item[*]/AABB_A", "FillRPGStats writes a constant (nAABB_A = -1)" },
	{ EExportKind::INFANTRY, "RPG/AnimDescs/item[*]/data/item[*]/AABB_D", "FillRPGStats writes a constant (nAABB_D = -1)" },	{ EExportKind::VSO, "VSODescription/AIClasses", "FillRPGStats rebuilds the mask from the four passability flags and inverts it, so a file whose mask holds other bits (or none) comes back with the four flags and every higher bit set" },
	{ EExportKind::VSO, "VSODescription/Type", "C3DRiverFrame::FillRPGStats never sets the type and its GetRPGStats imports nothing, so a river comes back as TYPE_UNKNOUN; the road export sets it from the Road type item and the road tests read it back" },
	{ EExportKind::VSO, "VSODescription/Priority", "C3DRiverFrame::FillRPGStats never sets the priority, so a river comes back with 0; the road export writes it from Visual priority and the road tests read it back" },
	{ EExportKind::VSO, "VSODescription/SoilParams", "an older river file omits SoilParams and the struct reads its default (0); the export writes the node" },
	{ EExportKind::VSO, "VSODescription/Bottom/Disturbance", "FillRPGStats writes a constant (bottom.fDisturbance = 0)" },
	{ EExportKind::VSO, "VSODescription/Bottom/StreamSpeed", "FillRPGStats writes a constant (bottom.fStreamSpeed = 0)" },
	{ EExportKind::VSO, "VSODescription/Layers/item[*]/NumCells", "C3DRiverFrame::FillRPGStats gives every layer the bottom width as its cell count" },
	{ EExportKind::VSO, "VSODescription/Layers/item[*]/RelWidth", "FillRPGStats writes a constant (layer.fRelWidth = 1)" },
};

const SRoundTripLoss *FindRoundTripLoss( EExportKind kind, const std::string &szMessage )
{
	// "field <path>: ...", "dropped field <path>: ...", "extra field <path>: ..."
	const size_t nField = szMessage.find( "field " );
	if ( nField == std::string::npos )
		return nullptr;
	const size_t nStart = nField + 6;
	const std::string szPath = GenericPath( szMessage.substr( nStart, szMessage.find( ':', nStart ) - nStart ) );
	for ( const SRoundTripLoss &loss : kRoundTripLosses )
		if ( loss.kind == kind && PathMatches( szPath, loss.pszPath ) )
			return &loss;
	return nullptr;
}

SCompareResult CompareRoundTrip( EExportKind kind, const std::string &szExportedFile, const std::string &szShippedFile )
{
	SCompareResult result = CompareStats( kind, szExportedFile, szShippedFile );
	if ( result.status != ECompareStatus::DIFFERENT )
		return result;
	std::vector<std::string> remaining;
	for ( const std::string &szMessage : result.messages )
		if ( const SRoundTripLoss *pLoss = FindRoundTripLoss( kind, szMessage ) )
			result.excused.push_back( szMessage + " [" + pLoss->pszWhy + "]" );
		else
			remaining.push_back( szMessage );
	result.messages.swap( remaining );
	if ( result.messages.empty() )
		result.status = ECompareStatus::EQUAL;
	return result;
}

// What the MFC goldens (tools/zig/fixtures/resource_editor/*/golden, made by
// the shipped reseditor.exe in batch mode from the tracked fixtures) hold that
// the port deliberately or unavoidably does not reproduce. A path is the one a
// "field" message names; a path without a trailing '/' covers the node and
// what is below it. Every entry says why; the golden comparison lists each
// difference with that reason as pending, never as accepted: a cause that is
// not proven from MFC's source and the golden bytes waits for a regenerated
// golden, so none is silent and none counts as a pass. A difference marked
// proven has its cause shown from MFC's source and from a project the shipped
// editor itself saved, and is listed as accepted.
struct SGoldenDifference
{
	EExportKind kind;
	const char *pszPath;
	const char *pszWhy;
	bool bProven = false;
	// More than zero: the difference is the float rounding of the engine's GetPos2 and GetPos3 and is
	// accepted when the two floats are no further apart than this, whatever the cause says otherwise.
	float fNoise = 0;
};

// GetPos3 casts a ray through the screen point between the near and the far plane of the editor's
// projection (z from -600 to 1800, MainFrm.cpp) and meets the ground plane on it, so the coordinates
// it computes with reach 2500, where a float step is 2.4e-4 (6.1e-5 from 512 to 1024, 1.2e-4 up to
// 2048), and an origin is that result minus another one: a handful of roundings, at most 1e-3. The
// differences measured in the goldens are 1.6e-4 to 5.5e-4 for the grids' origins and 1.5e-5 for the
// explosion points at the origin, and the tiles and cells they come from are a whole 1 to 45 units
// apart, so a difference above the bound is a different value and stays a failure.
const float kEnginePositionNoise = 1e-3f;

#define BK_NO_DESC_BLOCK "CBuildingFrame::SaveRPGStats returns at once while no sprite is loaded or the project is new (BuildFrm.cpp:560), so a building the shipped editor saved has no cached <desc> block (mfc-new/buildingtest.bld has none) and its batch export's LoadRPGStats (BuildFrm.cpp:922) reads the struct's defaults: an empty KeyName, no rest or medical slots, and a MaxHP below 1 that SHPObjectRPGStats::operator& (RPGStats.cpp:215) resets to 100; the golden holds exactly those"

#define BK_ENGINE_NOISE "float rounding of the engine's GetPos2 / GetPos3 (see kEnginePositionNoise): the port computes the same value from the editor camera of the golden's maker (MfcEditorCamera) and the two floats differ by less than the bound"

const SGoldenDifference kGoldenDifferences[] = {
	{ EExportKind::BUILDING, "desc/KeyName", BK_NO_DESC_BLOCK, true },
	{ EExportKind::BUILDING, "desc/MaxHP", BK_NO_DESC_BLOCK, true },
	{ EExportKind::BUILDING, "desc/RestSlots", BK_NO_DESC_BLOCK, true },
	{ EExportKind::BUILDING, "desc/MedicalSlots", BK_NO_DESC_BLOCK, true },
	{ EExportKind::BUILDING, "desc/DirExplosions/", BK_ENGINE_NOISE " (BuildFrm.cpp: the explosion points pass GetPos2 and GetPos3 and come back as +-1.5e-5 where the port has exactly 0)", true, kEnginePositionNoise },
	{ EExportKind::OBJECT, "desc/origin", BK_ENGINE_NOISE " (CObjectFrame::SaveRPGStats, ObjectFrm.cpp:625: realZeroPos3 minus the GetPos3 of the grid's leftmost corner)", true, kEnginePositionNoise },
	{ EExportKind::OBJECT, "desc/VisOrigin", BK_ENGINE_NOISE " (CObjectFrame::SaveRPGStats, ObjectFrm.cpp:729)", true, kEnginePositionNoise },
	{ EExportKind::FENCE, "RPG/Stats/item[*]/Origin", BK_ENGINE_NOISE " (CFenceFrame::SaveRPGStats, FenceFrm.cpp:1009: the segment's origin is the sprite position minus GetPos3 of the grid's leftmost corner)", true, kEnginePositionNoise },
	{ EExportKind::FENCE, "RPG/Stats/item[*]/VisOrigin", BK_ENGINE_NOISE " (CFenceFrame::SaveRPGStats, FenceFrm.cpp:1070)", true, kEnginePositionNoise },
	{ EExportKind::BRIDGE, "RPG/Segments/item[*]/Origin", BK_ENGINE_NOISE " (CBridgeFrame::SaveRPGStats, BridgeFrm.cpp:1155: the origin is the sprite position minus GetPos3 of the center cross)", true, kEnginePositionNoise },
	{ EExportKind::BRIDGE, "RPG/Segments/item[*]/VisOrigin", BK_ENGINE_NOISE " (CBridgeFrame::SaveRPGStats, BridgeFrm.cpp:1155)", true, kEnginePositionNoise },
	{ EExportKind::BRIDGE, "RPG/FirePoints/", BK_ENGINE_NOISE " (CBridgeFrame::SaveRPGStats, BridgeFrm.cpp:1090: the fire point's position and picture position come back through GetPos2 and GetPos3 as +-1e-4 where the port has exactly 0)", true, kEnginePositionNoise },
	{ EExportKind::VSO, "VSODescription/SoilParams", "an upstream MFC bug, not the port's: C3DRoadCommonPropsItem::SetSoilParams (3dRoadTreeItem.h at 7f0f4be5c) reads 'nVal & ESP_DUST != 0x0', which C++ parses as nVal & (ESP_DUST != 0), that is nVal & 1; with ESP_DUST 0x10 and ESP_TRACE 0x01 MFC loads dust as false (and both flags true when the trace bit is set) on every project load (LoadRPGStats, GetRPGStats, SetSoilParams), so a project saved with SoilParams 16 exports 0 once reopened (verified by Johannes on win-home with six test files: a new project shows dust on and saves 16, the reopened one shows it off and exports 0). This repo fixed the precedence in aea2ebc4c; the port keeps the stored 16 by Johannes' decision (we are not taking over this bug, we are going to fix it)", true },
	{ EExportKind::BRIDGE, "RPG/SmokePoints/", BK_ENGINE_NOISE " (CBridgeFrame::SaveRPGStats, BridgeFrm.cpp:1115, the same round trip as the fire points)", true, kEnginePositionNoise },
};

#undef BK_ENGINE_NOISE
#undef BK_NO_DESC_BLOCK

// "field <path>: port <v> (float 0x<bits>), golden <v> (float 0x<bits>)": the
// golden's float is the port's float printed with six significant digits and
// read back, which is what MFC's XML writer does to every float (it formats a
// double with %lg, StreamIOLib/DataTreeXML.cpp:326) and the port's does not.
bool TwoFloatsOf( const std::string &szMessage, float *pfPort, float *pfGolden )
{
	const std::string szMark = "(float 0x";
	const size_t nPort = szMessage.find( szMark );
	if ( nPort == std::string::npos )
		return false;
	const size_t nGolden = szMessage.find( szMark, nPort + szMark.size() );
	if ( nGolden == std::string::npos )
		return false;
	auto bitsAt = [&]( size_t nAt ) { return static_cast<uint32_t>( std::strtoul( szMessage.c_str() + nAt + szMark.size(), nullptr, 16 ) ); };
	const uint32_t nPortBits = bitsAt( nPort ), nGoldenBits = bitsAt( nGolden );
	std::memcpy( pfPort, &nPortBits, sizeof( float ) );
	std::memcpy( pfGolden, &nGoldenBits, sizeof( float ) );
	return true;
}

bool IsMfcSixDigitFloat( const std::string &szMessage )
{
	float fPort, fGolden;
	if ( !TwoFloatsOf( szMessage, &fPort, &fGolden ) )
		return false;
	// A value that sits exactly between two six-digit decimals (0.001953125) is printed up by the shipped
	// editor's C runtime (the til golden holds 0.00195313) and to even by glibc (0.00195312), so the digit
	// string is tried for the value and for the value one step further from zero.
	const double fValue = fPort;
	const double fAway = std::nextafter( fValue, fValue < 0 ? -HUGE_VAL : HUGE_VAL );
	for ( const double fCandidate : { fValue, fAway } )
	{
		char szText[64];
		std::snprintf( szText, sizeof( szText ), "%lg", fCandidate );
		if ( static_cast<float>( std::strtod( szText, nullptr ) ) == fGolden )
			return true;
	}
	return false;
}

const SGoldenDifference *FindGoldenDifference( EExportKind kind, const std::string &szMessage )
{
	const size_t nField = szMessage.find( "field " );
	if ( nField == std::string::npos )
		return nullptr;
	const size_t nStart = nField + 6;
	const std::string szPath = GenericPath( szMessage.substr( nStart, szMessage.find( ':', nStart ) - nStart ) );
	for ( const SGoldenDifference &difference : kGoldenDifferences )
		if ( difference.kind == kind && PathMatches( szPath, difference.pszPath ) )
			return &difference;
	return nullptr;
}

SCompareResult CompareGolden( EExportKind kind, const std::string &szPortFile, const std::string &szGoldenFile )
{
	SCompareResult result = CompareStats( kind, szPortFile, szGoldenFile );
	if ( result.status != ECompareStatus::DIFFERENT )
		return result;
	std::vector<std::string> remaining;
	for ( const std::string &szMessage : result.messages )
		if ( const SGoldenDifference *pDifference = FindGoldenDifference( kind, szMessage ) )
		{
			float fPort = 0, fGolden = 0;
			if ( pDifference->fNoise > 0 && !( TwoFloatsOf( szMessage, &fPort, &fGolden ) && std::fabs( fPort - fGolden ) <= pDifference->fNoise ) )
				remaining.push_back( szMessage );
			else
				( pDifference->bProven ? result.excused : result.pending ).push_back( szMessage + " [" + pDifference->pszWhy + "]" );
		}
		else if ( IsMfcSixDigitFloat( szMessage ) )
			result.excused.push_back( szMessage + " [MFC's XML writer prints a float with six significant digits (%lg, DataTreeXML.cpp:326), the golden holds the port's value rounded that way]" );
		else
			remaining.push_back( szMessage );
	result.messages.swap( remaining );
	if ( result.messages.empty() )
		result.status = ECompareStatus::EQUAL;
	return result;
}

SCompareResult CompareBytes( const std::string &szPortFile, const std::string &szGoldenFile )
{
	std::string port, golden;
	if ( !ReadFileBytes( szPortFile, &port ) )
		return Unreadable( "port: cannot open " + szPortFile );
	if ( !ReadFileBytes( szGoldenFile, &golden ) )
		return Unreadable( "golden: cannot open " + szGoldenFile );
	SCompareResult result;
	result.nFieldsCompared = 1;
	if ( port == golden )
		return result;
	size_t nAt = 0;
	while ( nAt < port.size() && nAt < golden.size() && port[nAt] == golden[nAt] )
		++nAt;
	result.status = ECompareStatus::DIFFERENT;
	result.messages.push_back( "bytes differ at offset " + std::to_string( nAt ) + " (port " + std::to_string( port.size() ) +
	                           " bytes, golden " + std::to_string( golden.size() ) + ")" );
	return result;
}

SCompareResult CompareDxt( const std::string &szPortFile, const std::string &szGoldenFile, const SDxtTolerance &tolerance )
{
	std::string port, golden;
	if ( !ReadFileBytes( szPortFile, &port ) )
		return Unreadable( "port: cannot open " + szPortFile );
	if ( !ReadFileBytes( szGoldenFile, &golden ) )
		return Unreadable( "golden: cannot open " + szGoldenFile );
	// DDS_HEADER after the "DDS " magic: height at 12, width at 16, mip count
	// at 28, pixel format flags at 80 and FourCC at 84.
	auto field = []( const std::string &bytes, size_t nOffset ) -> unsigned {
		unsigned nValue = 0;
		for ( int i = 3; i >= 0; --i )
			nValue = ( nValue << 8 ) | static_cast<unsigned char>( bytes[nOffset + i] );
		return nValue;
	};
	for ( const std::string *pBytes : { &port, &golden } )
		if ( pBytes->size() < 128 || pBytes->compare( 0, 4, "DDS " ) != 0 )
			return Unreadable( std::string( pBytes == &port ? "port" : "golden" ) + ": not a DDS file" );
	SCompareResult result;
	static const struct { size_t nOffset; const char *pszName; } kHeader[] = {
		{ 12, "height" }, { 16, "width" }, { 28, "mip count" }, { 80, "pixel format flags" }, { 84, "FourCC" },
	};
	for ( const auto &header : kHeader )
	{
		++result.nFieldsCompared;
		if ( field( port, header.nOffset ) != field( golden, header.nOffset ) )
			result.messages.push_back( std::string( "DDS " ) + header.pszName + ": port " + std::to_string( field( port, header.nOffset ) ) +
			                           ", golden " + std::to_string( field( golden, header.nOffset ) ) );
	}
	if ( !result.messages.empty() )
	{
		result.status = ECompareStatus::DIFFERENT;
		return result;
	}
	if ( port == golden )
		return result;
	// The pixels differ: decode both and hold the deltas to the gate.
	SDdsImage portImage, goldenImage;
	std::string szError;
	if ( !DecodeDds( port, &portImage, &szError ) || !DecodeDds( golden, &goldenImage, &szError ) )
	{
		if ( portImage.szFourCC.empty() && goldenImage.szFourCC.empty() && szError.find( "uncompressed" ) != std::string::npos )
		{
			result.status = ECompareStatus::DIFFERENT;
			result.messages.push_back( "uncompressed DDS bytes differ; only DXT textures have a pixel tolerance" );
			return result;
		}
		return Unreadable( "DXT decode: " + szError );
	}
	const SDxtStats *pGate = tolerance.Find( portImage.szFourCC );
	if ( !tolerance.bLoaded )
		return Unreadable( "DXT pixels differ and no DXT tolerance is loaded (LoadDxtTolerance)" );
	if ( pGate == nullptr )
	{
		result.status = ECompareStatus::DIFFERENT;
		result.messages.push_back( "DXT pixels differ and dxt-tolerance.json has no gate for " + portImage.szFourCC );
		return result;
	}
	SDxtDelta delta;
	for ( size_t i = 0; i < portImage.mips.size(); ++i )
		delta.Add( static_cast<int>( i ), portImage.mips[i], goldenImage.mips[i] );
	++result.nFieldsCompared;
	const SDxtStats stats = delta.Stats();
	const struct { const char *pszName; int nValue, nGate; } kChecks[] = {
		{ "colour max delta", stats.nColourMax, pGate->nColourMax }, { "colour p99", stats.nColourP99, pGate->nColourP99 },
		{ "alpha max delta", stats.nAlphaMax, pGate->nAlphaMax }, { "alpha p99", stats.nAlphaP99, pGate->nAlphaP99 },
	};
	for ( const auto &check : kChecks )
		if ( check.nValue > check.nGate )
			result.messages.push_back( portImage.szFourCC + " " + check.pszName + " " + std::to_string( check.nValue ) + " exceeds the gate " +
			                           std::to_string( check.nGate ) );
	char szWorst[160];
	snprintf( szWorst, sizeof( szWorst ), "largest delta %d at mip %d pixel (%d,%d): port %08x, golden %08x", delta.nWorst, delta.nWorstMip,
	          delta.nWorstX, delta.nWorstY, delta.nWorstLeft, delta.nWorstRight );
	if ( !result.messages.empty() )
	{
		result.status = ECompareStatus::DIFFERENT;
		result.messages.push_back( szWorst );
		return result;
	}
	result.messages.push_back( "DXT pixels differ within the " + portImage.szFourCC + " gate: colour max " + std::to_string( stats.nColourMax ) +
	                           " p99 " + std::to_string( stats.nColourP99 ) + ", alpha max " + std::to_string( stats.nAlphaMax ) + " p99 " +
	                           std::to_string( stats.nAlphaP99 ) + "; " + szWorst );
	return result;
}

}
