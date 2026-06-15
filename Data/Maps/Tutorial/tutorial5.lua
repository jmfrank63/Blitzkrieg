function RevealObj0()
	ObjectiveChanged(0,0);
	Suicide();
end;
function RevealObj1()
	ObjectiveChanged(1,0);
	Suicide();
end;
function RevealObj2()
	ObjectiveChanged(2,0);
	Suicide();
end;
function RevealObj3()
	ObjectiveChanged(3,0);
	Suicide();
end;
function RevealObj4()
	ObjectiveChanged(4,0);
	Suicide();
end;
function RevealObj5()
	ObjectiveChanged(5,0);
	Suicide();
end;
function RevealObj6()
	ObjectiveChanged(6,0);
	Suicide();
end;
function RevealObj7()
	ObjectiveChanged(7,0);
	Suicide();
end;
function RevealObj8()
	ObjectiveChanged(8,0);
	Suicide();
end;
function RevealObj9()
	ObjectiveChanged(9,0);
	Suicide();
end;
function RevealObj10()
	ObjectiveChanged(10,0);
	Suicide();
end;

------------------------------

-- enemy aviation, destroy by AA Guns
function Objective0()
	if (GetNUnitsInScriptGroup(1000) <= 0) then
		ObjectiveChanged(0, 1);
		RunScript("RevealObj1", 3000);
		RunScript("Objective1", 4000);
		RunScript("LaunchScout1", 1000);
		SetIGlobalVar("temp.scout0", 1);
		EnableAviation(0, 1);
		Suicide();
	end;
end;

-- activate fighters
function Objective1()
	if (GetAviationState(0) == 1) then
		ObjectiveChanged(1, 1);
		RunScript("RevealObj2", 3000);
		RunScript("Objective2", 4000);
		Suicide();
	else
		AskClient("HighlightControl(20031)");
	end;
end;

-- destroy enemy's scouts by fighters, time of regeneration
function Objective2()
	if (GetNUnitsInScriptGroup(2000) == 0) then
		ObjectiveChanged(2, 1);
		RunScript("RevealObj3", 3000);
		RunScript("Objective3", 4000);
		SetIGlobalVar("temp.scout1", 1);
		AskClient("SetCamera(1100,2400)");
		EnableAviation(0, 0);
		Suicide();
	end;
end;

-- recon by scout
function Objective3()
	if (GetAviationState(0) == 0) then
		ObjectiveChanged(3, 1);
		RunScript("RevealObj5", 3000);
		RunScript("Objective5", 4000);
		EnableAviation(0,4);
		Suicide();
	else
		AskClient("HighlightControl(20032)");
	end;
end;

-- scout in the right zone
--function Objective4()
--	if GetNUnitsInArea(0,"Area")>=1 then
--		ObjectiveChanged(4, 1);
--		RunScript("RevealObj5", 1000);
--		RunScript("Objective5", 1000);
--		EnableAviation(0,4);
--		Suicide();
--	end;
--end;

-- activate groundattack planes
function Objective5()
	if (GetAviationState(0) == 4) then
		ObjectiveChanged(5, 1);
		RunScript("RevealObj6", 3000);
		RunScript("Objective6", 4000);
		Suicide();
	else
		AskClient("HighlightControl(20035)");
	end;
end;

-- destroy enemy's units(art-ry)
function Objective6()
	if (GetNUnitsInScriptGroup(2, 1) <= 0) then
		ObjectiveChanged(6, 1);
		RunScript("RevealObj7", 3000);
		RunScript("Objective7", 4000);
		ViewZone("View1", 1);
		AskClient("SetCamera(6900,7150)");
		EnableAviation(0, 3);
		Suicide();
	end;
end;

-- activate bombers
function Objective7()
	if (GetAviationState(0) == 3) then
		ObjectiveChanged(7, 1);
		RunScript("RevealObj8", 3000);
		RunScript("Objective8", 4000);
		Suicide();
	else
		AskClient("HighlightControl(20030)");
	end;
end;

-- destroy enemy's units(tracks)
function Objective8()
	if (GetNUnitsInScriptGroup(1, 1) <= 4) then
		ObjectiveChanged(8, 1);
		RunScript("RevealObj9", 3000);
		RunScript("Objective9", 4000);
		EnableAviation(0, 2);
		Suicide();
	end;
end;

-- activate paradroper
function Objective9()
	if (GetUnitState(777) == 27) then
		ObjectiveChanged(9, 1);
		RunScript("RevealObj10", 3000);
		RunScript("Objective10", 4000);
		Suicide();
	else
		AskClient("HighlightControl(20033)");
	end;
end;

-- destroy(finish) enemy's units
function Objective10()
	if (GetNUnitsInScriptGroup(1, 1) <= 0) then
		ObjectiveChanged(10, 1);
		RunScript("Victory", 7000);
		Suicide();
	end;
end;

-----------------------------
function SetAviation()
	SwitchWeather(0);
	SwitchWeatherAutomatic(0);
	DiableAviation(0, 1);
	DiableAviation(0, 2);
	DiableAviation(0, 3);
	DiableAviation(0, 4);
	DiableAviation(1, 1);
	DiableAviation(1, 2);
	DiableAviation(1, 3);
	DiableAviation(1, 4);
	Suicide();
end;

function LaunchScout0()
local A_Scout = 21;
--	if (GetIGlobalVar("temp.scout0", 0)==0) then
--		EnableAviation(1, 0);
		Cmd(A_Scout, 1000, 1, 1500, 10000);
		DiableAviation(1, 0);
--		RunScript("LaunchScout0", 30000);
		Suicide();
--	else
--		Suicide();
--	end;
end;

function LaunchScout1()
local A_Scout = 21;
--	if (GetIGlobalVar("temp.scout1", 0)==0) then
		EnableAviation(1, 0);
		Cmd(A_Scout, 2000, 1, 5000, 7500);
--		DiableAviation(1, 0);
--		RunScript("LaunchScout1", 30000);
--		Suicide();
--	else
		Suicide();
--	end;
end;

function Victory()
	Win(0);
	Suicide();
end;
-----
function Init()
	RunScript( "Objective0", 3000);
	RunScript( "RevealObj0", 2000);
	RunScript( "SetAviation", 0);
	RunScript( "LaunchScout0", 1000);
	SetIGlobalVar( "ParadropSquad.ScriptID", 777);
	SetCheatDifficultyLevel( 1 );
	SetIGlobalVar( "nogeneral_script", 1);
end;
