function BridgeAttack()
local A_Enter = 6;
local A_Swarm = 3;
local A_Rotate = 8;
local A_Uninstall = 18;
local A_Install = 17;
local A_ArtBomb = 16;
	if ((GetNUnitsInArea (0, "B1") > 0) and ( GetNUnitsInScriptGroup(3, 1) > 0)) then
--		GiveCommand( A_Uninstall, 2);
--		GiveQCommand( A_Rotate, 2, 6700, 7900);
--		GiveQCommand( A_Install, 2);
		GiveCommand( A_ArtBomb, 2, 6700, 7900);
		RunScript( "ArtStop", 80000);
		Suicide();
	end;
end;

function ArtStop()
local A_Stop = 9;
	Cmd(A_Stop, 2);
	Suicide();
end;

function ToWin()
	if ((GetIGlobalVar("temp.Daugavpils.objective.0", 0) * GetIGlobalVar("temp.Daugavpils.objective.1", 0) * GetIGlobalVar("temp.Daugavpils.objective.2", 0)) == 1) then
		Win(0);
		Suicide();
	end;
end;

function TobeDefeated()
local num = GetNUnitsInScriptGroup(1000);
	if ( GetIGlobalVar("temp.daugavpils.objective.150", 0) == 1) then
		num = num + GetNUnitsInScriptGroup(100);
	end;
	if ( num <= 0) then
		Loose();
		Suicide();
	end;
end;

-- destroy defence at north-east
function Objective0()
	if (GetNUnitsInScriptGroup(501, 1) <= 0) then
		SetIGlobalVar("temp.Daugavpils.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		RunScript( "RevealObjective2", 10000);
		Suicide();
	end;
end;

-- capture warehouse
function Objective1()
	if ( GetNUnitsInScriptGroup(5, 1) <= 0) then
		SetIGlobalVar("temp.Daugavpils.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
end;

-- destroy western defence near bridge
function Objective2()
	if (GetNUnitsInScriptGroup(10, 1) <= 0) then
		SetIGlobalVar("temp.Daugavpils.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Daugavpils.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Daugavpils.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Daugavpils.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Daugavpils.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function Reinforce1()
	if ( GetNUnitsInScriptGroup(10, 1) < 10) then
		LandReinforcement(1);
		SetIGlobalVar("temp.daugavpils.objective.150", 1);
		Suicide();
	end;
end;

function Init()
	RunScript( "BridgeAttack", 1000);
	RunScript( "ToWin", 5000);
	RunScript( "TobeDefeated", 5000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "RevealObjective0", 3000);
	RunScript( "Reinforce1", 5000);
end;
