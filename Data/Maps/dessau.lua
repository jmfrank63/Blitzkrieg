function TobeDefeated()
-- well, i don't know how to loose in this mission...
end;

function ToWin()
	if ( GetIGlobalVar("temp.allies_map.objective.5", 0) == 1) then

--		SetDifficultyLevel(1); -- set back to normal

		Win(0);
		Suicide();
	end;
end;

-- player calls recon plane
function Objective0()
	if ( GetAviationState( 0) == 0) then
		SetIGlobalVar( "temp.allies_map.objective.0", 1);
		ObjectiveChanged(0, 1);
--		SwitchWeatherAutomatic(0);
		RunScript( "DeployTanks", 1000);
		RunScript( "RevealArea", 8000);
		RunScript( "CallFighters", 8000);
		RunScript( "RevealObjective1", 40000);
		Suicide();
	else
		AskClient("HighlightControl(20032)");
	end;
end;

-- player calls fighters
function Objective1()
	if ( GetIGlobalVar( "temp.obj", 0) == 1) then
		if ( GetAviationState( 0) == 1) then
			SetIGlobalVar( "temp.allies_map.objective.1", 1);
			ObjectiveChanged(1, 1);

			RunScript( "EnableAvia2", 40000);
			RunScript( "DestroyPlanes", 30000);
			RunScript( "RevealObjective6", 5000);
			Suicide();
		else
			AskClient("HighlightControl(20031)");
		end;
	end;
end;

-- dogfight
function Objective6()
	if ( GetIGlobalVar( "temp.obj", 0) == 1) then
		if ( GetNUnitsInScriptGroup(10000, 1) <= 0) then
			SetIGlobalVar( "temp.allies_map.objective.6", 1);
			ObjectiveChanged(6, 1);

			if ( GetIGlobalVar( "temp.allies_map.objective.1", 0) == 0) then
				SetIGlobalVar( "temp.allies_map.objective.1", 1);
				ObjectiveChanged(1, 1);
				KillScript( "Objective1" );
			end;

			DisableAviation(1, 1); -- no aviation for AI!

--			LandReinforcement(1); -- AA guns
			RunScript( "DeployAAguns", 5000);
			RunScript( "CloseZone", 20000);
			RunScript( "EnableAvia2", 5000);
			RunScript( "RevealObjective2", 10000);
			Suicide();
		end;
	end;
end;

-- paradrop scout
function Objective2()
	if ( GetNUnitsInArea(0, "Drop") > 0) then
		SetIGlobalVar( "temp.allies_map.objective.2", 1);
		ObjectiveChanged(2, 1);

		RunScript( "PatrolBMW", 25000);

		RunScript( "ScoutProblem", 3000);

		if ( GetIGlobalVar( "temp.crap_Para", 0) == 0) then
			KillScript( "BlinkParadrop" );
		end;

		RunScript( "RevealObjective3", 5000);
		Suicide();
	end;
end;

-- sneak and eliminate big AAgun crews
function Objective3()
local AAguns1_ScriptID = 1;
--	if ( GetIGlobalVar( "temp.obj", 0) == 3) then
		if ( GetNUnitsInScriptGroup(AAguns1_ScriptID, 1) <= 0) then
			SetIGlobalVar( "temp.allies_map.objective.3", 1);
			ObjectiveChanged(3, 1);

			SetGameSpeed(0);

			if ( GetIGlobalVar( "temp.crap2", 0) == 0) then
				KillScript( "ScoutProblem" );
			end;

			RunScript( "RevealMap", 5000);
			RunScript( "RevealObjective4", 5000);
			Suicide();
		end;
--	end;
end;

-- call bombers and destroy small AAguns
function Objective4()
local AAguns2_ScriptID = 2;
	if ( GetIGlobalVar( "temp.obj", 0) == 4) then
		if (( GetIGlobalVar( "temp.lala1", 0) == 0) and ( GetAviationState( 0) == 3)) then
			RunScript( "EnableAvia4", 40000);
			SetIGlobalVar( "temp.lala1", 1);
		end;

		if ( GetNUnitsInScriptGroup(AAguns2_ScriptID, 1) <= 0) then
			SetIGlobalVar( "temp.allies_map.objective.4", 1);
			ObjectiveChanged(4, 1);

		if ( GetIGlobalVar( "temp.crap_Bomb", 0) == 0) then
			KillScript( "BlinkBomber" );
		end;

			RunScript( "RevealObjective5", 30000);
			Suicide();
		end;
	end;
end;

-- call close support and destroy all remaining enemy
function Objective5()
local Enemy = GetNUnitsInScriptGroup(101, 1) + GetNUnitsInScriptGroup(123, 1) + GetNUnitsInScriptGroup(300, 1) + GetNUnitsInScriptGroup(301, 1) + GetNUnitsInScriptGroup(302, 1) +
	GetNUnitsInScriptGroup(303, 1) + GetNUnitsInScriptGroup(304, 1);
	if ( Enemy <= 0) then
		SetIGlobalVar( "temp.allies_map.objective.5", 1);
		ObjectiveChanged(5, 1);

		Suicide();
	end;
end;

function ScoutProblem()
local paraID = GetIGlobalVar( "ParadropSquad.ScriptID", 0);
	if ( GetNUnitsInScriptGroup( paraID ) <= 0) then
		ObjectiveChanged(7, 0);
		SetIGlobalVar( "ParadropSquad.ScriptID", paraID + 1);
		SetIGlobalVar( "temp.akindofproblem", 7);
		RunScript( "Objective78", 3000);

		SetIGlobalVar( "temp.crap2", 1);
		Suicide();
		return 0;
	end;
	if ( GetNAmmo( paraID) <= 0) then
		ObjectiveChanged(8, 0);
		SetIGlobalVar( "ParadropSquad.ScriptID", paraID + 1);
		SetIGlobalVar( "temp.akindofproblem", 8);
		RunScript( "Objective78", 3000);

		SetIGlobalVar( "temp.crap2", 1);
		Suicide();
		return 0;
	end;
end;

function Objective78()
	if ( (GetAviationState( 0) == 2) and ( GetNScriptUnitsInArea( GetIGlobalVar( "ParadropSquad.ScriptID", 0), "Drop") > 0)) then
		ObjectiveChanged( GetIGlobalVar( "temp.akindofproblem", 0), 1);
		RunScript( "ScoutProblem", 3000);
		SetIGlobalVar( "temp.crap2", 0);
		Suicide();
	end;
end;

function EnableAvia2()
	if ( GetIGlobalVar( "temp.crap1", 0) == 0) then
		EnableAviation(0, 2); -- enable paradropper
		Trace("===EnableAvia2===");
		RunScript( "BlinkParadrop", 4000);

		SetIGlobalVar( "temp.crap1", 1);
	end;
	Suicide();
end;

function BlinkParadrop()
	if ( GetAviationState( 0) == 2) then
		SetIGlobalVar( "temp.crap_Para", 1);
		Suicide();
	else
		AskClient("HighlightControl(20033)");
	end;
end;

function BlinkBomber()
	if ( GetAviationState( 0) == 3) then
		SetIGlobalVar( "temp.crap_Bomb", 1);
		Suicide();
	else
		AskClient("HighlightControl(20030)");
	end;
end;

function EnableAvia4()
	EnableAviation(0, 4); -- enable close support
	Suicide();
end;

function BlinkCS()
	if ( GetAviationState( 0) == 4) then
		SetIGlobalVar( "temp.crap_CS", 1);
		Suicide();
	else
		AskClient("HighlightControl(20035)");
	end;
end;


function CallFighters()
local ACTION_FIGHTERS = 20;
	EnableAviation(1, 1);
	Cmd(ACTION_FIGHTERS, 10000, 1, 3846 + RandomInt(500), 3846 + RandomInt(500));
	Suicide();
end;

function DestroyPlanes()
	if ( GetNUnitsInScriptGroup(10000) > 0) then
		DamageObject( 10000, 0);
	end;
	Suicide();
end;

function CloseZone()
	ViewZone("View1", 0);
	Suicide();
end;

function DeployAAguns()
local ACTION_DEPLOY = 32;
local ACTION_MOVE = 0;

	Cmd(ACTION_DEPLOY, 200, GetObjCoord(200));
	QCmd(ACTION_MOVE, 200, 65*64, 43*64);

	Cmd(ACTION_DEPLOY, 201, GetObjCoord(201));
	QCmd(ACTION_MOVE, 201, 63*64, 43*64);

	Cmd(ACTION_DEPLOY, 202, GetObjCoord(202));
	QCmd(ACTION_MOVE, 202, 61*64, 43*64);

	Cmd(ACTION_DEPLOY, 203, GetObjCoord(203));
	QCmd(ACTION_MOVE, 203, 59*64, 43*64);

	Cmd(ACTION_DEPLOY, 204, GetObjCoord(204));
	QCmd(ACTION_MOVE, 204, 56*64, 43*64);

	Suicide();
end;

function DeployTanks()
local ACTION_MOVE = 0;

	Cmd(ACTION_MOVE, 300, 76*64, 82*64);

	Cmd(ACTION_MOVE, 301, 76*64, 86*64);

	Cmd(ACTION_MOVE, 302, 76*64, 90*64);

	Cmd(ACTION_MOVE, 303, 76*64, 94*64);

	Cmd(ACTION_MOVE, 304, 76*64, 98*64);

	Suicide();
end;

function MortarBig_Fire()
local ACTION_SUPPFIRE = 16;

	if ( GetNUnitsInScriptGroup(101) > 0) then
		Cmd(ACTION_SUPPFIRE, 101, 28*64, 30*64);
		RunScript( "MortarBig_Fire", RandomInt(5000) + 30000);
	end;
	Suicide();
end;

function Mortar_Fire()
local ACTION_SUPPFIRE = 16;
local Points = {{x = 35*64, y = 23*64}, {x = 18*64, y = 29*64}, {x = 27*64, y = 25*64}};
local i = GetIGlobalVar( "temp.points", 0) + 1;

	if ( GetNUnitsInScriptGroup(100) > 0) then
		Cmd(ACTION_SUPPFIRE, 100, Points[i].x, Points[i].y);

		if (i > 2) then
			i = 0;
		end;

		SetIGlobalVar( "temp.points", i);

	else Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.allies_map.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.allies_map.objective.1", 0) == 0) then
		SetIGlobalVar( "temp.obj", 1);
		EnableAviation(0, 1);
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.allies_map.objective.2", 0) == 0) then
		SetIGlobalVar( "temp.obj", 2);
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.allies_map.objective.3", 0) == 0) then
		SetIGlobalVar( "temp.obj", 3);
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.allies_map.objective.4", 0) == 0) then
		SetIGlobalVar( "temp.obj", 4);
		EnableAviation(0, 3);
		RunScript( "BlinkBomber", 4000);
--		SetDifficultyLevel(0); -- set easy
		ObjectiveChanged(4, 0);
	end;
	Suicide();
end;

function RevealObjective5()
	if ( GetIGlobalVar("temp.allies_map.objective.5", 0) == 0) then
		SetIGlobalVar( "temp.obj", 5);
		RunScript( "BlinkCS", 4000);
		ObjectiveChanged(5, 0);
	end;
	Suicide();
end;

function RevealObjective6()
	if ( GetIGlobalVar("temp.allies_map.objective.6", 0) == 0) then
		ObjectiveChanged(6, 0);
	end;
	Suicide();
end;

function Presets()
	SwitchWeatherAutomatic( 0);
	SwitchWeather( 0);

	DisableAviation(0, 1);
	DisableAviation(0, 2);
	DisableAviation(0, 3);
	DisableAviation(0, 4);

	DisableAviation(1, 0);
	DisableAviation(1, 1);
	DisableAviation(1, 2);
	DisableAviation(1, 3);
	DisableAviation(1, 4);
	RunScript( "DamageBridge", 2000);
	SetIGlobalVar( "ParadropSquad.ScriptID", 5000);
end;

function RevealArea()
	ViewZone("View1", 1);
	Suicide();
end;

function RevealMap()
	ViewZone("AllMap", 1);
	Suicide();
end;

function PatrolBMW()
local ACTION_SWARM = 3;
local Points1 = {{x = 2552, y = 6970}, {x = 2600, y = 4700}, {x = 4800, y = 4700}, {x = 4840, y = 7000}};
local i = GetIGlobalVar( "temp.points1", 0) + 1;

      Cmd (ACTION_SWARM, 800, Points1[i].x, Points1[i].y );
      if (i > 3) then
	i = 0;
      end;

      SetIGlobalVar( "temp.points1", i);
--	      RunScript( "PatrolBMW", 10000);

end;

function DamageBridge()
	DamageObject( 1234, 0);
	Suicide();
end;

function Alarm()
local ACTION_SWARM = 3;
local PARADE = 10;
local PARADE_PARAM = 3;

	if (GetNUnitsInArea(0, "Alarm") > 0) then
--   		Cmd(PARADE, 1111, PARADE_PARAM);
   		QCmd(ACTION_SWARM, 1111, 2550, 7000);
   		RunScript( "SWARM_SQUAD_1111", 10000 + RandomInt(10000) );
		Suicide();
	end;
end;

function SWARM_SQUAD_1111()
local ACTION_SWARM = 3;
	Cmd(ACTION_SWARM, 1111, 310, 7500);
	RunScript( "SWARM_SQUAD1_1111", 50000 + RandomInt(25000));
	Suicide();
end;

function SWARM_SQUAD1_1111()
local ACTION_SWARM = 3;
	if (GetNUnitsInArea(0, "Alarm" ) > 0 ) then
		Cmd(ACTION_SWARM, 1111, 3290, 5510);
		Suicide();
	end;
end;

function Init()
	RunScript( "ToWin", 4000);
--	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0", 4000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "Objective4", 2000);
	RunScript( "Objective5", 2000);
	RunScript( "Objective6", 2000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "MortarBig_Fire", 1000);
	RunScript( "Mortar_Fire", 15000);
	RunScript( "Alarm", 2000);
	Presets();
end;
