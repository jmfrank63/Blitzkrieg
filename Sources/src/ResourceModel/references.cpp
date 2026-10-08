#include "references.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace NResourceModel
{

namespace
{

std::string Lower( std::string s )
{
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
	return s;
}

bool StartsWith( const std::string &s, const std::string &p ) { return s.compare( 0, p.size(), p ) == 0; }
bool EndsWith( const std::string &s, const std::string &x )
{
	return s.size() >= x.size() && s.compare( s.size() - x.size(), x.size(), x ) == 0;
}

// One data-driven rule: files under any of `dirs` (MFC backslash form, lower
// case) ending in `ext` give an entry with `ext` stripped, and `strip` leading
// characters of the directory removed when `keepDir` is false (MFC passed ""
// as the directory for most lists, which keeps the full relative path).
struct SRule
{
	EReferenceType type;
	std::vector<std::string> dirs;
	std::string ext;
	std::string stripDir;   // prefix removed from the stored string
};

const std::vector<SRule> &Rules()
{
	static const std::vector<SRule> r = {
		{ EReferenceType::E_ANIMATIONS_REF, { "effects\\sprites\\" }, "\\1.xml", "effects\\sprites\\" },
		{ EReferenceType::E_FUNC_PARTICLES_REF, { "effects\\particles\\" }, ".xml", "effects\\particles\\" },
		{ EReferenceType::E_EFFECTS_REF, { "effects\\effects\\" }, ".xml", "effects\\effects\\" },
		{ EReferenceType::E_WEAPONS_REF, { "weapons\\" }, ".xml", "weapons\\" },
		{ EReferenceType::E_TEMPLATE_MISSIONS_REF, { "scenarios\\templatemissions\\", "scenarios\\custom\\missions\\" }, ".xml", "" },
		{ EReferenceType::E_SETTING_REF, { "scenarios\\settings\\" }, ".xml", "" },
		{ EReferenceType::E_CRATER_REF, { "objects\\terraobjects\\shell_hole\\" }, ".xml", "" },
		{ EReferenceType::E_DEATHHOLE_REF, { "objects\\terraobjects\\death_hole\\" }, ".xml", "" },
		{ EReferenceType::E_MAP_REF, { "maps\\" }, ".xml", "maps\\" },
		{ EReferenceType::E_MAP_REF, { "maps\\" }, ".bzm", "maps\\" },
		{ EReferenceType::E_MUSIC_REF, { "music\\" }, ".ogg", "" },
		{ EReferenceType::E_MOVIE_REF, { "movies\\" }, ".xml", "" },
		{ EReferenceType::E_SCENARIO_MISSIONS_REF, { "scenarios\\scenariomissions\\", "scenarios\\custom\\missions\\", "scenarios\\tutorials\\" }, ".xml", "" },
		{ EReferenceType::E_CHAPTERS_REF, { "scenarios\\chapters\\", "scenarios\\custom\\chapters\\" }, ".xml", "" },
		{ EReferenceType::E_PARTICLE_TEXTURE_REF, { "effects\\particles\\" }, "_h.dds", "" },
		{ EReferenceType::E_WATER_TEXTURE_REF, { "water\\" }, "_h.dds", "" },
	};
	return r;
}

}

const char *ReferenceTypeName( EReferenceType eType )
{
	static const char *names[kReferenceTypeCount] = {
		"E_ANIMATIONS_REF", "E_FUNC_PARTICLES_REF", "E_EFFECTS_REF", "E_WEAPONS_REF", "E_SOLDIER_REF",
		"E_ACTIONS_REF", "E_SCENARIO_MISSIONS_REF", "E_TEMPLATE_MISSIONS_REF", "E_CHAPTERS_REF", "E_SOUNDS_REF",
		"E_SETTING_REF", "E_ASKS_REF", "E_CRATER_REF", "E_DEATHHOLE_REF", "E_MAP_REF", "E_MUSIC_REF",
		"E_MOVIE_REF", "E_PARTICLE_TEXTURE_REF", "E_ROAD_TEXTURE_REF", "E_WATER_TEXTURE_REF" };
	int i = (int)eType;
	return ( i >= 0 && i < kReferenceTypeCount ) ? names[i] : "?";
}

std::size_t References::rebuild( const std::filesystem::path &dataRoot )
{
	for ( auto &l : m_lists )
		l.clear();

	// Collect every file under the root once, as lower-case backslash paths
	// (the MFC storage form) paired with the same path in original case, which
	// is what gets stored so the picker shows the file's own spelling.
	std::vector<std::pair<std::string, std::string>> files;
	std::error_code ec;
	for ( std::filesystem::recursive_directory_iterator it( dataRoot, std::filesystem::directory_options::skip_permission_denied, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		std::error_code ec2;
		if ( !it->is_regular_file( ec2 ) )
			continue;
		std::string rel = std::filesystem::relative( it->path(), dataRoot, ec2 ).generic_string();
		std::replace( rel.begin(), rel.end(), '/', '\\' );
		files.emplace_back( Lower( rel ), rel );
	}
	std::sort( files.begin(), files.end() );

	auto add = [this]( EReferenceType t, const std::string &s ) {
		auto &l = m_lists[(int)t];
		if ( std::find( l.begin(), l.end(), s ) == l.end() )
			l.push_back( s );
	};

	for ( const auto &f : files )
	{
		const std::string &low = f.first;
		for ( const SRule &r : Rules() )
		{
			if ( !EndsWith( low, r.ext ) )
				continue;
			for ( const std::string &d : r.dirs )
			{
				if ( !StartsWith( low, d ) )
					continue;
				std::string name = f.second.substr( 0, f.second.size() - r.ext.size() ).substr( r.stripDir.size() );
				if ( !name.empty() )
					add( r.type, name );
				break;
			}
		}
		// Asks: sounds\ack\<group>\<name>.xml, stored with the dir prefix,
		// exactly one level below the ack directory.
		if ( StartsWith( low, "sounds\\ack\\" ) && EndsWith( low, ".xml" ) )
		{
			std::string rest = f.second.substr( 11, f.second.size() - 11 - 4 );
			std::size_t p = rest.find( '\\' );
			if ( p != std::string::npos && rest.find( '\\', p + 1 ) == std::string::npos )
				add( EReferenceType::E_ASKS_REF, f.second.substr( 0, 11 ) + rest );
		}
		// Road textures: terrain\sets\<set>\roads3d\...._h.dds, stored with the
		// full relative path minus the extension suffix.
		if ( StartsWith( low, "terrain\\sets\\" ) && EndsWith( low, "_h.dds" ) )
		{
			std::string rest = low.substr( 13 );
			std::size_t p = rest.find( '\\' );
			if ( p != std::string::npos && StartsWith( rest.substr( p + 1 ), "roads3d\\" ) )
				add( EReferenceType::E_ROAD_TEXTURE_REF, f.second.substr( 0, f.second.size() - 6 ) );
		}
		// Sounds and soldiers came from the objects DB in MFC, which is engine
		// state. Portable stand-in (assumption, see T04 summary): sounds are
		// the stems of sounds\*.xml outside ack\; soldiers are the stems of
		// units\humans\<name>\1.xml directory names.
		if ( StartsWith( low, "sounds\\" ) && !StartsWith( low, "sounds\\ack\\" ) && EndsWith( low, ".xml" ) )
			add( EReferenceType::E_SOUNDS_REF, f.second.substr( 7, f.second.size() - 7 - 4 ) );
		if ( StartsWith( low, "units\\humans\\" ) && EndsWith( low, "\\1.xml" ) )
			add( EReferenceType::E_SOLDIER_REF, f.second.substr( 13, f.second.size() - 13 - 6 ) );
	}

	std::size_t total = 0;
	for ( auto &l : m_lists )
		total += l.size();
	return total;
}

const std::vector<std::string> &References::enumerate( EReferenceType eType ) const
{
	static const std::vector<std::string> empty;
	int i = (int)eType;
	return ( i >= 0 && i < kReferenceTypeCount ) ? m_lists[i] : empty;
}

}
