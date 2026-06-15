function Objective0()
	if ((GetNUnitsInScriptGroup(1000) + GetNUnitsInScriptGroup(1002)) < 2) then
		ObjectiveChanged(0, 2);
		SetIGlobalVar("temp.Summa.objective.0", 2);
		RunScript( "Defeat", 5000);
		Suicide();
	end;
end;

function Defeat()
	Loose();
	Suicide();
end;

function WinWin()
	Win(0);
	Suicide();
end;

function ToWin()
	if ((GetIGlobalVar("temp.Summa.objective.1", 0) * GetIGlobalVar("temp.Summa.objective.2", 0)) == 1) then
		ObjectiveChanged(0, 1);
		RunScript( "WinWin", 5000);
		Suicide();
	end;
end;

function Objective1()
local Numunits = GetNUnitsInScriptGroup(510, 1);
	if (Numunits <= 6) then
		SetIGlobalVar("temp.Summa.objective.1", 1);
		ObjectiveChanged(1, 1)
		Suicide();
	end;
end;

function Objective2()
local Numunits = GetNUnitsInScriptGroup(120);
	if (Numunits <= 0) then
		SetIGlobalVar("temp.Summa.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function Objective3()
	if ( GetIGlobalVar("temp.Summa.objective.100", 0) == 1) then
	local ammo1, ammo2 = GetNAmmo(1002);
		if ( ammo1 > 0) then
			SetIGlobalVar("temp.Summa.objective.3", 1);
			ObjectiveChanged(3, 1);
			Suicide();
		end;
	end;
end;

function Reinforce1()
	if ( GetNUnitsInScriptGroup(1001, 0) < 4) then
		LandReinforcement(1);
		Suicide();
	end;
end;

function RevealObjective0()
	if ( GetIGlobalVar("temp.Summa.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Summa.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Summa.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
		Suicide();
	else Suicide(); end;
end;

function RevealObjective3()
local ammo1, ammo2 = GetNAmmo(1002);
	if ( GetIGlobalVar("temp.Summa.objective.3", 0) == 0) then
	if ( ammo1 <= 0) then
		ObjectiveChanged(3, 0);
		SetIGlobalVar("temp.Summa.objective.100", 1);
		Suicide();
	end;
	end;
end;

function Init()
	RunScript( "RevealObjective0", 3000); -- "KVs must survive"
	RunScript( "RevealObjective1", 8000); -- "destroy pillboxes"
	RunScript( "RevealObjective2", 13000); -- "destroy command center"
	RunScript( "RevealObjective3", 2000); -- "KV-2 is out of ammo"

	RunScript( "Reinforce1", 30000);

	RunScript( "ToWin", 4000);
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000); -- "KV-2 is out of ammo"
end;
