function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 and ( nFirst == 10002 or nFirst == 2001 )) then --OK
		nMessage = SetProcessedFlag( 1048587 ) -- CMD_GAME_UNTIMEOUT
		AddMessage( nMessage, 1, 1 )
		return 1
	end

	return 0
end
