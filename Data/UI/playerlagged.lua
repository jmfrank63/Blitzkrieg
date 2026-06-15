function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936450  and nFirst >= 10010 and nFirst < 10027 ) then --window clicked
		nMessage = SetProcessedFlag( 2098186 ) -- MC_MP_DROP_LAGGED_PLAYER
		AddMessage( nMessage, nFirst - 10010, 0 )
		return 1
	end

	return 0
end
