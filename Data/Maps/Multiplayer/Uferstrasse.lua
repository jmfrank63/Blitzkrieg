function FlagReinforcement( nParty )
	if ( (nParty == 0) and (GetNUnitsInPartyUF(0) < 24)) then
	local num = RandomInt(3) + 100;
		LandReinforcement(num);
	end;
	if ( (nParty == 1) and (GetNUnitsInPartyUF(1) < 24)) then
	local num = RandomInt(3) + 200;
		LandReinforcement(num);
	end;
end;

function Begin()
	if (IsPlayerPresent(0) == 1) then
		RunScript("A10", 500);
		RunScript("A11", 500);
		RunScript("A12", 500);
		RunScript("Reinf10", 3000);
		RunScript("Reinf11", 3000);
		RunScript("Reinf12", 3000);
	end;
	if (IsPlayerPresent(1) == 1) then
		RunScript("A20", 500);
		RunScript("A21", 500);
		RunScript("A22", 500);
		RunScript("Reinf20", 3000);
		RunScript("Reinf21", 3000);
		RunScript("Reinf22", 3000);
	end;
	if (IsPlayerPresent(2) == 1) then
		RunScript("A30", 500);
		RunScript("A31", 500);
		RunScript("A32", 500);
		RunScript("Reinf30", 3000);
		RunScript("Reinf31", 3000);
		RunScript("Reinf32", 3000);
	end;
	if (IsPlayerPresent(3) == 1) then
		RunScript("A40", 500);
		RunScript("A41", 500);
		RunScript("A42", 500);
		RunScript("Reinf40", 3000);
		RunScript("Reinf41", 3000);
		RunScript("Reinf42", 3000);
	end;
	Suicide();
end;

function Reinf10()
	if (GetNUnitsInScriptGroup(10) == 0) and (GetIGlobalVar("temp.reinf10", 0) == 0) then
		RunScript("A10", 32000);
		SetIGlobalVar("temp.reinf10", 1);
	end;
end;

function Reinf11()
	if (GetNUnitsInScriptGroup(11) == 0) and (GetIGlobalVar("temp.reinf11", 0) == 0) then
		RunScript("A11", 32000);
		SetIGlobalVar("temp.reinf11", 1);
	end;
end;

function Reinf12()
	if (GetNUnitsInScriptGroup(12) == 0) and (GetIGlobalVar("temp.reinf12", 0) == 0) then
		RunScript("A12", 32000);
		SetIGlobalVar("temp.reinf12", 1);
	end;
end;

function Reinf20()
	if (GetNUnitsInScriptGroup(20) == 0) and (GetIGlobalVar("temp.reinf20", 0) == 0) then
		RunScript("A20", 32000);
		SetIGlobalVar("temp.reinf20", 1);
	end;
end;

function Reinf21()
	if (GetNUnitsInScriptGroup(21) == 0) and (GetIGlobalVar("temp.reinf21", 0) == 0) then
		RunScript("A21", 32000);
		SetIGlobalVar("temp.reinf21", 1);
	end;
end;

function Reinf22()
	if (GetNUnitsInScriptGroup(22) == 0) and (GetIGlobalVar("temp.reinf22", 0) == 0) then
		RunScript("A22", 32000);
		SetIGlobalVar("temp.reinf22", 1);
	end;
end;

function Reinf30()
	if (GetNUnitsInScriptGroup(30) == 0) and (GetIGlobalVar("temp.reinf30", 0) == 0) then
		RunScript("A30", 32000);
		SetIGlobalVar("temp.reinf30", 1);
	end;
end;

function Reinf31()
	if (GetNUnitsInScriptGroup(31) == 0) and (GetIGlobalVar("temp.reinf31", 0) == 0) then
		RunScript("A31", 32000);
		SetIGlobalVar("temp.reinf31", 1);
	end;
end;

function Reinf32()
	if (GetNUnitsInScriptGroup(32) == 0) and (GetIGlobalVar("temp.reinf32", 0) == 0) then
		RunScript("A32", 32000);
		SetIGlobalVar("temp.reinf32", 1);
	end;
end;

function Reinf40()
	if (GetNUnitsInScriptGroup(40) == 0) and (GetIGlobalVar("temp.reinf40", 0) == 0) then
		RunScript("A40", 32000);
		SetIGlobalVar("temp.reinf40", 1);
	end;
end;

function Reinf41()
	if (GetNUnitsInScriptGroup(41) == 0) and (GetIGlobalVar("temp.reinf41", 0) == 0) then
		RunScript("A41", 32000);
		SetIGlobalVar("temp.reinf41", 1);
	end;
end;

function Reinf42()
	if (GetNUnitsInScriptGroup(42) == 0) and (GetIGlobalVar("temp.reinf42", 0) == 0) then
		RunScript("A42", 32000);
		SetIGlobalVar("temp.reinf42", 1);
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

function A12()
	LandReinforcement(12);
	SetIGlobalVar("temp.reinf12", 0);
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

function A22()
	LandReinforcement(22);
	SetIGlobalVar("temp.reinf22", 0);
	Suicide();
end;

function A30()
	LandReinforcement(30);
	SetIGlobalVar("temp.reinf30", 0);
	Suicide();
end;

function A31()
	LandReinforcement(31);
	SetIGlobalVar("temp.reinf31", 0);
	Suicide();
end;

function A32()
	LandReinforcement(32);
	SetIGlobalVar("temp.reinf32", 0);
	Suicide();
end;

function A40()
	LandReinforcement(40);
	SetIGlobalVar("temp.reinf40", 0);
	Suicide();
end;

function A41()
	LandReinforcement(41);
	SetIGlobalVar("temp.reinf41", 0);
	Suicide();
end;

function A42()
	LandReinforcement(42);
	SetIGlobalVar("temp.reinf42", 0);
	Suicide();
end;

function EtherealBridge()
local num, totalnum = 901, 903;
local MaxHP = 1500;
	while (num <= totalnum)
	do
		if ( GetNUnitsInScriptGroup(num) > 0) then
		HP = GetObjectHPs(num);
			if ( HP < MaxHP) then
				DamageObject( num, HP - MaxHP);
			end;
		else DamageObject( num, -MaxHP);
		end;
		num = num + 1;
	end;
end;

function Init()
	RunScript( "EtherealBridge", 1000);
	RunScript( "Begin", 500);
end;
