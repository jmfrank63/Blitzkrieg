function TobeDefeated()
	if (GetNUnitsInScriptGroup (1000, 0) <= 0) then
		Loose();
		Suicide();
	end;
end;

function ToWin()
	if ((GetIGlobalVar("temp.Berlin.objective.1", 0) * GetIGlobalVar("temp.Berlin.objective.2", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function Reinf1()
local A_Swarm = 3;
	if (GetNUnitsInArea(0, "NearVillage1") > 0) then
		LandReinforcement(1);
		Suicide();
	end;
end;

function Reinf2()
local A_Swarm = 3;
	if ((GetNUnitsInScriptGroup(502, 1) <= 3) or (GetNUnitsInScriptGroup(503, 1) <= 3 )) then
		LandReinforcement(2);
		Suicide();
	end;
end;

function Objective0()
	if (((GetNUnitsInScriptGroup(501, 1) <= 1) and ( GetNUnitsInArea(0, "HeightsBottom") > 0)) or
	  ((GetNUnitsInScriptGroup(511, 1) <= 1) and ( GetNUnitsInArea(0, "HeightsTop") > 0))) then
		SetIGlobalVar("temp.Berlin.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		RunScript( "RevealObjective2", 10000);
		Suicide();
	end;
end;

function Objective1()
	if ((GetNUnitsInArea(0, "BerlinBottom") > 0) and ( GetNUnitsInScriptGroup(701, 1) <= 0)) then
		SetIGlobalVar("temp.Berlin.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

function Objective2()
	if ((GetNUnitsInArea(0, "BerlinTop") > 0) and ( GetNUnitsInScriptGroup(702, 1) <= 0)) then
		SetIGlobalVar("temp.Berlin.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Berlin.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Berlin.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Berlin.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function Sniper()
	ChangeFormation(1011, 2);
end;

function Init()
	RunScript( "ToWin", 4000);
	RunScript( "Reinf1", 2000);
	RunScript( "Reinf2", 2000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0",3000);
	RunScript( "Objective1",3000);
	RunScript( "Objective2",3000);
	RunScript( "RevealObjective0", 2000);
	RunScript( "Sniper", 1000);
end;
