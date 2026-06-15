function Objective0()
local num = GetIGlobalVar("temp.objective.110", 0) * GetIGlobalVar("temp.objective.111", 0) *
	GetIGlobalVar("temp.objective.120", 0) * GetIGlobalVar("temp.objective.121", 0) *
	GetIGlobalVar("temp.objective.130", 0) * GetIGlobalVar("temp.objective.140", 0) *
	GetIGlobalVar("temp.objective.141", 0) * GetIGlobalVar("temp.objective.150", 0) *
	GetIGlobalVar("temp.objective.151", 0);

	if (num == 1) then
	local num2 = GetNUnitsInScriptGroup(10) + GetNUnitsInScriptGroup(11) + GetNUnitsInScriptGroup(20) + GetNUnitsInScriptGroup(21) + GetNUnitsInScriptGroup(30) +
		GetNUnitsInScriptGroup(40) + GetNUnitsInScriptGroup(41) + GetNUnitsInScriptGroup(50) +
		GetNUnitsInScriptGroup( 51 + GetIGlobalVar("temp.stack.2", 0) / 10);
	if (num2 <= 0) then
		ObjectiveChanged(0, 1);
		SetIGlobalVar("temp.BattleOfBulge.objective.0", 1);
		Suicide();
	end;
	end;

	if ( GetNUnitsInArea(1, "Town") > 1) then
		SetIGlobalVar("temp.BattleOfBulge.objective.0", 2);
		ObjectiveChanged(0, 2);
		Suicide();
	end;
end;

function Objective1()
	if ( GetIGlobalVar("temp.objective.151", 0) ~= 0) then
	local num = GetIGlobalVar("temp.stack.2", 0);
	if ( (GetNUnitsInScriptGroup(510 + num)) <= 0) then
		SetIGlobalVar("temp.BattleOfBulge.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
	end;

	if ( GetNUnitsInArea(1, "Town") > 1) then
		SetIGlobalVar("temp.BattleOfBulge.objective.1", 2);
		ObjectiveChanged(1, 2);
		Suicide();
	end;

end;

function ToWin()
	if ( (GetIGlobalVar("temp.BattleOfBulge.objective.0", 0) * GetIGlobalVar("temp.BattleOfBulge.objective.1", 0)) == 1) then
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
	if ( GetIGlobalVar("temp.BattleOfBulge.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.BattleOfBulge.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.BattleOfBulge.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function LaunchAttack10()
	LandReinforcement(10);
	RunScript( "CheckReinf10", 3000);
	RunScript( "Remind1", 5000);
	Suicide();
end;

function CheckReinf10()
	SetIGlobalVar("temp.objective.110", 1);
	Cmd (39, 10, 1);
	Suicide();
end;

function Remind1()
	if ( GetNUnitsInScriptGroup(1) <= 0) then
	if ( GetNUnitsInScriptGroup(10) > 0) then
		Cmd(3, 10, 2200, 7700);
		Suicide();
	else	Suicide();
	end;
	end;
end;

function LaunchAttack11()
	LandReinforcement(11);
	RunScript( "CheckReinf11", 3000);
	RunScript( "Remind11", 5000);
	Suicide();
end;

function CheckReinf11()
	SetIGlobalVar("temp.objective.111", 1);
	Cmd (39, 11, 2010);
	Suicide();
end;

function Remind11()
	if ( GetNUnitsInScriptGroup(2010) <= 0) then
	if ( GetNUnitsInScriptGroup(11) > 0) then
		Cmd(3, 11, 2200, 7700);
		Suicide();
	else	Suicide();
	end;
	end;
end;

function LaunchAttack20()
	LandReinforcement(20);
	SetSGlobalVar("temp.stack.1", "temp.objective.120");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function LaunchAttack21()
	LandReinforcement(21);
	SetSGlobalVar("temp.stack.1", "temp.objective.121");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function LaunchAttack30()
	LandReinforcement(30);
	SetSGlobalVar("temp.stack.1", "temp.objective.130");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function LaunchAttack40()
	LandReinforcement(40);
	RunScript( "CheckReinf40", 3000);
	RunScript( "Remind4", 5000);
	Suicide();
end;

function CheckReinf40()
	SetIGlobalVar("temp.objective.140", 1);
	Cmd(39, 40, 4);
	Suicide();
end;

function Remind4()
	if ( GetNUnitsInScriptGroup(4) <= 0) then
	if ( GetNUnitsInScriptGroup(40) > 0) then
		Cmd(3, 40, 1800, 11300);
		Suicide();
	else	Suicide();
	end;
	end;
end;

function LaunchAttack41()
	LandReinforcement(41);
	SetSGlobalVar("temp.stack.1", "temp.objective.141");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function LaunchAttack50()
	LandReinforcement(50);
	RunScript( "CheckReinf50", 3000);
	RunScript( "Remind5", 5000);
	Suicide();
end;

function CheckReinf50()
	SetIGlobalVar("temp.objective.150", 1);
	Cmd(39, 50, 5);
	Suicide();
end;

function Remind5()
	if ( GetNUnitsInScriptGroup(5) <= 0) then
	if ( GetNUnitsInScriptGroup(50) > 0) then
		Cmd(3, 50, 1500, 11800);
		Suicide();
	else	Suicide();
	end;
	end;
end;


function LaunchAttack51()
local num = RandomInt(2);
	LandReinforcement(51 + num);
	num = num * 10;
	SetIGlobalVar("temp.stack.2", num);
	RunScript( "CheckReinf51", 3000);
	Suicide();
end;

function CheckReinf51()
local num = GetIGlobalVar("temp.stack.2", 0);
	SetIGlobalVar("temp.objective.151", 1);
	Cmd(39, 511 + num, 510 + num); -- Wirbelwind follow Maus
	Suicide();
end;

function CheckReinf()
local strName = GetSGlobalVar("temp.stack.1", 0);
	SetIGlobalVar(strName, 1);
	Suicide();
end;

function DisableAvia()
	SwitchWeatherAutomatic(0);
	SwitchWeather(1);
	DiableAviation(0, 0);
	DiableAviation(0, 1);
	DiableAviation(0, 2);
	DiableAviation(0, 3);
	DiableAviation(0, 4);
	DiableAviation(1, 0);
	DiableAviation(1, 1);
	DiableAviation(1, 2);
	DiableAviation(1, 3);
	DiableAviation(1, 4);

	AddIronMan(911);
	Suicide();
end;

function EnableAvia()
	SwitchWeather(0);
	EnableAviation(0, 0);
	EnableAviation(0, 1);
	EnableAviation(0, 2);
	EnableAviation(0, 3);
	EnableAviation(0, 4);
	EnableAviation(1, 0);
	EnableAviation(1, 1);
	EnableAviation(1, 2);
	EnableAviation(1, 3);
	EnableAviation(1, 4);
	Suicide();
end;

function TobeDefeated()
	if ( (GetIGlobalVar( "temp.BattleOfBulge.objective.0", 0) == 2) or ( GetIGlobalVar( "temp.BattleOfBulge.objective.1", 0) == 2)) then
		Loose();
		Suicide();
	end;
end;

function Raid()
	if ( GetIGlobalVar( "temp.BattleOfBulge.objective.0", 0) ~= 1) then
		LandReinforcement(911);
	else Suicide();
	end;
end;

function Init()
	RunScript( "RevealObjective0", 3000); -- "Repell all enemy attacks"
--	RunScript( "RevealObjective1", 8000); -- ""
--	RunScript( "RevealObjective2", 13000); -- ""
	RunScript( "DisableAvia", 100);
	RunScript( "EnableAvia", 1000000);

	RunScript( "LaunchAttack10", 1000);
	RunScript( "LaunchAttack40", 20000);

	RunScript( "LaunchAttack20", 230000);
	RunScript( "LaunchAttack50", 200000);

	RunScript( "LaunchAttack21", 470000);
	RunScript( "LaunchAttack41", 500000);

	RunScript( "LaunchAttack11", 800000);
	RunScript( "LaunchAttack30", 820000);

	RunScript( "LaunchAttack51", 1000000);
	RunScript( "Reinforce1", 940000);

	RunScript( "ToWin", 4000);

	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
--	RunScript( "Objective2", 2000);
	RunScript( "TobeDefeated", 5000);
	RunScript( "Raid", 280000);
end;
