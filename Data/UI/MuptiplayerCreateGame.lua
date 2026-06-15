function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10001 or ( nMessageCode >= 10003 and nMessageCode <= 10007 )  ) then
		AddMessage( 65537, nMessageCode, 1 )		--IMC_OK
		return 1
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 10001 and nFirst <= 10007 ) ) then --OK
		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 0, 1 )
		return 1
	end

	if ( nMessageCode == 536936451 and nFirst == 1000 ) then --selection changed
		local nMessage = SetProcessedFlag( 536936451 );
		AddMessage( nMessage, nFirst, nSecond );
		return 1
	end

	if ( nMessageCode == 536936480 ) then		--EDIT BOX return
		local nMessage = SetProcessedFlag( 536936480 );
		AddMessage( nMessage, nFirst, nSecond );
		return 1
	end

	if ( nMessageCode == 536936481 ) then		--EDIT BOX escape
		local nMessage = SetProcessedFlag( 536936481 );
		AddMessage( nMessage, nFirst, nSecond );
		return 1
	end

	if ( nMessageCode == 536936453 and nFirst == 1000 ) then --LIST DOUBLE CLICK
		local nMessage = SetProcessedFlag( 10004 )	--send it to the game as an CREATE button pressed
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
