function LuaProcessMessage( nMessageCode, nFirst, nSecond )

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

	if ( nMessageCode == 536936482 ) then
		local nMessage = SetProcessedFlag( 7779 );
		AddMessage( nMessage, 0, nSecond );
		return 1
	end
		

	return 0
end
