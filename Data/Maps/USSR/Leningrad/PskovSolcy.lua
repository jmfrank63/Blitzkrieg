function Reinforce1E()
	LandReinforcement( 3);
	Suicide();
end;

function Reinforce2E()
local num = GetIGlobalVar("template.hunted.killed", 0); -- get percentage of destroyed hunted units
	LandReinforcement( 1);
	if ( num < 100) then
		LandReinforcement(1002);
		SetIGlobalVar("temp.objective.202", 1);
	end;
	if ( num < 66) then
		LandReinforcement(1003);
		SetIGlobalVar("temp.objective.203", 1);
	end;
	if ( num < 33) then
		LandReinforcement(1004);
		SetIGlobalVar("temp.objective.204", 1);
	end;
	RunScript( "CheckReinforce", 5000);
	Suicide();
end;

function CheckReinforce()
local num = GetIGlobalVar("temp.PskovSolcy.objective.100", 0);
	SetIGlobalVar("temp.PskovSolcy.objective.100", num + 1);
	Suicide();
end;

function Reinforce3E()
local num = GetIGlobalVar("template.hunted.killed", 0); -- get percentage of destroyed hunted units
	LandReinforcement( 4);
	if ( num < 100) then
		LandReinforcement(1005);
		SetIGlobalVar("temp.objective.205", 1);
	end;
	if ( num < 66) then
		LandReinforcement(1006);
		SetIGlobalVar("temp.objective.206", 1);
	end;
	if ( num < 33) then
		LandReinforcement(1007);
		SetIGlobalVar("temp.objective.207", 1);
	end;
	RunScript( "CheckReinforce", 5000);
	Suicide();
end;

function ReinforceUSSR()
local A_Move = 0;
	LandReinforcement( 2);
--	GiveCommand(A_Move, 10, 8500, 2200);
	SetIGlobalVar("temp.PskovSolcy.objective.101", 1);
	Suicide();
end;

function ReinforcePoezd()
	LandReinforcement(300);
	Suicide();
end;

function ToWin()
	if ((GetIGlobalVar("temp.PskovSolcy.objective.4", 0) * GetIGlobalVar("temp.PskovSolcy.objective.5", 0) * GetIGlobalVar("temp.PskovSolcy.objective.6", 0)) == 1) then
		RunScript( "WinWin", 4000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function TobeDefeated()
local Numunits = GetNUnitsInScriptGroup(1000, 0) + GetNUnitsInScriptGroup(1001, 0);
	if ( GetIGlobalVar("temp.PskovSolcy.objective.101", 0) == 1) then
		Numunits = Numunits + GetNUnitsInScriptGroup(10, 0);
	end;
	if ((Numunits <= 0 ) or ( GetIGlobalVar( "temp.PskovSolcy.objective.1", 0) == 2)) then
		Loose();
		Suicide();
	end;

end;

function Attack1()
local A_Swarm = 3;
local A_Follow = 39;
	GiveCommand( A_Swarm, 1, 5100, 3000);
	GiveQCommand( A_Swarm, 1, 7400, 3100);
	GiveQCommand( A_Swarm, 1, 9100, 2100);

	GiveCommand( A_Swarm, 51, 5100, 1600);
	GiveQCommand( A_Swarm, 51, 8800, 1600);

	GiveCommand( A_Follow, 2, 1);

	if ( GetIGlobalVar("temp.objective.202", 0) == 1) then
		GiveCommand( A_Follow, 1002, 1);
	end;

	if ( GetIGlobalVar("temp.objective.203", 0) == 1) then
		GiveCommand( A_Follow, 1003, 1);
	end;

	GiveCommand( A_Follow, 52, 51);

	if ( GetIGlobalVar("temp.objective.204", 0) == 1) then
		GiveCommand( A_Follow, 1004, 51);
	end;

	RunScript( "Remind1", 4000);
	RunScript( "Remind2", 4000);
	Suicide();
end;

function Remind1()
local A_Swarm = 3;
	if ( GetNUnitsInScriptGroup(1) <= 0) then
	if ( GetNUnitsInScriptGroup(2) > 0) then
		GiveQCommand( A_Swarm, 2, 9100, 2100);
	end;
	if ( ( GetIGlobalVar("temp.objective.202", 0) == 1) and (GetNUnitsInScriptGroup(1002) > 0)) then
		Cmd( A_Swarm, 1002, 9100, 2100);
	end;
	if ( ( GetIGlobalVar("temp.objective.203", 0) == 1) and (GetNUnitsInScriptGroup(1003) > 0)) then
		Cmd( A_Swarm, 1002, 9100, 2100);
	end;
	Suicide();
	end;
end;

function Remind2()
local A_Swarm = 3;
	if ( GetNUnitsInScriptGroup(51) <= 0) then
	if ( GetNUnitsInScriptGroup(52) > 0) then
		Cmd( A_Swarm, 52, 8800, 1600);
	end;
	if ( ( GetIGlobalVar("temp.objective.204", 0) == 1) and (GetNUnitsInScriptGroup(1004) > 0)) then
		Cmd( A_Swarm, 1004, 8800, 1600);
	end;
	Suicide();
	end;
end;

function Attack2()
local A_Swarm = 3;
	GiveCommand( A_Swarm, 4, 5100, 3000);
	GiveQCommand( A_Swarm, 4, 7400, 3100);
	GiveQCommand( A_Swarm, 4, 9100, 2100);

	if ( GetIGlobalVar("temp.objective.206", 0) == 1) then
		GiveCommand( A_Swarm, 1006, 5100, 3000);
		GiveQCommand( A_Swarm, 1006, 7400, 3100);
		GiveQCommand( A_Swarm, 1006, 9100, 2100);
	end;

	if ( GetIGlobalVar("temp.objective.207", 0) == 1) then
		GiveCommand( A_Swarm, 1007, 5100, 3000);
		GiveQCommand( A_Swarm, 1007, 7400, 3100);
		GiveQCommand( A_Swarm, 1007, 9100, 2100);
	end;

	GiveCommand( A_Swarm, 54, 5100, 1600);
	GiveQCommand( A_Swarm, 54, 8800, 1600);

	if ( GetIGlobalVar("temp.objective.205", 0) == 1) then
		GiveCommand( A_Swarm, 1005, 5100, 1600);
		GiveQCommand( A_Swarm, 1005, 8800, 1600);
	end;

	Suicide();
end;

--function StopEm()
--	if ( GetNUnitsInScriptGroup( 25 ) <= 0) then
--		KillScript("Reinforce3E");
--		KillScript("Attack3");
--		Suicide();
--	end;
--end;

-- "repel enemy attacks"
function Objective1()
local n = 2;
local str1 = "temp.objective.";
	if ( GetIGlobalVar("temp.PskovSolcy.objective.100", 0) == 2) then
	local Numunits = GetNUnitsInScriptGroup(2, 1) + GetNUnitsInScriptGroup(52, 1) + GetNUnitsInScriptGroup(4, 1) +
		GetNUnitsInScriptGroup(54, 1) + GetNUnitsInScriptGroup(1, 1) + GetNUnitsInScriptGroup(51, 1);
	while (n <= 7)
	do
		str = str1 .. (n + 200);
		if ( GetIGlobalVar( str, 0 ) == 1) then
			Numunits = Numunits + GetNUnitsInScriptGroup(1000 + n);
		end;
		n = n + 1;
	end;

	if ( Numunits <= 0) then
		ObjectiveChanged(1, 1);
		SetIGlobalVar("temp.PskovSolcy.objective.1", 1);

		RunScript("RevealObjective2", 5000);
		RunScript("RevealObjective3", 10000);
		Suicide();
	end;
	if ( GetNUnitsInArea(1, "Loose") > 0) then
		ObjectiveChanged(1, 2);
		SetIGlobalVar( "temp.PskovSolcy.objective.1", 2);
		Suicide();
	end;
	end;
end;

-- "enter village"
function Objective2()
	if ( GetNUnitsInScriptGroup(8, 1) <= 0) then
		SetIGlobalVar("temp.PskovSolcy.objective.2", 1);
		ObjectiveChanged(2, 1);

		RunScript("RevealObjective4", 5000);
		Suicide();
	end;
end;

-- clear of mines
function Objective3()
	if ( GetNUnitsInScriptGroup(350) <= 0) then
		SetIGlobalVar("temp.PskovSolcy.objective.3", 1);
		ObjectiveChanged(3, 1);
		RunScript( "ReinforcePoezd", 10000);
		Suicide();
	end;
end;

-- "destroy enemy defence line"
function Objective4()
	if ( GetNUnitsInScriptGroup(81, 1) <= 1) then
		SetIGlobalVar("temp.PskovSolcy.objective.4", 1);
		ObjectiveChanged(4, 1);

		RunScript("RevealObjective5", 5000);
		RunScript("RevealObjective6", 10000);
		Suicide();
	end;
end;

-- "destroy all enemy artillery batteries"
function Objective5()
	if (( GetNUnitsInScriptGroup(95, 1) + GetNUnitsInScriptGroup(5, 1)) <= 0) then
		SetIGlobalVar("temp.PskovSolcy.objective.5", 1);
		ObjectiveChanged(5, 1);
		Suicide();
	end;
end;

-- "capture or destroy the church"
function Objective6()
	if ( ((GetNUnitsInArea(0, "Church") > 0) and (GetNUnitsInScriptGroup(28, 1) <= 0)) or ( GetNUnitsInScriptGroup(2712) <= 0)) then
		SetIGlobalVar("temp.PskovSolcy.objective.6", 1);
		ObjectiveChanged(6, 1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0);
	end;
	Suicide();
end;

function RevealObjective5()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.5", 0) == 0) then
		ObjectiveChanged(5, 0);
	end;
	Suicide();
end;

function RevealObjective6()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.6", 0) == 0) then
		ObjectiveChanged(6, 0);
	end;
	Suicide();
end;

-- objective 0 "deploy your troops"
function CallHQ()
	if ( GetIGlobalVar("temp.PskovSolcy.objective.150", 0) == 0) then
		ObjectiveChanged(0, 1);
		SetIGlobalVar("temp.PskovSolcy.objective.0", 1);

		RunScript( "RevealObjective1", 10000);

		RunScript( "Reinforce1E", 11000);
		RunScript( "Reinforce2E", 11000);
		RunScript( "Reinforce3E", 240000);

		RunScript( "ReinforceUSSR", 150000);

		RunScript( "Attack1", 14000);
		RunScript( "Attack2", 243000);

		SetIGlobalVar("temp.PskovSolcy.objective.150", 1);
		Suicide();
	end;
end;

function Init()
	RunScript( "ToWin", 3000);
	RunScript( "TobeDefeated", 3000);

	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "Objective4", 2000);
	RunScript( "Objective5", 2000);
	RunScript( "Objective6", 2000);
	RunScript( "CallHQ", 120000); -- crap

	RunScript( "RevealObjective0", 3000);

end;
