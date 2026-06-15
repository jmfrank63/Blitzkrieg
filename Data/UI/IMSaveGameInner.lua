function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936453 and nFirst == 1000 ) then --LIST DOUBLE CLICK
		local nMessage = SetProcessedFlag( 10002 )	--send it to the game as an CREATE button pressed
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 536936480 ) then		--editbox return
		local nMessage = SetProcessedFlag( 7777 );
		AddMessage( nMessage, 0, nSecond );
		return 1
	end

	if ( nMessageCode == 536936481) then			--editbox escape
		local nMessage = SetProcessedFlag( 7778 );
		AddMessage( nMessage, 0, nSecond );
		return 1
	end

	if ( nMessageCode == 536936451 and nFirst == 1000 ) then --SELECTION_CHANGED
		local nMessage = SetProcessedFlag( 1000 )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end
	
	return 0
end
