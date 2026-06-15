function BMmovefire()
local A_Rotate = 8;
local A_Uninstall = 18;
local A_Install = 17;
local A_ArtBomb = 16;

	if (GetNUnitsInScriptGroup (5) > 0) then
--		GiveCommand(A_Uninstall, 5);
		GiveQCommand(0, 5, 6900, 11050);
		GiveQCommand(0, 125, 6900, 11200);
--		GiveQCommand(A_Rotate, 5, 3000, 11100);
--		GiveQCommand(A_Install, 5);
		GiveQCommand(A_ArtBomb, 5, 3000, 11100);
		GiveQCommand(A_ArtBomb, 125, 3000, 11100);
		Suicide();
	end;
end;

function BMmoveout()
local A_Uninstall = 18;

	if (GetNUnitsInScriptGroup (5) > 0) then
--		GiveCommand(A_Uninstall, 5);
		GiveQCommand( 0, 5, 11000, 11000);
		Suicide();
	end;
end;

function Rush_KV1()
local A_Swarm = 3;

	if (GetNUnitsInScriptGroup (1) > 0) then
		GiveCommand( A_Swarm, 1, 6200, 8200);
		GiveQCommand( A_Swarm, 1, 6000, 9750);
		GiveQCommand( A_Swarm, 1, 4700, 11100);
		GiveQCommand( A_Swarm, 1, 3000, 11100);
	end;
	Suicide();
end;

function Rush_inf()
local A_Swarm = 3;

	if (GetNUnitsInScriptGroup (11) > 0) then
		GiveCommand( A_Swarm, 11, 500, 10600);
		GiveQCommand( A_Swarm, 11, 2900, 10000);
		Suicide();
	end;
end;

function Rush_T34_1()
local A_Swarm = 3;

	if (GetNUnitsInScriptGroup (2) > 0) then
		GiveCommand( A_Swarm, 2, 4000, 9700);
		Suicide();
	end;
end;

function Rush_T34_2()
local A_Swarm = 3;

	if (GetNUnitsInScriptGroup (3) > 0) then
		GiveCommand( A_Swarm, 3, 2300, 9900);
		Suicide();
	end;
end;

function Rush_T34_3()
local A_Swarm = 3;

	if (GetNUnitsInScriptGroup (4) > 0) then
		GiveCommand( A_Swarm, 4, 2300, 9900);
		Suicide();
	end;
end;

function RushStation()
local A_Swarm = 3;
	LandReinforcement(1);
	Suicide();
end;

function BombersToStation()
local A_Bombers = 36; -- ground attack plane
	GiveCommand( A_Bombers, 10000, 1, 10300, 3800);
	Suicide();
end;

function ToWin()
	if ( (GetIGlobalVar("temp.Kharkov43.objective.0", 0) * GetIGlobalVar("temp.Kharkov43.objective.1", 0) *
	  GetIGlobalVar("temp.Kharkov43.objective.2", 0)) == 1) then
		RunScript( "WinWin", 5000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function TobeDefeated()
	if ( (GetNUnitsInScriptGroup(1000) <= 0) or (GetNUnitsInScriptGroup(99) <= 2 ) or ( GetIGlobalVar("temp.Kharkov43.objective.3", 0) == 2) ) then
		Loose();
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar( "temp.Kharkov43.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		AddIronMan(100);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar( "temp.Kharkov43.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar( "temp.Kharkov43.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function Objective0()
local num = GetNUnitsInScriptGroup(1) + GetNUnitsInScriptGroup(2) + GetNUnitsInScriptGroup(3) + GetNUnitsInScriptGroup(4);

	if ( num <= 0) then
		SetIGlobalVar("temp.Kharkov43.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		Suicide();
	end;
end;

function Objective1()
local A_Swarm = 3;

	if (GetNScriptUnitsInArea(99, "Capturable") > 2) then
		ChangePlayer (7100, 0);
		SetIGlobalVar("temp.Kharkov43.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "ArmorCrew", 3000);
		RunScript( "RushStation", 60000);
--		RunScript( "BombersToStation", 50000);
		RunScript( "RevealObjective2", 100000);
		Suicide();
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(15, 1) <= 0) then
		SetIGlobalVar("temp.Kharkov43.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function Objective3()
	if ( GetNUnitsInScriptGroup(1002) <= 0) then
		SetIGlobalVar("temp.Kharkov43.objective.3", 2);
		ObjectiveChanged(3, 2);
		Suicide();
	end;
end;

function Sniper()
	ChangeFormation(1011, 2);
	Suicide();
end;

function ArmorCrew()
local n = 1;
	while ( n <= 5 ) do
		if (( GetNUnitsInScriptGroup( n + 5100 ) > 0) and ( GetNUnitsInScriptGroup( n + 5200 ) > 0 ) ) then
--			Cmd();
			Cmd(0, 5200 + n, GetObjCoord(5100 + n));
		end;
		n = n + 1;
	end;
	RunScript( "TestCrewArrived", 1000);
	Suicide();
end;

function TestCrewArrived()
local lenconst = 10000; -- 100^2
local n = 1;
local totalnum = 0;
local k = 0;
local ArmorListID = {};
local CrewListID = {};

	while ( n <= 5 ) do
		if ( GetNUnitsInScriptGroup( n + 5100, 0 ) <= 0 ) then
			k = k + 1;
			ArmorListID[k] = n + 5100;
			CrewListID[k] = n + 5200;
		end;
		n = n + 1;
	end;

	totalnum = k;

	n = 1;
	k = 0;

	while ( n <= totalnum ) do
		if ( GetNUnitsInScriptGroup( ArmorListID[n] ) > 0 ) then
			if ( GetUnitState(CrewListID[n]) == 1) then
				DeleteReinforcement( CrewListID[n] );
				ChangePlayer( ArmorListID[n], 0);
				k = k + 1;
--				Cmd(3, );
			end;
		end;
		n = n + 1;
	end;

	if ( k == totalnum ) then
		Suicide();
	end;
end;

function len2( ID1, ID2 )
local x1, y1 = GetObjCoord(ID1);
local x2, y2 = GetObjCoord(ID2);
	return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
end;

function SaveDot()
	if ( GetObjectHPs(15) < 3000) then
		LandReinforcement(2);
		Suicide();
	end;
end;

function Init()
	RunScript( "BMmovefire", 33000);
	RunScript( "BMmoveout", 123000);
	RunScript( "Rush_KV1", 240000);
	RunScript( "Rush_inf", 243000);
	RunScript( "Rush_T34_1", 248000);
	RunScript( "Rush_T34_2", 253000);
	RunScript( "Rush_T34_3", 273000);
--	RunScript( "BombersToStation", 60000);
	RunScript( "ToWin", 10000);
	RunScript( "TobeDefeated", 6000);

	RunScript( "RevealObjective0", 3000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
--	RunScript( "Sniper", 1000);
	RunScript( "SaveDot", 2000);
end;
