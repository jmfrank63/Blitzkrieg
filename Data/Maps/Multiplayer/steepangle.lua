function FlagReinforcement( nParty )
local num;
	if ( (nParty == 0) and (GetNUnitsInPartyUF(0) < 20)) then
		if ( IsPlayerPresent( 0 ) == 1 ) then
			num = RandomInt(7) + 100;
			LandReinforcement(num);
		end;
		if ( IsPlayerPresent( 1 ) == 1 ) then
			num = RandomInt(7) + 200;
			LandReinforcement(num);
		end;
	end;

	if ( (nParty == 1) and (GetNUnitsInPartyUF(1) < 20)) then
		if ( IsPlayerPresent( 2 ) == 1 ) then
			num = RandomInt(7) + 300;
			LandReinforcement(num);
		end;
		if ( IsPlayerPresent( 3 ) == 1 ) then
			num = RandomInt(7) + 400;
			LandReinforcement(num);
		end;
	end;
end;

function Begin()
	if ( IsPlayerPresent( 0 ) == 1 ) then
		RunScript("A10", 500);
		RunScript("A11", 500);
		RunScript("Reinf10", 3000);
		RunScript("Reinf11", 3000);
	end;
	if ( IsPlayerPresent( 1 ) == 1 ) then
		RunScript("A20", 500);
		RunScript("A21", 500);
		RunScript("Reinf20", 3000);
		RunScript("Reinf21", 3000);
	end;
	if ( IsPlayerPresent( 2 ) == 1 ) then
		RunScript("A30", 500);
		RunScript("A31", 500);
		RunScript("Reinf30", 3000);
		RunScript("Reinf31", 3000);
	end;
	if ( IsPlayerPresent( 3 ) == 1 ) then
		RunScript("A40", 500);
		RunScript("A41", 500);
		RunScript("Reinf40", 3000);
		RunScript("Reinf41", 3000);
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

function Init()
	RunScript("Begin", 500);
end;
