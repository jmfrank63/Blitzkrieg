function RevealObjective()
	ObjectiveChanged( GetIGlobalVar( "temp.num", 0), 0);
	Suicide();
end;

-- moving squads
function Objective0()
local STATE_MOVE = 32;
	if ((GetUnitState(1000) == STATE_MOVE) and ( GetUnitState( 1001) == STATE_MOVE)) then
		ObjectiveChanged(0, 1);

		RunScript("Objective1", 3000);
		SetIGlobalVar( "temp.num", 1);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- move into truck
function Objective1()
local STATE_REST_ON_BOARD = 3;
	if ((GetUnitState(1000) == STATE_REST_ON_BOARD) and ( GetUnitState( 1001) == STATE_REST_ON_BOARD)) then
		ObjectiveChanged(1, 1);

		RunScript("Objective2", 3000);
		SetIGlobalVar( "temp.num", 2);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- moving (in trucks) to north
function Objective2()
local STATE_REST_ON_BOARD = 3;
	if ((GetNUnitsInArea(0, "Area2") >= 2) and (GetUnitState(1000) == STATE_REST_ON_BOARD) and ( GetUnitState( 1001) == STATE_REST_ON_BOARD)) then
		ObjectiveChanged(2, 1);

		RunScript("Objective3", 4000);
		SetIGlobalVar( "temp.num", 3);
		RunScript("RevealObjective", 4000);
		Suicide();
	end;
end;

-- leaving truck
function Objective3()
local STATE_REST_ON_BOARD = 3;
	AskClient("HighlightControl(20005)");
	if ((GetUnitState(1000) ~= STATE_REST_ON_BOARD) and (GetUnitState(1001) ~= STATE_REST_ON_BOARD)) then
		ObjectiveChanged(3, 1);
		ChangePlayer(1002, 2);   -- otbiraem gruzoviki
		Cmd(0, 1002, 1800, 400);

		RunScript("Objective5", 3000);
		SetIGlobalVar( "temp.num", 5);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- moving trucks through swamp (obsolete)
function Objective4()
	if (GetNScriptUnitsInArea(1002, "Area3") >= 2) then
		ObjectiveChanged(4, 1);
		ChangePlayer(1002, 2);   -- otbiraem gruzoviki
		Cmd(0, 1002, 1800, 400);

		RunScript("Objective5", 3000);
		SetIGlobalVar( "temp.num", 5);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- moving infantry through swamp
function Objective5()
	if (GetNUnitsInArea(0, "House2") >= 2) then
		ObjectiveChanged(5, 1);

		RunScript("Objective6", 3000);
		SetIGlobalVar( "temp.num", 6);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- moving infantry into building
function Objective6()
local STATE_REST_IN_BUILDING = 8;
	if ((GetNUnitsInArea(0, "House1") >= 2) and (GetUnitState(1000) == STATE_REST_IN_BUILDING) and (GetUnitState(1001) == STATE_REST_IN_BUILDING)) then
		ObjectiveChanged(6, 1);

		RunScript("Objective7", 3000);
		SetIGlobalVar( "temp.num", 7);
		RunScript("RevealObjective", 2000);
		Suicide();
	end
end;

-- leaving building
function Objective7()
local STATE_REST = 1;
	if (( GetNUnitsInScriptGroup(501, 0) <= 0) and (GetUnitState(1000) == STATE_REST) and (GetUnitState(1001) == STATE_REST)) then
		ObjectiveChanged(7, 1);

		RunScript("Objective8", 3000);
		SetIGlobalVar( "temp.num", 8);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- defend in building2
function Objective8()
local STATE_REST_IN_BUILDING = 8;
	if ((GetNUnitsInArea(0, "House3") >= 2) and (GetUnitState(1000) == STATE_REST_IN_BUILDING) and (GetUnitState(1001) == STATE_REST_IN_BUILDING)) then
		if ( GetIGlobalVar( "temp.check.2", 0) == 0) then
			RunScript("Reinf2", 200);
		else

		if (GetNUnitsInScriptGroup(2, 1) <= 0) then
			ObjectiveChanged(8, 1);

			RunScript("Objective9", 4000);
			SetIGlobalVar( "temp.num", 9);
			RunScript("RevealObjective", 2000);
			Suicide();
		end;
		end;
	end;
end;

-- march formation
function Objective9()
local ACTION_STOP = 9;
	if ((GetSquadInfo(1001) == 1) and (GetSquadInfo(1000) == 1)) then
	if (GetNUnitsInArea(0, "Area4") >= 1)  then
		ObjectiveChanged (9, 1);
		Cmd(ACTION_STOP, 1000);
		Cmd(ACTION_STOP, 1001);

		RunScript("Objective10", 3000);
		SetIGlobalVar( "temp.num", 10);
		RunScript("RevealObjective", 2000);
		RunScript( "MOCKBA", 2000);

		Suicide();
	end;
	else
		AskClient("HighlightControl(20072)");
		AskClient("HighlightControl(20051)");
	end;
end;

function MOCKBA()
local cond1 = 0;
	if ( GetIGlobalVar( "temp.tutorial4.objective.10", 0) == 1) then
		Suicide();
	else
	if ( GetIGlobalVar( "temp.opa1", 0) == 1) then
		if ( GetNUnitsInScriptGroup(443) <= 0) then
			cond1 = 1;
		else
			cond1 = 0;
		end;
	else
		if ((GetNUnitsInScriptGroup(1000) + GetNUnitsInScriptGroup(1001)) <= 0) then
			cond1 = 1;
		else
			cond = 0;
		end;
	end;

	if ( cond1 == 1) then
		LandReinforcement(443);
		AskClient( "SetCamera(680, 4060)" );
		SetIGlobalVar( "temp.opa1", 1);
	end;
	end;
end;

-- leja pod artobstrelom
function Objective10()
	if ((GetSquadInfo(1000) == 2) or (GetSquadInfo(1001) == 2) or ( GetSquadInfo(444) == 2) or ( GetSquadInfo(443) == 2)) then
		if ( GetIGlobalVar( "temp.check.3", 0) == 0) then
			RunScript( "Reinf3", 500);
			SetIGlobalVar( "temp.check.3", 1);
		end;
	if ( GetNUnitsInArea(0, "Area5") >= 1) then
		ObjectiveChanged(10, 1);
		SetIGlobalVar( "temp.tutorial4.objective.10", 1);
		DeleteReinforcement( 3);

		RunScript("Objective11", 3000);
		SetIGlobalVar( "temp.num", 11);
		RunScript("RevealObjective", 2000);
		RunScript( "Reinf4", 2000);

		RunScript( "ACTPAXAHb", 2000);
		Suicide();
	end;
	else
		AskClient("HighlightControl(20072)");
		AskClient("HighlightControl(20052)");
	end;
end;

function ACTPAXAHb()
local cond1 = 0;
	if ( GetIGlobalVar( "temp.opa", 0) == 1) then
		if ( GetNUnitsInScriptGroup(444) <= 0) then
			cond1 = 1;
		else
			cond1 = 0;
		end;
	else
	if ( GetIGlobalVar( "temp.opa1", 0) == 1) then
		if ( GetNUnitsInScriptGroup(443) <= 0) then
			cond1 = 1;
		else
			cond1 = 0;
		end;
	else
		if ((GetNUnitsInScriptGroup(1000) + GetNUnitsInScriptGroup(1001)) <= 0) then
			cond1 = 1;
		else
			cond = 0;
		end;
	end;
	end;

	if ( cond1 == 1) then
		LandReinforcement(444);
		AskClient( "SetCamera(1800, 1770)" );
		SetIGlobalVar( "temp.opa", 1);
	end;
end;

-- begom
function Objective11()
	if ((GetSquadInfo(1000) == 3) or (GetSquadInfo(1001) == 3) or ( GetSquadInfo(444) == 3) or ( GetSquadInfo(443) == 3)) then
	if ( GetNUnitsInScriptGroup(4) <= 0) then
		ObjectiveChanged(11, 1);

		RunScript("Objective12", 3000);
		SetIGlobalVar( "temp.num", 12);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
	else
		AskClient("HighlightControl(20072)");
		AskClient("HighlightControl(20053)");
	end;
end;

-- return to normal
function Objective12()
	if ((GetSquadInfo(1000) == 0) or (GetSquadInfo(1001) == 0) or ( GetSquadInfo(444) == 0) or ( GetSquadInfo(443) == 0)) then
		ChangePlayer(1000, 2);
		Cmd(0, 1000, 2050, 490);

		ChangePlayer(1001, 2);
		Cmd(0, 1001, 2050, 490);

		ChangePlayer( 444, 2);
		ChangePlayer( 443, 2);

		Cmd(0, 444, 2050, 490);
		Cmd(0, 443, 2050, 490);

		ObjectiveChanged(12, 1);
		RunScript("Reinf5_USA", 2000);
		RunScript( "SniperKilled", 4000);

		RunScript("Objective13", 3000);
		SetIGlobalVar( "temp.num", 13);
		RunScript("RevealObjective", 2000);
		Suicide();
	else
		AskClient("HighlightControl(20072)");
		AskClient("HighlightControl(20050)");
	end;
end;

function SniperKilled()
	if ( GetNUnitsInScriptGroup(1003) <= 0) then
		RunScript("Reinf5_USA", 1000);
	end;
end;

-- sniper hide
function Objective13()
	if (GetSquadInfo(1003) == 2) then
		ObjectiveChanged(13, 1);
		RunScript("Reinf6", 2000);

		RunScript("Objective14", 3000);
		SetIGlobalVar( "temp.num", 14);
		RunScript("RevealObjective", 2000);
		Suicide();
	else
		AskClient("HighlightControl(20072)");
		AskClient("HighlightControl(20054)");
	end;
end;

-- kill guns
function Objective14()
	if (GetNUnitsInScriptGroup(6, 1) <= 0) then
		ObjectiveChanged(14, 1);
		ChangePlayer(1003, 2);
		DeleteReinforcement( 6);
		RunScript("Reinf7_USA", 2000);

		RunScript("Objective16", 3000); -- 16
		SetIGlobalVar( "temp.num", 16); -- 16
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- default formation (obsolete)
function Objective15()
	if (GetSquadInfo(1003) == 0) then
		ObjectiveChanged(15, 1);
		ChangePlayer(1003, 2);
		RunScript("Reinf7_USA", 2000);

		RunScript("Objective16", 3000);
		SetIGlobalVar( "temp.num", 16);
		RunScript("RevealObjective", 2000);
		Suicide();
	else
		AskClient("HighlightControl(20072)");
		AskClient("HighlightControl(20050)");
	end;
end;

-- camouflage1
function Objective16()
	if (GetNUnitsInArea(0, "Zone4") >= 1) then
		Cmd (9, 1004);
		ObjectiveChanged(16, 1);

		RunScript("Objective17", 3000);
		SetIGlobalVar( "temp.num", 17);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- camuflage2
function Objective17()
	if (GetNUnitsInArea(0, "Zone6") >= 1) then
		Cmd (9, 1004);
		ObjectiveChanged(17, 1);

		RunScript("Objective18", 4000);
		SetIGlobalVar( "temp.num", 18);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- binocular1
function Objective18()
	AskClient("HighlightControl(20034)");
	if (GetUnitState(1004) == 33) then
		ObjectiveChanged (18, 1);
		ChangePlayer( 1004, 2);

		RunScript("Reinf8_USA", 2000);

		RunScript("Objective20", 3000);
		SetIGlobalVar( "temp.num", 20);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- binocular2 (on mountain) (obsolete)
function Objective19()
	if ((GetNUnitsInArea(0, "Area8") >= 1) and (GetUnitState(1004) == 33)) then
		ObjectiveChanged (19, 1);
		ChangePlayer( 1004, 2);

		RunScript("Reinf8_USA", 2000);

		RunScript("Objective20", 4000);
		SetIGlobalVar( "temp.num", 20);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- disband squad
function Objective20()
	AskClient("HighlightControl(20061)");
	if (GetSquadInfo(1005) == -1) then
		ObjectiveChanged(20, 1);

		RunScript("Objective21", 3000);
		SetIGlobalVar( "temp.num", 21);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- move squad into 3 buildings
function Objective21()
	if ((GetNUnitsInArea(0, "House10") >= 1) and (GetNUnitsInArea(0, "House11") >= 1) and (GetNUnitsInArea(0, "House12") >= 1)) then
		ObjectiveChanged(21, 1);

		RunScript("Objective22", 3000);
		SetIGlobalVar( "temp.num", 22);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- form squad
function Objective22()
	AskClient("HighlightControl(20062)");
	if (GetSquadInfo(1005) >= 0) then
		ObjectiveChanged(22, 1);
		RunScript("Objective23", 3000);
		SetIGlobalVar( "temp.num", 23);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- doiti do arii i jdat ataki1
function Objective23()
	if (GetNUnitsInArea(0, "Area5") >= 1) then
		ObjectiveChanged(23, 1);

		RunScript("Reinf9", 1000);
		RunScript("ObjectiveFictive", 2000);
		Suicide();
	end;
end;

-- otboi ataki1
function ObjectiveFictive()
	if (GetNUnitsInScriptGroup(9) <= 0) or (GetNUnitsInScriptGroup(1005) <= 0) then
		ChangePlayer(9, 2);
		ChangePlayer(1005, 2);

		RunScript("Reinf10_USA", 2000);

		RunScript("Objective24", 3000);
		SetIGlobalVar( "temp.num", 24);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

-- voiti v okop i jdat ataki2
function Objective24()
	if (GetUnitState(1006) == 9) then
		ObjectiveChanged(24, 1);
		RunScript("Reinf11", 1000);

		RunScript( "TestKill", 3000);
		RunScript("Objective25", 3000);
		SetIGlobalVar( "temp.num", 25);
		RunScript("RevealObjective", 2000);
		Suicide();
	end;
end;

function TestKill()
	if ( GetNUnitsInScriptGroup(1006) <= 0) then
		Reinf10_USA();
	end;
end;

-- otboi ataki2
function Objective25()
	if (GetNUnitsInScriptGroup(11) <= 0) then
		ObjectiveChanged(25, 1);
		RunScript("Victory", 5000);
		Suicide();
	end;
end;


-------------------------------------------------------------------------------------

function Victory()
	Win(0);
	Suicide();
end;

function Reinf2()
	LandReinforcement(2);
	RunScript( "Check2", 2000);
	Suicide();
end;

function Check2()
	SetIGlobalVar( "temp.check.2", 1);
	Suicide();
end;

function Reinf3()
	LandReinforcement(3);
--	SetIGlobalVar( "temp.check.3", 1);
	Suicide();
end;

function Reinf4()
	LandReinforcement(4);
	SetIGlobalVar( "temp.check.4", 1);
	Suicide();
end;

function Reinf5_USA()
	LandReinforcement(5);
	SetIGlobalVar( "temp.check.5", 1);
	AskClient( "SetCamera(2828,707)" );
	Suicide();
end;

function Reinf6()
	LandReinforcement(6);
	SetIGlobalVar( "temp.check.6", 1);
	Suicide();
end;

function Reinf7_USA()
	LandReinforcement(7);
	SetIGlobalVar( "temp.check.7", 1);
	Suicide();
end;

function Reinf8_USA()
	LandReinforcement(8);
	SetIGlobalVar( "temp.check.8", 1);
	AskClient( "SetCamera(800,4100)" );
	Suicide();
end;

function Reinf9()
	LandReinforcement(9);
	SetIGlobalVar( "temp.check.9", 1);
	Suicide();
end;

function Reinf10_USA()
	LandReinforcement(10);
	SetIGlobalVar( "temp.check.10", 1);
	AskClient( "SetCamera(2300,2200)" );
	Suicide();
end;

function Reinf11()
	LandReinforcement(11);
	SetIGlobalVar( "temp.check.11", 1);
	Suicide();
end;

function Guard1()
	Cmd(0, 1111, 4870, 3411);
	RunScript("Guard2", 4000);
	Suicide();
end;

function Guard2()
	Cmd(0, 1111, 4663, 3440);
	RunScript("Guard1", 4000);
	Suicide();
end;

function CheckLost()
	if ( GetNUnitsInParty(0) <= 0) then
		if (GetIGlobalVar("temp.step1",0)==1) then
			LandReinforcement(1000);
			AskClient("SetCamera(1600,160)");
			RunScript("CheckLost", 3000);
			Suicide();
		end;
		if (GetIGlobalVar("temp.step2",0)==1) then
			LandReinforcement(1004);
			AskClient("SetCamera(2678,427)");
			RunScript("CheckLost", 3000);
			Suicide();
		end;
--- locked
		if (GetIGlobalVar("temp.step3",0)==1) then
			RunScript("CheckLost", 3000);
			Suicide();
		end;
	end;
end;

function StopAction()
	Cmd(9, 66);
	Suicide();
end;

function Presets()
	SwitchWeather( 0);
	SwitchWeatherAutomatic( 0);
	Suicide();
end;

----------------------------------------------------------------------------------------

function Init()
	RunScript("RevealObjective", 1000);
	RunScript("Objective0", 1500);

	RunScript( "Presets", 500);
--	RunScript("CheckLost", 3000);
	RunScript("Guard1", 1000);
	SetCheatDifficultyLevel( 1 );
	SetIGlobalVar( "nogeneral_script", 1);
end;
