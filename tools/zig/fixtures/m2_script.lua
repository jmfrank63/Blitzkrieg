-- The M2 test script (04-10, Lua 4, the dialect of the shipped mission scripts).
-- Used by the game-reads-it M2 scenario (Sources/editor/app/game_reads_m2.zig):
-- the scenario names it as the map's script file, copies it beside the test
-- copy of the map, and the game runs it. Its Trace calls print numbers only, and
-- with BK_MAP_TRACE set the game mirrors each one as "BK_MAP_TRACE: lua <text>".
function Init()
	-- The script ran.
	Trace( 1 )
	-- The area the scenario drew, found by its name: the call answers the centre x
	-- and y, then two sizes, in map units as the map stores them.
	local x, y, r = GetScriptAreaParams( "m2_area" )
	Trace( x )
	Trace( y )
	-- Group 900 holds the script ID (4245) of the unit the scenario placed and
	-- held back. LandReinforcement only queues it: the game lands one queued
	-- unit at a time, every 200 game ticks (CScripts::LandSuspendedReiforcements),
	-- so the count is traced from Report, which runs a dozen times.
	LandReinforcement( 900 )
	RunScript( "Report", 100, 12 )
end

function Report()
	Trace( GetNUnitsInScriptGroup( 4245 ) )
end
