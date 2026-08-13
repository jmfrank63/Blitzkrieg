function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10001 ) then
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

	if ( nMessageCode == 536936448 and nFirst == 10010 ) then --NEW PROFILE
		local nMessage = SetProcessedFlag( 10010 )
		AddMessage( nMessage, 10010, 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10011 ) then --RENAME PROFILE
		local nMessage = SetProcessedFlag( 10011 )
		AddMessage( nMessage, 10011, 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10012 ) then --DELETE PROFILE
		local nMessage = SetProcessedFlag( 10012 )
		AddMessage( nMessage, 10012, 1 )
		return 1
	end

	if ( nMessageCode == 536936451 and nFirst == 2100 ) then --PROFILE LIST SELECTION_CHANGED
		local nMessage = SetProcessedFlag( 2100 )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 536936453 and nFirst == 2100 ) then --PROFILE LIST DOUBLE CLICK
		local nMessage = SetProcessedFlag( 10002 )	--send it to the game as an OK button
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 536936482 or nMessageCode == 536936480 or nMessageCode == 536936481 ) then --NOTIFY TEXTEDIT
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
