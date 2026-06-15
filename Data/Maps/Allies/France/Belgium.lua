function TobeDefeated()
local num = GetNUnitsInScriptGroup (1000, 0);
	if ( GetIGlobalVar("temp.Belgium.objective.100", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(10, 0);
	end;
	if ( num <= 0) then
		Loose();
		Suicide();
	end;
end;

function ToWin()
	if ( GetIGlobalVar("temp.Belgium.objective.4", 0) == 1) then
		Win(0);
		Suicide();
	end;
end;

-- player reinf
function Reinf1()
	if ( GetIGlobalVar("temp.Belgium.objective.0", 0) == 1) then
		LandReinforcement(1);
		SetIGlobalVar("temp.Belgium.objective.100", 1);
		Suicide();
	end;
end;

-- #1 enemy reinf
function Reinf2()
	LandReinforcement(2);
	RunScript( "Check2", 3000);
	Suicide();
end;

function Check2()
	SetIGlobalVar("temp.Belgium.objective.101", 1);
	Suicide();
end;

-- #2 enemy reinf
function Reinf3()
	LandReinforcement(3);
	RunScript( "Check3", 3000);
	Suicide();
end;

function Check3()
	SetIGlobalVar("temp.Belgium.objective.102", 1);
	Suicide();
end;

-- "free central town"
function Objective0()
	if (GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.Belgium.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "Reinf1", 5000);
		RunScript( "Reinf2", 30000);
		RunScript( "RevealObjective1", 10000);
		Suicide();
	end;
end;

-- destroy incoming troops
function Objective1()
	if ( GetIGlobalVar("temp.Belgium.objective.101", 0) == 1) then
	if ( GetNUnitsInScriptGroup(20, 1) <= 0) then
		SetIGlobalVar("temp.Belgium.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 5000);
		RunScript( "RevealObjective3", 10000);
		Suicide();
	end;
	end;
end;

-- "destroy artillery near the southern town"
function Objective2()
	if ( (GetNUnitsInScriptGroup(31, 1) <= 0) and (GetUnitState(31) ~= 24)) then
		SetIGlobalVar("temp.Belgium.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

-- "free northern town, capture storage"
function Objective3()
	if ( GetNUnitsInScriptGroup(2, 1) <= 0) then
		SetIGlobalVar("temp.Belgium.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
end;

-- destroy #2 incoming troops
function Objective4()
	if ( GetIGlobalVar("temp.Belgium.objective.102", 0) == 1) then
	if ( GetNUnitsInScriptGroup(21, 1) <= 0) then
		SetIGlobalVar("temp.Belgium.objective.4", 1);
		ObjectiveChanged(4, 1);
		Suicide();
	end;
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Belgium.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Belgium.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Belgium.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Belgium.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.Belgium.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0);
		Suicide();
	else Suicide(); end;
end;

function ARevealObjective4()
	if ( (GetIGlobalVar("temp.Belgium.objective.2", 0) * GetIGlobalVar("temp.Belgium.objective.3", 0)) == 1) then
		RunScript( "RevealObjective4", 5000 );
		RunScript( "Reinf3", 30000);
		Suicide();
	end;
end;

function Init()
	RunScript( "ToWin", 4000);
--	RunScript( "Reinf1", 2000);
--	RunScript( "Reinf2", 2000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "Objective4", 2000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "ARevealObjective4", 3000);
end;
