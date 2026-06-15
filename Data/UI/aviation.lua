function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 and nFirst == 12001 ) then --CALL AVIATION
		--send message to show aviation buttons
		local nNewMessageCode = SetProcessedFlag( 524 )
		AddMessage( nNewMessageCode, 0, 0 )
		return 1
	end

	if ( nMessageCode == 215 ) then		--DISABLE AVIATION
		AddMessage( 65600, 12001, nFirst )	--SET ANIMATION TIME
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 216 ) then		--ENABLE AVIATION
		AddMessage( 65568, 12001, 1 )	--ENABLE WINDOW
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
