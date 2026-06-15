function TobeDefeated()
local num = GetNUnitsInScriptGroup (1000, 0);
	if ( num <= 0) then
		Loose();
		Suicide();
	end;
end;

function ToWin()
	if ( (GetIGlobalVar("temp.Ardennes40.objective.1", 0) * GetIGlobalVar("temp.Ardennes40.objective.5", 0)) == 1) then
		RunScript( "WinWin", 5000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
       	Suicide();
end;

-- #1 enemy reinf
function Reinf1()
	LandReinforcement(1);
	RunScript( "Reinf1Cmds", 3000);
	Suicide();
end;

function Reinf1Cmds()
local A_Follow = 39;
	Cmd (A_Follow, 27, 1);
	SetIGlobalVar("temp.Ardennes40.objective.100", 1);
	Suicide();
end;

-- objective 2
function CallHQ()
	if ( GetIGlobalVar("temp.Ardennes40.objective.150", 0) == 1) then
		SetIGlobalVar("temp.Ardennes40.objective.2", 1);
		ObjectiveChanged(2, 1);

		RunScript( "Reinf1", 2000);
 		RunScript( "RevealObjective3", 5000);
		SetIGlobalVar("temp.Ardennes40.objective.150", 2);
		Suicide();
	end;
end;

-- "eliminate ambush"
function Objective0()
	if ( GetNUnitsInScriptGroup(6, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes40.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective6", 5000);
		Suicide();
	end;
end;

function Objective6()
	if ( GetNUnitsInScriptGroup(1099, 0) > 0) then
		SetIGlobalVar("temp.Ardennes40.objective.6", 1);
		ObjectiveChanged(6, 1);

		RunScript( "RevealObjective1", 5000);
		Suicide();
	end;
end;

-- "capture r/w station and warehouse"
function Objective1()
	if ( GetIGlobalVar("temp.Ardennes40.objective.150", 0) == 0) then
	if ( GetNUnitsInScriptGroup(2, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes40.objective.1", 1);
		ObjectiveChanged(1, 1);
		SetIGlobalVar("temp.Ardennes40.objective.150", 1);
		RunScript( "RevealObjective2", 5000);

		RunScript( "CallHQ", 120000); -- defence is ready / crap - CallHQ button deleted
-- poezd fun
		LandReinforcement(2712);
		RunScript( "Poezd", 5000);
--
		Suicide();
	end;
	else
	if ( GetIGlobalVar("temp.Ardennes40.objective.150", 0) == 3) then
	if ( GetNUnitsInScriptGroup(2, 1) + GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes40.objective.1", 1);
		ObjectiveChanged(1, 1);
		RunScript( "RevealObjective5", 5000);
		Suicide();
	end;
	end;
	end;
end;

-- destroy incoming troops
function Objective3()
	if ( ((GetIGlobalVar("temp.Ardennes40.objective.100", 0) == 1)) and ( GetIGlobalVar("temp.Ardennes40.objective.150", 0) ~= 3)) then
	if ( GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes40.objective.3", 1);
		ObjectiveChanged(3, 1);

		RunScript( "RevealObjective5", 5000);
		Suicide();
	end;
	end;
end;

-- secret "find and kill enemy general near the southeast town"
function Objective4()
	if ( GetNUnitsInScriptGroup(5, 1) <= 0) then
		SetIGlobalVar("temp.Ardennes40.objective.4", 1);
		ObjectiveChanged(4, 1);
		Suicide();
	end;
end;

-- "secure northern town and capture warehouse"
function Objective5()
	if ( (GetNUnitsInArea(0, "Warehouse1") > 0) and (GetNUnitsInScriptGroup(300, 1) <= 0) and ((GetNUnitsInScriptGroup(3, 1) + GetNUnitsInScriptGroup(4, 1)) <= 2)) then
		SetIGlobalVar("temp.Ardennes40.objective.5", 1);
		ObjectiveChanged(5, 1);
		Suicide();
	end;
end;

function SomeCheck()
	if ( GetIGlobalVar("temp.Ardennes40.objective.1", 0) == 0) then
		if ( GetNUnitsInScriptGroup(3, 1) + GetNUnitsInScriptGroup(4, 1) <= 4) then
			RunScript( "Reinf1", 2000);
			SetIGlobalVar("temp.Ardennes40.objective.150", 3);
			Suicide();
		end;
	end;
end;

-- if train is near station, give it to player
function Poezd()
	if ( GetNScriptUnitsInArea( 2712, "Poezd") > 0) then
		ChangePlayer(2712, 0);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Ardennes40.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Ardennes40.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Ardennes40.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Ardennes40.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.Ardennes40.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0);
	end;
	Suicide();
end;

function RevealObjective5()
	if ( GetIGlobalVar("temp.Ardennes40.objective.5", 0) == 0) then
		ObjectiveChanged(5, 0);
	end;
	Suicide();
end;

function RevealObjective6()
	if ( GetIGlobalVar("temp.Ardennes40.objective.6", 0) == 0) then
		ObjectiveChanged(6, 0);
	end;
	Suicide();
end;

function Init()
	RunScript( "TobeDefeated", 4000);
	RunScript( "ToWin", 4000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
--	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "Objective4", 2000);
	RunScript( "Objective5", 2000);
	RunScript( "Objective6", 2000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "SomeCheck", 3000);
end;
