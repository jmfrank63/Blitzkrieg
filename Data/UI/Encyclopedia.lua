function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10002 ) then
		AddMessage( 65537, 10001, 1 )		--IMC_CANCEL
		return 1
	end

	if ( nMessageCode >= 10001 and nMessageCode <= 10006 ) then
		AddMessage( 65537, nMessageCode, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 10001 and nFirst <= 10006 ) ) then --OK
		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 0, 1 )
		return 1
	end

	return 0
end
