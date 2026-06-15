function Objective0()
	if ( GetNUnitsInScriptGroup(10, 1) <= 0) then
		SetIGlobalVar("temp.template01.objective.0", 1);
		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective1", 5000);
		RunScript( "Rush1", 60000);
		Suicide();
	end;
end;

function Rush1()
--local x1, y1, hw1, hl1 = GetScriptAreaParams("Bridge_as");
--local x2, y2, hw2, hl2 = GetScriptAreaParams("Warehouse");
	Cmd(3, 18, GetScriptAreaParams("Bridge_as"));
--	QCmd(3, 18, x2, y2);
	Suicide();
end;

function Objective1()
	if ( GetNUnitsInScriptGroup(18, 1) <= 0) then
		SetIGlobalVar("temp.template01.objective.1", 1);
		ObjectiveChanged(1, 1);

		RunScript( "RevealObjective2", 5000);
		Suicide();
	end;
end;

function Objective2()
	if ( GetNUnitsInScriptGroup(15, 1) <= 0) then
		SetIGlobalVar("temp.template01.objective.2", 1);
		ObjectiveChanged(2, 1);
		Suicide();
	end;
end;

function ToWin()
	if ( (GetIGlobalVar("temp.template01.objective.0", 0) * GetIGlobalVar("temp.template01.objective.1", 0) * GetIGlobalVar("temp.template01.objective.2", 0)) == 1) then
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
	if ( GetIGlobalVar("temp.template01.objective.0", 0) == 0) then
		ObjectiveChanged(0, 0);
	end;
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.template01.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.template01.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function Init()
	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "RevealObjective0", 2000);
--	RunScript( "RevealObjective1", 7000);
	RunScript( "ToWin", 4000);
	RunScript( "TobeDefeated", 4000);
end;
