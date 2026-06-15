nMission = 0

function ClearState( nControl )
	i = 1000
	while ( i <= 1000 ) do
		if ( i ~= nControl ) then
			AddMessage( 65536, i, 0 )		--CHANGE_STATE to 0 (clear state)
		end
		i = i + 1
	end
end

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 and nFirst == 1000 ) then
		if ( nSecond == 1 ) then
			ClearState( 1000 )
			nMission = 0
			return 1
		end
		if ( nMission == 0 and nSecond == 0 ) then
			AddMessage( 65536, 1000, 1 )		--SET_STATE
			nMission = -1
			return 1
		end
		return 0
	end

	return 0
end
