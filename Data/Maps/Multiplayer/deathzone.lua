function FlagReinforcement(nParty)
	if ( (nParty == 0) and (IsPlayerPresent(0) == 1) and (GetNUnitsInPartyUF(0) <= 18)) then
	local num = RandomInt(4) + 100;
		LandReinforcement(num);
		if (GetIGlobalVar("temp.guns0", 0) == 0) then
			SetIGlobalVar("temp.guns0", 1);
			RunScript("GiveGuns0", 500);
			RunScript("ReinfGuns0", 3000);
		end;
	end;
	if ( (nParty == 1) and (IsPlayerPresent(1) == 1) and (GetNUnitsInPartyUF(1) <= 18)) then
	local num = RandomInt(4) + 200;
		LandReinforcement(num);
		if (GetIGlobalVar("temp.guns1", 0) == 0) then
			SetIGlobalVar("temp.guns1", 1);
			RunScript("GiveGuns1", 500);
			RunScript("ReinfGuns1", 3000);
		end;
	end;
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
	Suicide();
end;

function ReinfGuns0()
	if (GetNUnitsInScriptGroup(104,0) == 0) and (GetIGlobalVar("temp.reinfguns0",0) == 0) then
		RunScript("GiveGuns0", 120000);
		RunScript("ReinfGuns0",2000);
		SetIGlobalVar("temp.reinfguns0",1);
		Suicide();
	end;
end;

function ReinfGuns1()
	if (GetNUnitsInScriptGroup(204,1) == 0) and (GetIGlobalVar("temp.reinfguns1",0) == 0) then
		RunScript("GiveGuns1", 120000);
		RunScript("ReinfGuns1",2000);
		SetIGlobalVar("temp.reinfguns1",1);
		Suicide();
	end;
end;

function GiveGuns0()
	LandReinforcement(104);
	SetIGlobalVar("temp.reinfguns0",0);
	Suicide();
end;

function GiveGuns1()
	LandReinforcement(204);
	SetIGlobalVar("temp.reinfguns1",0);
	Suicide();
end;

function Reinf10()
	if (GetNUnitsInScriptGroup(10) == 0) and (GetIGlobalVar("temp.reinf10",0) == 0) then
		RunScript("A10", 32000);
		RunScript("Reinf10",2000);
		SetIGlobalVar("temp.reinf10",1);
		Suicide();
	end;
end;

function Reinf11()
	if (GetNUnitsInScriptGroup(11) == 0) and (GetIGlobalVar("temp.reinf11",0) == 0) then
		RunScript("A11", 32000);
		RunScript("Reinf11",2000);
		SetIGlobalVar("temp.reinf11",1);
		Suicide();
	end;
end;

function Reinf20()
	if (GetNUnitsInScriptGroup(20) == 0) and (GetIGlobalVar("temp.reinf20",0) == 0) then
		RunScript("A20", 32000);
		RunScript("Reinf20",2000);
		SetIGlobalVar("temp.reinf20",1);
		Suicide();
	end;
end;

function Reinf21()
	if (GetNUnitsInScriptGroup(21) == 0) and (GetIGlobalVar("temp.reinf21",0) == 0) then
		RunScript("A21", 32000);
		RunScript("Reinf21",2000);
		SetIGlobalVar("temp.reinf21",1);
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


function Init()
	RunScript("Begin", 500);
end;
