--
-- Written by Vinny, also known as Udot
--

function Reinforce1E()
	LandReinforcement( 1);
	SetSGlobalVar("temp.stack.1", "temp.objective.110");
--	CheckReinf("temp.objective.110");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce11E()
	LandReinforcement( 11);
	SetSGlobalVar("temp.stack.1", "temp.objective.111");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce12E()
	LandReinforcement( 12);
	SetSGlobalVar("temp.stack.1", "temp.objective.112");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce13E()
	LandReinforcement( 13);
	SetSGlobalVar("temp.stack.1", "temp.objective.113");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce2E()
	LandReinforcement( 2);
	SetSGlobalVar("temp.stack.1", "temp.objective.120");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce21E()
	LandReinforcement( 21);
	SetSGlobalVar("temp.stack.1", "temp.objective.121");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce3E()
	SetSGlobalVar("temp.stack.1", "temp.objective.130");
	LandReinforcement( 3);
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce31E()
	LandReinforcement( 31);
	SetSGlobalVar("temp.stack.1", "temp.objective.131");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce32E()
	LandReinforcement( 32);
	SetSGlobalVar("temp.stack.1", "temp.objective.132");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce33E()
	LandReinforcement( 33);
	SetSGlobalVar("temp.stack.1", "temp.objective.133");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce34E()
	LandReinforcement( 34);
	SetSGlobalVar("temp.stack.1", "temp.objective.134");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce4E()
	LandReinforcement( 4);
	SetSGlobalVar("temp.stack.1", "temp.objective.140");
	RunScript( "CheckReinf", 3000);
	Suicide();

end;

function Reinforce41E()
	LandReinforcement( 41);
	SetSGlobalVar("temp.stack.1", "temp.objective.141");
	RunScript( "CheckReinf", 3000);
	Suicide();

end;

function Reinforce42E()
	LandReinforcement( 42);
	SetSGlobalVar("temp.stack.1", "temp.objective.142");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce5E()
	LandReinforcement( 5);
	SetSGlobalVar("temp.stack.1", "temp.objective.150");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce51E()
	LandReinforcement( 51);
	SetSGlobalVar("temp.stack.1", "temp.objective.151");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function Reinforce52E()
	LandReinforcement( 52);
	SetSGlobalVar("temp.stack.1", "temp.objective.152");
	RunScript( "CheckReinf", 3000);
	Suicide();
end;

function CheckReinf()
local strName = GetSGlobalVar("temp.stack.1", 0);
	SetIGlobalVar(strName, 1);
	Suicide();
end;

function ReinforceUSSR1()
	LandReinforcement( 100);
	Suicide();
end;

function ReinforceUSSR2()
	LandReinforcement( 101);
	Suicide();
end;

function Attack1()
local A_Follow = 39;
	GiveCommand(A_Follow, 1, 10);

	RunScript( "Remind1", 4000);
	Suicide();
end;

function Attack11()
local A_Follow = 39;
	GiveCommand( A_Follow, 11, 110);

	RunScript( "Remind2", 4000);
	Suicide();
end;

function Remind1()
	if ( GetNUnitsInScriptGroup(10) <= 0) then
	if ( GetNUnitsInScriptGroup(1) > 0) then
		Cmd(3, 1, 5500, 9700);
		Suicide();
	else Suicide();
	end;
	end;
end;

function Remind2()
	if ( GetNUnitsInScriptGroup(110) <= 0) then
	if ( GetNUnitsInScriptGroup(11) > 0) then
		Cmd(3, 11, 5500, 9700);
		Suicide();
	else Suicide();
	end;
	end;
end;

function Attack12()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(12) > 0) then
	GiveCommand( A_Swarm, 12, 3500, 9700);
	Suicide();
	end;
end;

function Attack13()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(13) > 0) then
	GiveCommand( A_Swarm, 13, 3500, 9700);
	Suicide();
	end;
end;

function Attack2()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(2) > 0) then
	GiveCommand( A_Swarm, 2, 1070, 3950);
	GiveQCommand( A_Swarm, 2, 3800, 5400);
	GiveQCommand( A_Swarm, 2, 3700, 9500);
	Suicide();
	end;
end;

function Attack21()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(21) > 0) then
	GiveCommand( A_Swarm, 21, 1070, 3950);
	GiveQCommand(A_Swarm, 21, 3800, 5400);
	GiveQCommand( A_Swarm, 21, 3700, 9500);
	Suicide();
	end;
end;

function Attack3()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(3) > 0) then
	GiveCommand( A_Swarm, 3, 6000, 9000);
	Suicide();
	end;
end;

function Attack31()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(31) > 0) then
	GiveCommand( A_Swarm, 31, 6000, 9000);
	Suicide();
	end;
end;

function Attack32()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(32) > 0) then
	GiveCommand(A_Swarm, 32, 6000, 9000);
	Suicide();
	end;
end;

function Attack33()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(33) > 0) then
	GiveCommand( A_Swarm, 33, 6000, 9000);
	Suicide();
	end;
end;

function Attack34()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(34) > 0) then
	GiveCommand( A_Swarm, 34, 6000, 9000);
	Suicide();
	end;
end;

function Attack4()
local A_Follow = 39;
	GiveCommand( A_Follow, 4, 40);

	RunScript( "Remind3", 4000);
	Suicide();
end;

function Remind3()
	if ( GetNUnitsInScriptGroup(40) <= 0) then
	if ( GetNUnitsInScriptGroup(4) > 0) then
		Cmd(3, 4, 5900, 9500);
		Suicide();
	else Suicide();
	end;
	end;
end;

function Attack41()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(41) > 0) then
	GiveCommand(A_Swarm, 41, 5900, 9500);
	Suicide();
	end;
end;

function Attack42()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(42) > 0) then
	GiveCommand( A_Swarm, 42, 5900, 9500);
	Suicide();
	end;
end;

function Attack5()
local A_Follow = 39;
	GiveCommand( A_Follow, 5, 50);

	RunScript( "Remind4", 4000);
	Suicide();
end;

function Attack51()
local A_Follow = 39;
	GiveCommand( A_Follow, 51, 510);

	RunScript( "Remind5", 4000);
	Suicide();
end;

function Remind4()
	if ( GetNUnitsInScriptGroup(50) <= 0) then
	if ( GetNUnitsInScriptGroup(5) > 0) then
		Cmd(3, 5, 3600, 9500);
		Suicide();
	else Suicide();
	end;
	end;
end;

function Remind5()
	if ( GetNUnitsInScriptGroup(510) <= 0) then
	if ( GetNUnitsInScriptGroup(51) > 0) then
		Cmd(3, 51, 3600, 9500);
		Suicide();
	else Suicide();
	end;
	end;
end;

function Attack52()
local A_Swarm = 3;
	if (GetNUnitsInScriptGroup(52) > 0) then
		GiveCommand(A_Swarm, 52, 3600, 9500);
		Suicide();
	end;
end;

function ToWin()
	if ( (GetIGlobalVar("temp.Kursk.objective.0", 0) * GetIGlobalVar("temp.Kursk.objective.6", 0)) == 1) then
		RunScript( "RevealObjective7", 3000);
		RunScript( "WinWin", 8000);
		Suicide();
	end;
end;

function WinWin()
	Win(0);
	Suicide();
end;

function TobeDefeated()
	if (GetNUnitsInScriptGroup(1000, 0) <= 0) then
		Loose();
		Suicide();
	end;
end;

function Objective0()
	if ( (GetIGlobalVar("temp.Kursk.objective.1", 0) * GetIGlobalVar("temp.Kursk.objective.2", 0) * GetIGlobalVar("temp.Kursk.objective.3", 0) *
	  GetIGlobalVar("temp.Kursk.objective.4", 0) * GetIGlobalVar("temp.Kursk.objective.5", 0)) == 1) then
		SetIGlobalVar("temp.Kursk.objective.0", 1);

		ObjectiveChanged(0, 1);

		RunScript( "RevealObjective6", 5000);
		Suicide();
	end;
end;

function Objective1()
	if ((GetIGlobalVar("temp.objective.110",0) * GetIGlobalVar("temp.objective.120",0) * GetIGlobalVar("temp.objective.130",0) *
	  GetIGlobalVar("temp.objective.150",0)) == 1) then

	local num = GetNUnitsInScriptGroup(10) + GetNUnitsInScriptGroup(1) + GetNUnitsInScriptGroup(2) + GetNUnitsInScriptGroup(3) + GetNUnitsInScriptGroup(50) +
	  GetNUnitsInScriptGroup(5);
	if (num <= 3) then
		SetIGlobalVar("temp.Kursk.objective.1", 1);
		ObjectiveChanged(1, 1);
		Suicide();
	end;
	end;
end;

function Objective2()
	if ((GetIGlobalVar("temp.objective.111",0) * GetIGlobalVar("temp.objective.131",0) * GetIGlobalVar("temp.objective.140",0) *
	  GetIGlobalVar("temp.objective.151",0)) == 1) then

	local num = GetNUnitsInScriptGroup(11) + GetNUnitsInScriptGroup(110) + GetNUnitsInScriptGroup(31) + GetNUnitsInScriptGroup(4) + GetNUnitsInScriptGroup(51) +
	  GetNUnitsInScriptGroup(40) + GetNUnitsInScriptGroup(510);
	if (num <= 3) then
		SetIGlobalVar("temp.Kursk.objective.2", 1);
  		ObjectiveChanged(2, 1);
		Suicide();
	end;
	end;
end;

function Objective3()
	if ((GetIGlobalVar("temp.objective.112",0) * GetIGlobalVar("temp.objective.132",0) * GetIGlobalVar("temp.objective.141",0)) == 1) then

	local num = GetNUnitsInScriptGroup(12) + GetNUnitsInScriptGroup(32) + GetNUnitsInScriptGroup(41);
	if (num <= 1) then
		SetIGlobalVar("temp.Kursk.objective.3", 1);
		ObjectiveChanged(3, 1);
		Suicide();
	end;
	end;
end;

function Objective4()
	if ((GetIGlobalVar("temp.objective.121",0) * GetIGlobalVar("temp.objective.133",0) * GetIGlobalVar("temp.objective.152",0)) == 1) then

	local num = GetNUnitsInScriptGroup(21) + GetNUnitsInScriptGroup(33) + GetNUnitsInScriptGroup(52);
	if (num <= 1) then
		SetIGlobalVar("temp.Kursk.objective.4", 1);
		ObjectiveChanged(4, 1);
		Suicide();
	end;
	end;
end;

function Objective5()
	if ((GetIGlobalVar("temp.objective.113",0) * GetIGlobalVar("temp.objective.134",0) * GetIGlobalVar("temp.objective.142",0)) == 1) then

	local num = GetNUnitsInScriptGroup(13) + GetNUnitsInScriptGroup(34) + GetNUnitsInScriptGroup(42);
	if (num <= 0) then
		SetIGlobalVar("temp.Kursk.objective.5", 1);
		ObjectiveChanged(5, 1);
		Suicide();
	end;
	end;
end;

function Objective6()
local num = GetNUnitsInScriptGroup(99, 1);
	if (num <= 0) then
		SetIGlobalVar("temp.Kursk.objective.6", 1);
		ObjectiveChanged(6, 1);

		Suicide();
	end;
end;

function RevealObjective0()
	ObjectiveChanged(0, 0);
	Suicide();
end;

function RevealObjective1()
	if ( GetIGlobalVar("temp.Kursk.objective.1", 0) == 0) then
		ObjectiveChanged(1, 0);
	end;
	Suicide();
end;

function RevealObjective2()
	if ( GetIGlobalVar("temp.Kursk.objective.2", 0) == 0) then
		ObjectiveChanged(2, 0);
	end;
	Suicide();
end;

function RevealObjective3()
	if ( GetIGlobalVar("temp.Kursk.objective.3", 0) == 0) then
		ObjectiveChanged(3, 0);
	end;
	Suicide();
end;

function RevealObjective4()
	if ( GetIGlobalVar("temp.Kursk.objective.4", 0) == 0) then
		ObjectiveChanged(4, 0);
	end;
	Suicide();
end;

function RevealObjective5()
	if ( GetIGlobalVar("temp.Kursk.objective.5", 0) == 0) then
		ObjectiveChanged(5, 0);
	end;
	Suicide();
end;

function RevealObjective6()
	if ( GetIGlobalVar("temp.Kursk.objective.6", 0) == 0) then
		ObjectiveChanged(6, 0);
	end;
	Suicide();
end;

function RevealObjective7()
	if ( GetIGlobalVar("temp.Kursk.objective.7", 0) == 0) then
		ObjectiveChanged(7, 0);
	end;
	Suicide();
end;


function Init()
 -- 1
	RunScript( "RevealObjective1", 8000);
	RunScript( "Reinforce1E", 10000);
	RunScript( "Reinforce2E", 15000);
	RunScript( "Reinforce3E", 20000);
	RunScript( "Reinforce5E", 25000);
 	RunScript( "Attack1", 15000);
	RunScript( "Attack2", 20000);
--	RunScript( "Attack3", 17000);
 	RunScript( "Attack5", 30000);
-- 2
	RunScript( "RevealObjective2", 220000);
	RunScript( "Reinforce11E", 300000);
	RunScript( "Reinforce31E", 266000);
	RunScript( "Reinforce4E", 210000);
	RunScript( "Reinforce51E", 320000);
	RunScript( "Attack11", 305000);
--	RunScript( "Attack31", 267000);
	RunScript( "Attack4", 215000);
	RunScript( "Attack51", 325000);
-- 3
	RunScript( "RevealObjective3", 450000);
	RunScript( "Reinforce12E", 460000);
	RunScript( "ReinforceUSSR1", 440000);
	RunScript( "Reinforce32E", 500000);
	RunScript( "Reinforce41E", 480000);
	RunScript( "Attack12", 465000);
--	RunScript( "Attack32", 501000);
	RunScript( "Attack41", 485000);
-- 4
	RunScript( "RevealObjective4", 690000);
	RunScript( "Reinforce21E", 690000);
	RunScript( "Reinforce33E", 680000);
	RunScript( "Reinforce52E", 720000);
	RunScript( "Attack21", 695000);
--	RunScript( "Attack33", 681000);
	RunScript( "Attack52", 725000);
-- 5
	RunScript( "RevealObjective5", 910000);
	RunScript( "ReinforceUSSR2", 1000000);
	RunScript( "Reinforce13E", 950000);
	RunScript( "Reinforce34E", 900000);
	RunScript( "Reinforce42E", 930000);
	RunScript( "Attack13", 955000);
--	RunScript( "Attack34", 901000);
	RunScript( "Attack42", 935000);

	RunScript( "TobeDefeated", 6000);

	RunScript( "RevealObjective0", 3000); -- "u must defend"

	RunScript( "Objective0", 2000);
	RunScript( "Objective1", 2000);
	RunScript( "Objective2", 2000);
	RunScript( "Objective3", 2000);
	RunScript( "Objective4", 2000);
	RunScript( "Objective5", 2000);
	RunScript( "Objective6", 2000);
	RunScript( "ToWin", 2000);
end;
