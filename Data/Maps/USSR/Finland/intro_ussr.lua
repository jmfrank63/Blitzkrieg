function ReinfInf()
	RunScript( "CheckInf", 3000);
	LandReinforcement(1);

	RunScript( "LostInf", 3000);
	Suicide();
end;

function LostInf()
	if ( GetNUnitsInScriptGroup(1001) <= 0) then
		RunScript( "ReinfInf", 3000);
		Suicide();
	end;
end;

function ReinfEnemy()
	RunScript( "CheckEnemy", 3000);
	LandReinforcement(10);
	Suicide();
end;

function CheckEnemy()
	SetIGlobalVar( "temp.enemy", 1);
	Suicide();
end;

function CheckInf()
	SetIGlobalVar( "temp.inf", 1);
	Suicide();
end;

function CheckSniper()
	SetIGlobalVar( "temp.sniper", 1);
	Suicide();
end;

function CheckGuns()
	SetIGlobalVar( "temp.guns", 1);
	Suicide();
end;

function ReinfGuns()
	RunScript( "CheckGuns", 3000);
	LandReinforcement(3);
	Suicide();
end;

function ReinfSniper()
	RunScript( "CheckSniper", 3000);
	LandReinforcement(2);
	Suicide();
end;

function ReinfCar()
	LandReinforcement(4);
	Suicide();
end;

function Objective0()
local ACTION_STOP = 9;
	if ( GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar( "temp.intro_ussr.objective.0", 1);
		ObjectiveChanged(0, 1);
		Cmd(ACTION_STOP, 999);

		RunScript( "ReinfInf", 5000);
		RunScript( "RevealObjective10", 10000);
		RunScript( "Objective10", 2000);
		Suicide();
	end;
end;

function Objective1()
local FORMATION_AGGRESSIVE = 3;
	if ( GetIGlobalVar( "temp.inf", 0) == 1) then
	if ( GetSquadInfo( 1001) == FORMATION_AGGRESSIVE) then
		SetIGlobalVar( "temp.intro_ussr.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 5000);
		RunScript( "Objective2", 2000);
		Suicide();
	end;
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(2, 1) <= 0) then
		SetIGlobalVar( "temp.intro_ussr.objective.2", 1);
		ObjectiveChanged(2, 1);
		ViewZone("House", 0);

		KillScript( "LostInf" );

		RunScript( "ReinfGuns", 5000);
		RunScript( "RevealObjective3", 10000);
		RunScript( "Objective3", 2000);
		Suicide();
	end;
end;

function Objective3()
local STATE_TOWED = 24;
	if ( GetIGlobalVar( "temp.guns", 0) == 1) then
	if ( GetUnitState( 1010) ~= STATE_TOWED) then
		SetIGlobalVar( "temp.intro_ussr.objective.3", 1);
		ObjectiveChanged(3, 1);

		RunScript( "ReinfEnemy", 5000);
		RunScript( "RevealObjective4", 10000);
		Suicide();
	end;
	end;
end;

function Objective4()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown", 0) == 0) then
		SetIGlobalVar( "temp.intro_ussr.objective.4", 1);
		ObjectiveChanged(4, 1);

		RunScript( "RevealObjective5", 5000);
		RunScript( "Objective5", 2000);
		Suicide();
	end;
end;

function Objective5()
local ACTION_STOP = 9;
	if ( GetIGlobalVar( "temp.enemy", 0) == 1) then
	if ( GetNUnitsInScriptGroup(3, 1) < 3) then
		SetIGlobalVar("temp.intro_ussr.objective.5", 1);
		ObjectiveChanged(5, 1);
		DeleteReinforcement(3);
		Cmd(ACTION_STOP, 1010);

	    	RunScript( "RevealObjective6", 5000);
		Suicide();
	end;
	end;
end;

function Objective6()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown", 0) == 0) then
		SetIGlobalVar( "temp.intro_ussr.objective.6", 1);
		ObjectiveChanged(6, 1);

		RunScript( "RevealObjective7", 5000);
		RunScript( "Objective7", 2000);
		Suicide();
	end;
end;

function Objective7()
	if ( (GetNUnitsInScriptGroup(4, 1) <= 0) and ( GetNUnitsInScriptGroup(5, 0) > 0)) then
		SetIGlobalVar("temp.intro_ussr.objective.7", 1);
		ObjectiveChanged(7, 1);

		RunScript( "ReinfSniper", 5000);
		RunScript( "ReinfCar", 5000);
		RunScript( "RevealObjective8", 10000);
		RunScript( "Objective8", 2000);
    		Suicide();
	end;
end;

function Objective8()
local STATE_REST_ON_BOARD = 3;
	if ( (GetNScriptUnitsInArea(1002, "Station") > 0) and ( GetUnitState( 1002) ~= STATE_REST_ON_BOARD)) then
		SetIGlobalVar("temp.intro_ussr.objective.8", 1);
		ObjectiveChanged(8, 1);

		RunScript( "RevealObjective9", 5000);
		RunScript( "Objective9", 2000);
		Suicide();
	end;
end;

function Objective9()
	if ( GetNUnitsInScriptGroup(6, 1) <= 0) then
		SetIGlobalVar("temp.intro_ussr.objective.9", 1);
		ObjectiveChanged(9, 1);
		Suicide();
	end;
end;

function Objective10()
	if ( GetAviationState( 0) > -1) then
		SetIGlobalVar("temp.intro_ussr.objective.10", 1);
		ObjectiveChanged(10, 1);
		SwitchWeatherAutomatic( 1);

		RunScript( "RevealObjective1", 5000);
		RunScript( "Objective1", 2000);
		Suicide();
	end;
end;

function ToWin()
local num = GetIGlobalVar("temp.intro_ussr.objective.0", 0) * GetIGlobalVar("temp.intro_ussr.objective.1", 0) * GetIGlobalVar("temp.intro_ussr.objective.2", 0) *
	GetIGlobalVar("temp.intro_ussr.objective.3", 0) * GetIGlobalVar("temp.intro_ussr.objective.5", 0) * GetIGlobalVar("temp.intro_ussr.objective.7", 0) *
	GetIGlobalVar("temp.intro_ussr.objective.8", 0) * GetIGlobalVar("temp.intro_ussr.objective.9", 0) * GetIGlobalVar("temp.intro_ussr.objective.10", 0);
	if ( num == 1) then
		ObjectiveChanged(12, 1);
		SetIGlobalVar( "temp.intro_ussr.objective.11", 1);
		SetIGlobalVar( "temp.intro_ussr.objective.12", 1);
		RunScript( "WinWin", 4000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.intro_ussr.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		RunScript( "BlinkSwarm", 4000);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar( "temp.intro_ussr.objective.1", 0) == 0) then
	local x, y = GetScriptAreaParams("Camera");

	x = Round(x / 1.4142);
	y = Round(y / 1.4142);

	local strQuery = "SetCamera(" .. x .. "," .. y .. ")";

		AskClient(strQuery);
	       	Trace(strQuery);
		ObjectiveChanged(1, 0);
		RunScript( "BlinkFormations", 4000);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.intro_ussr.objective.2", 0) == 0) then
	local x, y = GetScriptAreaParams("House");

	x = Round(x / 1.4142);
	y = Round(y / 1.4142);

	local strQuery = "SetCamera(" .. x .. "," .. y .. ")";

		AskClient(strQuery);
		Trace(strQuery);
		ViewZone("House", 1);

		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.intro_ussr.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.intro_ussr.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0);
		RunScript( "Objective4", 1000);
	end;
	Suicide();
end;

function RevealObjective5()
	if ( GetIGlobalVar("temp.intro_ussr.objective.5", 0) == 0) then
		ObjectiveChanged(5, 0);
		RunScript( "BlinkSupFire", 4000);
	end;
	Suicide();
end;

function RevealObjective6()
	if ( GetIGlobalVar("temp.intro_ussr.objective.6", 0) == 0) then
		ObjectiveChanged(6, 0);
		RunScript( "Objective6", 1000);
	end;
	Suicide();
end;

function RevealObjective7()
	if ( GetIGlobalVar("temp.intro_ussr.objective.7", 0) == 0) then
		ObjectiveChanged(7, 0);
	end;
	Suicide();
end;

function RevealObjective8()
	if ( GetIGlobalVar("temp.intro_ussr.objective.8", 0) == 0) then
		ObjectiveChanged(8, 0);
	end;
	Suicide();
end;

function RevealObjective9()
	if ( GetIGlobalVar("temp.intro_ussr.objective.9", 0) == 0) then
		ObjectiveChanged(9, 0);
	end;
	Suicide();
end;

function RevealObjective10()
	if ( GetIGlobalVar("temp.intro_ussr.objective.10", 0) == 0) then
		SwitchWeather( 0);
		SwitchWeatherAutomatic( 0);
		ObjectiveChanged(10, 0);
	end;
	Suicide();
end;

function BlinkSwarm()
local STATE_SWARM = 11;
	if (( GetUnitState( 999) == STATE_SWARM) or ( GetIGlobalVar( "temp.intro_ussr.objective.0", 0) == 1)) then
		Suicide();
	else
		AskClient( "HighlightControl(20003)" );
	end;
end;

function BlinkFormations()
local FORMATION_AGGRESSIVE = 3;
	if (( GetSquadInfo( 1001) == FORMATION_AGGRESSIVE) or ( GetIGlobalVar( "temp.intro_ussr.objective.1", 0) == 1)) then
		Suicide();
	else
		AskClient( "HighlightControl(20072)" );
		AskClient( "HighlightControl(20053)" );
	end;
end;

function BlinkSupFire()
local STATE_SUPPRESSIVE_FIRE = 34;
	if (( GetUnitState( 1010) == STATE_SUPPRESSIVE_FIRE) or ( GetIGlobalVar( "temp.intro_ussr.objective.5", 0) == 1)) then
		Suicide();
	else
		AskClient( "HighlightControl(20013)" );
	end;
end;

function NewSniper()
	if ( GetIGlobalVar( "temp.sniper", 0) == 1) then
	if ( GetNUnitsInScriptGroup(1002, 0) <= 0) then
		RunScript( "ReinfSniper", 0);
		ObjectiveChanged(11, 0);
		SetIGlobalVar( "temp.sniper", 0);
	end;
	end;
end;

function Round( N)
	SetIGlobalVar( "temp.0", N);
	return GetIGlobalVar( "temp.0", 0);
end;

function Init()
	RunScript( "Objective0", 2000);
--	RunScript( "Objective1", 2000);
--	RunScript( "Objective2", 2000);
--	RunScript( "Objective3", 2000);
--	RunScript( "Objective5", 2000);
--	RunScript( "Objective7", 2000);
--	RunScript( "Objective8", 2000);
--	RunScript( "Objective9", 2000);
--	RunScript( "Objective10", 2000);
	RunScript( "RevealObjective0", 2000);
	RunScript( "NewSniper", 2000);

	RunScript( "ToWin", 4000);
end;
