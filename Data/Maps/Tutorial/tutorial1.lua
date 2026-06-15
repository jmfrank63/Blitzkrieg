--text objective
function Objective0()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(0, 1);
		RunScript("RevealObj1", 3500);
		RunScript("AddTank", 1500);
		RunScript("CheckLost", 2500);
		RunScript("TestSelect", 3500);
		Suicide();
	end;
end;
--move,use roads in tutorial
function Objective2()
	if (GetNUnitsInArea(0, "Zone1") >=1) then
		ObjectiveChanged(2, 1);
		RunScript("RevealObj3", 2000);
		RunScript("Objective3", 3000);
		Suicide();
	end;
end;
--move at north, scrolling
function Objective3()
	if (GetNUnitsInArea(0, "Area1") >=1) then
		ObjectiveChanged(3, 1);
		RunScript("RevealObj4", 2000);
		RunScript("Objective4", 3000);
		Suicide();
	end;
end;
----
function Objective4()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(4, 1);
		RunScript("RevealObj5", 1000);
		RunScript("Objective5", 3000);
		Suicide();
	end;
end;

--move to lake
function Objective5()
	if (GetNUnitsInArea(0, "Area2") >=1) then
		ObjectiveChanged(5, 1);
		RunScript("Reinf10" , 500);
		RunScript("RevealObj6", 2000);
		RunScript("Objective6", 3000);
		Suicide();
	end;
end;
----
function Objective6()
	if (GetNUnitsInScriptGroup(10, 1) <=0 ) then
		ObjectiveChanged(6, 1);
		RunScript("RevealObj7", 2000);
		RunScript("Objective7", 3000);
		Suicide();
	end;
end;
----
function Objective7()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(7, 1);
		RunScript("RevealObj8", 1000);
		RunScript("FictiveObjective0" , 500);
		Suicide();
	end;
end;
--swarm test
function FictiveObjective0()
	if (GetUnitState(1000) == 11) then
		RunScript("Reinf20" , 500);
		RunScript("Objective8", 3000);
		Suicide();
	end;
end;
---agr. movement
function Objective8()
	if (GetNUnitsInScriptGroup(20, 1) <=0 ) then
		ObjectiveChanged(8, 1);
		RunScript("Reinf30" , 500);
		RunScript("RevealObj9", 2000);
		RunScript("Objective9", 3000);
		Suicide();
	end;
end;
--agr.movemet, f1,f5,f7
function Objective9()
	if (GetNUnitsInScriptGroup(30, 1) <=0 ) then
		ObjectiveChanged(9, 1);
		RunScript("Reinf40" , 500);
		RunScript("RevealObj10", 2000);
		RunScript("Objective10", 3000);
		Suicide();
	end;
end;
--agr.movement, minimap
function Objective10()
	if (GetNUnitsInScriptGroup(40, 1) <=0 ) then
		ObjectiveChanged(10, 1);
		RunScript("RevealObj11", 2000);
		RunScript("Objective11", 3000);
		Suicide();
	end;
end;
--movement,game speed&pause
function Objective11()
	if (GetNUnitsInArea(0, "Area7") >=1) then
		ObjectiveChanged(11, 1);
		RunScript("RevealObj12", 2000);
		RunScript("Objective12", 3000);
		Suicide();
	end;
end;
function Objective12()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(12, 1);
		RunScript("RevealObj13", 1000);
		RunScript("Objective13", 3000);
		Suicide();
	end;
end;
--movement, control panel
function Objective13()
	if (GetNUnitsInArea(0, "Area8") >=1) then
		ObjectiveChanged(13, 1);
		RunScript("RevealObj14", 2000);
		RunScript("Objective14", 3000);
		Suicide();
	end;
end;
---congr-s
function Objective14()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		RunScript("Victory", 2000);
		Suicide();
	end;
end;
-------------------------------------------

function RevealObj0()
	SwitchWeather( 0);
	SwitchWeatherAutomatic( 0);
	ObjectiveChanged(0, 0);
	Suicide();
end;

function RevealObj1()
	ObjectiveChanged(1, 0);
	Suicide();
end;

function RevealObj2()
	ObjectiveChanged(2, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj3()
	ObjectiveChanged(3, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj4()
	ObjectiveChanged(4, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj5()
	ObjectiveChanged(5, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj6()
	ObjectiveChanged(6, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj7()
	ObjectiveChanged(7, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj8()
	ObjectiveChanged(8, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj9()
	ObjectiveChanged(9, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj10()
	ObjectiveChanged(10, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj11()
	ObjectiveChanged(11, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj12()
	ObjectiveChanged(12, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj13()
	ObjectiveChanged(13, 0);
	Cmd(9,1000);
	Suicide();
end;

function RevealObj14()
	ObjectiveChanged(14, 0);
	Cmd(9,1000);
	Suicide();
end;

-----------------------------------------
function TestSelect()
	if ( GetIGlobalVar("temp.tutorial1.objective.0", 0) == 0) then
		AskClient("GetSelectedUnits()");
	else Suicide(); end;
end;

--select, Objective1
function GetSelectedUnitsFeedBack(n)
	if (n == 1000) then
		SetIGlobalVar("temp.tutorial1.objective.0", 1);
		ObjectiveChanged(1, 1);
		RunScript( "RevealObj2", 2000);
		RunScript("Objective2", 3000);
		Trace("selected %g", n);
	end;
end;


function CheckLost()
	if (GetNUnitsInParty(0) <= 0) then
		LandReinforcement(1000);
		AskClient("SetCamera(1900,200)");
		ChangeSelection(1000 , 1);
		RunScript("CheckLost", 4000);
		Suicide();
	end;
end;

function Victory()
	Win(0);
	Suicide();
end;

function AddTank()
	LandReinforcement(1000);
	AskClient("SetCamera(1900,200)");
	Suicide();
end;

function Reinf10()
	LandReinforcement(10);
	Suicide();
end;

function Reinf20()
	LandReinforcement(20);
	Suicide();
end;

function Reinf30()
	LandReinforcement(30);
	Suicide();
end;

function Reinf40()
	LandReinforcement(40);
	Suicide();
end;


-----------------------------------------
function Init()
	RunScript("RevealObj0", 1000);
	RunScript("Objective0", 2000);
	SetCheatDifficultyLevel( 1 );
	SetIGlobalVar( "nogeneral_script", 1);
end;
