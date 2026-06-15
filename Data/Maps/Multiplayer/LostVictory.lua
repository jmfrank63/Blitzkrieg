function FlagReinforcement (nParty)
	if ( (nParty == 0) and (GetNUnitsInPartyUF(0) < 18)) then
		RunScript("Flag0", 100);
	end;
	if ( (nParty == 1) and (GetNUnitsInPartyUF(1) < 18)) then
		RunScript("Flag1", 100);
	end;
end;
-----------------------------------------------------------------------
function Flag0()
	while (5 == 5)
	do
	if (GetIGlobalVar("temp.010", 0) == 0) then
	local num1 = RandomInt(2) + 101;
			SetIGlobalVar("temp.010", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.011", 0) == 0) then
	local num1 = RandomInt(2) + 101;
			SetIGlobalVar("temp.011", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.012", 0) == 0) then
	local num1 = RandomInt(2) + 101;
			SetIGlobalVar("temp.012", 1);
			LandReinforcement(num1);
			break;
	end;
----
	if (GetIGlobalVar("temp.020", 0) == 0) then
	local num1 = RandomInt(2) + 103;
			SetIGlobalVar("temp.020", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.021", 0) == 0) then
	local num1 = RandomInt(2) + 103;
			SetIGlobalVar("temp.021", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.022", 0) == 0) then
	local num1 = RandomInt(2) + 103;
			SetIGlobalVar("temp.022", 1);
			LandReinforcement(num1);
			break;
	end;
----
	if (GetIGlobalVar("temp.030", 0) == 0) then
	local num1 = RandomInt(2) + 105;
			SetIGlobalVar("temp.030", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.031", 0) == 0) then
	local num1 = RandomInt(2) + 105;
			SetIGlobalVar("temp.031", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.032", 0) == 0) then
	local num1 = RandomInt(2) + 105;
			SetIGlobalVar("temp.032", 1);
			LandReinforcement(num1);
			break;
	end;
----
	if (GetIGlobalVar("temp.040", 0) == 0) then
	local num1 = RandomInt(2) + 107;
			SetIGlobalVar("temp.040", 1);
			SetIGlobalVar("temp.050", 0);
			SetIGlobalVar("temp.051", 0);
			SetIGlobalVar("temp.052", 0);
			LandReinforcement(num1);
			Suicide();
	end;
	if (GetIGlobalVar("temp.041", 0) == 0) then
	local num1 = RandomInt(2) + 107;
			SetIGlobalVar("temp.041", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.042", 0) == 0) then
	local num1 = RandomInt(2) + 107;
			SetIGlobalVar("temp.042", 1);
			LandReinforcement(num1);
			break;
	end;

----
	if (GetIGlobalVar("temp.050", 0) == 0) then
	local num1 = RandomInt(2) + 109;
			SetIGlobalVar("temp.050", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.051", 0) == 0) then
	local num1 = RandomInt(2) + 109;
			SetIGlobalVar("temp.051", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.052", 0) == 0) then
	local num1 = RandomInt(2) + 109;
			SetIGlobalVar("temp.052", 1);
			SetIGlobalVar("temp.040", 0);
			SetIGlobalVar("temp.041", 0);
			SetIGlobalVar("temp.042", 0);
			LandReinforcement(num1);
			break;
	end;
	end;
	Suicide();
end;
----------------------------------------------------------------------
function Flag1()
	while (5 == 5)
	do
	if (GetIGlobalVar("temp.110", 0) == 0) then
	local num1 = RandomInt(2) + 201;
			SetIGlobalVar("temp.110", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.111", 0) == 0) then
	local num1 = RandomInt(2) + 201;
			SetIGlobalVar("temp.111", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.112", 0) == 0) then
	local num1 = RandomInt(2) + 201;
			SetIGlobalVar("temp.112", 1);
			LandReinforcement(num1);
			break;
	end;
----
	if (GetIGlobalVar("temp.120", 0) == 0) then
	local num1 = RandomInt(2) + 203;
			SetIGlobalVar("temp.120", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.121", 0) == 0) then
	local num1 = RandomInt(2) + 203;
			SetIGlobalVar("temp.121", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.122", 0) == 0) then
	local num1 = RandomInt(2) + 203;
			SetIGlobalVar("temp.122", 1);
			LandReinforcement(num1);
			break;
	end;
----
	if (GetIGlobalVar("temp.130", 0) == 0) then
	local num1 = RandomInt(2) + 205;
			SetIGlobalVar("temp.130", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.131", 0) == 0) then
	local num1 = RandomInt(2) + 205;
			SetIGlobalVar("temp.131", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.132", 0) == 0) then
	local num1 = RandomInt(2) + 205;
			SetIGlobalVar("temp.132", 1);
			LandReinforcement(num1);
			break;
	end;
----
	if (GetIGlobalVar("temp.140", 0) == 0) then
	local num1 = RandomInt(2) + 207;
			SetIGlobalVar("temp.140", 1);
			SetIGlobalVar("temp.150", 0);
			SetIGlobalVar("temp.151", 0);
			SetIGlobalVar("temp.152", 0);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.141", 0) == 0) then
	local num1 = RandomInt(2) + 207;
			SetIGlobalVar("temp.141", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.142", 0) == 0) then
	local num1 = RandomInt(2) + 207;
			SetIGlobalVar("temp.142", 1);
			LandReinforcement(num1);
			break;
	end;

----
	if (GetIGlobalVar("temp.150", 0) == 0) then
	local num1 = RandomInt(2) + 209;
			SetIGlobalVar("temp.150", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.151", 0) == 0) then
	local num1 = RandomInt(2) + 209;
			SetIGlobalVar("temp.151", 1);
			LandReinforcement(num1);
			break;
	end;
	if (GetIGlobalVar("temp.152", 0) == 0) then
	local num1 = RandomInt(2) + 209;
			SetIGlobalVar("temp.152", 1);
			SetIGlobalVar("temp.140", 0);
			SetIGlobalVar("temp.141", 0);
			SetIGlobalVar("temp.142", 0);
			LandReinforcement(num1);
			break;
	end;
	end;
	Suicide();
end;
--------------------------------------------------------------------------------------

function SetIGlobalVars()
	SetIGlobalVar("temp.010", 0);
	SetIGlobalVar("temp.011", 0);
	SetIGlobalVar("temp.012", 0);
	SetIGlobalVar("temp.020", 0);
	SetIGlobalVar("temp.021", 0);
	SetIGlobalVar("temp.022", 0);
	SetIGlobalVar("temp.030", 0);
	SetIGlobalVar("temp.031", 0);
	SetIGlobalVar("temp.032", 0);
	SetIGlobalVar("temp.040", 0);
	SetIGlobalVar("temp.041", 0);
	SetIGlobalVar("temp.042", 0);
	SetIGlobalVar("temp.050", 0);
	SetIGlobalVar("temp.051", 0);
	SetIGlobalVar("temp.052", 0);
	SetIGlobalVar("temp.110", 0);
	SetIGlobalVar("temp.111", 0);
	SetIGlobalVar("temp.112", 0);
	SetIGlobalVar("temp.120", 0);
	SetIGlobalVar("temp.121", 0);
	SetIGlobalVar("temp.122", 0);
	SetIGlobalVar("temp.130", 0);
	SetIGlobalVar("temp.131", 0);
	SetIGlobalVar("temp.132", 0);
	SetIGlobalVar("temp.140", 0);
	SetIGlobalVar("temp.141", 0);
	SetIGlobalVar("temp.142", 0);
	SetIGlobalVar("temp.150", 0);
	SetIGlobalVar("temp.151", 0);
	SetIGlobalVar("temp.152", 0);
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


function Begin()
	if (IsPlayerPresent(0) == 1) then
		RunScript("A10", 500);
		RunScript("A11", 500);
		RunScript("Reinf10", 5000);
		RunScript("Reinf11", 5000);
	end;
	if (IsPlayerPresent(1) == 1) then
		RunScript("A20", 500);
		RunScript("A21", 500);
		RunScript("Reinf20", 5000);
		RunScript("Reinf21", 5000);
	end;
	Suicide();
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

function EtherealBridge()
local num, totalnum = 901, 902;
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
	RunScript("Begin", 1000);
	RunScript("SetIGlobalVars", 500);
--	RunScript( "EtherealBridge", 1000);
end;
