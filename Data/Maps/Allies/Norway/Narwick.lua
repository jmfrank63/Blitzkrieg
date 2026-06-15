--function TobeDefeated()
--	if (GetNUnitsInScriptGroup (1000) <= 0) then
--	Loose();
--	Suicide();
--	end;
--end;

function ToWin()
	Win(0);
	Suicide();
end;

function Reinf1()
	if ( GetIGlobalVar("temp.Narwick.objective.100", 0) == 0) then
	if ( GetNUnitsInScriptGroup(1000) <= 2) then
		LandReinforcement(1);
		SetIGlobalVar("temp.Narwick.objective.100", 1);
		Suicide();
	end;
	else Suicide(); end;
end;

function Reinf1repeat()
	if ( GetIGlobalVar("temp.Narwick.objective.100", 0) == 1) then
		if ( GetNUnitsInScriptGroup(100) <= 0) then
			LandReinforcement(1);
		end;
	end;
end;

function Objective0()
	if (GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.Narwick.objective.0", 1);
		ObjectiveChanged(0, 1);
		RunScript( "ToWin", 10000);
		Suicide();
	end;
end;

-- hidden
function Objective1()
	if ( GetNUnitsInParty(1) <= 0) then
		SetIGlobalVar("temp.Narwick.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

function RevealObjective0()
	ObjectiveChanged(0, 0);
	Suicide();
end;

function Scene01()
local A_MOVE = 0;
	if ( GetNUnitsInArea(0, "Scene01") > 0) then
		Cmd(A_MOVE, 501, 2785, 2520);
		RunScript( "Scene02", 500);
		Suicide();
	end;
end;

function Scene02()
local STATE_REST = 1;
local A_LOAD = 4;
	if ( GetUnitState( 501) == STATE_REST) then
		Cmd(A_LOAD, 501, 502);
		RunScript( "Scene03", 500);
		Suicide();
	end;
end;

function Scene03()
local STATE_REST_BOARD = 3;
local A_MOVE = 0;
	if ( GetUnitState( 501) == STATE_REST_BOARD) then
		Cmd(A_MOVE, 502, 2820, 2060);
		QCmd(A_MOVE, 502, 3400, 1360);
		QCmd(A_MOVE, 502, 3800, 924);
		QCmd(A_MOVE, 502, 4410, 500);
		Suicide();
	end;
end;

function Init()
--	RunScript( "ToWin", 4000);
	RunScript( "Reinf1", 2000);
	RunScript( "Reinf1repeat", 2000);
--	RunScript( "Reinf2", 2000);
--	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0",2000);
	RunScript( "Objective1",2000);
	RunScript( "Scene01", 1000);
	RunScript( "RevealObjective0", 3000);
end;
