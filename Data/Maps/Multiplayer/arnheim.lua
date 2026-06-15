function FlagReinforcement(nParty)
local num;
	if (nParty == 0) then
		if (GetNUnitsInPlayerUF(0) < 30) then
			num = 100;
			LandReinforcement(num);
		end;
	end;
	if (nParty == 1) then
		if (GetNUnitsInPlayerUF(1) < 30) then
			num = RandomInt(3) + 200;
			LandReinforcement(num);
		end;
	end;
end;

function Begin()
	if (IsPlayerPresent(0) == 1) then
		RunScript("A10", 500);
		RunScript("A11", 500);
		RunScript("Reinf10", 3000);
		RunScript("Reinf11", 3000);
	end;
	if (IsPlayerPresent(1) == 1) then
		RunScript("A20", 500);
		RunScript("A21", 500);
		RunScript("Reinf20", 3000);
		RunScript("Reinf21", 3000);
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


function A10()
	LandReinforcement(10);
	SetIGlobalVar("temp.reinf10", 0);
	Suicide();
end;

function A11()
	LandReinforcement(11);
	SetIGlobalVar("temp.reinf11", 0);
	Suicide();
end;

function A20()
	LandReinforcement(20);
	SetIGlobalVar("temp.reinf20", 0);
	Suicide();
end;

function A21()
	LandReinforcement(21);
	SetIGlobalVar("temp.reinf21", 0);
	Suicide();
end;

function Init()
--	RunScript( "EtherealBridge", 1000);
	RunScript("Begin", 500);
end;
