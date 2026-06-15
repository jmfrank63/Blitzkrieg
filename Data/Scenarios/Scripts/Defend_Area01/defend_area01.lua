function Objective0()
local n = 1011;
local num = 0;
	if ( GetNUnitsInScriptGroup(18, 1) <= 0) then
		SetIGlobalVar("temp.template01.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		Suicide();
	end;
	while (n <= 1014)
	do
		num = num + GetNUnitsInScriptGroup(n, 0);
		n = n + 1;
	end;
	if ( num < 2) then
		SetIGlobalVar("temp.template01.objective.0", 2);
		ObjectiveChanged(0, 2);

		Suicide();
	end;

end;

function Rush1()
local Coords = {x = 0, y = 0};

--	Coords.x, Coords.y = GetScriptAreaParams("Escape");
	Cmd(3, 18, GetScriptAreaParams("Escape"));
	Suicide();
end;

function Objective1()
	if ( GetNUnitsInScriptGroup(15, 1) <= 0) then
		SetIGlobalVar("temp.template01.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

function ToWin()
	if ( (GetIGlobalVar("temp.template01.objective.0", 0) * GetIGlobalVar("temp.template01.objective.1", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function TobeDefeated()
	if ( (GetNUnitsInScriptGroup(1000, 0) <= 0) or ( GetIGlobalVar( "temp.template01.objective.0", 0) == 2)) then
		Loose();
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.template01.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	RunScript( "Rush1", GetIGlobalVar( "temp.timetowait", 0));
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.template01.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function DamageGuns()
local n = 1011;
	while (n <= 1014)
	do
		DamageObject(n, RandomInt(15) + 20 );
		n = n + 1;
	end;
	Suicide();
end;

function Presets()
	if (GetSGlobalVar( "Campaign.Current.Name", 0) ~= "scenarios\\campaigns\\allies\\allies") then -- if not playing allies
		SetIGlobalVar( "temp.timetowait", 90000); -- 90 seconds to prepare
	else
		SetIGlobalVar( "temp.timetowait", 120000); -- 120 seconds to prepare if playing allies
	end;
	Suicide();
end;

function Init()
	RunScript( "DamageGuns", 0);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "RevealObjective0", 2000);
--	RunScript( "Rush1", 80000);
	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Presets", 0);
end;
