function StartAttack()
local A_Swarm = 3;
	GiveCommand( A_Swarm, 1, 1400, 9500);
	QCmd( A_Swarm, 1, 1700, 11600);
	Suicide();
end;

function NextAttack()
local A_Swarm = 3;
	GiveCommand( A_Swarm, 703, 7500, 11100);
	QCmd( A_Swarm, 703, 2900, 11100);

	GiveCommand( A_Swarm, 713, 8200, 11000);
	QCmd( A_Swarm, 713, 2900, 12000);

	Suicide();
end;

function NearBridge()
local A_Swarm = 3;
	if ((GetNUnitsInArea (0, "NearBridge") > 2) and ( GetNUnitsInScriptGroup(2) > 0)) then
		Cmd( A_Swarm, 2, 2000, 7700);
		QCmd( A_Swarm, 2, 1400, 9500);
		QCmd( A_Swarm, 2, 1700, 11600);
		Suicide();
	end;
end;

-- objective 2
function CallHQ()
local A_Swarm = 3;
local A_Move = 0;
	if ( GetIGlobalVar("temp.Kharkov42.objective.1", 0) == 1) then
	if ( GetIGlobalVar("temp.Kharkov42.objective.2", 0) == 0) then
		SetIGlobalVar("temp.Kharkov42.objective.2", 1);
		ObjectiveChanged(2, 1);

		LandReinforcement(100);
		RunScript( "AttackAll", 5000);

		RunScript( "EvacArtillery", 8000);
		RunScript( "RevealObjective3", 5000);
		Suicide();
	end;
	end;
end;

function AttackAll()
local A_Swarm = 3;
local A_Move = 0;
	Cmd(A_Swarm, 9, 2700, 6900);
	QCmd(A_Swarm, 9, 4600, 7960);
	QCmd(A_Swarm, 9, 7800, 7800);
	QCmd(A_Swarm, 9, 11100, 7300);

	Cmd(A_Move, 82, 2700, 6900);
	QCmd(A_Move, 82, 4600, 7960);
	QCmd(A_Move, 82, 7800, 7800);
	QCmd(A_Move, 82, 11100, 7300);

	Cmd(A_Swarm, 8, 3400, 4760);
	QCmd(A_Swarm, 8, 6200, 4700);
	QCmd(A_Swarm, 8, 11300, 5500);

	Cmd(A_Move, 81, 3400, 4760);
	QCmd(A_Move, 81, 6200, 4700);
	QCmd(A_Move, 81, 11300, 5500);

	Cmd(A_Swarm, 7, 4600, 2500);
	QCmd(A_Swarm, 7, 7900, 4600);
	QCmd(A_Swarm, 7, 11200, 6400);

	RunScript( "Objective3", 4000);

	Suicide();
end;

function EvacArtillery()
	LandReinforcement(200);
	RunScript( "GiveOrders", 4000);
	Suicide();
end;

function GiveOrders()
local A_Move = 0;
local A_TakeArt = 31;
local num = 1;
local total = 15;
	while (num < total)
	do
		numID = 200 + num;
		if ( GetNUnitsInScriptGroup(300 + num) > 0) then
			Cmd(A_TakeArt, numID, 300 + num);
			QCmd(A_Move, numID, 2700, 7700);
		else
			Cmd(A_Move, numID, 2700, 7700);
		end;
		QCmd(A_Move, numID, 4600, 7960);
		QCmd(A_Move, numID, 7800, 7800);
		QCmd(A_Move, numID, 11100, 7300);
		num = num + 1;
	end;
	Suicide();

end;

function NearSafeZone()
local A_GroundAttack = 36;
	if ( GetNUnitsInArea(0, "NearSafezone") > 1) then
		Cmd (A_GroundAttack, 10000, 1, 8200, 5000);
		Suicide();
	else
	if ( GetNUnitsInArea(0, "NearSafezone2") > 1) then
		Cmd (A_GroundAttack, 10000, 1, 7200, 8600);
		Suicide();
	end;
	end;
end;

function ToWin()
	Win(0);
	Suicide();
end;

function TobeDefeated()
	Loose();
	Suicide();
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Kharkov42.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Kharkov42.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();

end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Kharkov42.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();

end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Kharkov42.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();

end;

function Objective0()
	if ( GetNUnitsInScriptGroup(10, 0) <= 0) then
		ObjectiveChanged(0, 2);
		SetIGlobalVar("temp.Kharkov42.objective.0", 2);
		RunScript( "TobeDefeated", 5000);
		Suicide();
	end;
	if ( (GetNUnitsInScriptGroup(1) + GetNUnitsInScriptGroup(703) + GetNUnitsInScriptGroup(713)) <= 0) then
		ObjectiveChanged(0, 1);
		SetIGlobalVar("temp.Kharkov42.objective.0", 1);
		Suicide();
	end;
end;

function Objective1()
	if ( (GetNUnitsInScriptGroup(11, 1) + GetNUnitsInScriptGroup(2, 1)) <= 0) then
		ObjectiveChanged(1, 1);
		SetIGlobalVar("temp.Kharkov42.objective.1", 1);
		RunScript( "RevealObjective2", 5000);
		RunScript( "CallHQ", 180000); -- crap
		Suicide();
	end;
end;

function Objective3()
local num = GetNScriptUnitsInArea(9, "Survive") + GetNScriptUnitsInArea(8, "Survive") + GetNScriptUnitsInArea( 7, "Survive") +
		GetNScriptUnitsInArea(81, "Survive") + GetNScriptUnitsInArea(82, "Survive");
local num1 = GetNUnitsInScriptGroup(7, 1) + GetNUnitsInScriptGroup(8, 1) + GetNUnitsInScriptGroup(9, 1) + GetNUnitsInScriptGroup(81, 1) + GetNUnitsInScriptGroup(82, 1);
	if (num > 6 ) then
		SetIGlobalVar("temp.Kharkov42.objective.3", 2);
		ObjectiveChanged(3, 2);

		RunScript( "TobeDefeated", 10000);
		Suicide();
	end;

	if (num1 <= 6) then
		SetIGlobalVar("temp.Kharkov42.objective.3", 1);
		ObjectiveChanged(3, 1);

		RunScript( "ToWin", 10000);
		Suicide();
	end;
end;

function Presets()
	AddIronMan(1);
	AddIronMan(3);
	AddIronMan(703);
	AddIronMan(713);
	AddIronMan(1104);
	Suicide();
end;

--- Patrols ---------------------------------

function Patrol1()
local i = GetIGlobalVar( "temp.points1", 0);

	if ( GetNUnitsInScriptGroup(1101) > 0) then
	if ( GetNScriptUnitsInArea( 1101, "Patrol1" .. i ) > 0 ) then
		RunScript( "Patrol1I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol1I()
local i = GetIGlobalVar( "temp.points1", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1101) > 0) then
		Cmd ( 3, 1101, GetScriptAreaParams( "Patrol1" .. i ) );
		RunScript( "Patrol1", 2000);

		SetIGlobalVar( "temp.points1", i);
	end;
	Suicide();
end;

function Patrol2()
local i = GetIGlobalVar( "temp.points2", 0);

	if ( GetNUnitsInScriptGroup(1102) > 0) then
	if ( GetNScriptUnitsInArea( 1102, "Patrol2" .. i ) > 0 ) then
		RunScript( "Patrol2I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol2I()
local i = GetIGlobalVar( "temp.points2", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1102) > 0) then
		Cmd ( 3, 1102, GetScriptAreaParams( "Patrol2" .. i ) );
		RunScript( "Patrol2", 2000);

		SetIGlobalVar( "temp.points2", i);
	end;
	Suicide();
end;

function Patrol3()
local i = GetIGlobalVar( "temp.points3", 0);

	if ( GetNUnitsInScriptGroup(1103) > 0) then
	if ( GetNScriptUnitsInArea( 1103, "Patrol3" .. i ) > 0 ) then
		RunScript( "Patrol3I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol3I()
local i = GetIGlobalVar( "temp.points3", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1103) > 0) then
		Cmd ( 3, 1103, GetScriptAreaParams( "Patrol3" .. i ) );
		RunScript( "Patrol3", 2000);

		SetIGlobalVar( "temp.points3", i);
	end;
	Suicide();
end;

function Patrol4()
local i = GetIGlobalVar( "temp.points4", 0);

	if ( GetNUnitsInScriptGroup(1104) > 0) then
	if ( GetNScriptUnitsInArea( 1104, "Patrol4" .. i ) > 0 ) then
		RunScript( "Patrol4I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol4I()
local i = GetIGlobalVar( "temp.points4", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1104) > 0) then
		Cmd ( 3, 1104, GetScriptAreaParams( "Patrol4" .. i ) );
		RunScript( "Patrol4", 2000);

		SetIGlobalVar( "temp.points4", i);
	end;
	Suicide();
end;

function Patrol5()
local i = GetIGlobalVar( "temp.points5", 0);

	if ( GetNUnitsInScriptGroup(1105) > 0) then
	if ( GetNScriptUnitsInArea( 1105, "Patrol5" .. i ) > 0 ) then
		RunScript( "Patrol5I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol5I()
local i = GetIGlobalVar( "temp.points5", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1105) > 0) then
		Cmd ( 3, 1105, GetScriptAreaParams( "Patrol5" .. i ) );
		RunScript( "Patrol5", 2000);

		SetIGlobalVar( "temp.points5", i);
	end;
	Suicide();
end;

function Patrol6()
local i = GetIGlobalVar( "temp.points6", 0);

	if ( GetNUnitsInScriptGroup(1106) > 0) then
	if ( GetNScriptUnitsInArea( 1106, "Patrol6" .. i ) > 0 ) then
		RunScript( "Patrol6I", RandomInt(5000) + 3000 );
		Suicide();
	end;
	else Suicide();
	end;
end;

function Patrol6I()
local i = GetIGlobalVar( "temp.points6", 0) + 1;

	if (i > 5) then
		i = 1;
	end;
	if ( GetNUnitsInScriptGroup(1106) > 0) then
		Cmd ( 3, 1106, GetScriptAreaParams( "Patrol6" .. i ) );
		RunScript( "Patrol6", 2000);

		SetIGlobalVar( "temp.points6", i);
	end;
	Suicide();
end;

---------------------------------------------


function Init()
	RunScript( "Presets", 100);

	RunScript( "StartAttack", 20000);
	RunScript( "NextAttack", 320000);
--	RunScript( "NearBridge", 2000);
--	RunScript( "NearSafeZone", 2000);

	RunScript( "RevealObjective0", 3000);
	RunScript( "RevealObjective1", 8000);

	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
--	RunScript( "Objective3", 2000);

--	RunScript( "ToWin", 7000);
--	RunScript( "TobeDefeated", 6000);
--	RunScript( "AntiArti1", 210000);
--	RunScript( "AntiArti1Stop", 250000);
	RunScript( "Patrol1I", 2000);
	RunScript( "Patrol2I", 2000);
	RunScript( "Patrol3I", 2000);
	RunScript( "Patrol4I", 2000);
	RunScript( "Patrol5I", 2000);
	RunScript( "Patrol6I", 2000);
end;
