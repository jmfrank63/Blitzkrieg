-- capture the town
function Objective0()
	if ((GetNUnitsInScriptGroup(1, 1) <= 0) and ( GetNUnitsInScriptGroup(9800, 0) == 1)) then
		SetIGlobalVar("temp.tunis.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 3000);
		RunScript( "Rush", 20000);
		Suicide();
	end;
end;

-- defend the town
function Objective1()
local num = GetNUnitsInScriptGroup(2, 1) + GetNUnitsInScriptGroup(20, 1) + GetNUnitsInScriptGroup(21, 1);
	if (num <= 0) then
		SetIGlobalVar("temp.tunis.objective.1", 1);
		RunScript( "Rush2", 5000);
		RunScript( "Reinf1", 10000);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 3000);
		Suicide();
	else
	if (( GetIGlobalVar( "temp.tunis.objective.0", 0) == 1) and (GetNUnitsInScriptGroup(9800, 1) == 1)) then
		SetIGlobalVar( "temp.tunis.objective.1", 2);
		ObjectiveChanged(1, 2);
		Suicide();
	end;
	end;
end;

-- capture town at the north-east
function Objective2()
	if ( (GetNUnitsInScriptGroup(5, 1) <= 0) and ( GetNUnitsInScriptGroup(9801, 0) == 1)) then
		SetIGlobalVar("temp.tunis.objective.2", 1);
		RunScript( "Rush3", 1000);
		ObjectiveChanged(2, 1);

		RunScript( "RevealObjective3", 3000);
		Suicide();
	end;
end;

-- defend the town from incoming troops
function Objective3()
	if ( GetNUnitsInScriptGroup(3, 1) <= 0) then
		SetIGlobalVar("temp.tunis.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	else
	if (( GetIGlobalVar( "temp.tunis.objective.2", 0) == 1) and (GetNUnitsInScriptGroup(9801, 1) == 1)) then
		SetIGlobalVar( "temp.tunis.objective.3", 2);
		ObjectiveChanged(1, 2);
		Suicide();
	end;
	end;
end;

-- hidden - destroy enemy artillery
function Objective4()
	if ( GetNUnitsInScriptGroup(4, 1) <= 0) then
		SetIGlobalVar("temp.tunis.objective.4", 1);
		ObjectiveChanged(4, 1);
		Suicide();
	end;
end;

function ToWin()
	if ((GetIGlobalVar("temp.tunis.objective.0",0) * GetIGlobalVar("temp.tunis.objective.1",0) *
	     GetIGlobalVar("temp.tunis.objective.2",0) * GetIGlobalVar("temp.tunis.objective.3",0)) == 1) then
		RunScript( "Victory", 4000);
		Suicide();
	end;
end;

function Victory()
	Win(0);
	Suicide();
end;

function TobeDefeated()
local num = GetNUnitsInScriptGroup(1000, 0);
	if ( GetIGlobalVar("temp.tunis.objective.150", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(100, 0);
	end;
	if ( (num <= 0) or (GetIGlobalVar( "temp.tunis.objective.1", 0) == 2) or (GetIGlobalVar( "temp.tunis.objective.3", 0) == 2)) then
		RunScript( "Kopez", 5000);
		Suicide();
	end;
end;

function Kopez()
	Loose();
	Suicide();
end;

function Rush()
local A_Swarm = 3;
	Cmd(A_Swarm, 20, 8560, 2960);
	Cmd(A_Swarm, 21, 8660, 3900);
	Cmd(A_Swarm, 2, 8400, 3290);
	Suicide();
end;

function Rush2()
local A_Swarm = 3;
	Cmd(A_Swarm, 3, 1600, 4700);
	QCmd(A_Swarm, 3, 1600, 8100);
	Suicide();
end;

function Rush3()
local A_Swarm = 3;
	Cmd(A_Swarm, 3, 3300, 10200);
	QCmd(A_Swarm, 3, 11200, 10200);
	Suicide();
end;

function Reinf1()
	LandReinforcement(100);
	SetIGlobalVar("temp.tunis.objective.150", 1);
	Suicide();
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.tunis.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.tunis.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.tunis.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.tunis.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function Init()
	RunScript("Objective0", 2000);
	RunScript("Objective1", 2000);
	RunScript("Objective2", 2000);
	RunScript("Objective3", 2000);
	RunScript("Objective4", 2000);
	RunScript( "RevealObjective0", 3000);
--	RunScript("Rush", 120000);
	RunScript("ToWin", 2000);
	RunScript("TobeDefeated", 2000);
end;
