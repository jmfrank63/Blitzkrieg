------------------------------ reveal objectives
function RevealObj0()
	ObjectiveChanged(0,0);
	Suicide();
end;

function RevealObj1()
	ObjectiveChanged(1,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj2()
	ObjectiveChanged(2,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj3()
	ObjectiveChanged(3,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj4()
	ObjectiveChanged(4,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj5()
	ObjectiveChanged(5,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj6()
	ObjectiveChanged(6,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj7()
	ObjectiveChanged(7,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj8()
	ObjectiveChanged(8,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj9()
	ObjectiveChanged(9,0);
	Cmd(9,1000);
	Cmd(9,1001);
	Suicide();
end;

function RevealObj10()
	ObjectiveChanged(10,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj11()
	ObjectiveChanged(11,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj12()
	ObjectiveChanged(12,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj13()
	ObjectiveChanged(13,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj14()
	ObjectiveChanged(14,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj15()
	ObjectiveChanged(15,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj16()
	ObjectiveChanged(16,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj17()
	ObjectiveChanged(17,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj18()
	ObjectiveChanged(18,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj19()
	ObjectiveChanged(19,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj20()
	ObjectiveChanged(20,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj21()
	ObjectiveChanged(21,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj22()
	ObjectiveChanged(22,0);
	Cmd(9,100);
	Suicide();
end;

function RevealObj23()
	ObjectiveChanged(23,0);
	Cmd(9,100);
	Suicide();
end;

-------------------------------------------------------------------------- check objectives

-- mass select
function Objective0()
	if (GetIGlobalVar("temp.tutorial2.objective0.fixed", 0) == 0) then
		RunScript("TestSelect", 1000);
	else
		ObjectiveChanged(0, 1);
		RunScript("RevealObj1", 1500);
		RunScript("Objective1", 4000);
		RunScript("Reinf10", 1000);
		Suicide();
	end;
end;

-- test on mass swarm
function Objective1()
	AskClient("HighlightControl(20003)");
	if ((GetUnitState(1000) == 11) and (GetUnitState(1001) == 11)) then
		ObjectiveChanged(1, 1);
		RunScript("RevealObj2", 1000);
		RunScript("Objective2", 2000);
		Suicide();
	end;
end;

-- kill tank
function Objective2()
	if ( GetNUnitsInScriptGroup(10) <= 0) then
		ObjectiveChanged(2, 1);
		RunScript("RevealObj3", 1000);
		RunScript("Objective3", 2000);
		Suicide();
	end;
end;

-- follow
function Objective3()
	if ( IsFollowing(1001) == 1) then
		ObjectiveChanged(3, 1);
		RunScript( "RevealObj4", 2000);
		RunScript( "Objective4", 3000);
		RunScript("Reinf20", 1000);
		Suicide();
	end;
end;

-- following and attack, economy light tanks.
function Objective4()
	if ( GetNUnitsInScriptGroup(20) <= 0) then
		ObjectiveChanged(4, 1);
		RunScript( "RevealObj5", 2000);
		RunScript( "Objective5", 4000);
		Suicide();
	end;
end;

-- about hit points
function Objective5()
	AskClient("HighlightIndicator(10)");
	ChangeSelection(1000,0);
	ChangeSelection(1001,0);
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(5, 1);
		RunScript( "RevealObj6", 2000);
		RunScript( "Objective6", 4000);
		Suicide();
	end;
end;

-- about primary ammo
function Objective6()
	AskClient("HighlightIndicator(11)");
	ChangeSelection(1000,0);
	ChangeSelection(1001,0);
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(6, 1);
		RunScript( "RevealObj7", 2000);
		RunScript( "Objective7", 4000);
		Suicide();
	end;
end;

-- about secondary ammo
function Objective7()
	AskClient("HighlightIndicator(12)");
	ChangeSelection(1000,0);
	ChangeSelection(1001,0);
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(7, 1);
		RunScript( "RevealObj8", 2000);
		RunScript( "Objective8", 4000);
		Suicide();
	end;
end;

-- about morale - old, EXP now
function Objective8()
	AskClient("HighlightIndicator(13)");
	ChangeSelection(1000,0);
	ChangeSelection(1001,0);
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(8, 1);
		RunScript("Reinf30", 1000);
		RunScript( "RevealObj9", 2000);
		RunScript( "Objective9", 3000);
		Suicide();
	end;
end;

-- kill next tank, watch HP & ammo bars(text)
function Objective9()
	if ( GetNUnitsInScriptGroup(30) <= 0) then
		ObjectiveChanged(9, 1);
		RunScript( "RevealObj10", 2000);
		RunScript( "Objective10", 3000);
		RunScript( "GiveUnits", 500);
		RunScript( "TakeUnits", 600);
		SetIGlobalVar("temp.step1",0);
		SetIGlobalVar("temp.step2",1);
		Suicide();
	end;
end;

-- move to road junc.
function Objective10()
	if (( GetNUnitsInArea(0,"Area3") >= 1) and (GetUnitState(100) == 1)) then
		ObjectiveChanged(10, 1);
		RunScript( "RevealObj11", 2000);
		RunScript( "Objective11", 3000);
		Suicide();
	end;
end;

-- stop and defence, prepare to counter tank
function Objective11()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(11, 1);
		RunScript( "RevealObj12", 2000);
		RunScript( "Objective12", 3000);
		RunScript("Attack1", 1500);
		Suicide();
	end;
end;

-- counter first tank
function Objective12()
	if (GetNUnitsInScriptGroup(40)<=0) then
		ObjectiveChanged(12, 1);
		RunScript( "RevealObj13", 2000);
		RunScript( "Objective13", 4000);
		RunScript( "GiveUnits", 5000);
		Suicide();
	end;
end;

-- standground and defence, prepare to counter tank
function Objective13()
	AskClient("HighlightControl(20017)");
	if (IsStandGround(100)==1) then
		ObjectiveChanged(13, 1);
		RunScript( "RevealObj14", 2000);
		RunScript( "Objective14", 3000);
		RunScript("Attack2", 2000);
		Suicide();
	end;
end;

-- counter second tank
function Objective14()
	if (GetNUnitsInScriptGroup(50) <=0) then
		ObjectiveChanged(14, 1);
		RunScript( "RevealObj15", 2000);
		RunScript( "Objective15", 4000);
		Suicide();
	end;
end;

-- entrench objective
function Objective15()
	AskClient("HighlightControl(20016)");
	if (IsEntrenched(100)==1) then
		ObjectiveChanged(15, 1);
		RunScript( "RevealObj16", 2000);
		RunScript( "Objective16", 4000);
		Suicide()
	end;
end;

-- ambush defence
function Objective16()
	AskClient("HighlightControl(20010)");
	if GetUnitState(100)==15 then
		ObjectiveChanged(16, 1);
		RunScript("Attack3", 1000);
		RunScript("RevealObj17", 2000);
		RunScript("Objective17", 4000);
		Suicide();
	end;
end;

-- counter third tank
function Objective17()
	if (GetNUnitsInScriptGroup(60)<=0) then
		ObjectiveChanged(17, 1);
		RunScript( "RevealObj18", 2000);
		RunScript( "Objective18", 3000);
		ViewZone("View0", 1);
		AskClient("SetCamera(1820,777)");
		Suicide();
	end;
end;

-- move to pillbox
function Objective18()
	if (GetNUnitsInArea(0, "Area4")>=1) then
		ObjectiveChanged(18, 1);
		SetIGlobalVar("temp.step2",0);
		SetIGlobalVar("temp.step3",1);
		RunScript( "RevealObj19", 2000);
		RunScript( "Objective19", 4000);
		ViewZone("View0", 0);
		Suicide();
	end;
end;

-- destroy pillbox
function Objective19()
--	if (GetNUnitsInScriptGroup(200) <= 0) then
	AskClient("HighlightControl(20002)");
	if ( GetNUnitsInScriptGroup(200) <= 0) then
		ObjectiveChanged(19, 1);
		RunScript( "RevealObj20", 2000);
		RunScript( "Objective20", 3000);
		Suicide();
	end;
end;

-- àction queue
function Objective20()
	if (GetNUnitsInArea(0, "Area1") >=1) then
		ObjectiveChanged(20, 1);
		RunScript( "RevealObj21", 2000);
		RunScript( "Objective21", 3000);
		Suicide();
	end;
end;

-- group assignment
function Objective21()
	if (GetNUnitsInArea(0, "Area2") >=1) then
		ObjectiveChanged(21, 1);
		RunScript("Reinf70", 1000);
		RunScript( "RevealObj22", 2000);
		RunScript( "Objective22", 3000);
		ViewZone("View1", 1);
		AskClient("SetCamera(1520,3707)");
		Suicide();
	end;
end;

-- armor and penetration, attack on SPG
function Objective22()
	RunScript("StopAction", 100);
	if ( GetNUnitsInScriptGroup(70) <= 0) then
		ObjectiveChanged(22, 1);
		RunScript("Reinf80", 1000);
		RunScript( "RevealObj23", 2000);
		RunScript( "Objective23", 3000);
		ViewZone("View1", 0);
		Suicide();
	end;
end;

-- infantry attack, backmove
function Objective23()
	if ( GetNUnitsInScriptGroup(80) <= 0) then
		ObjectiveChanged(23, 1);
		RunScript( "Victory", 7000);
		Suicide();
	end;
end;

---------------------------------

function CheckLost()
	if ( GetNUnitsInParty(0) <= 0) then
		if (GetIGlobalVar("temp.step1", 0) == 1) then
			LandReinforcement(1000);
			LandReinforcement(1001);
			AskClient("SetCamera(5800,5555)");
			RunScript("CheckLost", 3000);
			Suicide();
		end;
		if (GetIGlobalVar("temp.step2", 0) == 1) then
			LandReinforcement(1);
			AskClient("SetCamera(6300,650)");
			RunScript("CheckLost", 3000);
			Suicide();
		end;
		if (GetIGlobalVar("temp.step3", 0) == 1) then
			LandReinforcement(1);
			AskClient("SetCamera(6300,650)");
			Cmd(3,100,1850,750);
			RunScript("CheckLost", 3000);
			Suicide();
		end;
	end;
end;

function TestSelect()
	AskClient("GetSelectedUnits()");
	SetIGlobalVar("temp.select",0);
	Trace("test select");
	Suicide();
end;

function GetSelectedUnitsFeedBack(n)
	if (n == 1001) then
		SetIGlobalVar("temp.select", GetIGlobalVar( "temp.select", 0) + 1);
	end;
	if (n == 1000) then
		SetIGlobalVar("temp.select", GetIGlobalVar( "temp.select", 0) + 1);
	end;
	if (GetIGlobalVar("temp.select", 0) == 2) then
		SetIGlobalVar("temp.tutorial2.objective0.fixed", 1);
	end;
end;

function Victory()
	Win(0);
	Suicide();
end;

-- delete tanks (to refresh forces)
function DeleteUnits0()
	DeleteReinforcement(1000);
	DeleteReinforcement(1001);
	Suicide();
end;

-- take away damaged tanks
function TakeUnits()
	ChangePlayer(1000, 2);
	ChangePlayer(1001, 2);
	RunScript("DeleteUnits0",200);
	Suicide();
end;

-- give more tanks
function GiveUnits()
	LandReinforcement(1);
	AskClient("SetCamera(6000,600)");
--	Cmd(3,100,7700,1000);
	Suicide();
end;

function SetIGlobalVars()
	SetIGlobalVar("temp.step1", 1);
	SetIGlobalVar("temp.step2", 0);
	SetIGlobalVar("temp.step3", 0);
	Suicide();
end;

function StopAction()
	Cmd(9,70);
	Suicide();
end;

function Start()
	SwitchWeather( 0);
	SwitchWeatherAutomatic( 0);

	LandReinforcement(1000);
	LandReinforcement(1001);
	Suicide();
end;
--------------------------------------
function Attack1()
	LandReinforcement(40);
	Suicide();
end;

function Attack2()
	LandReinforcement(50);
	Suicide();
end;

function Attack3()
	LandReinforcement(60);
	Suicide();
end;

-------------------------------------

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
function Reinf70()
	LandReinforcement(70);
	Suicide();
end;
function Reinf80()
	LandReinforcement(80);
	Suicide();
end;

------------------------------------

function Init()
	RunScript("Start", 0);
	RunScript("RevealObj0", 1000);
	RunScript("Objective0", 1500);
	RunScript("CheckLost", 2000);
	RunScript( "SetIGlobalVars", 500);
	SetCheatDifficultyLevel( 1 );
	SetIGlobalVar( "nogeneral_script", 1);
end;
