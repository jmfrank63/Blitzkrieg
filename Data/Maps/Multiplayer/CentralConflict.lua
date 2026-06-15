function FlagReinforcement(nParty)
local num;
	if (nParty == 0) then
		if (GetNUnitsInPlayerUF(0) < 18) then
			num = GetTypeByProbability() + 99;
			LandReinforcement(num);
		end;
		if (GetNUnitsInPlayerUF(1) < 18) then
			num = GetTypeByProbability() + 199;
			LandReinforcement(num);
		end;
		if (GetNUnitsInPlayerUF(2) < 18) then
			num = GetTypeByProbability() + 299;
			LandReinforcement(num);
		end;
	end;
	if (nParty == 1) then
		if (GetNUnitsInPlayerUF(3) < 18) then
			num = GetTypeByProbability() + 399;
			LandReinforcement(num);
		end;
		if (GetNUnitsInPlayerUF(4) < 18) then
			num = GetTypeByProbability() + 499;
			LandReinforcement(num);
		end;
		if (GetNUnitsInPlayerUF(5) < 18) then
			num = GetTypeByProbability() + 599;
			LandReinforcement(num);
		end;
	end;
end;

function GetTypeByProbability()
local ProbTable = { 30, 20, 20, 10, 10, 10 };
local SumTable = {0};
local rnd = RandomInt(100);
local i = 1;
local total = 6;

	while (i <= total) do
		SumTable[i + 1] = SumTable[i] + ProbTable[i];

		if ( ( rnd >= SumTable[i] ) and ( rnd < SumTable[i + 1] ) ) then
 			return i;
		end;
		i = i + 1;
	end;
	return 0;
end;
function Begin()
	if (IsPlayerPresent(0)==1) then
		RunScript("A10",500);
		RunScript("A11",500);
		RunScript("Reinf10",3000);
		RunScript("Reinf11",3000);
	end;
	if (IsPlayerPresent(1)==1) then
		RunScript("A20",500);
		RunScript("A21",500);
		RunScript("Reinf20",3000);
		RunScript("Reinf21",3000);
	end;
	if (IsPlayerPresent(2)==1) then
		RunScript("A30",500);
		RunScript("A31",500);
		RunScript("Reinf30",3000);
		RunScript("Reinf31",3000);
	end;
	if (IsPlayerPresent(3)==1) then
		RunScript("A40",500);
		RunScript("A41",500);
		RunScript("Reinf40",3000);
		RunScript("Reinf41",3000);
	end;
	if (IsPlayerPresent(4)==1) then
		RunScript("A50",500);
		RunScript("A51",500);
		RunScript("Reinf50",3000);
		RunScript("Reinf51",3000);
	end;
	if (IsPlayerPresent(5)==1) then
		RunScript("A60",500);
		RunScript("A61",500);
		RunScript("Reinf60",3000);
		RunScript("Reinf61",3000);
	end;
	Suicide();
end;

function Reinf10()
	if (GetNUnitsInScriptGroup(10) == 0) and (GetIGlobalVar("temp.reinf10", 0) == 0) then
		RunScript("A10", 32000);
		RunScript("Reinf10", 2000);
		SetIGlobalVar("temp.reinf10", 1);
		Suicide();
	end;
end;

function Reinf11()
	if (GetNUnitsInScriptGroup(11) == 0) and (GetIGlobalVar("temp.reinf11", 0) == 0) then
		RunScript("A11", 32000);
		RunScript("Reinf11", 2000);
		SetIGlobalVar("temp.reinf11", 1);
		Suicide();
	end;
end;

function Reinf20()
	if (GetNUnitsInScriptGroup(20) == 0) and (GetIGlobalVar("temp.reinf20", 0) == 0) then
		RunScript("A20", 32000);
		RunScript("Reinf20", 2000);
		SetIGlobalVar("temp.reinf20", 1);
		Suicide();
	end;
end;

function Reinf21()
	if (GetNUnitsInScriptGroup(21) == 0) and (GetIGlobalVar("temp.reinf21", 0) == 0) then
		RunScript("A21", 32000);
		RunScript("Reinf21", 2000);
		SetIGlobalVar("temp.reinf21", 1);
		Suicide();
	end;
end;

function Reinf30()
	if (GetNUnitsInScriptGroup(30) == 0) and (GetIGlobalVar("temp.reinf30", 0) == 0) then
		RunScript("A30", 32000);
		RunScript("Reinf30", 2000);
		SetIGlobalVar("temp.reinf30", 1);
		Suicide();
	end;
end;

function Reinf31()
	if (GetNUnitsInScriptGroup(31) == 0) and (GetIGlobalVar("temp.reinf31", 0) == 0) then
		RunScript("A31", 32000);
		RunScript("Reinf31", 2000);
		SetIGlobalVar("temp.reinf31", 1);
		Suicide();
	end;
end;

function Reinf40()
	if (GetNUnitsInScriptGroup(40) == 0) and (GetIGlobalVar("temp.reinf40", 0) == 0) then
		RunScript("A40", 32000);
		RunScript("Reinf40", 2000);
		SetIGlobalVar("temp.reinf40", 1);
		Suicide();
	end;
end;

function Reinf41()
	if (GetNUnitsInScriptGroup(41) == 0) and (GetIGlobalVar("temp.reinf41", 0) == 0) then
		RunScript("A41", 32000);
		RunScript("Reinf41", 2000);
		SetIGlobalVar("temp.reinf41", 1);
		Suicide();
	end;
end;

function Reinf50()
	if (GetNUnitsInScriptGroup(50) == 0) and (GetIGlobalVar("temp.reinf50", 0) == 0) then
		RunScript("A50", 32000);
		RunScript("Reinf50", 2000);
		SetIGlobalVar("temp.reinf50", 1);
		Suicide();
	end;
end;

function Reinf51()
	if (GetNUnitsInScriptGroup(51) == 0) and (GetIGlobalVar("temp.reinf51", 0) == 0) then
		RunScript("A51", 32000);
		RunScript("Reinf51", 2000);
		SetIGlobalVar("temp.reinf51", 1);
		Suicide();
	end;
end;

function Reinf60()
	if (GetNUnitsInScriptGroup(60) == 0) and (GetIGlobalVar("temp.reinf60", 0) == 0) then
		RunScript("A60", 32000);
		RunScript("Reinf60", 2000);
		SetIGlobalVar("temp.reinf60", 1);
		Suicide();
	end;
end;

function Reinf61()
	if (GetNUnitsInScriptGroup(61) == 0) and (GetIGlobalVar("temp.reinf61", 0) == 0) then
		RunScript("A61", 32000);
		RunScript("Reinf61", 2000);
		SetIGlobalVar("temp.reinf61", 1);
		Suicide();
	end;
end;

function A10()
	LandReinforcement(10);
	SetIGlobalVar("temp.reinf10",0);
	Suicide();
end;

function A11()
	LandReinforcement(11);
	SetIGlobalVar("temp.reinf11",0);
	Suicide();
end;

function A20()
	LandReinforcement(20);
	SetIGlobalVar("temp.reinf20",0);
	Suicide();
end;

function A21()
	LandReinforcement(21);
	SetIGlobalVar("temp.reinf21",0);
	Suicide();
end;

function A30()
	LandReinforcement(30);
	SetIGlobalVar("temp.reinf30",0);
	Suicide();
end;

function A31()
	LandReinforcement(31);
	SetIGlobalVar("temp.reinf31",0);
	Suicide();
end;

function A40()
	LandReinforcement(40);
	SetIGlobalVar("temp.reinf40",0);
	Suicide();
end;

function A41()
	LandReinforcement(41);
	SetIGlobalVar("temp.reinf41",0);
	Suicide();
end;

function A50()
	LandReinforcement(50);
	SetIGlobalVar("temp.reinf50", 0);
	Suicide();
end;

function A51()
	LandReinforcement(51);
	SetIGlobalVar("temp.reinf51", 0);
	Suicide();
end;

function A60()
	LandReinforcement(60);
	SetIGlobalVar("temp.reinf60", 0);
	Suicide();
end;

function A61()
	LandReinforcement(61);
	SetIGlobalVar("temp.reinf61", 0);
	Suicide();
end;

function Init()
--	RunScript( "EtherealBridge", 1000);
	RunScript("Begin", 500);
end;
