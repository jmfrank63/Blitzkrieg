function LuaProcessMessage( nMessageCode, nFirst, nSecond )
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

	return 0
end
