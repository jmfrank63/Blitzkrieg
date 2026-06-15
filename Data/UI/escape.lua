function LuaProcessMessage( nMessageCode, nFirst, nSecond )
  if ( nMessageCode == 536936448 and nFirst == 10000 ) then	--STATE_CHANGED
		local nNewMessageCode = SetProcessedFlag( 2097265 )
		AddMessage( nNewMessageCode, 0, 0 )		--HIDE_ESCAPE_MENU_WITHOUT_UNPAUSE

		nMessageCode = SetProcessedFlag( 1048593 )
		AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME

		local nNewMessageCode = SetProcessedFlag( 2097249 )
    AddMessage( nNewMessageCode, 0, 0 )		--SHOW_SAVE_MENU

		return 1
  end


  if ( nMessageCode == 536936448 and nFirst == 10001 ) then	--STATE_CHANGED
		local nNewMessageCode = SetProcessedFlag( 2097265 )
		AddMessage( nNewMessageCode, 0, 0 )		--HIDE_ESCAPE_MENU_WITHOUT_UNPAUSE

		nMessageCode = SetProcessedFlag( 1048593 )
		AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME

		local nNewMessageCode = SetProcessedFlag( 2097250 )
    AddMessage( nNewMessageCode, 0, 0 )		--SHOW_LOAD_MENU

		return 1
	end

  if ( nMessageCode == 536936448 and nFirst == 10002 ) then	--STATE_CHANGED
		local nNewMessageCode = SetProcessedFlag( 2097265 )
		AddMessage( nNewMessageCode, 0, 0 )		--HIDE_ESCAPE_MENU_WITHOUT_UNPAUSE

		nMessageCode = SetProcessedFlag( 1048593 )
		AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME

		local nNewMessageCode = SetProcessedFlag( 2098194 )
		  AddMessage( nNewMessageCode, 0, 0 )		--SHOW_options_screen

		return 1
	end

  if ( nMessageCode == 536936448 and nFirst == 10004 ) then	--STATE_CHANGED
		local nNewMessageCode = SetProcessedFlag( 2097265 )
		AddMessage( nNewMessageCode, 0, 0 )		--HIDE_ESCAPE_MENU_WITHOUT_UNPAUSE

		local nNewMessageCode = SetProcessedFlag( 2097252 )
    AddMessage( nNewMessageCode, 0, 0 )		--SHOW_QUIT_MENU

		return 1
  end

  if ( nMessageCode == 536936448 and nFirst == 10005 ) then	--STATE_CHANGED
		local nNewMessageCode = SetProcessedFlag( 2097253 )
    AddMessage( nNewMessageCode, 0, 0 )		--HIDE_ESCAPE_MENU
		return 1
  end

	if ( nMessageCode == 536936448 and nFirst == 10003 ) then	--STATE_CHANGED
		local nNewMessageCode = SetProcessedFlag( 2097257 )
		AddMessage( nNewMessageCode, 0, 0 )		--SHOW_OBJECTIVES
		return 1
	end

  return 0
end
