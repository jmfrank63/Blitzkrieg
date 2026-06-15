function RushToCity1()
local A_Swarm = 3;
local A_Follow = 39;
	GiveCommand(A_Swarm, 50, 11500, 11000);
	Cmd(A_Swarm, 60, 11200, 9600);

	Cmd(A_Follow, 52, 50);
	Cmd(A_Follow, 62, 60);
	RunScript( "Remind1", 4000);
	RunScript( "Remind2", 4000);
	Suicide();
end;

function Remind1()
local A_Swarm = 3;
	if ( GetNUnitsInScriptGroup(50) <= 0) then
	if ( GetNUnitsInScriptGroup(52) > 0) then
		Cmd(A_Swarm, 52, 9100, 11400);
		QCmd(A_Swarm, 52, 11000, 11750);
		Suicide();
	else Suicide();
	end;
	end;
end;

function Remind2()
local A_Swarm = 3;
	if ( GetNUnitsInScriptGroup(60) <= 0) then
	if ( GetNUnitsInScriptGroup(62) > 0) then
		Cmd(A_Swarm, 62, 9400, 9500);
		QCmd(A_Swarm, 62, 12000, 10000);
		Suicide();
	else Suicide();
	end;
	end;
end;

function TobeDefeated()
	if ((GetNUnitsInScriptGroup (1000, 0) <= 0) or (GetIGlobalVar("temp.Stalingrad.objective.0",0) == 2)) then
		Loose();
		Suicide();
	end;
end;

function Objective0()
local num = GetNUnitsInScriptGroup(52) + GetNUnitsInScriptGroup(62);
local num2 = GetNUnitsInScriptGroup(50) + GetNUnitsInScriptGroup(60);

	if ((num <= 0) and (num2 <= 0)) then
		SetIGlobalVar("temp.Stalingrad.objective.0", 1);
		ObjectiveChanged(0, 1);
		Suicide();
	end;
	if (GetNUnitsInArea(1, "City") > 2) then
		SetIGlobalVar("temp.Stalingrad.objective.0", 2);
		ObjectiveChanged(0, 2);
		Suicide();
	end;

end;

function Objective1()
	if ( GetNUnitsInScriptGroup(500, 1) <= 0) then
		SetIGlobalVar("temp.Stalingrad.objective.1", 1);
		SetIGlobalVar("temp.Stalingrad.objective.150", 1);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 5000);
		RunScript( "CallHQ", 180000); -- crap
		Suicide();
	end;
end;

function Objective3()
	if ( GetIGlobalVar("temp.Stalingrad.objective.151", 0) == 1) then
	num = GetNUnitsInScriptGroup(10) + GetNUnitsInScriptGroup(20) + GetNUnitsInScriptGroup(31) + GetNUnitsInScriptGroup(41);
	if ( GetIGlobalVar("temp.objective.202", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(1002);
	end;
	if ( GetIGlobalVar("temp.objective.203", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(1003);
	end;
	if ( GetIGlobalVar("temp.objective.204", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(1004);
	end;
	if ( GetIGlobalVar("temp.objective.205", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(1005);
	end;
	if (num <= 0) then
		SetIGlobalVar("temp.Stalingrad.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
	end;
end;

-- objective 2
function CallHQ()
local A_Swarm = 3;
local A_Follow = 39;
	if ( GetIGlobalVar("temp.Stalingrad.objective.150", 0) == 1) then
		SetIGlobalVar("temp.Stalingrad.objective.2", 1);
		ObjectiveChanged(2, 1);

		RunScript( "RevealObjective3", 5000);
		RunScript( "CounterAttack", 6000);
		Suicide();
	end;
end;

function CounterAttack()
local num = GetIGlobalVar("template.hunted.killed", 0); -- get percentage of destroyed hunted units
	LandReinforcement( 1);
	if ( num < 100) then
		LandReinforcement(1002);
		SetIGlobalVar("temp.objective.202", 1);
	end;
	if ( num < 75) then
		LandReinforcement(1003);
		SetIGlobalVar("temp.objective.203", 1);
	end;
	if ( num < 50) then
		LandReinforcement(1004);
		SetIGlobalVar("temp.objective.204", 1);
	end;
	if ( num < 25) then
		LandReinforcement(1005);
		SetIGlobalVar("temp.objective.205", 1);
	end;
	RunScript( "CAcmds", 5000);
	Suicide();
end;

function CAcmds()
local A_Swarm = 3;
local A_Follow = 39;
	GiveCommand(A_Follow, 31, 10);
	if ( GetIGlobalVar("temp.objective.202", 0) == 1) then
		GiveCommand(A_Follow, 1002, 10);
	end;
	if ( GetIGlobalVar("temp.objective.203", 0) == 1) then
		GiveCommand(A_Follow, 1003, 10);
	end;
	GiveCommand(A_Follow, 41, 20);
	if ( GetIGlobalVar("temp.objective.204", 0) == 1) then
		GiveCommand(A_Follow, 1004, 20);
	end;
	if ( GetIGlobalVar("temp.objective.205", 0) == 1) then
		GiveCommand(A_Follow, 1005, 20);
	end;

	GiveCommand(A_Swarm, 10, 4200, 6900);
	GiveCommand(A_Swarm, 20, 6200, 6300);

	Cmd(A_Swarm, 501, 7000, 6000);
	QCmd(A_Swarm, 501, 5000, 6000);

	RunScript( "Remind3", 4000);
	RunScript( "Remind4", 4000);
	SetIGlobalVar("temp.Stalingrad.objective.151", 1);
	Suicide();
end;

function Remind3()
local A_Swarm = 3;
	if ( GetNUnitsInScriptGroup(10) <= 0) then
	if ( GetNUnitsInScriptGroup(31) > 0) then
		Cmd(3, 31, 4900, 6100);
	end;
	if ( ( GetIGlobalVar("temp.objective.202", 0) == 1) and (GetNUnitsInScriptGroup(1002) > 0)) then
		Cmd( A_Swarm, 1002, 4900, 6100);
	end;
	if ( ( GetIGlobalVar("temp.objective.203", 0) == 1) and (GetNUnitsInScriptGroup(1003) > 0)) then
		Cmd( A_Swarm, 1003, 4900, 6100);
	end;
	Suicide();
	end;
end;

function Remind4()
local A_Swarm = 3;
	if ( GetNUnitsInScriptGroup(20) <= 0) then
	if ( GetNUnitsInScriptGroup(41) > 0) then
		Cmd(3, 41, 6300, 6400);
	end;
	if ( ( GetIGlobalVar("temp.objective.204", 0) == 1) and (GetNUnitsInScriptGroup(1004) > 0)) then
		Cmd( A_Swarm, 1004, 6300, 6400);
	end;
	if ( ( GetIGlobalVar("temp.objective.205", 0) == 1) and (GetNUnitsInScriptGroup(1005) > 0)) then
		Cmd( A_Swarm, 1005, 6300, 6400);
	end;
	Suicide();
	end;
end;

function ToWin()
	if ((GetIGlobalVar("temp.Stalingrad.objective.0",0) * GetIGlobalVar("temp.Stalingrad.objective.1",0) * GetIGlobalVar("temp.Stalingrad.objective.2",0) *
	  GetIGlobalVar("temp.Stalingrad.objective.3", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Stalingrad.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Stalingrad.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Stalingrad.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Stalingrad.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function Init()
 	RunScript( "RushToCity1", 70000);

	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);

	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
--	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);

	RunScript( "RevealObjective0", 2000);
	RunScript( "RevealObjective1", 7000);
end;
