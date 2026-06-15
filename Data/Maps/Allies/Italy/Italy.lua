function ToWin()
	if ( (GetIGlobalVar("temp.Italy.objective.0", 0) * GetIGlobalVar("temp.Italy.objective.1", 0) * GetIGlobalVar("temp.Italy.objective.2", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function TobeDefeated()
	if ( (GetNUnitsInScriptGroup(1000, 0) + GetNUnitsInScriptGroup(1001, 0)) <= 0) then
		Loose();
		Suicide();
	end;
end;

function Objective0()
	if (GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.Italy.objective.0", 1);
		ObjectiveChanged(0, 1);
		Suicide();
	end;
end;

function Objective1()
	if ( GetNUnitsInScriptGroup(2, 1) <= 0) then
		SetIGlobalVar("temp.Italy.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(3, 1) <= 0) then
		SetIGlobalVar("temp.Italy.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Italy.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Italy.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Italy.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

--- Patrols ---------------------------------

function Patrol1()
local i = GetIGlobalVar( "temp.points1", 0);

	if ( GetNUnitsInScriptGroup(1101) > 0) then
	if ( GetNScriptUnitsInArea( 1101, "Patrol1" .. i ) > 0 ) then
		RunScript( "Patrol1I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol1I()
local i = GetIGlobalVar( "temp.points1", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1101) > 0) then
		Cmd ( 3, 1101, GetScriptAreaParams( "Patrol1" .. i ) );
		RunScript( "Patrol1", 2000);

		SetIGlobalVar( "temp.points1", i);
	end;
	Suicide();
end;

function Patrol2()
local i = GetIGlobalVar( "temp.points2", 0);

	if ( GetNUnitsInScriptGroup(1102) > 0) then
	if ( GetNScriptUnitsInArea( 1102, "Patrol2" .. i ) > 0 ) then
		RunScript( "Patrol2I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol2I()
local i = GetIGlobalVar( "temp.points2", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1102) > 0) then
		Cmd ( 3, 1102, GetScriptAreaParams( "Patrol2" .. i ) );
		RunScript( "Patrol2", 2000);

		SetIGlobalVar( "temp.points2", i);
	end;
	Suicide();
end;

function Patrol3()
local i = GetIGlobalVar( "temp.points3", 0);

	if ( GetNUnitsInScriptGroup(1103) > 0) then
	if ( GetNScriptUnitsInArea( 1103, "Patrol3" .. i ) > 0 ) then
		RunScript( "Patrol3I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol3I()
local i = GetIGlobalVar( "temp.points3", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1103) > 0) then
		Cmd ( 3, 1103, GetScriptAreaParams( "Patrol3" .. i ) );
		RunScript( "Patrol3", 2000);

		SetIGlobalVar( "temp.points3", i);
	end;
	Suicide();
end;

function Patrol4()
local i = GetIGlobalVar( "temp.points4", 0);

	if ( GetNUnitsInScriptGroup(1104) > 0) then
	if ( GetNScriptUnitsInArea( 1104, "Patrol4" .. i ) > 0 ) then
		RunScript( "Patrol4I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol4I()
local i = GetIGlobalVar( "temp.points4", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1104) > 0) then
		Cmd ( 3, 1104, GetScriptAreaParams( "Patrol4" .. i ) );
		RunScript( "Patrol4", 2000);

		SetIGlobalVar( "temp.points4", i);
	end;
	Suicide();
end;

function Patrol5()
local i = GetIGlobalVar( "temp.points5", 0);

	if ( GetNUnitsInScriptGroup(1105) > 0) then
	if ( GetNScriptUnitsInArea( 1105, "Patrol5" .. i ) > 0 ) then
		RunScript( "Patrol5I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol5I()
local i = GetIGlobalVar( "temp.points5", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1105) > 0) then
		Cmd ( 3, 1105, GetScriptAreaParams( "Patrol5" .. i ) );
		RunScript( "Patrol5", 2000);

		SetIGlobalVar( "temp.points5", i);
	end;
	Suicide();
end;

---------------------------------------------

function Init()
	RunScript( "ToWin", 4000);
--	RunScript( "Reinf1", 2000);
--	RunScript( "Reinf2", 2000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0",2000);
	RunScript( "Objective1",2000);
	RunScript( "Objective2",2000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "RevealObjective1", 8000);
	RunScript( "RevealObjective2", 13000);
	RunScript( "Patrol1I", 2000);
	RunScript( "Patrol2I", 2000);
	RunScript( "Patrol3I", 2000);
	RunScript( "Patrol4I", 2000);
--	RunScript( "Patrol5I", 2000);
end;
