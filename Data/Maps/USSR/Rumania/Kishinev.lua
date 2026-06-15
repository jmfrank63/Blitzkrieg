function TobeDefeated()
	if (GetNUnitsInScriptGroup (1000, 0) <= 0) then
		Loose();
		Suicide();
	end;
end;

function ToWin()
	if ((GetIGlobalVar("temp.Kishinev.objective.0", 0) * GetIGlobalVar("temp.Kishinev.objective.1", 0) * GetIGlobalVar("temp.Kishinev.objective.2", 0) *
	  GetIGlobalVar( "temp.Kishinev.objective.3", 0)) == 1) then
		WinWin();
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function Objective0()
	if (GetNUnitsInScriptGroup(300, 1) <= 0) then
		SetIGlobalVar("temp.Kishinev.objective.0", 1);
		ObjectiveChanged(0, 1);
		Suicide();
	end;
end;

function Objective1()
	if ((GetNUnitsInScriptGroup(9001, 0) > 0) and ( GetNUnitsInScriptGroup(9000, 1) <= 0)) then
		SetIGlobalVar("temp.Kishinev.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

function Objective2()
	if ((GetNUnitsInScriptGroup(9002, 0) > 0) and ( GetNUnitsInScriptGroup(9010, 1) <= 0)) then
		SetIGlobalVar("temp.Kishinev.objective.2", 1);
		ObjectiveChanged(2, 1);

		RunScript( "RevealObjective3", 5000);
		Suicide();
	end;
end;

function Objective3()
	if ((GetNUnitsInScriptGroup(9003, 0) > 0) and ( GetNUnitsInScriptGroup(9020, 1) <= 0)) then
		SetIGlobalVar("temp.Kishinev.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Kishinev.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Kishinev.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( (GetIGlobalVar("temp.Kishinev.objective.0", 0) * GetIGlobalVar("temp.Kishinev.objective.1", 0)) == 1) then
	if ( GetIGlobalVar("temp.Kishinev.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
	end;
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Kishinev.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function Sniper()
	ChangeFormation(1011, 2);
	Suicide();
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

function Init()
	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0", 3000);
	RunScript( "Objective1", 3000);
	RunScript( "Objective2", 3000);
	RunScript( "Objective3", 3000);

	RunScript( "RevealObjective0", 2000);
	RunScript( "RevealObjective1", 7000);
	RunScript( "RevealObjective2", 10000);

	RunScript( "Patrol1I", 2000);
	RunScript( "Patrol2I", 2000);
	RunScript( "Patrol3I", 2000);
--	RunScript( "Sniper", 1000);
end;
