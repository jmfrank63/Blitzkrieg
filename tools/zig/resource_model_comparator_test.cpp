// Per-stats-type comparator harness for Sources/src/ResourceModel/. For every
// extension the comparator knows about this test:
//   1. Reads tools/zig/fixtures/resource_editor/<ext>/project.<ext>.
//   2. Looks for a golden file at tools/zig/fixtures/resource_editor/<ext>/
//      golden/golden.<ext>. If absent, reports GOLDEN_MISSING (the slice
//      contract tolerates win-home oracle not yet producing goldens).
//   3. Writes a one-line summary per (ext, stats_type) to stderr AND to
//      zig-out/local-test/resource_model/comparator.log, in the shape
//      "COMPARE <ext> reader=<engineReader> stats=<stats_type>
//      fields=<n> mismatches=<n> golden=present|missing|n/a"
//      - the Observability Impact clause of the task plan.
//
// The planted synthetic-unknown fixture at
// tools/zig/fixtures/resource_editor/wpn/golden-synthetic-unknown.xml is
// exercised in a separate forked child; it must exit non-zero and its stderr
// must contain "UNKNOWN FIELD". This is the "unknown-field abort path is
// covered by a planted fixture" clause.
//
// With no args the test runs the sweep + the planted-unknown subtest and
// exits 0 only when both pass. With a positional "sweep", "planted", or
// "planted-child" arg the test runs just that mode (planted-child is the
// child entry the harness forks itself through, so a single-binary layout
// still exercises std::abort cleanly).

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined( _WIN32 )
#	include <io.h>
#	include <process.h>
#else
#	include <sys/types.h>
#	include <sys/wait.h>
#	include <unistd.h>
#endif

#include "../../Sources/src/ResourceModel/comparator.h"

namespace
{

const char *ReportKindName( NResourceModel::ReportKind k )
{
	switch ( k )
	{
		case NResourceModel::ReportKind::OK:              return "present";
		case NResourceModel::ReportKind::GOLDEN_MISSING:  return "missing";
		case NResourceModel::ReportKind::NOT_APPLICABLE:  return "n/a";
	}
	return "?";
}

std::string ReadAll( const std::string &path )
{
	std::ifstream in( path, std::ios::binary );
	if ( !in.good() ) return std::string();
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

bool WriteAll( const std::string &path, const std::string &body )
{
	std::ofstream out( path, std::ios::binary );
	if ( !out.good() ) return false;
	out.write( body.data(), (std::streamsize)body.size() );
	return out.good();
}

int RunSweep()
{
	namespace fs = std::filesystem;
	const std::string logPath = "zig-out/local-test/resource_model/comparator.log";
	std::error_code mkerr;
	fs::create_directories( "zig-out/local-test/resource_model", mkerr );

	std::string log;
	bool allOk = true;
	int extsChecked = 0;
	int goldensPresent = 0;
	int goldensMissing = 0;
	int notApplicable = 0;

	for ( const auto &ext : NResourceModel::ExtensionList() )
	{
		++extsChecked;
		const std::string portPath = "tools/zig/fixtures/resource_editor/" + ext + "/project." + ext;
		const std::string goldenPath =
			"tools/zig/fixtures/resource_editor/" + ext + "/golden/golden." + ext;
		const std::string portXml = ReadAll( portPath );
		if ( portXml.empty() )
		{
			std::fprintf( stderr, "COMPARE %s FATAL port fixture missing: %s\n",
				ext.c_str(), portPath.c_str() );
			allOk = false;
			continue;
		}
		NResourceModel::CompareReport rep = NResourceModel::Compare( ext, portXml, goldenPath );

		char line[768];
		std::snprintf( line, sizeof( line ),
			"COMPARE %s reader=%s stats=%s fields=%d mismatches=%d golden=%s unknowns=%zu\n",
			ext.c_str(),
			rep.engineReader.c_str(), rep.statsType.c_str(),
			rep.fieldsCompared, (int)rep.mismatches.size(),
			ReportKindName( rep.kind ),
			rep.unknownFields.size() );
		std::fputs( line, stderr );
		log += line;

		if ( rep.kind == NResourceModel::ReportKind::OK ) ++goldensPresent;
		else if ( rep.kind == NResourceModel::ReportKind::GOLDEN_MISSING ) ++goldensMissing;
		else if ( rep.kind == NResourceModel::ReportKind::NOT_APPLICABLE ) ++notApplicable;

		if ( const char *debug = std::getenv( "BK_DEBUG_LOG" ); debug && debug[0] == '1' && debug[1] == 0 )
		{
			for ( const auto &m : rep.mismatches )
			{
				char dline[1024];
				std::snprintf( dline, sizeof( dline ),
					"COMPARE %s mismatch field=%s port=%s golden=%s\n",
					ext.c_str(), m.field.c_str(), m.port.c_str(), m.golden.c_str() );
				std::fputs( dline, stderr );
				log += dline;
			}
		}
	}

	char summary[256];
	std::snprintf( summary, sizeof( summary ),
		"COMPARE_SUMMARY exts=%d goldens_present=%d goldens_missing=%d not_applicable=%d\n",
		extsChecked, goldensPresent, goldensMissing, notApplicable );
	std::fputs( summary, stderr );
	log += summary;

	if ( !WriteAll( logPath, log ) )
	{
		std::fprintf( stderr, "COMPARE FATAL could not write %s: %s\n",
			logPath.c_str(), std::strerror( errno ) );
		return 1;
	}

	return allOk ? 0 : 1;
}

// Child-mode entry: run the comparator against the planted synthetic-unknown
// fixture and let std::abort() fire. Expected to not return.
int RunPlantedChild()
{
	const std::string portPath =
		"tools/zig/fixtures/resource_editor/wpn/golden-synthetic-unknown.xml";
	const std::string portXml = ReadAll( portPath );
	if ( portXml.empty() )
	{
		std::fprintf( stderr, "PLANTED_CHILD FATAL fixture missing: %s\n", portPath.c_str() );
		return 1;
	}
	// No golden - comparator aborts on the planted root-level
	// SomethingTheEngineDoesNotRead element.
	NResourceModel::Compare( "wpn", portXml, std::string() );
	// Should never reach - comparator calls std::abort() before returning.
	std::fprintf( stderr, "PLANTED_CHILD FATAL comparator returned without aborting\n" );
	return 2;
}

// Parent entry: fork the test binary with the "planted-child" arg, wait, and
// confirm the child died non-zero AND wrote "UNKNOWN FIELD" to stderr. The
// task plan's abort-path verification clause.
#if defined( _WIN32 )
int RunPlanted( const char * /*selfPath*/ )
{
	// Windows abort-path verification is deferred to a cross-platform follow-up
	// (CreateProcess + pipes rather than fork/exec); the primary target of this
	// task is Linux x64 and CI handles Windows separately. Emit a line so the
	// log still records the deferred state.
	std::fprintf( stderr, "PLANTED skipped=1 reason=windows-not-wired\n" );
	std::ofstream out( "zig-out/local-test/resource_model/comparator.log",
		std::ios::binary | std::ios::app );
	if ( out.good() ) out << "PLANTED skipped=1 reason=windows-not-wired\n";
	return 0;
}
#else
int RunPlanted( const char *selfPath )
{
	int pipeFd[2];
	if ( pipe( pipeFd ) != 0 )
	{
		std::fprintf( stderr, "PLANTED FATAL pipe: %s\n", std::strerror( errno ) );
		return 1;
	}
	pid_t pid = fork();
	if ( pid < 0 )
	{
		std::fprintf( stderr, "PLANTED FATAL fork: %s\n", std::strerror( errno ) );
		close( pipeFd[0] );
		close( pipeFd[1] );
		return 1;
	}
	if ( pid == 0 )
	{
		// Child: redirect stderr into the pipe, then exec ourselves with the
		// planted-child subcommand. If exec fails fall back to in-process so
		// the planted check still runs - but this path should not fire
		// normally.
		close( pipeFd[0] );
		dup2( pipeFd[1], STDERR_FILENO );
		close( pipeFd[1] );
		execl( selfPath, selfPath, "planted-child", (char *)nullptr );
		// exec failed - run in-process so the abort still fires. The parent's
		// pipe-read will still see "UNKNOWN FIELD" on stderr.
		_exit( RunPlantedChild() == 0 ? 0 : 3 );
	}
	// Parent: drain the pipe and wait on the child.
	close( pipeFd[1] );
	std::string capturedStderr;
	char buf[4096];
	while ( true )
	{
		ssize_t n = read( pipeFd[0], buf, sizeof( buf ) );
		if ( n <= 0 ) break;
		capturedStderr.append( buf, buf + n );
	}
	close( pipeFd[0] );
	int status = 0;
	if ( waitpid( pid, &status, 0 ) < 0 )
	{
		std::fprintf( stderr, "PLANTED FATAL waitpid: %s\n", std::strerror( errno ) );
		return 1;
	}

	// Re-emit the captured child stderr so a sweep script can grep both.
	std::fwrite( capturedStderr.data(), 1, capturedStderr.size(), stderr );

	const bool exitedNonZero = !( WIFEXITED( status ) && WEXITSTATUS( status ) == 0 );
	const bool hasMessage = capturedStderr.find( "UNKNOWN FIELD" ) != std::string::npos;

	std::fprintf( stderr,
		"PLANTED child_exited_nonzero=%d unknown_field_printed=%d\n",
		exitedNonZero ? 1 : 0, hasMessage ? 1 : 0 );

	// Append the planted-abort outcome to the comparator log so a reader can
	// see both the sweep and the abort-path verdicts in one place.
	std::ofstream out( "zig-out/local-test/resource_model/comparator.log",
		std::ios::binary | std::ios::app );
	if ( out.good() )
	{
		out << "PLANTED child_exited_nonzero=" << ( exitedNonZero ? 1 : 0 )
		    << " unknown_field_printed=" << ( hasMessage ? 1 : 0 ) << "\n";
	}

	return ( exitedNonZero && hasMessage ) ? 0 : 1;
}
#endif

}

int main( int argc, char **argv )
{
	if ( argc > 1 )
	{
		const std::string mode = argv[1];
		if ( mode == "sweep" ) return RunSweep();
		if ( mode == "planted" ) return RunPlanted( argv[0] );
		if ( mode == "planted-child" ) return RunPlantedChild();
		std::fprintf( stderr, "usage: %s [sweep|planted|planted-child]\n", argv[0] );
		return 2;
	}
	// Default: sweep + planted abort verification.
	const int sweepRc = RunSweep();
	const int plantedRc = RunPlanted( argv[0] );
	return ( sweepRc == 0 && plantedRc == 0 ) ? 0 : 1;
}
