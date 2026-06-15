function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10001 ) then
		AddMessage( 65537, 10001, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 10002 ) then
		AddMessage( 65537, 10001, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 10001 ) then --CANCEL
		local nMessage = SetProcessedFlag( 10001 )
		AddMessage( nMessage, 10001, 1 )
		return 1
	end

	return 0
end
