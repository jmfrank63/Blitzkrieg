function TobeDefeated()
	if (GetNUnitsInScriptGroup (1000, 0) <= 0) then
		Loose();
		Suicide();
	end;
end;

function ToWin()
	if (( GetIGlobalVar("temp.Ardennes44.objective.2", 0) * GetIGlobalVar("temp.Ardennes44.objective.3", 0) *
	  GetIGlobalVar("temp.Ardennes44.objective.4", 0) * GetIGlobalVar("temp.Ardennes44.objective.5", 0)) == 1) then
		RunScript( "WinWin", 4000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function RevealObjective0()
	if ( GetIGlobalVar( "temp.Ardennes44.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar( "temp.Ardennes44.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar( "temp.Ardennes44.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar( "temp.Ardennes44.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function RevealObjective4()
	if ( GetIGlobalVar( "temp.Ardennes44.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0);
	end;
	Suicide();
end;

function RevealObjective5()
	if ( GetIGlobalVar( "temp.Ardennes44.objective.5", 0) == 0) then
		ObjectiveChanged(5, 0);
	end;
	Suicide();
end;

function NearBridge()
	if ( GetNScriptUnitsInArea( 1001, "NearBridge") > 0) then -- if Maus is near bridge then
		DamageObject(1234, 0); -- destroy bridge (ID = 1234)
		Suicide();
	end;
end;

-- destroy north (#1) group
function Objective0()
	if ( GetNUnitsInScriptGroup(1, 1) < 4) then
		SetIGlobalVar("temp.Ardennes44.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		RunScript( "Reinforce1", 10000);
		Suicide();
	end;
end;

-- repell enemy counterattack at north
function Objective1()
	if ( GetIGlobalVar("temp.Ardennes44.objective.101", 0) == 1) then
	if ( GetNUnitsInScriptGroup(10, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes44.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 5000);
		Suicide();
	end;
	end;
end;

-- destroy enemy long range artillery
function Objective2()
	if ( GetNUnitsInScriptGroup(5, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes44.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

-- destroy #2 group
function Objective3()
	if ( GetNUnitsInScriptGroup(2, 1) < 3) then
		SetIGlobalVar("temp.Ardennes44.objective.3", 1);
		ObjectiveChanged(3, 1);

		RunScript( "Reinforce2", 10000);
		Suicide();
	end;
end;

-- destroy #3 group
function Objective4()
	if ( GetNUnitsInScriptGroup(3, 1) < 3) then
		SetIGlobalVar("temp.Ardennes44.objective.4", 1);
		ObjectiveChanged(4, 1);
		RunScript( "Reinforce3", 10000);
		Suicide();
	end;
end;

-- capture general warehouses in southwest town
function Objective5()
	if ( GetNUnitsInScriptGroup(4, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes44.objective.5", 1);
		ObjectiveChanged(5, 1);
		Suicide();
	end;
end;

function Reinforce1()
	LandReinforcement(1);
	RunScript( "CheckReinforce1", 5000);
	Suicide();
end;

function CheckReinforce1()
	SetIGlobalVar("temp.Ardennes44.objective.101", 1);
	Suicide();
end;

function Reinforce2()
	LandReinforcement(2);
	Suicide();
end;

function Reinforce3()
	LandReinforcement(3);
	Suicide();
end;

function EnemyParadrop()
	if ( (GetIGlobalVar("temp.Ardennes44.objective.3", 0) * GetIGlobalVar("temp.Ardennes44.objective.4", 0)) == 1) then
		DisableAllUSA();
		RunScript( "CallAvia", 180000);
		Suicide();
	end;
end;

function CheckParadrop()
	if ( GetUnitState( 10001) > 0) then
		SwitchWeatherAutomatic(1); -- switch auto weather on
		Suicide();
	end;
end;

function FormationChange()
	ChangeFormation(9, 2);
	Suicide();
end;

function Paradrop()
local A_Para = 22;
	AddIronMan(10001); -- no supply vehicles for them
	Cmd(A_Para, 10000, 1, 13500, 5500);
	RunScript( "CheckParadrop", 5000);
	Suicide();
end;

function CallAvia()
	SwitchWeatherAutomatic(0); -- switch auto weather off
	SwitchWeather(0); -- switch weather of
	RunScript("Paradrop", GetIGlobalVar( "Weather.TimeToFadeOff", 0));
	Suicide();
end;

function DisableAllUSA()
	DiableAviation(1, 0);
	DiableAviation(1, 1);
	DiableAviation(1, 2);
	DiableAviation(1, 3);
	DiableAviation(1, 4);
	Suicide();
end;

function Init()
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "Objective4", 2000);
	RunScript( "Objective5", 2000);
	RunScript( "ToWin", 5000);
	RunScript( "TobeDefeated", 5000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "RevealObjective3", 8000);
	RunScript( "RevealObjective4", 13000);
	RunScript( "RevealObjective5", 18000);
	RunScript( "EnemyParadrop", 10000);
	RunScript( "NearBridge", 1000);
	RunScript( "FormationChange", 1000);
end;
