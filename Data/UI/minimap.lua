function LuaProcessMessage( nMessageCode, nFirst, nSecond )

	local nTemp = ProcessMessageWithLink( nMessageCode, nFirst );
	if ( nTemp ~= 0 ) then
		return 1
	end


	if ( nMessageCode == 536936448 and nFirst == 12000 ) then --ESCAPE MENU
		local nMessage = SetProcessedFlag( 2097248 )
		AddMessage( nMessage, 0, 0 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 12001 ) then --LAST OBJECTIVE
		local nMessage = SetProcessedFlag( 2097266 )
		AddMessage( nMessage, 1, 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 12003 ) then --CALL AVIATION
--		OutputValue( "Process aviation", nFirst )
		local bProcess = IsGameButtonProcessing();
		if ( bProcess ~= 0 ) then
			--send message to show aviation buttons
			local nNewMessageCode = SetProcessedFlag( 524 )
			AddMessage( nNewMessageCode, 0, 0 )
		end
		return 1
	end

	if ( nMessageCode == 215 ) then		--DISABLE AVIATION
--		OutputValue( "Disable aviation", nFirst )
		AddMessage( 65600, 12003, nFirst )	--SET ANIMATION TIME
		AddMessage( 65808, 12003, 0 )	-- disable color filler
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 216 ) then		--ENABLE AVIATION
--		OutputValue( "Enable aviation", 1 )
		AddMessage( 65568, 12003, 1 )	--ENABLE WINDOW
		AddMessage( 65792, 12003, 0 )	--enable color filler
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 12002 ) then --SET MARKER
		OutputValue( "Set marker notify clicked", nSecond )
		if ( nSecond == 1 ) then
			local nMessage = SetProcessedFlag( 59 )			--USER_ACTION_PLACE_MARKER
			AddMessage( nMessage, 0, 0 )
		else
			local nMessage = SetProcessedFlag( 256 + 59 )	--USER_ACTION_PLACE_MARKER clear
			AddMessage( nMessage, 0, 0 )
		end
		return 1
	end

	if ( nMessageCode == 256 + 59 ) then --SET MARKER clear state
		OutputValue( "Set marker external clear", 1 )
		AddMessage( 65536, 12002, 0 )	--CHANGE_STATE to 0 (clear state)
		return 1
	end

--	if ( nMessageCode == 536936448 and nFirst == 12003 ) then --SWITCH FORMATION
--		local nMessage = SetProcessedFlag( 2097266 )
--		AddMessage( nMessage, 1, 1 )
--		return 1
--	end

--	if ( nMessageCode == 536936448 and nFirst == 12002 ) then --PAUSE GAME
--		local nMessage = SetProcessedFlag( 1048592 )
--		AddMessage( nMessage, 1, 1 )		--PAUSE_GAME
--		return 1
--	end

--	if ( nMessageCode == 536936448 and nFirst == 12003 ) then --CALL STAFF
--		local nMessage = SetProcessedFlag( 110 )
--		AddMessage( nMessage, 1, 1 )
--		return 1
--	end

	return 0
end
