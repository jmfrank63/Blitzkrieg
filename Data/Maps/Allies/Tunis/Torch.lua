function ToWin()
	Win(0);
	Suicide();
end;

function Defeat()
	Loose();
	Suicide();
end;

function TobeDefeated()
	if ( (GetNUnitsInScriptGroup(1000, 0) + GetNUnitsInScriptGroup(1001, 0)) <= 0) then
		RunScript( "Defeat", 3000);
		Suicide();
	end;
end;

function Objective0()
	if (GetNUnitsInScriptGroup(1, 1) <= 0) then
		SetIGlobalVar("temp.Torch.objective.0", 1);
		ObjectiveChanged(0, 1);
		RunScript( "RevealObjective1", 5000);
		Suicide();
	else
	if ( GetNUnitsInScriptGroup(1001, 0) <= 0) then
		SetIGlobalVar("temp.Torch.objective.0", 2);
		ObjectiveChanged(0, 2);
		RunScript( "Defeat", 10000);
		Suicide();
	end;
	end;
end;

function Objective1()
	if ( GetNUnitsInScriptGroup(2, 1) <= 0) then
		SetIGlobalVar("temp.Torch.objective.1", 1);
		ObjectiveChanged(1, 1);
		RunScript( "ToWin", 10000);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Torch.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		AddIronMan( 1 );
		Suicide();
	else Suicide(); end;
end;


function RevealObjective1()
	if ( GetIGlobalVar("temp.Torch.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function Init()
--	RunScript( "ToWin", 4000);
--	RunScript( "Reinf1", 2000);
--	RunScript( "Reinf2", 2000);
	RunScript( "TobeDefeated", 4000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "RevealObjective0", 3000);
end;
