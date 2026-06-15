function Reinf0()
	LandReinforcement(100);
	AskClient( "SetCamera(5230,600)" );
	DeleteReinforcement( 1000);

	RunScript( "SaveAmmoAndHP", 2000);
	Suicide();
end;

function Reinf1()
	LandReinforcement(1);
	AskClient( "SetCamera(3750,2120)" );
	Suicide();
end;

function Reinf2()
	LandReinforcement(10);
	AskClient( "SetCamera(3750,2120)" );
	Suicide();
end;

function RevealObjective()
	ObjectiveChanged( GetIGlobalVar( "temp.num", 0), 0);
	Suicide();
end;

-- tell about general depots
function Objective0()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown", 0) == 0) then
		ObjectiveChanged(0, 1);

		RunScript("Objective1", 3000);
		SetIGlobalVar( "temp.num", 1);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- fire at general warehouse
function Objective1()
	if (GetObjectHPs(1) < 1000) then
		RunScript( "Reinf0", 2000);

		ObjectiveChanged(1, 1);

		RunScript("Objective4", 4000);
		SetIGlobalVar( "temp.num", 4);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- supply range (obsolete)
function Objective2()
	if ( GetNUnitsInArea(0, "Zone1") >= 3) then
		ObjectiveChanged(2, 1);

		RunScript("Objective3", 4000);

		Suicide();
	end;
end;

-- supply range
function Objective3()
local num = 1001;
	if (GetIGlobalVar( "temp.num", 0) == 3) then
	if (GetIGlobalVar("Mission.Current.ObjectiveShown", 0) == 0) then
		ObjectiveChanged(3, 1);

		RunScript("Objective4", 3000);
		SetIGlobalVar( "temp.num", 4);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
	else
	while (num < 1003)
	do
		if ( IsUnitUnderSupply(num) == 0) then
			SetIGlobalVar( "temp.num", 3);
			RunScript("RevealObjective", 2000);

			break;
		end;
		num = num + 1;
	end;
	end;
end;

-- temp depot
function Objective4()
	if ( GetNUnitsInScriptGroup(501, 0) == 1) then
		ObjectiveChanged(4, 1);
		RunScript( "Reinf1", 2000);

		RunScript("Objective5", 3000);
		SetIGlobalVar( "temp.num", 5);
		RunScript("RevealObjective", 2000);
		RunScript( "CheckOnTruck", 2000);
		RunScript( "BlinkSupply", 4000);
		Suicide();
	end;
end;

function BlinkSupply()
local STATE_RESUPPLY = 37;
	if ( GetUnitState( 100) == STATE_RESUPPLY) then
		Suicide();
	else
		AskClient( "HighlightControl(20027)" );
	end;
end;

function BlinkHR()
local STATE_HR = 38;
	if ( GetUnitState( 100) == STATE_HR) then
		Suicide();
	else
		AskClient( "HighlightControl(20036)" );
	end;
end;

function BlinkRepair()
local STATE_REPAIR = 36;
	if ( GetUnitState( 101) == STATE_REPAIR) then
		Suicide();
	else
		AskClient( "HighlightControl(20026)" );
	end;
end;

function BlinkDemine()
local STATE_DEMINE = 31;
	if ( GetUnitState( 101) == STATE_DEMINE) then
		Suicide();
	else
		AskClient( "HighlightControl(20022)" );
	end;
end;

function BlinkPlaceMine()
local STATE_PLACEMINE = 41;
	if ( GetUnitState( 101) == STATE_PLACEMINE) then
		Suicide();
	else
		AskClient( "HighlightControl(20071)" );
		AskClient( "HighlightControl(20021)" );
	end;
end;

function BlinkPlaceObj()
local STATE_PLACEOBJ = 40;
	if ( GetUnitState( 101) == STATE_PLACEOBJ) then
		Suicide();
	else
		AskClient( "HighlightControl(20070)" );
		AskClient( "HighlightControl(20029)" );
	end;
end;

function BlinkShells()
local SHELLTYPE_AGITATION = 1;
	if ( GetActiveShellType( 201) == SHELLTYPE_AGITATION) then
--		RunScript( "BlinkSupFire", 4000);
		Suicide();
	else
		AskClient( "HighlightControl(20073)" );
		AskClient( "HighlightControl(20056)" );
	end;
end;

-- using supply truck
function Objective5()
local num = 1001;
local str = "temp.params.";
local cnt = 0;
	while (num < 1003)
	do
		local str1 = str .. num;
		local num1 = num + 1000;
		local str2 = str .. num1;
		if ( GetNUnitsInScriptGroup(num) > 0) then
			ammo1, ammo2 = GetNAmmo(num);
		end;
		if (((ammo1 == GetIGlobalVar(str1, 0)) and (ammo2 == GetIGlobalVar(str2, 0))) or (GetNUnitsInScriptGroup(num, 0) <= 0)) then
			cnt = cnt + 1;
		else break;
		end;
		num = num + 1;
	end;
	if (cnt == 2) then
		ObjectiveChanged(5, 1);
		KillScript( "BlinkSupply" );

		RunScript( "BlinkHR", 4000);

		RunScript("Objective6", 3000);
		SetIGlobalVar( "temp.num", 6);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- human resupply
function Objective6()
	if ( GetNUnitsInScriptGroup(2, 0) > 0) then
		ObjectiveChanged(6, 1);
		RunScript( "Reinf2", 2000);
		KillScript( "BlinkHR" );

		RunScript( "BlinkRepair", 4000);

		RunScript("Objective9", 3000);
		SetIGlobalVar( "temp.num", 9);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

function CheckOnTruck()
local STATE_RESUPPLY = 37;
	if ( GetIGlobalVar( "temp.100", 0) == 0) then
		if ( GetUnitState(100) == STATE_RESUPPLY) then
			SetIGlobalVar("temp.100", 1);
		end;
	else
		if ( GetUnitState(100) == 0) then
			SetIGlobalVar( "temp.num", 8);
			RunScript("RevealObjective", 2000);
			RunScript("Objective8", 3000);

			Suicide();
		end;
	end;
end;

-- obsolete
function Objective7()
local STATE_HR = 38;
	if ( GetUnitState(100) == STATE_HR) then
		ObjectiveChanged(7, 1);
		RunScript( "RevealObjective9", 5000);
		Suicide();
	end;
end;

-- truck moving for resources
function Objective8()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown", 0) == 0) then
		ObjectiveChanged(8, 1);
		Suicide();
	end;
end;

-- repair units
function Objective9()
local STATE_REPAIR = 36;
local ACTION_MOVE = 0;
--	local num = 1000;
--	local str = "temp.tutorial6.objective.";
--	local cnt = 0;
--		while (num < 1003)
--		do
--			local num1 = num + 2000;
--			local str2 = str .. num1;
--			if ( GetNUnitsInScriptGroup(num) > 0) then
--				local hp = GetObjectHPs(num);
--			end;
--			if ((hp == GetIGlobalVar(str2, 0)) or (GetNUnitsInScriptGroup(num, 0) <= 0)) then
--				cnt = cnt + 1;
--			else break;
--			end;
--			num = num + 1;
--		end;
--		if (cnt == 3) then
--			SetIGlobalVar("temp.tutorial6.objective.9", 1);
--			ObjectiveChanged(9, 1);
--
--			RunScript( "RevealObjective10", 5000);
--			Suicide();
--		end;
	if ( GetUnitState(101) == STATE_REPAIR) then
		ObjectiveChanged(9, 1);
		KillScript( "BlinkRepair" );

		ChangePlayer( 1001, 2);
		ChangePlayer( 1002, 2);
		ChangePlayer( 100, 2);
		ChangePlayer( 2, 2);
		Cmd(ACTION_MOVE, 1001, 530, 2200);
		Cmd(ACTION_MOVE, 1002, 530, 2200);
		Cmd(ACTION_MOVE, 100, 530, 2200);

		SetIGlobalVar( "temp.num", 10);
		RunScript("RevealObjective", 2000);
		RunScript("Objective10", 3000);
		Suicide();
	end;
end;

-- repair bridge
function Objective10()
	if ( GetNUnitsInScriptGroup(502) > 0) then
		ObjectiveChanged(10, 1);

		RunScript( "BlinkDemine", 4000);

		SetIGlobalVar( "temp.num", 11);
		RunScript("RevealObjective", 2000);
		RunScript("Objective11", 3000);
		Suicide();
	end;
end;

-- demine
function Objective11()
	if (( GetNUnitsInScriptGroup(500) <= 0) or (GetNMinesInScriptArea("Mines") <= 0)) then
		ObjectiveChanged(11, 1);
		KillScript( "BlinkDemine" );

		RunScript( "BlinkPlaceMine", 4000);

		SetIGlobalVar( "temp.num", 12);
		RunScript("RevealObjective", 2000);
		RunScript("Objective12", 3000);
		Suicide();
	end;
end;

-- place mine
function Objective12()
	if (GetNMinesInScriptArea("BuildArea") > 0) then
		ObjectiveChanged(12, 1);
		KillScript( "BlinkPlaceMine" );

		RunScript( "BlinkPlaceObj", 4000);

		SetIGlobalVar( "temp.num", 13);
		RunScript("RevealObjective", 2000);
		RunScript("Objective13", 3000);
		Suicide();
	end;
end;

-- building eng
function Objective13()
	if ( (GetNTrenchesInScriptArea("BuildArea") > 0) or (GetNAntitankInScriptArea("BuildArea") > 0) or (GetNAPFencesInScriptArea("BuildArea") > 0)) then
		ObjectiveChanged(13, 1);

		SetIGlobalVar( "temp.num", 14);
		RunScript("RevealObjective", 2000);
		RunScript("Objective14", 3000);
		Suicide();
	end;
end;

-- bridge project
function Objective14()
	if ( GetObjectHPs(504) > 0) then
		ObjectiveChanged(14, 1);

--		ChangePlayer( 101, 2);

--		AskClient( "SetCamera(2900,6430)" );
--		LandReinforcement(911);

		RunScript( "Victory", 4000);
		Suicide();
	end;
end;

-- morale
function Objective15()
	if ( GetNUnitsInScriptGroup(912) <= 0) then
		Win(0);
		Suicide();
	end;
end;

function View()
local x, y = 566, 5586;
local strQuery = "SetCamera(" .. x .. "," .. y .. ")";
	AskClient( strQuery );
	ViewZone("Zone2", 1);
	Suicide();
end;

-- enemy morale
function Objective16()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown", 0) == 0) then
		ObjectiveChanged(16, 1);

		RunScript( "Reinf3", 2000);

		RunScript( "BlinkShells", 4000);

		SetIGlobalVar( "temp.num", 17);
		RunScript("RevealObjective", 2000);
		RunScript("Objective17", 2000);
		Suicide();
	end;
end;

-- morale shells
function Objective17()
	if ( GetUnitMorale(4) * 100 <= 33) then
		ObjectiveChanged(17, 1);

		SetIGlobalVar( "temp.num", 18);
		RunScript("RevealObjective", 2000);
		RunScript("Objective18", 2000);

		RunScript( "View", 2000);

		RunScript( "T_34", 2000);
		LandReinforcement(3);
		Suicide();
	end;
end;

-- general's car
function Objective18()
	if ( GetNUnitsInScriptGroup(4) <= 0) then
		RunScript( "Victory", 4000);
		Suicide();
	end;
end;

function T_34()
	if ( GetNUnitsInScriptGroup(505) <= 0) then
		LandReinforcement(911);
		Suicide();
	end;
end;

function Victory()
	Win(0);
	Suicide();
end;

function Reinf3()
	LandReinforcement(2);
	AskClient("SetCamera(3630,6150)");
	Suicide();
end;

function SaveAmmoAndHP()
local num = 1001;
local str = "temp.params.";
	while (num < 1003)
	do
		local str1 = str .. num;
		local num1 = num + 1000;
		local str2 = str .. num1;
		num1 = num1 + 1000;
--		local str3 = str .. num1;
		local ammo1, ammo2 = GetNAmmo(num);
--		local hp = GetObjectHPs(num);
		SetIGlobalVar(str1, ammo1);
		SetIGlobalVar(str2, ammo2);
--		SetIGlobalVar(str3, hp);
		num = num + 1;
	end;
	Suicide();
end;

function DestroyBridges()
	DamageObject(502, 0);
	LandReinforcement(1234);

	SwitchWeather( 0);
	SwitchWeatherAutomatic( 0);

	Suicide();
end;

function Init()
	RunScript( "DestroyBridges", 2000);

	RunScript( "Objective0", 4000);
	RunScript( "RevealObjective", 2000);
	SetCheatDifficultyLevel( 1 );
	SetIGlobalVar( "nogeneral_script", 1);
end;
