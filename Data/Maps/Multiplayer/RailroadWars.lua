---get train
function Bonus()
	if (GetNUnitsInArea(0, "House00") >= 1) then
		ChangePlayer(1000,0);
		Suicide();
	end;
	if (GetNUnitsInArea(1, "House01") >= 1) then
		ChangePlayer(1000,1);
		Suicide();
	end;
	if (GetNUnitsInArea(2, "House10") >= 1) then
		ChangePlayer(1000,2);
		Suicide();
	end;
	if (GetNUnitsInArea(3, "House11") >= 1) then
		ChangePlayer(1000, 3);
		Suicide();
	end;
end;

function Init()
	RunScript("Bonus", 1000);
end;
