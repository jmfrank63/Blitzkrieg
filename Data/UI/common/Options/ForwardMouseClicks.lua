function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936450 ) then --window clicked
		local nMessage = SetProcessedFlag( 7777 );
		AddMessage( nMessage, 7777, nSecond );
		return 1
	end

	
	if ( nMessageCode == 536936484 ) then --window Right clicked
		local nMessage = SetProcessedFlag( 7779 );
		AddMessage( nMessage, 7779, nSecond );
		return 1
	end

	if ( nMessageCode == 536936449 ) then			--slider position changed
		local nMessage = SetProcessedFlag( 7778 );
		AddMessage( nMessage, 7778, nSecond );
		return 1
	end
		

	return 0
end
