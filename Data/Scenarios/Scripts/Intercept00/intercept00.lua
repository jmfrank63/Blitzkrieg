function Objective0()
	if ( GetNUnitsInScriptGroup(13, 1) <= 0) then
		SetIGlobalVar("temp.template01.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		RunScript( "Rush1", GetIGlobalVar( "temp.timetowait", 0));
		Suicide();
	end;
end;

function Rush1()
--local x1, y1, hw1, hl1 = GetScriptAreaParams("Ambush");
--local x2, y2, hw2, hl2 = GetScriptAreaParams("Warehouse");
	Cmd(3, 18, GetScriptAreaParams("Ambush"));
	QCmd(3, 18, GetScriptAreaParams("Warehouse"));
	Suicide();
end;

function Objective1()
--	if (GetNUnitsInScriptGroup(18, 1) == GetNScriptUnitsInArea( 18, "Warehouse")) then -- condition: current number of units is equal number of units in area
--		SetIGlobalVar("temp.template01.objective.1", 1);
--		ObjectiveChanged(1, 1);
--		Suicide();
--	end;
	if (GetNUnitsInScriptGroup(18, 1) <= 0) then
		SetIGlobalVar("temp.template01.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
	if ( GetNScriptUnitsInArea( 18, "Warehouse") > 0) then
		SetIGlobalVar("temp.template01.objective.1", 2);
		ObjectiveChanged(1, 2);
		Suicide();
	end;
end;

function ToWin()
local startnumunits = GetIGlobalVar( "temp.startnumofunits", 0);
	if ( ((GetIGlobalVar("temp.template01.objective.0", 0) * GetIGlobalVar("temp.template01.objective.1", 0)) ~= 0) and
	  (GetNUnitsInScriptGroup(18, 1) == GetNScriptUnitsInArea( 18, "Warehouse"))) then
		SetIGlobalVar("template.hunted.killed", 100 - ((GetNUnitsInScriptGroup(18) / startnumunits) * 100) ); -- save percentage of destroyed hunted units
		RunScript( "WinWin", 5000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function TobeDefeated()
	if ( GetNUnitsInScriptGroup(1000, 0) <= 0) then
		Loose();
		Suicide();
	end;
end;

function RevealObjective0()
	SetIGlobalVar( "temp.startnumofunits", GetNUnitsInScriptGroup(18)); -- save number of units in group 18
	ObjectiveChanged(0, 0);
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar( "temp.template01.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar( "temp.template01.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function Presets()
--	DisableAviation(0, 0); give scout
	DisableAviation(0, 1);
	DisableAviation(0, 2);

	if (GetSGlobalVar( "Campaign.Current.Name", 0) ~= "scenarios\\campaigns\\allies\\allies") then -- if not playing allies
		DisableAviation(0, 4); -- then also disable ground attack plane
		DisableAviation(0, 3); -- and bombers
		SetIGlobalVar( "temp.timetowait", 100000); -- 100 seconds to prepare ambush
	else
		SetIGlobalVar( "temp.timetowait", 140000); -- 140 seconds to prepare ambush if playing allies
	end;
	Suicide();
end;

function Init()
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "RevealObjective0", 2000);
--	RunScript( "RevealObjective1", 7000);
	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Presets", 0);
end;
