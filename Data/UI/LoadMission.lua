function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10001 ) then
		OutputValue( "FuckOff", 1 )
		AddMessage( 65537, 10001, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 10002 ) then
		AddMessage( 65537, 10002, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10002 ) then --OK
		local nMessage = SetProcessedFlag( 10002 )
		AddMessage( nMessage, 10002, 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10001 ) then --CANCEL
		local nMessage = SetProcessedFlag( 10001 )
		AddMessage( nMessage, 10001, 1 )
		return 1
	end

	if ( nMessageCode == 536936451 and nFirst == 1000 ) then --SELECTION_CHANGED
		local nMessage = SetProcessedFlag( 1000 )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 536936453 and nFirst == 1000 ) then --LIST DOUBLE CLICK
		local nMessage = SetProcessedFlag( 10002 )	--send it to the game as an OK button
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
