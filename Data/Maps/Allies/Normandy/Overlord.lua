function Objective0()
local num = GetIGlobalVar("temp.objective.110", 0) * GetIGlobalVar("temp.objective.111", 0) *
	GetIGlobalVar("temp.objective.120", 0) * GetIGlobalVar("temp.objective.121", 0) *
	GetIGlobalVar("temp.objective.130", 0) * GetIGlobalVar("temp.objective.140", 0) *
	GetIGlobalVar("temp.objective.150", 0) * GetIGlobalVar("temp.objective.151", 0);

	if ( num == 1) then
	local num2 = GetNUnitsInScriptGroup(10) + GetNUnitsInScriptGroup(11) + GetNUnitsInScriptGroup(20) + GetNUnitsInScriptGroup(21) + GetNUnitsInScriptGroup(30) +
		GetNUnitsInScriptGroup(40) + GetNUnitsInScriptGroup(50) + GetNUnitsInScriptGroup(51);

	if ( num2 <= 0) then
		ObjectiveChanged(0, 1);
		SetIGlobalVar("temp.Overlord.objective.0", 1);

		RunScript( "RevealObjective1", 5000);
		RunScript( "RevealObjective2", 10000);
		Suicide();
	end;
	end;

	if ( GetNUnitsInArea(1, "Loose") > 0) then
		SetIGlobalVar( "temp.Overlord.objective.0", 2);
		ObjectiveChanged(0, 2);
		Suicide();
	end;
end;

function Objective1()
	if ( GetNUnitsInScriptGroup(99, 1) <= 0) then
		ChangePlayer(911, 2);
		SetIGlobalVar("temp.Overlord.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(98, 1) <= 0) then
		SetIGlobalVar("temp.Overlord.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function ToWin()
	if ((GetIGlobalVar("temp.Overlord.objective.0", 0) * GetIGlobalVar("temp.Overlord.objective.1", 0) * GetIGlobalVar("temp.Overlord.objective.2", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function Reinforce1()
--	if ( GetNUnitsInScriptGroup(1001) < 4) then
		LandReinforcement(100);
		Suicide();
--	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Overlord.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Overlord.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Overlord.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function LaunchAttack10()
	LandReinforcement(10);
	SetIGlobalVar("temp.objective.110", 1);
	Suicide();
end;

function LaunchAttack11()
	LandReinforcement(11);
	SetIGlobalVar("temp.objective.111", 1);
	Suicide();
end;

function LaunchAttack20()
	LandReinforcement(20);
	SetIGlobalVar("temp.objective.120", 1);
	Suicide();
end;

function LaunchAttack21()
	LandReinforcement(21);
	SetIGlobalVar("temp.objective.121", 1);
	Suicide();
end;

function LaunchAttack30()
	LandReinforcement(30);
	SetIGlobalVar("temp.objective.130", 1);
	Suicide();
end;

function LaunchAttack40()
	LandReinforcement(40);
	SetIGlobalVar("temp.objective.140", 1);
	Suicide();
end;

function LaunchAttack50()
	LandReinforcement(50);
	SetIGlobalVar("temp.objective.150", 1);
	Suicide();
end;

function LaunchAttack51()
	LandReinforcement(51);
	SetIGlobalVar("temp.objective.151", 1);
	Suicide();
end;

function Sniper()
	AddIronMan(911);
	ChangeFormation(1024, 2);
	Suicide();
end;

function TobeDefeated()
	if ( GetIGlobalVar( "temp.Overlord.objective.0", 0) == 2) then
		Loose();
		Suicide();
	end;
end;

function Init()
	RunScript( "RevealObjective0", 3000); -- "Repell all enemy attacks"

	RunScript( "LaunchAttack10", 1000);
	RunScript( "LaunchAttack30", 10000);

	RunScript( "LaunchAttack11", 200000);
	RunScript( "LaunchAttack20", 210000);

	RunScript( "LaunchAttack21", 400000);
	RunScript( "LaunchAttack40", 410000);
	RunScript( "LaunchAttack50", 430000);

	RunScript( "LaunchAttack51", 600000);

	RunScript( "Reinforce1", 300000);

	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 2000);

	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Sniper", 1000);
end;
