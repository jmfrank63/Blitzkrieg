function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10001 ) then		--IMC_CANCEL
		OutputValue( "SelectCampaign CANCEL message", nMessageCode )
		AddMessage( 65537, 10001, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 10001 and nFirst <= 10004 ) ) then	--IMC_CANCEL
		--10001 - CANCEL
		--10002, 10003, 10004 - GERMAN or RUSSIAN or ALLIES campaigns
		OutputValue( "CANCEL PRESSED", nFirst )
		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 0, 1 )
		return 1
	end

	return 0
end
