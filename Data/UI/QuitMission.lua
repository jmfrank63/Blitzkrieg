function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10004 ) then
		AddMessage( 65537, 10004, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10001 ) then --RESTART_MISSION
		local nMessage = SetProcessedFlag( 10001 )
		AddMessage( nMessage, 10001, 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10002 ) then --QUIT_MISSION
		local nMessage = SetProcessedFlag( 10002 )
		AddMessage( nMessage, 10002, 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10003 ) then --EXIT_PROGRAM
		local nMessage = SetProcessedFlag( 10003 )
		AddMessage( nMessage, 10003, 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10004 ) then --PREVIOUS
		local nMessage = SetProcessedFlag( 10004 )
		AddMessage( nMessage, 10004, 1 )
		return 1
	end

	return 0
end
