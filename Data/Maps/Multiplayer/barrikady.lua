function VictoryCond()
	if ( GetNUnitsInScriptGroup(2712) <= 0) then
		Win(0);
		Suicide();
	end;
	if ( GetNUnitsInScriptGroup(2711) <= 0) then
		Win(1);
		Suicide();
	end;

end;

function Init()
--	RunScript( "VictoryCond", 5000);
end;
