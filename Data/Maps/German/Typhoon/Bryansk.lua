function Objective0()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup (2, 1) < 7) then
		Cmd(A_Swarm, 10, 7200, 3400);
		QCmd( A_Swarm, 10, 5800, 8700 );
		QCmd(A_Swarm, 10, 700, 8600);
		SetIGlobalVar("temp.Bryansk.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		Suicide();
	else
	if (GetNUnitsInScriptGroup (1, 1) < 5) then
		GiveCommand(A_Swarm, 10, 6900, 2400 );
		SetIGlobalVar("temp.Bryansk.objective.0", 2);
		ObjectiveChanged(0, 2);

		RunScript( "RevealObjective1", 5000);
		Suicide();
	end;
	end;
end;

function Objective1()
local num = GetNUnitsInScriptGroup(1, 1);
	if ( GetIGlobalVar("temp.Bryansk.objective.0", 0) == 2) then
		num = num + GetNUnitsInScriptGroup(10, 1);
	end;
	if ( num <= 0) then
		SetIGlobalVar("temp.Bryansk.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 5000);
		Suicide();
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(501, 1) <= 0) then
		SetIGlobalVar("temp.Bryansk.objective.2", 1);
		ObjectiveChanged(2, 1);

		RunScript( "RevealObjective3", 5000);
		Suicide();
	end;
end;

function Objective3()
	if ((GetNUnitsInArea (0, "Station") > 0) or ( GetNUnitsInScriptGroup(500) <= 0)) then
		SetIGlobalVar("temp.Bryansk.objective.3", 1);
		ObjectiveChanged(3, 1);

		RunScript( "RevealObjective4", 5000);
		Suicide();
	end;
end;

function Objective4()
local num = GetNUnitsInScriptGroup(2, 1);
	if ( GetIGlobalVar("temp.Bryansk.objective.0", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(10, 1);
	end;
	if ( num <= 0) then
		SetIGlobalVar("temp.Bryansk.objective.4", 1);
		ObjectiveChanged(4, 1);
		Suicide();
	end;
end;

function TestObj3()
	if ( GetIGlobalVar("temp.Bryansk.objective.3", 0) ~= 1) then
	if ( GetNUnitsInScriptGroup(2, 1) < 5) then
		LandReinforcement(1);
		Suicide();
	end;
	else
		Suicide();
	end;
end;

function TobeDefeated()
	if (GetNUnitsInScriptGroup (1000, 0) <= 0) then
		Loose();
		Suicide();
	end;
end;

function ToWin()
	if ((GetIGlobalVar("temp.Bryansk.objective.2", 0) * GetIGlobalVar("temp.Bryansk.objective.3", 0) * GetIGlobalVar("temp.Bryansk.objective.4", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Bryansk.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Bryansk.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Bryansk.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Bryansk.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.Bryansk.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0);
		Suicide();
	else Suicide(); end;
end;

function TM180_A()
local y = RandomInt(3800) + 3300;
	Cmd(0, 991, 5000, y);
	RunScript( "TM180_B", 160000);
	Suicide();
end;

function TM180_B()
local y = RandomInt(3800) + 3300;
	Cmd(0, 991, 5000, y);
	RunScript( "TM180_A", 160000);
	Suicide();

end;

function Init()
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "Objective4", 2000);
	RunScript( "ToWin", 5000);
	RunScript( "TobeDefeated", 5000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "TestObj3", 5000);
	RunScript( "TM180_A", 2000);
--	RunScript( "RevealObjective1", 8000);
--	RunScript( "RevealObjective2", 13000);
end;
