function Objective0()
	if ( GetNUnitsInScriptGroup(12, 1) <= 0) then
		SetIGlobalVar("temp.template00.objective.0", 1);
		ObjectiveChanged(0, 1);
		Suicide();
	end;
end;

function Objective1()
	if ( GetNUnitsInScriptGroup(13, 1) <= 0) then
		SetIGlobalVar("temp.template00.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(16, 1) <= 0) then
		SetIGlobalVar("temp.template00.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function Objective3()
	if ( GetNUnitsInScriptGroup(15, 1) <= 0) then
		SetIGlobalVar("temp.template00.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
end;

function ToWin()
	if ( (GetIGlobalVar("temp.template00.objective.0", 0) * GetIGlobalVar("temp.template00.objective.1", 0) * GetIGlobalVar("temp.template00.objective.2", 0) *
		GetIGlobalVar("temp.template00.objective.3", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function TobeDefeated()
	if ( GetNUnitsInScriptGroup(1000, 0) <= 0) then
		Loose();
		Suicide();
	end;
end;

function RevealObjective0()
	ObjectiveChanged(0, 0);
	Suicide();
end;

function RevealObjective1()
	ObjectiveChanged(1, 0);
	Suicide();
end;

function RevealObjective2()
	ObjectiveChanged(2, 0);
	Suicide();
end;

function RevealObjective3()
	ObjectiveChanged(3, 0);
	Suicide();
end;

------------------------
function Patrol701()
local ScriptAreas = {};
	ScriptAreas[701] = "center_artil";
	ScriptAreas[702] = "center_sm";
	ScriptAreas[703] = "center_station";
	ScriptAreas[704] = "center_base";

local coeffs = {};
	coeffs[701] = 1448;
	coeffs[702] = 1448;
	coeffs[703] = 1448;
	coeffs[704] = 800;

local delays = {};
	delays[701] = 16000;
	delays[702] = 16000;
	delays[703] = 16000;
	delays[704] = 8000;

local ACTION_SWARM = 3;
local PatrolID = 701;
local vector = {x = 0, y = 0};
local center = {x = 0, y = 0};

	if ( GetNUnitsInScriptGroup(PatrolID) <= 0) then Suicide(); end;
	center.x, center.y = GetScriptAreaParams( ScriptAreas[PatrolID] ); -- center of patrol area

	if ( GetIGlobalVar( "temp.got.coords" .. PatrolID, 0) == 0) then
		vector.x, vector.y = GetObjCoord(PatrolID); -- starting coordinates

		vector.x = vector.x - center.x; -- relative vector
		vector.y = vector.y - center.y;

		vector = norm( vector );

		vector.x = vector.x * coeffs[PatrolID];
		vector.y = vector.y * coeffs[PatrolID];

		SetIGlobalVar( "temp.got.coords" .. PatrolID, 1);
		SetIGlobalVar( "temp.ccw" .. PatrolID, RandomInt(2));
	else
		vector.x = GetFGlobalVar( "temp.vector" .. PatrolID .. ".x", 0);
		vector.y = GetFGlobalVar( "temp.vector" .. PatrolID .. ".y", 0);
	end;

	local ccw = GetIGlobalVar( "temp.ccw" .. PatrolID, 0);

	vector = rot45( vector, ccw );

	if (CheckMapBounds( vector, center ) == 1) then -- if coord is out of map bounds
		ccw = 1 - ccw;
		SetIGlobalVar( "temp.ccw" .. PatrolID, ccw );
		vector = rot90( vector, ccw ); -- then rotate vector back
	end;

	Cmd (ACTION_SWARM, PatrolID, vector.x + center.x, vector.y + center.y);

	SetFGlobalVar( "temp.vector" .. PatrolID .. ".x", vector.x);
	SetFGlobalVar( "temp.vector" .. PatrolID .. ".y", vector.y);

	RunScript( "Patrol" .. PatrolID .. "I", delays[PatrolID] + RandomInt(4000));
	Suicide();

end;

------------------------
function Patrol702()
local ScriptAreas = {};
	ScriptAreas[701] = "center_artil";
	ScriptAreas[702] = "center_sm";
	ScriptAreas[703] = "center_station";
	ScriptAreas[704] = "center_base";

local coeffs = {};
	coeffs[701] = 1448;
	coeffs[702] = 1448;
	coeffs[703] = 1448;
	coeffs[704] = 800;

local delays = {};
	delays[701] = 16000;
	delays[702] = 16000;
	delays[703] = 16000;
	delays[704] = 8000;

local ACTION_SWARM = 3;
local PatrolID = 702;
local vector = {x = 0, y = 0};
local center = {x = 0, y = 0};

	if ( GetNUnitsInScriptGroup(PatrolID) <= 0) then Suicide(); end;
	center.x, center.y = GetScriptAreaParams( ScriptAreas[PatrolID] ); -- center of patrol area

	if ( GetIGlobalVar( "temp.got.coords" .. PatrolID, 0) == 0) then
		vector.x, vector.y = GetObjCoord(PatrolID); -- starting coordinates

		vector.x = vector.x - center.x; -- relative vector
		vector.y = vector.y - center.y;

		vector = norm( vector );

		vector.x = vector.x * coeffs[PatrolID];
		vector.y = vector.y * coeffs[PatrolID];

		SetIGlobalVar( "temp.got.coords" .. PatrolID, 1);
		SetIGlobalVar( "temp.ccw" .. PatrolID, RandomInt(2));
	else
		vector.x = GetFGlobalVar( "temp.vector" .. PatrolID .. ".x", 0);
		vector.y = GetFGlobalVar( "temp.vector" .. PatrolID .. ".y", 0);
	end;

	local ccw = GetIGlobalVar( "temp.ccw" .. PatrolID, 0);

	vector = rot45( vector, ccw );

	if (CheckMapBounds( vector, center ) == 1) then -- if coord is out of map bounds
		ccw = 1 - ccw;
		SetIGlobalVar( "temp.ccw" .. PatrolID, ccw );
		vector = rot90( vector, ccw ); -- then rotate vector back
	end;

	Cmd (ACTION_SWARM, PatrolID, vector.x + center.x, vector.y + center.y);

	SetFGlobalVar( "temp.vector" .. PatrolID .. ".x", vector.x);
	SetFGlobalVar( "temp.vector" .. PatrolID .. ".y", vector.y);

	RunScript( "Patrol" .. PatrolID .. "I", delays[PatrolID] + RandomInt(4000));
	Suicide();

end;

------------------------
function Patrol703()
local ScriptAreas = {};
	ScriptAreas[701] = "center_artil";
	ScriptAreas[702] = "center_sm";
	ScriptAreas[703] = "center_station";
	ScriptAreas[704] = "center_base";

local coeffs = {};
	coeffs[701] = 1448;
	coeffs[702] = 1448;
	coeffs[703] = 1448;
	coeffs[704] = 800;

local delays = {};
	delays[701] = 16000;
	delays[702] = 16000;
	delays[703] = 16000;
	delays[704] = 8000;

local ACTION_SWARM = 3;
local PatrolID = 703;
local vector = {x = 0, y = 0};
local center = {x = 0, y = 0};

	if ( GetNUnitsInScriptGroup(PatrolID) <= 0) then Suicide(); end;
	center.x, center.y = GetScriptAreaParams( ScriptAreas[PatrolID] ); -- center of patrol area

	if ( GetIGlobalVar( "temp.got.coords" .. PatrolID, 0) == 0) then
		vector.x, vector.y = GetObjCoord(PatrolID); -- starting coordinates

		vector.x = vector.x - center.x; -- relative vector
		vector.y = vector.y - center.y;

		vector = norm( vector );

		vector.x = vector.x * coeffs[PatrolID];
		vector.y = vector.y * coeffs[PatrolID];

		SetIGlobalVar( "temp.got.coords" .. PatrolID, 1);
		SetIGlobalVar( "temp.ccw" .. PatrolID, RandomInt(2));
	else
		vector.x = GetFGlobalVar( "temp.vector" .. PatrolID .. ".x", 0);
		vector.y = GetFGlobalVar( "temp.vector" .. PatrolID .. ".y", 0);
	end;

	local ccw = GetIGlobalVar( "temp.ccw" .. PatrolID, 0);

	vector = rot45( vector, ccw );

	if (CheckMapBounds( vector, center ) == 1) then -- if coord is out of map bounds
		ccw = 1 - ccw;
		SetIGlobalVar( "temp.ccw" .. PatrolID, ccw );
		vector = rot90( vector, ccw ); -- then rotate vector back
	end;

	Cmd (ACTION_SWARM, PatrolID, vector.x + center.x, vector.y + center.y);

	SetFGlobalVar( "temp.vector" .. PatrolID .. ".x", vector.x);
	SetFGlobalVar( "temp.vector" .. PatrolID .. ".y", vector.y);

	RunScript( "Patrol" .. PatrolID .. "I", delays[PatrolID] + RandomInt(4000));
	Suicide();

end;

------------------------
function Patrol704()
local ScriptAreas = {};
	ScriptAreas[701] = "center_artil";
	ScriptAreas[702] = "center_sm";
	ScriptAreas[703] = "center_station";
	ScriptAreas[704] = "center_base";

local coeffs = {};
	coeffs[701] = 1448;
	coeffs[702] = 1448;
	coeffs[703] = 1448;
	coeffs[704] = 800;

local delays = {};
	delays[701] = 16000;
	delays[702] = 16000;
	delays[703] = 16000;
	delays[704] = 8000;

local ACTION_SWARM = 3;
local PatrolID = 704;
local vector = {x = 0, y = 0};
local center = {x = 0, y = 0};

	if ( GetNUnitsInScriptGroup(PatrolID) <= 0) then Suicide(); end;
	center.x, center.y = GetScriptAreaParams( ScriptAreas[PatrolID] ); -- center of patrol area

	if ( GetIGlobalVar( "temp.got.coords" .. PatrolID, 0) == 0) then
		vector.x, vector.y = GetObjCoord(PatrolID); -- starting coordinates

		vector.x = vector.x - center.x; -- relative vector
		vector.y = vector.y - center.y;

		vector = norm( vector );

		vector.x = vector.x * coeffs[PatrolID];
		vector.y = vector.y * coeffs[PatrolID];

		SetIGlobalVar( "temp.got.coords" .. PatrolID, 1);
		SetIGlobalVar( "temp.ccw" .. PatrolID, RandomInt(2));
	else
		vector.x = GetFGlobalVar( "temp.vector" .. PatrolID .. ".x", 0);
		vector.y = GetFGlobalVar( "temp.vector" .. PatrolID .. ".y", 0);
	end;

	local ccw = GetIGlobalVar( "temp.ccw" .. PatrolID, 0);

	vector = rot45( vector, ccw );

	if (CheckMapBounds( vector, center ) == 1) then -- if coord is out of map bounds
		ccw = 1 - ccw;
		SetIGlobalVar( "temp.ccw" .. PatrolID, ccw );
		vector = rot90( vector, ccw ); -- then rotate vector back
	end;

	Cmd (ACTION_SWARM, PatrolID, vector.x + center.x, vector.y + center.y);

	SetFGlobalVar( "temp.vector" .. PatrolID .. ".x", vector.x);
	SetFGlobalVar( "temp.vector" .. PatrolID .. ".y", vector.y);

	RunScript( "Patrol" .. PatrolID .. "I", delays[PatrolID] + RandomInt(4000));
	Suicide();

end;

----------------------------
function CheckMapBounds( ...)
local vec = arg[1];
local center = arg[2];
local xmax, ymax = GetMapSize(); -- get map bounds

	if ( ((vec.x + center.x) <= 0) or ((vec.y + center.y) <= 0) or ((vec.x + center.x) >= xmax) or ((vec.y + center.y) >= ymax)) then
		return 1;
	end;

	return 0;
end;

----------------------------
function rot90( ... )
local vec = arg[1];
local ccw = arg[2];

	if ( ccw == 0) then
		ccw = -1;
	else
		ccw = 1;
	end;

	local x1 = vec.y * ccw;
	local y1 = vec.x * ( -ccw);

	return {x = x1, y = y1};
end;
----------------------------
function rot45( ... )
local sqrt2div2 = 0.7071;
local vec = arg[1];
local ccw = arg[2];

	if ( ccw == 0) then
		ccw = -1;
	else
		ccw = 1;
	end;

	local x1 = vec.x * sqrt2div2 + vec.y * ccw * sqrt2div2;
	local y1 = vec.x * (- ccw * sqrt2div2) + vec.y * sqrt2div2;

	return {x = x1, y = y1};
end;

----------------------------
function norm( ... )
	local vec = arg[1];
	local len1 = len (vec);

	local x1 = vec.x / len1;
	local y1 = vec.y / len1;

	return {x = x1, y = y1};
end;

----------------------------
function len( ... )
	local vec = arg[1];

	return sqrt( vec.x * vec.x + vec.y * vec.y );
end;

----------------------------
function sqrt( x)
local ITNUM = 4;
local sp = 0
local i = ITNUM;
local inv = 0;
local a, b;
	if ( x <= 0) then return 0; end;

	if ( x < 1) then
		x = 1 / x;
		inv = 1;
	end;

	while ( x > 16) do
		sp = sp + 1;
		x = x / 16;
	end;

	a = 2;
-- Newtonian algorithm
	while (i > 0) do
		b = x / a;
		a = a + b;
		a = a * 0.5;
		i = i - 1;
	end;
--
	while ( sp > 0) do
		sp = sp - 1;
		a = a * 4;
	end;

	if ( inv == 1) then
		a = 1 / a;
	end;
	return a;
end;
----------------------------

----------------------------

function Patrol701I()
	RunScript( "Patrol701", 1000);
	Suicide();
end;
----------------------------

function Patrol702I()
	RunScript( "Patrol702", 1000);
	Suicide();
end;
----------------------------

function Patrol704I()
	RunScript( "Patrol704", 1000);
	Suicide();
end;
----------------------------

function Init()
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "RevealObjective0", 2000);
	RunScript( "RevealObjective1", 7000);
	RunScript( "RevealObjective2", 12000);
	RunScript( "RevealObjective3", 17000);
	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);

	RunScript( "Patrol701", 1000);
	RunScript( "Patrol702", 1000);
	RunScript( "Patrol704", 1000);
end;
