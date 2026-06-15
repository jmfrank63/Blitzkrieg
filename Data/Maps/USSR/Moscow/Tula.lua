-- rush to station
function RushToStation1()
local A_Swarm = 3;
local A_Follow = 39;
	GiveCommand(A_Swarm, 1, 700, 1900);
	GiveCommand(A_Follow, 3, 1);
	Suicide();
end;

function RushToStation2()
local A_Swarm = 3;
	GiveCommand(A_Swarm, 2, 2200, 3400);
	GiveQCommand(A_Swarm, 2, 3100, 1600);
	Suicide();
end;

function Reinforce1()
local A_Swarm = 3;
	if ( GetNUnitsInArea(0, "NearBridges") > 0) then
		LandReinforcement(1);
--		GiveCommand(A_Swarm, 11, 2000, 1600);
--		GiveQCommand(A_Swarm, 11, 2000, 5000);
--		if ( GetIGlobalVar("Tula.objective.0", 0) == 0) then
--			SetIGlobalVar("Tula.objective.0", 2);
--			ObjectiveChanged(0, 2);
--		end;
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
	if ((GetIGlobalVar("temp.Tula.objective.0", 0) * GetIGlobalVar("temp.Tula.objective.1", 0) * GetIGlobalVar("temp.Tula.objective.2", 0)) == 1) then
		RunScript( "WinWin", 4000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function BridgeSecure()
	if (GetNUnitsInArea(0, "Objective1") > 0) then
		Cmd(3, 912, 5300, 1600); -- send SdKfz222
		QCmd(14, 912);
		Suicide();
	end;
end;
function Objective0()
	if ( GetIGlobalVar("temp.Tula.objective.0", 0) == 0) then
	if ((GetNUnitsInArea(0, "Objective1") > 0) and ( GetNUnitsInScriptGroup(911, 1) <= 0)) then
		SetIGlobalVar("temp.Tula.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript("RevealObjective1", 5000);
		Suicide();
	end;
	else Suicide();
	end;
end;

function Objective1()
	if ( GetIGlobalVar( "temp.Tula.objective.0", 0) == 1) then
	if ((GetNUnitsInArea(0, "Station") > 0) or ( GetNUnitsInScriptGroup(1004, 1) <= 0)) then
		SetIGlobalVar("temp.Tula.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript("RevealObjective2", 5000);
		RunScript("RushToStation1", 5000);
		RunScript("RushToStation2", 25000);
		Suicide();
	end;
	end;
end;

function Objective2()
	if ((GetNUnitsInScriptGroup(1, 1) + GetNUnitsInScriptGroup(2, 1) + GetNUnitsInScriptGroup(3, 1)) <= 0) then
		if ( GetIGlobalVar("temp.Tula.objective.1", 0) == 1) then
			ObjectiveChanged(2, 1);
		end;
		SetIGlobalVar("temp.Tula.objective.2", 1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Tula.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Tula.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Tula.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function Init()
--	RunScript( "RushToStation", 5000);
	RunScript( "Reinforce1", 2000);
	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "BridgeSecure", 2000);

	RunScript( "RevealObjective0", 2000);
end;
