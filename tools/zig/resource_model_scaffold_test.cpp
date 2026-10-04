// Scaffold-shape smoke for Sources/src/ResourceModel/. Reads a single project
// fixture (default: tools/zig/fixtures/resource_editor/wpn/project.wpn),
// Load()s it into a Project, Save()s it, and asserts byte-identity. Writes
// `SCAFFOLD <ext> bytes=<n> rt_equal=<0|1>` to stderr; BK_DEBUG_LOG=1 also
// prints the parse tree depth and child counts.
//
// Intentionally a one-fixture scaffold: T06 wires `test-resource-model` on the
// five engine targets and sweeps every repo fixture through the same shape.
// This target exists so T01's "Done when" clause - "the files compile standalone
// as a C++17 translation unit set" - is observable and so T02's first typed
// item class has a target to regression against before touching the sweep.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "../../Sources/src/ResourceModel/future_blob.h"
#include "../../Sources/src/ResourceModel/project.h"
#include "../../Sources/src/ResourceModel/xml.h"

namespace
{

bool DebugLog()
{
	const char *env = std::getenv( "BK_DEBUG_LOG" );
	return env && env[0] == '1' && env[1] == 0;
}

std::string ReadAll( const std::string &path )
{
	std::ifstream in( path, std::ios::binary );
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

int TreeDepth( const NResourceXml::Node &n )
{
	int d = 0;
	for ( const auto &c : n.children )
		if ( c.kind == NResourceXml::Node::Element )
			d = std::max( d, 1 + TreeDepth( c ) );
	return d;
}

int ElementCount( const NResourceXml::Node &n )
{
	int count = ( n.kind == NResourceXml::Node::Element ) ? 1 : 0;
	for ( const auto &c : n.children )
		count += ElementCount( c );
	return count;
}

}

int main( int argc, char **argv )
{
	// A single positional override, so a CI job that wants to sweep (T06) can
	// loop over fixtures without re-shelling. Default: the wpn fixture.
	const char *fixture = ( argc > 1 ) ? argv[1] : "tools/zig/fixtures/resource_editor/wpn/project.wpn";
	// Pick the extension from the directory name, which is the sub-editor key
	// the port cares about (wpn, mcp, trc, ...).
	std::string ext = "unknown";
	{
		std::string path = fixture;
		size_t slash = path.find_last_of( "/\\" );
		if ( slash != std::string::npos )
		{
			size_t prev = path.find_last_of( "/\\", slash - 1 );
			if ( prev != std::string::npos )
				ext = path.substr( prev + 1, slash - prev - 1 );
		}
	}

	std::string in = ReadAll( fixture );
	if ( in.empty() )
	{
		std::fprintf( stderr, "SCAFFOLD %s FATAL fixture missing or empty: %s\n", ext.c_str(), fixture );
		return 2;
	}

	NResourceModel::Project project;
	std::string err;
	if ( !NResourceModel::Load( in, project, err ) )
	{
		std::fprintf( stderr, "SCAFFOLD %s FATAL parse: %s\n", ext.c_str(), err.c_str() );
		return 2;
	}

	// The scaffold loader wraps the root in a FutureBlob (no typed classes
	// registered yet). Prove the loader did that, so T02 breaking this assumption
	// shows up here rather than inside the sweep.
	if ( !project.root || !NResourceModel::FutureBlob::IsFutureBlob( *project.root ) )
	{
		std::fprintf( stderr, "SCAFFOLD %s FATAL root is not a FutureBlob\n", ext.c_str() );
		return 2;
	}

	std::string out = NResourceModel::Save( project );
	bool rt_equal = ( in == out );

	std::fprintf( stderr, "SCAFFOLD %s bytes=%zu rt_equal=%d\n", ext.c_str(), in.size(), rt_equal ? 1 : 0 );

	if ( DebugLog() )
	{
		std::fprintf( stderr, "SCAFFOLD %s depth=%d elements=%d root=%s\n",
			ext.c_str(), TreeDepth( project.document.root ),
			ElementCount( project.document.root ),
			project.document.root.name.c_str() );
		if ( !rt_equal )
		{
			size_t mismatch = 0;
			size_t n = std::min( in.size(), out.size() );
			while ( mismatch < n && in[mismatch] == out[mismatch] ) ++mismatch;
			std::fprintf( stderr, "SCAFFOLD %s first mismatch at byte %zu (in=0x%02X out=0x%02X), sizes in=%zu out=%zu\n",
				ext.c_str(), mismatch,
				mismatch < in.size() ? (unsigned char)in[mismatch] : 0u,
				mismatch < out.size() ? (unsigned char)out[mismatch] : 0u,
				in.size(), out.size() );
		}
	}

	return rt_equal ? 0 : 1;
}
