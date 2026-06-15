function RevealObj0()
	ObjectiveChanged(0, 0);
	Suicide();
end;
function RevealObj1()
	ObjectiveChanged(1, 0);
	Suicide();
end;
function RevealObj2()
	ObjectiveChanged(2, 0);
	Suicide();
end;
function RevealObj3()
	ObjectiveChanged(3, 0);
	Suicide();
end;
function RevealObj4()
	ObjectiveChanged(4, 0);
	Suicide();
end;
function RevealObj5()
	ObjectiveChanged(5, 0);
	Suicide();
end;
function RevealObj6()
	ObjectiveChanged(6, 0);
	Suicide();
end;
function RevealObj7()
	ObjectiveChanged(7, 0);
	Suicide();
end;
function RevealObj8()
	ObjectiveChanged(8, 0);
	Suicide();
end;
function RevealObj9()
	ObjectiveChanged(9, 0);
	Suicide();
end;
function RevealObj10()
	ObjectiveChanged(10, 0);
	Suicide();
end;
function RevealObj11()
	ObjectiveChanged(11, 0);
	Suicide();
end;
function RevealObj12()
	ObjectiveChanged(12, 0);
	Suicide();
end;
function RevealObj13()
	ObjectiveChanged(13, 0);
	Suicide();
end;
function RevealObj14()
	ObjectiveChanged(14, 0);
	Suicide();
end;
function RevealObj15()
	ObjectiveChanged(15, 0);
	Suicide();
end;

-----------------------

---move to defend position
function Objective0()
	if (GetNUnitsInArea(0, "Objective0") >= 7) then
		ObjectiveChanged(0, 1);
		RunScript("Objective1", 3000);
		RunScript("RevealObj1", 1000);
		Suicide();
	end;
end;

---rotate guns
function Objective1()
local n = 1001;
	while ( (GetFrontDir(n)*360/65536 < 110) and (GetFrontDir(n)*360/65536 > 70) and (GetUnitState(n) == 1) and (n < 1003))
	do
		Trace(n);
		n = n + 1;
	end;
	if ( n == 1003) then
		ObjectiveChanged(1, 1);
		RunScript("Objective2", 4500);
		RunScript("RevealObj2", 1000);
		Suicide();
	else
		AskClient("HighlightControl(20006)");
	end;
end;

---entrench
function Objective2()
	if (IsEntrenched(1001) == 1) and (IsEntrenched(1002) == 1) then
		ObjectiveChanged(2, 1);
		RunScript("RevealObj3", 2000);
		RunScript("Objective3", 4500);
		Suicide();
	else
		AskClient("HighlightControl(20016)");
	end;
end;

---ambush
function Objective3()
	if (GetUnitState(1001) == 15) and (GetUnitState(1002) == 15) then
		ObjectiveChanged(3, 1);
		RunScript("Objective4", 3000);
		RunScript("RevealObj4", 1000);
		RunScript("Attack0", 2000);
		Suicide();
	else
		AskClient("HighlightControl(20010)");
	end;
end;

---deflect tank attack
function Objective4()
	if (GetNUnitsInScriptGroup(1) <= 0) then
		ObjectiveChanged(4, 1);
		RunScript("RestoreGuns", 1000);
		RunScript("Objective5", 2000);
		RunScript("RevealObj5", 1000);
		RunScript("Reinf0", 1000);
		Suicide();
	end;
end;

---link guns
function Objective5()
	if (GetUnitState(1001) == 24) or (GetUnitState(1002) == 24) then
		ObjectiveChanged(5, 1);
		RunScript("Objective6", 4000);
		RunScript("RevealObj6", 3000);
		RunScript("ReinfA", 1000);
		Suicide();
	end;
end;


---launch attack by closing window
function Objective6()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(6, 1);
		RunScript( "RevealObj7", 2000);
		RunScript( "Objective7", 3000);
		RunScript( "Attack1", 500);
		Suicide();
	end;
end;

---stop attack
function Objective7()
	if (GetNUnitsInScriptGroup(2) <= 0) then
		ObjectiveChanged(7, 1);
		RunScript( "Objective8", 4000);
		RunScript( "RevealObj8", 3000);
		RunScript( "Attack2", 20000);
		RunScript( "Reinf1", 1000);
		Suicide();
	end;
end;

---rang.fire
function Objective8()
local STATE_RANGEFIRE = 16;
	if ( GetIGlobalVar( "temp.3", 0) == 1) then
	if (GetNUnitsInScriptGroup(3) <= 0) then
		ObjectiveChanged(8, 1);
		RunScript("Objective9", 4000);
		RunScript("RevealObj9", 3000);
		RunScript("Attack3", 3000);
		Suicide();
	end;
	if ( GetUnitState( 20) ~= STATE_RANGEFIRE) then
		AskClient("HighlightControl(20012)");
	end;
	end;
end;

---supr.fire
function Objective9()
local STATE_SUPPRFIRE = 34;
	if (GetNUnitsInScriptGroup(4, 1) <= 1) then
		ObjectiveChanged(9, 1);
		DeleteReinforcement( 4);
		ViewZone("View0", 0);
		Cmd(9, 20); -- stop for A-19 guns
		RunScript("Objective11", 4500);
		RunScript("RevealObj11", 5000);
		Suicide();
	else
	if ( GetUnitState( 20) ~= STATE_SUPPRFIRE) then
		AskClient("HighlightControl(20013)");
	end;
	end;
end;

-- shell1 morale shells - obsolete
function Objective10()
	if (GetActiveShellType(20) == 1) then
		ObjectiveChanged(10, 1);
		RunScript("Objective11", 4500);
		RunScript("RevealObj11", 5000);
		Suicide();
	else
		AskClient("HighlightControl(20073)");
		AskClient("HighlightControl(20056)");
	end;
end;

---shell2
function Objective11()
	if (GetActiveShellType(20) == 2) then
		ObjectiveChanged(11, 1);
		RunScript("Objective12", 4500);
		RunScript("RevealObj12", 5000);
		Suicide();
	else
		AskClient("HighlightControl(20073)");
		AskClient("HighlightControl(20057)");
	end;
end;

---shell0
function Objective12()
	if (GetActiveShellType(20) == 0) then
		ObjectiveChanged(12, 1);
		RunScript("Objective13", 4000);
		RunScript("RevealObj13", 3000);
--		RunScript("Attack4", 2000);
		RunScript("Reinf2", 2000);
		Suicide();
	else
		AskClient("HighlightControl(20073)");
		AskClient("HighlightControl(20055)");
	end;
end;

---SPG
function Objective13()
	if (GetIGlobalVar("Mission.Current.ObjectiveShown",0) == 0) then
		ObjectiveChanged(13, 1);
		RunScript("Objective14", 3000);
		RunScript("RevealObj14", 5000);
--		RunScript("Delete1", 15000);
		ChangePlayer(5, 2);
		ViewZone("View1", 1);
		Suicide();
	end;
end;

---attack building, show lower range
function Objective14()
	if (GetObjectHPs(100) <= 0) then
		ObjectiveChanged(14, 1);
		RunScript("Objective15", 3000);
		RunScript("RevealObj15", 5000);
		RunScript("Reinf3", 2000);

		ViewZone( "DOT", 1 );
		ViewZone("View1", 0);
--		RunScript( "ReloadRocket", 4000);
		ChangePlayer(30,0);
		Suicide();
	end;
end;

---attack pillbox
function Objective15()
	if (GetObjectHPs(200) <= 0) then
		ObjectiveChanged(15, 1);
		RunScript("Victory", 7000);
		Suicide();
	else
	if ( GetObjectHPs(200) <= 500) then
		DamageObject(GetObjectHPs(200));
		ObjectiveChanged(15, 1);
		RunScript("Victory", 7000);
		Suicide();
	end;
	end;
end;
------------------------
function ReloadRocket()
	if ( GetNAmmo( 40 ) < 12) then
		Cmd( 32, 10, GetObjCoord( 10 ) );
		QCmd( 23, 10, GetObjCoord( 40 ) );
		Suicide();
	end;
end;

function Victory()
	Win(0);
	Suicide();
end;

function Delete1()
	DeleteReinforcement(5);
	Suicide();
end;

function Begin()
	SwitchWeather( 0);
	SwitchWeatherAutomatic( 0);
	A1();
	A2();
	Suicide();
end;

function Attack0()
	LandReinforcement(1);
	Suicide();
end;

function Attack1()
	LandReinforcement(2);
	Cmd(3,2,8888,6100);
	Suicide();
end;

function Attack2()
	LandReinforcement(3);
	RunScript( "Check3", 2000);
--	Cmd(0,3,8760,6005);
	Suicide();
end;

function Check3()
	SetIGlobalVar( "temp.3", 1);
	Suicide();
end;

function Attack3()
	LandReinforcement(4);
--	Cmd(16, 4, 8525, 6710);
	RunScript( "RevealView0", 30000);
	Suicide();
end;

function RevealView0()
	ViewZone("View0", 1);
	Suicide();
end;

function Attack4()
	LandReinforcement(5);
	Suicide();
end;

--

function Reinf0()
	LandReinforcement(10);
	Cmd(3,10,6270,7243);
	Suicide();
end;

function Reinf1()
	LandReinforcement(20);
	AskClient("SetCamera(5100,6500)");
	Suicide();
end;


function ReinfA()
	RunScript("A3",100);
	RunScript("A4",100);
	AskClient("SetCamera(6060,4646)");
	Suicide();
end;

function Reinf2()
	LandReinforcement(30);
	AskClient( "SetCamera(1900,6720)" );
--	DeleteReinforcement( 1001);
--	DeleteReinforcement( 1002);
	Suicide();
end;

function Reinf3()
	LandReinforcement(40);
	AskClient( "SetCamera(5440,6920)" ); -- Rocket artillery

	RunScript( "CheckKatyusha", 4000);
	Suicide();
end;

function CheckKatyusha()
	if ( GetNUnitsInScriptGroup(40) <= 0) then
		LandReinforcement(40);
	end;
end;

function KillBridges()
	DamageObject( 10001, 0);
	DamageObject( 10002, 0);
	DamageObject( 10003, 0);
	Suicide();
end;

function RestoreGuns()
	if (GetNUnitsInScriptGroup(1001, 0) <= 0) then
		A1();
	end;
	if (GetNUnitsInScriptGroup(1002, 0) <= 0) then
		A2();
	end;
end;

function A1()
	LandReinforcement(1001);
end;

function A2()
	LandReinforcement(1002);
end;

function A3()
	LandReinforcement(1003);
	Suicide();
end;

function A4()
	LandReinforcement(1004);
	Suicide();
end;
----------------------
function Init()
	RunScript("KillBridges", 1000);
	RunScript("Begin", 0);
	RunScript("RevealObj0", 1000);
	RunScript("Objective0", 2000);
	RunScript( "RestoreGuns", 4000);
	SetCheatDifficultyLevel( 1 );
	SetIGlobalVar( "nogeneral_script", 1);
end;
