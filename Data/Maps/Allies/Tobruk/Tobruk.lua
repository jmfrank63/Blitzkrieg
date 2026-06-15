function TobeDefeated()
local num = GetNUnitsInScriptGroup (1000, 0) + GetNUnitsInScriptGroup(1001, 0);
	if ( num <= 0) then
		Loose();
		Suicide();
	end;
end;

function ToWin()
	if ( (GetIGlobalVar("temp.Tobruk.objective.2", 0) * GetIGlobalVar("temp.Tobruk.objective.0", 0) * GetIGlobalVar("temp.Tobruk.objective.1", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

-- destroy armor group
function Objective0()
	if (GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.Tobruk.objective.0", 1);
		ObjectiveChanged(0, 1);
		Suicide();
	end;
end;

-- destroy AAguns
function Objective1()
	if ( GetNUnitsInScriptGroup(2, 1) <= 0) then
		SetIGlobalVar("temp.Tobruk.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

-- deblockade the town
function Objective2()
	if ( (GetNScriptUnitsInArea( 100, "Town") > 2) or ( GetNUnitsInScriptGroup(99, 1) <= 10)) then
		SetIGlobalVar("temp.Tobruk.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

-- destroy artillery - secret objective
function Objective3()
	if ( (GetNUnitsInScriptGroup(4, 1) <= 0) and (GetUnitState(4) ~= 24)) then
		SetIGlobalVar("temp.Tobruk.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Tobruk.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Tobruk.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Tobruk.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function Patrol()
local A_Swarm = 0;
	if ( GetNUnitsInScriptGroup(101) > 0) then
		if (( GetNScriptUnitsInArea(101, "Point_B") > 0) and ( GetIGlobalVar("temp.Tobruk.objective.101", 0) == 0)) then
			Cmd(A_Swarm, 101, 3850, 2050);
			QCmd(A_Swarm, 101, 4020, 3060);
			QCmd(A_Swarm, 101, 3920, 5350);
			QCmd(A_Swarm, 101, 5120, 6050);
			QCmd(A_Swarm, 101, 5700, 6660);
			QCmd(A_Swarm, 101, 6050, 7650);
			SetIGlobalVar("temp.Tobruk.objective.101", 1); -- moving to "Point_A"
		else
		if (( GetNScriptUnitsInArea(101, "Point_A") > 0) and ( GetIGlobalVar("temp.Tobruk.objective.101", 0) == 1)) then
			Cmd(A_Swarm, 101, 5700, 6660);
			QCmd(A_Swarm, 101, 5120, 6050);
			QCmd(A_Swarm, 101, 3920, 5350);
			QCmd(A_Swarm, 101, 4020, 3060);
			QCmd(A_Swarm, 101, 3850, 2050);
			QCmd(A_Swarm, 101, 4160, 1360);
			SetIGlobalVar("temp.Tobruk.objective.101", 0); -- moving to "Point_B"
		end;
		end;
	else Suicide();
	end;
end;

-- tank rush check
function SomeCheck()
local A_Swarm = 3;
local strSectors = {"Sector_1", "Sector_2", "Sector_3", "Sector_4", "Sector_5", "Sector_6"};
local iCoord = {{6100, 7300}, {5500, 6200}, {5000, 5600}, {4300, 4900}, {4000, 3900}, {4000, 2700}};
local num = 1;
	if (GetIGlobalVar("temp.Tobruk.objective.0", 0) == 0) then
		while (num < 7)
		do
			if ( GetNScriptUnitsInArea( 1001, strSectors[num]) > 1) then
				Cmd(A_Swarm, 1, iCoord[num][1], iCoord[num][2]);
				Suicide();
				break;
			end;
			num = num + 1;
		end;
	else Suicide();
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
	RunScript( "RevealObjective0", 3000);
	RunScript( "RevealObjective1", 8000);
	RunScript( "RevealObjective2", 13000);
	RunScript( "Patrol", 10000);
--	RunScript( "SomeCheck", 3000);
end;
