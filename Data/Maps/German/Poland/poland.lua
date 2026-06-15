-- bridge to be exploded / objective "sniper sabotage"
function Objective0()
local num = GetNUnitsInArea(0, "Bridgepost");
	if (num > 0) then
		if ( GetIGlobalVar("temp.poland.objective.0", 0) == 0) then
			SetIGlobalVar("temp.poland.objective.0", 2);
			ObjectiveChanged(0, 2);
		end;
		-- explode bridge
		DamageObject(2000, 0);

		Suicide();
	else
	if (GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.poland.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		Suicide();
	end;
	end;
end;

function Objective0a()
	if ( GetIGlobalVar("temp.poland.objective.0", 0) == 0) then
		if (( GetIGlobalVar("temp.objective.1", 0) == 1) and ( GetNUnitsInScriptGroup(10001) <= 0)) then
			SetIGlobalVar("temp.poland.objective.0", 2);
			ObjectiveChanged(0, 2);

			RunScript( "RevealObjective1", 5000);
			Suicide();
		end;
	end;
end;

function IsBridgeAlive()
	if ( GetNUnitsInScriptGroup(2000) <= 0) then
		SetIGlobalVar("temp.objective.2", 1);
		RunScript( "RevealObjective4", 5000);
		Suicide();
	end;
end;

function Paradrop()
local A_Para = 22;
	EnableAviation(0, 2); -- enable paradropper
	Cmd(A_Para, 10000, 0, 1300, 3050); -- 1370, 3700
	DisableAviation(0, 2); -- disable paradropper
	RunScript( "CheckPara", 2000);
--	RunScript( "EnableAvia", 300000);
 	Suicide();
end;

function CheckPara()
	if ( GetUnitState( 10001) == 27) then
		SetIGlobalVar("temp.objective.1", 1);
		SwitchWeatherAutomatic(1);
		Suicide();
	end;
end;

-- objective "bridge guard"
function Objective1()
	if (GetNUnitsInScriptGroup(4, 1) <= 1) then
		SetIGlobalVar("temp.poland.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 5000);
		Suicide();
	end;
end;

-- objective "destroy polish defence"
function Objective2()
	if ((GetNUnitsInScriptGroup(2, 1) < 3) and (GetNUnitsInScriptGroup(3, 1) <= 0)) then
		SetIGlobalVar("temp.poland.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

-- secret objective accomplished ("destroy all enemy troops")
function Objective3()
	if (GetNUnitsInParty(1) <= 0) then
		SetIGlobalVar("temp.poland.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
end;

function Objective4()
	if ( GetIGlobalVar("temp.objective.2", 0) == 1) then
		if ( GetNUnitsInScriptGroup(2000) > 0) then
			SetIGlobalVar("temp.poland.objective.4", 1);
			ObjectiveChanged(4, 1);

			if ( GetIGlobalVar("temp.objective.3", 0) == 0) then
				RunScript( "RevealObjective1", 5000);
			end;
			Suicide();
		end;
	end;
end;

function Reinforcement1()
	if (GetNUnitsInScriptGroup(1000, 0) < 5) then
		LandReinforcement(1);
		Suicide();
	end;
end;

function ToWin()
	if ( (GetIGlobalVar("temp.poland.objective.1", 0) * GetIGlobalVar("temp.poland.objective.2", 0)) == 1) then
		RunScript( "WinWin", 5000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
       	Suicide();
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.poland.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0); -- reveal objective "eliminate sentry"
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.poland.objective.1", 0) == 0) then
		SetIGlobalVar("temp.objective.3", 1);
		ObjectiveChanged(1, 0); -- reveal objective "destroy bridge guard"
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.poland.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0); -- reveal objective "destroy polish defence"
		Suicide();
	end;
	Suicide();
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.poland.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0); -- reveal objective "repair bridge"
	end;
	Suicide();
end;

function DisableAvia()
	SwitchWeatherAutomatic(0);
	SwitchWeather(0);
	DiableAviation(0, 2); -- paradropper
	DiableAviation(0, 0);
	DiableAviation(0, 1);
	DiableAviation(0, 3);
	DiableAviation(0, 4);
	Suicide();
end;

function EnableAvia()
	EnableAviation(0, 0);
	EnableAviation(0, 1);
	EnableAviation(0, 3);
	EnableAviation(0, 4);
	Suicide();
end;

function GuardWalk1()
	if ( GetNUnitsInScriptGroup(1) > 0) then
		Cmd (3, 1, 3008, 2165);

		RunScript( "GuardWalk2", 5000);
	end;
	Suicide();
end;

function GuardWalk2()
	if ( GetNUnitsInScriptGroup(1) > 0) then
		Cmd (3, 1, 3200, 2118);

		RunScript( "GuardWalk1", 5000);
	end;
	Suicide();
end;

function Init()
	RunScript("Objective0", 2000);
	RunScript("Objective0a", 2000);
	RunScript("Objective1", 2000);
	RunScript("Objective2", 2000);
	RunScript("Objective3", 2000);
	RunScript("Objective4", 2000);
	RunScript( "IsBridgeAlive", 2000);
	RunScript("Paradrop", 3000 + GetIGlobalVar( "Weather.TimeToFadeOff", 0));
	RunScript("Reinforcement1", 3000);
	RunScript("ToWin", 2000);
	RunScript( "RevealObjective0", 6000);
	RunScript( "DisableAvia", 0);
	RunScript( "GuardWalk1", 1000);
end;

