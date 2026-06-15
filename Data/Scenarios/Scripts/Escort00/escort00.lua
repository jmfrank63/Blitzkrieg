function Rush1()
	if ( GetNUnitsInScriptGroup(18) > 0) then
	local Team = {};
	local n, m = 1, 0;
	while (n <= 4)
	do
		if (GetNUnitsInScriptGroup(1000 + n) > 0) then
			m = m + 1;
			Team[m] = 1000 + n;
		end;
		n = n + 1;
	end;
	if (m > 0) then
		local Coords = {{x = 0, y = 0}, {x = 0, y = 0}, {x = 0, y = 0}, {x = 0, y = 0}};
		n = 1;
		while (n <= m)
		do
			Coords[n].x, Coords[n].y = GetObjCoord(Team[n]);
			n = n + 1;
		end;
		n = RandomInt(m) + 1;
		Cmd(3, 18, Coords[n].x, Coords[n].y);
	end;
--	RunScript( "Rest", 10000);
	else Suicide();
	end;
end;

function Rest()
	if ( GetNUnitsInScriptGroup(18) > 0) then
		Cmd(9, 18);
	end;
	Suicide();
end;

function Objective0()
	n = 1001;
	while (n <= 1004) do
		if ( GetNScriptUnitsInArea( n, "Escape") > 0) then
			SetIGlobalVar("temp.template01.objective.0", 1);
			ObjectiveChanged(0, 1);
			Suicide();
			break;
		end;
		n = n + 1;
	end;

	local num = GetNUnitsInScriptGroup(1001, 0) + GetNUnitsInScriptGroup(1002, 0) + GetNUnitsInScriptGroup(1003, 0) + GetNUnitsInScriptGroup(1004, 0);
	if (num <= 0) then
		SetIGlobalVar("temp.template01.objective.0", 2);
		ObjectiveChanged(0, 2);
		Suicide();
	end;
end;

function ToWin()
local n = GetIGlobalVar("temp.stack.1", 0); -- get number of units in group 1001 at start
	if ( GetIGlobalVar("temp.template01.objective.0", 0) == 1) then
		RunScript( "WinWin", 5000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function TobeDefeated()
	if (( GetNUnitsInScriptGroup(1000, 0) <= 0) or (GetIGlobalVar("temp.template01.objective.0", 0) == 2)) then
		Loose();
		Suicide();
	end;
end;

function RevealObjective0()
	SetIGlobalVar("temp.stack.1", GetNUnitsInScriptGroup(1001)); -- save number of units in group 18
	ObjectiveChanged(0, 0);
	Suicide();
end;

function StartRushCheck()
local n = 1;
local coord = {x = 0, y = 0};
local lenconst = 1024 * 1024; -- one patch distance
local UnitsConst = 2;
	if ( GetNUnitsInPlayerUF(1) <= ( GetIGlobalVar( "temp.unitsnumber", 0) - UnitsConst)) then
		RunScript( "Rush1", 13000);
		Suicide();
		return 0;
	end;
	while (n <= 4)
	do
		coord.x, coord.y = GetObjCoord(n + 1000);
		coord_old_x, coord_old_y = GetIGlobalVar( "temp.coords_x." .. (n + 1000), 0), GetIGlobalVar( "temp.coords_y." .. (n + 1000), 0);
		len = (coord.x - coord_old_x) * (coord.x - coord_old_x) + (coord.y - coord_old_y) * (coord.y - coord_old_y);
		if ( len > lenconst) then
			RunScript( "Rush1", 13000);
			Suicide();
			break;
		end;
		n = n + 1;
	end;
end;

function SavePositions()
local n = 1;
	while (n <= 4)
	do
		coords1x, coords1y = GetObjCoord(n + 1000);
		SetIGlobalVar( "temp.coords_x." .. (n + 1000), coords1x);
		SetIGlobalVar( "temp.coords_y." .. (n + 1000), coords1y);
		n = n + 1;
	end;
	SetIGlobalVar( "temp.unitsnumber", GetNUnitsInPlayerUF(1));
	Suicide();
end;

function Init()
	RunScript( "Objective0", 2000);
	RunScript( "RevealObjective0", 2000);
	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "StartRushCheck", 3000);
	RunScript( "SavePositions", 0);
end;
