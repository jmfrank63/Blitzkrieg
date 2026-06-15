function RushToEscape1()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(4, 1) > 2) then
		GiveCommand(A_Swarm, 4, 11500, 8800);
		Suicide();
	end;
end;

function RushToEscape2()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(1, 1) > 3) then
		GiveCommand(A_Swarm, 1, 11900, 3500);
		Suicide();
	end;
end;

function ReinforceUSSR1()
	LandReinforcement(1);
	RunScript( "CheckReinf1", 3000);
	Suicide();
end;

function ReinforceUSSR2()
	LandReinforcement(2);
	RunScript( "CheckReinf2", 3000);
	Suicide();
end;

function ReinforceUSSR3()
	LandReinforcement(3);
	RunScript( "CheckReinf3", 3000);
	Suicide();
end;

function CheckReinf1()
	SetIGlobalVar("temp.Ukraine44.objective.101", 1);
	Suicide();
end;

function CheckReinf2()
	SetIGlobalVar("temp.Ukraine44.objective.102", 1);
	Suicide();
end;

function CheckReinf3()
	SetIGlobalVar("temp.Ukraine44.objective.103", 1);
	Suicide();
end;

function Objective0()
local num = GetNUnitsInScriptGroup(1, 1) + GetNUnitsInScriptGroup(4, 1);
local num1 = GetNUnitsInArea (1, "Escape1") + GetNUnitsInArea (1, "Escape2");
	if (num <= 0) then
		SetIGlobalVar("temp.Ukraine44.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "ReinforceUSSR1", 10000);
		Suicide();
	else
	if (num1 > 3) then
		SetIGlobalVar("temp.Ukraine44.objective.0", 2);
		ObjectiveChanged(0, 2);

		RunScript( "Defeat", 5000);
		Suicide();
	end;
	end;
end;

function Objective1()
	if ( GetNUnitsInScriptGroup(1099, 1) <= 0) then
		SetIGlobalVar("temp.Ukraine44.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "ReinforceUSSR2", 10000);
		RunScript( "RevealObjective2", 5000);
		Suicide();
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(1098, 1) <= 0) then
		SetIGlobalVar("temp.Ukraine44.objective.2", 1);
		ObjectiveChanged(2, 1);

		RunScript( "ReinforceUSSR3", 10000);
		RunScript( "RevealObjective3", 5000);
		Suicide();
	end;
end;

function Objective3()
	if ( GetNUnitsInScriptGroup(1097, 1) <= 0) then
		SetIGlobalVar("temp.Ukraine44.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
end;

function Defeat()
	Loose();
	Suicide();
end;

function TobeDefeated()
local num = GetNUnitsInScriptGroup(1000, 0);
	if ( GetIGlobalVar("temp.Ukraine44.objective.101", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(11, 0);
	end;

	if ( GetIGlobalVar("temp.Ukraine44.objective.102", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(21, 0);
	end;

	if ( GetIGlobalVar("temp.Ukraine44.objective.103", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(31, 0);
	end;

	if ( num <= 0) then
		RunScript( "Defeat", 3000);
		Suicide();
	end;
end;

function ToWin()
	if ( GetIGlobalVar("temp.Ukraine44.objective.0", 0) * GetIGlobalVar("temp.Ukraine44.objective.1", 0) * GetIGlobalVar("temp.Ukraine44.objective.2", 0) *
	  GetIGlobalVar("temp.Ukraine44.objective.3", 0) == 1) then
		Win(0);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Ukraine44.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Ukraine44.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Ukraine44.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Ukraine44.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
		Suicide();
	else Suicide(); end;
end;

function Init()
	RunScript( "RushToEscape1", 35000);
	RunScript( "RushToEscape2", 20000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "RevealObjective1", 8000);
	RunScript( "ToWin", 10000);
	RunScript( "TobeDefeated", 3000);
end;
