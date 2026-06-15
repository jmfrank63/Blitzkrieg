-- GLOBALS DESCRIPTION
-- 0 - unused objective
-- 1-14 - objectives
-- 100 - check var for last (tank) reinforcement
-- 101 - check var for reinforcement to be given after JS-2 and infantry destroyed
-- 102 - chack var for scout presence (1 - present)
-- 103 - check var for sniper state
-- 104 - check var for tank swap reinf state
-- 105 - check var for tank swap state
-- 106 - check var for "Karl ammo" objective
-- 107 - check var for tank swap reinf #3 arrived state
-- END OF DESCRIPTION
-- Written by Udot

-- infantry attacks
function Attack10()
local A_Swarm = 3;
	Cmd(A_Swarm, 10, 2000, 900);
	Suicide();
end;

-- JS-2 no.1 attacks
function Attack11()
local A_Swarm = 3;
	Cmd(A_Swarm, 11, 1850, 3600);
	QCmd(A_Swarm, 11, 1850, 900);
	Suicide();
end;

-- JS-2 no.2 attacks
function Attack12()
local A_Swarm = 3;
	Cmd(A_Swarm, 12, 2300, 3600);
	QCmd(A_Swarm, 12, 2300, 900);
	Suicide();
end;

-- JS-2 no.3 attacks
function Attack13()
local A_Swarm = 3;
	Cmd(A_Swarm, 13, 2000, 3600);
	QCmd(A_Swarm, 13, 2000, 900);
	Suicide();
end;

-- scout
function PA3BEDKA1()
local A_Scout = 21;
	EnableAviation(1, 0);
	Cmd(A_Scout, 10000, 1, 1900, 1500, 1700, 2000);
	SetIGlobalVar("temp.gameplay.objective.102", 1); -- check scout appeared
	RunScript("LandScoutInit", 115000);
	Suicide();
end;

-- scout
function PA3BEDKA()
local A_Scout = 21;
	if (GetIGlobalVar("temp.gameplay.objective.4", 0) == 0) then -- check if scout is destroyed ++
		Cmd(A_Scout, 10000, 1, 1900, 1500, 1700, 2000);
		SetIGlobalVar("temp.gameplay.objective.102", 1); -- check scout appeared
	end;
end;

-- scout land initiator / first land
function LandScoutInit()
	SetIGlobalVar("temp.gameplay.objective.102", 0); -- check scout landed
	if (GetIGlobalVar("temp.gameplay.objective.4", 0) == 0) then -- check if scout is destroyed
		RunScript("LandScout", 220000);
	end;
	Suicide();
end;

-- scout land test
function LandScout()
	SetIGlobalVar("temp.gameplay.objective.102", 0); -- check scout landed
	if (GetIGlobalVar("temp.gameplay.objective.4", 0) == 1) then -- check if scout is destroyed
		Suicide();
	end;
end;

-- JS-2 and enemy infantry destroyed -> sniper and Karl gun reinforcement -> Karl must survive
function Reinforcement1()
	if ((GetIGlobalVar("temp.gameplay.objective.1", 0) * GetIGlobalVar("temp.gameplay.objective.2", 0)) == 1) then
		LandReinforcement(1); -- squad, Pz IV, Karl gun arrived
		LandReinforcement(5); -- sniper #1 arrived
		RunScript( "CheckReinf1", 3000);

		RunScript("RevealObj10", 5000);
		RunScript("RevealObj4", 10000);
		Suicide();
	end;
end;

function CheckReinf1()
	SetIGlobalVar("temp.gameplay.objective.103", 1); -- check sniper #1 arrived
	SetIGlobalVar("temp.gameplay.objective.101", 1); -- check reinforcement
	Suicide();
end;

function RevealObj10() -- now 5
	if ( GetIGlobalVar( "temp.gameplay.objective.5", 0) == 0) then
		ObjectiveChanged(5, 0); -- reveal objective "Karl must survive" ++
	end;
	Suicide();
end;

function RevealObj4() -- now 6
	if ( GetIGlobalVar( "temp.gameplay.objective.6", 0) == 0) then
		ObjectiveChanged(6, 0); -- reveal objective "sneak into enemy rear area" ++
	end;
	Suicide();
end;

-- our reinforcement at station & enemy tank group arrived
function Reinforcement2()
local A_Move = 0;
local A_Rotate = 8;
local A_Ambush = 14;

	if (GetIGlobalVar("temp.gameplay.objective.12", 0) == 1) then --++

	RunScript("Poezd", 100);
	RunScript("SubReinf1", 1000);
	RunScript("SubReinf2", 4000);
	RunScript("SubReinf3", 7000);

	LandReinforcement(3); -- enemy troops arrived
--	SetIGlobalVar("temp.gameplay.objective.100", 1); -- check reinforcement
	Suicide();
	end;
end;

function Poezd()
	if ( GetNUnitsInArea(0, "PoezdArea") <= 0) then
		LandReinforcement(13);
		Suicide();
	end;
end;

function SubReinf1()
	LandReinforcement(10); -- our troops arrived #1
--	Cmd(0, 1100, 940, 6740);
	Suicide();
end;

function SubReinf2()
	LandReinforcement(11); -- our troops arrived #1
--	Cmd(0, 1101, 620, 6740);
	Suicide();
end;

function SubReinf3()
	LandReinforcement(12); -- our troops arrived #1
--	Cmd(0, 1102, 220, 6740);
	SetIGlobalVar("temp.gameplay.objective.100", 1); -- check reinforcement (the last)
	Suicide();
end;

function BlinkFighter()
	if ( GetAviationState( 0) == 1) then
		Suicide();
	else
		AskClient("HighlightControl(20031)");
	end;
end;

function BlinkBomber()
	if ( GetAviationState( 0) == 3) then
		Suicide();
	else
		AskClient("HighlightControl(20030)");
	end;
end;

-- infantry killed / enemy have captured platz
function Objective1()
	if (GetNUnitsInScriptGroup(10) <= 0) then
		SetIGlobalVar("temp.gameplay.objective.1", 1);
		ObjectiveChanged(1, 1);
		RunScript("Objective2reveal", 6000);
		RunScript("GiveTanks", 3000);

		RunScript("PA3BEDKA1", 1000);
		RunScript("PA3BEDKA", 220000);
		RunScript("Objective3reveal", 37000);

		Suicide();
	end;
	if (GetNScriptUnitsInArea(10, "Platz") > 0) then
		SetIGlobalVar("temp.gameplay.objective.1", 2);
		ObjectiveChanged(1, 2);
		Suicide();
	end;
end;

-- all JS-2 killed
function Objective2()
local num = GetNUnitsInScriptGroup(11) + GetNUnitsInScriptGroup(12) + GetNUnitsInScriptGroup(13);
	if (num <= 0) then
		SetIGlobalVar("temp.gameplay.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

-- scout destroyed
function Objective3() -- now 4
local ID_Scout = 10000;
	if (GetIGlobalVar("temp.gameplay.objective.102", 0) == 1) then -- check if scout appeared
	if (GetNUnitsInScriptGroup(ID_Scout) <= 0) then
		SetIGlobalVar("temp.gameplay.objective.4", 1); --+
		ObjectiveChanged(4, 1); --+
		DisableAviation(1, 0);

		KillScript( "BlinkFighter" );
		Suicide();
	end;
	end;
end;

-- sniper have sneaked in the rear area
function Objective4() -- now 6
local snipID = 1002;
	if (GetIGlobalVar("temp.gameplay.objective.101", 0) == 1) then -- check if reinforcement arrived
	if (GetIGlobalVar("temp.gameplay.objective.103", 0) == 3) then
		snipID = 1020;
	end;
	if (GetNScriptUnitsInArea(snipID, "Sniper") > 0) then
		SetIGlobalVar("temp.gameplay.objective.6", 1); --++
		ObjectiveChanged(6, 1); --++
		RunScript("RevealObj5", 5000);
		Suicide();
	end;
	end;
end;

-- reveal objective "destroy aaguns"
function RevealObj5() -- now 8
	if ( GetIGlobalVar( "temp.gameplay.objective.8", 0) == 0) then
		ObjectiveChanged(8, 0); --++
	end;
	Suicide();
end;

-- sniper must survive / give another sniper if killed
function Objective13() -- now 7
	if (GetIGlobalVar("temp.gameplay.objective.8", 0) == 0) then --++
		if (GetIGlobalVar("temp.gameplay.objective.103", 0) == 1) then -- sniper #1 arrived last
		if (GetNUnitsInScriptGroup(1002) <= 0) then -- sniper #1 killed
			ObjectiveChanged(7, 0); -- remind with objective "sniper must survive"++
			RunScript("GiveSniper", 7000); -- sniper #2 arrived
			SetIGlobalVar("temp.gameplay.objective.103", 2); -- check sniper #2 arrived
		end;
		else
		if (GetIGlobalVar("temp.gameplay.objective.103", 0) == 3) then -- sniper #2 arrived last
		if (GetNUnitsInScriptGroup(1020) <= 0) then -- sniper #2 killed
			ObjectiveChanged(7, 0); -- remind with objective "sniper must survive"++
			SetIGlobalVar("temp.gameplay.objective.103", 2);
			RunScript("GiveSniper", 7000); -- sniper #2 arrived once again
      		end;
		end;
		end;
	else
		Suicide();
	end;
end;

function GiveSniper()
	LandReinforcement(4);
	SetIGlobalVar("temp.gameplay.objective.103", 3);
	Suicide();
end;

-- 1003 #1 reinf tank, 1000 startup tank, 1010 #2 reinf tank, 1011 #3 reinf tank
function Objective12() -- now 3
	if (GetIGlobalVar("temp.gameplay.objective.12", 0) == 0) then -- check if "clear station" objective complete++
		if (GetIGlobalVar("temp.gameplay.objective.104", 0) == 1) then -- tank reinf #2 arrived last
			local total_tanks = GetNUnitsInScriptGroup(1010); -- default tank reinf is #2
			if (GetIGlobalVar("temp.gameplay.objective.101", 0) == 1) then -- reinforcement #1 arrived
				total_tanks = total_tanks + GetNUnitsInScriptGroup(1003);
			end;
			if (GetIGlobalVar("temp.gameplay.objective.107", 0) == 1) then -- reinforcement #3 arrived
				total_tanks = total_tanks + GetNUnitsInScriptGroup(1011); -- now we can add number of units from group 1011
			end;
			if (total_tanks <= 0) then
				RunScript("GiveTanks", 15000);
				SetIGlobalVar("temp.gameplay.objective.104", 0); -- tanks are on a way
				ObjectiveChanged(3, 2); --+
			end;
		end;
	else
		Suicide();
	end;
end;

function GiveTanks()
local A_Swarm = 3;
	if (GetIGlobalVar("temp.gameplay.objective.105", 0) == 0) then -- #2 tanks
		LandReinforcement(6); -- #2 tank reinf arrived
		SetIGlobalVar("temp.gameplay.objective.105", 1); -- check #2 tank reinf arrived
	else -- #3 tanks
		LandReinforcement(7); -- #3 tank reinf arrived
		SetIGlobalVar("temp.gameplay.objective.105", 0); -- check #3 tank reinf arrived
		SetIGlobalVar("temp.gameplay.objective.107", 1); -- #3 arrived!!!!! needed for tank sequence
	end;
	SetIGlobalVar("temp.gameplay.objective.104", 1);
	Suicide();
end;

function Objective14() -- now 11
	if (GetIGlobalVar("temp.gameplay.objective.101", 0) == 1) then
	if (GetNUnitsInScriptGroup(1001) > 0) then
	if (GetIGlobalVar("temp.gameplay.objective.106", 0) == 0) then
		if (GetNAmmo(1001) <= 0) then
			ObjectiveChanged(11, 0); --+
			SetIGlobalVar("temp.gameplay.objective.106", 1);
		end;
	else
		if (GetNAmmo(1001) > 0) then
			ObjectiveChanged(11, 1); --+
			SetIGlobalVar("temp.gameplay.objective.106", 1);
			Suicide();
		end;
	end;
	end;
	end;
end;

-- aaguns destroyed
function Objective5() -- now 8
	if ( GetNUnitsInScriptGroup(2711, 1) <= 0) then
		SetIGlobalVar("temp.gameplay.objective.8", 1); --+
		ObjectiveChanged(8, 1); --+
		RunScript("RevealObj6", 5000);
		Suicide();
	end;
end;

-- reveal objective "destroy artillery"
function RevealObj6() --now 9
	if ( GetIGlobalVar( "temp.gameplay.objective.9", 0) == 0) then
		ObjectiveChanged(9, 0); --+
--        	SwitchWeatherAutomatic(0);
		SwitchWeather( 0);
		RunScript( "SWA", GetIGlobalVar( "Weather.TimeToFadeOff", 0) + 1000);
		EnableAviation(0, 3);
		RunScript( "BlinkBomber", 4000);
	end;
	Suicide();
end;

-- artillery destroyed
function Objective6() -- now 9
	if ( GetNUnitsInScriptGroup(2712, 1) <= 0) then
		SetIGlobalVar("temp.gameplay.objective.9", 1); --+
		ObjectiveChanged(9, 1); --+
		RunScript("RevealObj7", 5000);
		SwitchWeatherAutomatic( 1);
		Suicide();
	end;
end;

-- reveal objective "clear DOT"
function RevealObj7() -- now 10
	if ( GetIGlobalVar( "temp.gameplay.objective.10", 0) == 0) then
		ObjectiveChanged(10, 0); --+
		ViewZone("Pillbox", 1);
	end;
		Suicide();
end;

-- dot cleared
function Objective7() -- now 10
	if (GetNUnitsInScriptGroup(300) <=0) then
		SetIGlobalVar("temp.gameplay.objective.10", 1); --+
		ObjectiveChanged(10, 1); --+
		RunScript("RevealObj8", 5000);
		ViewZone("Pillbox", 0);
		Suicide();
	end;
end;

-- reveal objective "clear station"
function RevealObj8() -- now 12
	if ( GetIGlobalVar( "temp.gameplay.objective.12", 0) == 0) then
		ObjectiveChanged(12, 0); --+
	end;
	Suicide();
end;

-- station cleared
function Objective8() -- now 12
	if (GetNUnitsInScriptGroup(301) <=0) then
		SetIGlobalVar("temp.gameplay.objective.12", 1); --+
		ObjectiveChanged(12, 1); --+
		RunScript("Reinforcement2", 10000);
		RunScript("RevealObj9", 13000);
		Suicide();
	end;
end;

-- reveal objective "destroy enemy tank group"
function RevealObj9() -- now 13
	if ( GetIGlobalVar( "temp.gameplay.objective.13", 0) == 0) then
		ObjectiveChanged(13, 0); --+
	end;
	Suicide();
end;

-- enemy tank group destroyed / our reinforcement destroyed
function Objective9() -- now 13
	if (GetIGlobalVar("temp.gameplay.objective.100", 0) == 1) then

local num = GetNUnitsInScriptGroup(21);

	if (num <= 0) then
		SetIGlobalVar("temp.gameplay.objective.13", 1); --+
		ObjectiveChanged(13, 1); --+
		RunScript("ToWin", 10000);
		Suicide();
	end;
local num_our_tank = GetNUnitsInScriptGroup(1100) + GetNUnitsInScriptGroup(1101) + GetNUnitsInScriptGroup(1102) + GetNUnitsInScriptGroup(1010) + GetNUnitsInScriptGroup(1011);
	if (num_our_tank <= 0) then
		SetIGlobalVar("temp.gameplay.objective.13", 2); --+
		ObjectiveChanged(13, 2); --+
		Suicide();
	end;
	end;
end;

-- Karl gun destroyed
function Objective10() -- now 5
	if (GetIGlobalVar("temp.gameplay.objective.101", 0) == 1) then

	if (GetNUnitsInScriptGroup(1001) <= 0) then
		SetIGlobalVar("temp.gameplay.objective.5", 2); --+
		ObjectiveChanged(5, 2); --+

		if ((GetIGlobalVar("temp.gameplay.objective.9", 0) == 1) and -- if "clear DOT" objective revealed++
		(GetIGlobalVar("temp.gameplay.objective.10", 0) == 0)) then -- and DOT is not cleared++
			SetIGlobalVar("temp.gameplay.objective.10", 2); --++
			ObjectiveChanged(10, 2); -- then objective "clear DOT" is failed  ++
		end;
		Suicide();
	end;
	end;
end;

function SWA()
       	SwitchWeatherAutomatic(0);
	Suicide();
end;

function GiveTrucksIfKilled()
local num;
	if ( GetIGlobalVar( "temp.31415926", 0) == 1) then
		num = GetNUnitsInScriptGroup(778);
	else
		num = GetNUnitsInScriptGroup(777);
	end;
	if ( num <= 0) then
		LandReinforcement(778);
		SetIGlobalVar( "temp.31415926", 1);
		Suicide();
	end;
end;

-- only when enemy tank group destroyed
function ToWin()
	if ((GetIGlobalVar("temp.gameplay.objective.13", 0) == 1) and ( GetIGlobalVar( "temp.lost", 0) == 0)) then --+
		SetIGlobalVar( "temp.gameplay.objective.5", 1);
		ObjectiveChanged(5, 1);
		Win(0);
		Suicide();
	end;
end;

function ToLoose()
	if ((GetIGlobalVar("temp.gameplay.objective.13", 0) == 2) or -- our tank reinforcement destroyed++
	(GetIGlobalVar("temp.gameplay.objective.5", 0) == 2) or -- Karl killed ++
	(GetIGlobalVar("temp.gameplay.objective.1", 0) == 2) or ( GetNUnitsInParty(0) <= 0)) then -- enemy infantry have entered location
		SetIGlobalVar( "temp.lost", 1);
		Loose();
		Suicide();
	end;
end;

function Objective01reveal()
--	ObjectiveChanged(0, 0); -- reveal startup objectives
	ObjectiveChanged(1, 0);
--	LandReinforcement(2712);

	Suicide();
end;

function Objective2reveal()
	if (GetIGlobalVar("temp.gameplay.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0); -- reveal objective "kill JS-2"
		Suicide();
	end;
end;

function Objective3reveal() -- now 4
	if (GetIGlobalVar("temp.gameplay.objective.4", 0) == 0) then --+
		ObjectiveChanged(4, 0); -- reveal objective "kill scout plane"++
		RunScript( "ForceCallFighters", 140000);
		RunScript( "BlinkFighter", 4000);
		Suicide();
	end;
end;

function DisableAvia()
	DisableAviation(0, 3);
	DisableAviation(1, 0);
	Suicide();
end;

-- stpd objective
function RevealObjective15()
	ObjectiveChanged(15, 0);
	Suicide();
end;

function Objective0()
	SetIGlobalVar("temp.gameplay.objective.0", 1);
	ObjectiveChanged(0, 1);
	Suicide();
end;

function ForceCallFighters()
	if ( ( GetNUnitsInScriptGroup(10000) > 0) and ( GetAviationState( 0) <= 0)) then
		Cmd( 20, 10555, 0, 2000, 1500);
		Suicide();
	end;
end;

function Init()
	RunScript("Attack10", 26000);
	RunScript("Attack11", 55000);
	RunScript("Attack12", 58000);
	RunScript("Attack13", 61000);
--	RunScript("LandScout", 150000);
	RunScript("Reinforcement1", 6000);
--	RunScript("Reinforcement2", 10000);
	RunScript("Objective01reveal", 5000);
	RunScript( "DisableAvia", 0);
--	RunScript("Objective0", 5000);
	RunScript("Objective1", 2000);
--	RunScript("Objective2reveal", 100000);
	RunScript("Objective2", 2000);
	RunScript("Objective3", 2000);
	RunScript("Objective4", 2000);
	RunScript("Objective5", 2000);
	RunScript("Objective6", 2000);
	RunScript("Objective7", 2000);
	RunScript("Objective8", 2000);
	RunScript("Objective9", 2000);
	RunScript("Objective10", 2000);
	RunScript("Objective12", 2000);
	RunScript("Objective13", 2000);
	RunScript("Objective14", 2000);
	RunScript( "RevealObjective15", 1000);
--	RunScript("ToWin", 10000);
	RunScript("ToLoose", 4100);
	RunScript( "GiveTrucksIfKilled", 6000);
end;
