function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10002 or nMessageCode == 10001 ) then
		AddMessage( 65537, 10002, 1 )		--IMC_CANCEL
		return 1
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 10001 and nFirst <= 10005 ) ) then -- service buttons (OK, Cancel)
		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 0, 1 )
		return 1
	end

	if ( nMessageCode == 536936451 or nMessageCode == 536936452 ) then
		--forward these messages to the game
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
