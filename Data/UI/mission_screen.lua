bShowEscape = 0 
bShowHelp = 0
bShowObjectives = 0
bShowSingleObjective = 0
--bShowSB = 0
bPause = 0
bShowWin = 0
bShowLose = 0

function LuaLoad( ... )
	OutputValue( "agrumentov", arg.n );

	if ( arg.n < 2 ) then
		return
	end
	bShowEscape = arg[2]

	if ( arg.n < 4 ) then
		return
	end
	bShowHelp = arg[4]

	if ( arg.n < 6 ) then
		return
	end
	bShowObjectives = arg[6]

	if ( arg.n < 8 ) then
		return
	end
--	bShowSB = arg[8]

	if ( arg.n < 10 ) then
		return
	end
	bPause = arg[10]

	if ( arg.n < 12 ) then
		return
	end
	bShowSingleObjective = arg[12]

	if ( arg.n < 14 ) then
		return
	end
	bShowWin = arg[14]

	if ( arg.n < 16 ) then
		return
	end
	bShowLose = arg[16]
end

function LuaSave()
	SaveLuaValue( 1, bShowEscape )
	SaveLuaValue( 2, bShowHelp )
	SaveLuaValue( 3, bShowObjectives )
--	SaveLuaValue( 4, bShowSB )
	SaveLuaValue( 5, bPause )
	SaveLuaValue( 6, bShowSingleObjective )
	SaveLuaValue( 7, bShowWin )
	SaveLuaValue( 8, bShowLose )
end

function LuaProcessMessage( nMessageCode, nFirst, nSecond )


	local nTemp = ProcessMessageWithLink( nMessageCode, nFirst );
	if ( nTemp ~= 0 ) then
		return 1
	end

	if ( nMessageCode == 2097233 ) then		--ENTER
--		if ( bSendAllies == 1 ) then
--			bSendAllies = 0
--			return 1
--		end

		if ( bShowEscape == 1 ) then
			return 1
		end

--		OutputValue( "all msg", 1 )
		local nMessageCode = SetProcessedFlag( 2097233 )
		AddMessage( nMessageCode, 0, 0 )		--ENTER CHAT MODE
		return 1
	end

	if ( nMessageCode == 2097234 ) then		--CTRL + ENTER
--		OutputValue( "allies msg", 1 )
		local nMessageCode = SetProcessedFlag( 2097234 )
		AddMessage( nMessageCode, 0, 0 )		--ENTER ALLIED CHAT MODE
		return 1
	end

	if ( nMessageCode == 2097254 and bShowEscape == 0 ) then	--F1, SHOW HELP WINDOW
		local bShow = 0

		if ( bShow == 0 ) then
			local nMessageCode = SetProcessedFlag( 1048593 )
			AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME
			bPause = 1
		end

		bShowHelp = 1
		AddMessage( 65552, 3000, bShowHelp )	--SHOW_WINDOW HELP DIALOG
		AddMessage( 65584, 3000, 1 )		--SET MODAL FLAG

		return 1
	end

	if ( nMessageCode == 2097255 ) then	--HIDE HELP WINDOW
		bShowHelp = 0
		AddMessage( 65552, 3000, bShowHelp )	--SHOW_WINDOW HELP
		AddMessage( 65584, 3000, 0 )		--REMOVE MODAL FLAG

		local nMessageCode = SetProcessedFlag( 1048594 )
		AddMessage( nMessageCode, 0, 0 )		--UNPAUSE_GAME
		bPause = 0

		return 1
	end

	if ( nMessageCode == 2097248 and bShowHelp == 1 ) then	--ESC
		bShowHelp = 0
		AddMessage( 65552, 3000, bShowHelp )		--SHOW_WINDOW HELP
		AddMessage( 65584, 3000, 0 )		--REMOVE MODAL FLAG

		local nMessageCode = SetProcessedFlag( 1048594 )
		AddMessage( nMessageCode, 0, 0 )		--UNPAUSE_GAME
		bPause = 0

		return 1
	end

	if ( nMessageCode == 2097265 ) then				--HIDE ESC WITHOUT UNPAUSE
		bShowEscape = 0
		AddMessage( 65552, 2000, bShowEscape )	--SHOW_WINDOW
		AddMessage( 65584, 2000, bShowEscape )	--CHANGE MODAL FLAG
		bPause = bShowEscape

		return 1
	end

	if ( nMessageCode == 2097257 ) then				--SHOW_OBJECTIVES
		local bShow = 0
		if ( bShowHelp ~= 0 ) then
			bShow = 1
			bShowHelp = 0
			AddMessage( 65552, 3000, bShowHelp )	--SHOW_WINDOW HELP
			AddMessage( 65584, 3000, 0 )		--REMOVE MODAL FLAG
		end

		if ( bShow == 0 ) then
			local nMessageCode = SetProcessedFlag( 1048593 )
			AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME
			bPause = 1
		end

		bShowEscape = 0
		AddMessage( 65552, 2000, bShowEscape )			--SHOW_WINDOW (HIDE)

	end

	if ( nMessageCode == 2097266 ) then				--SHOW_SINGLE_OBJECTIVE
		local bShow = 0
		if ( bShowHelp ~= 0 ) then
			bShow = 1
			bShowHelp = 0
			AddMessage( 65552, 3000, bShowHelp )	--SHOW_WINDOW HELP
			AddMessage( 65584, 3000, 0 )		--REMOVE MODAL FLAG
		end

		bShowEscape = 0
		AddMessage( 65552, 2000, bShowEscape )			--SHOW_WINDOW (HIDE)

	end

--ANIMATED STATUS BAR BEGIN
	if ( nMessageCode == 536936448 and nFirst == 110 ) then	--STATE_CHANGED
		if ( nSecond == 1 ) then
			AddMessage( 65568, 40000, 0 )	--ENABLE WINDOW (disable)
			AddMessage( 65552, 40000, 2 )	--SHOW_WINDOW UI_SW_LAST
			SetUserProfileVar( "Mission.UnitExtendedInfo.opened", 1 );
			return 1
		else
			AddMessage( 65568, 40000, 0 )	--ENABLE WINDOW (disable)
			AddMessage( 65552, 40000, 0 )	--SHOW_WINDOW UI_SW_HIDE
			SetUserProfileVar( "Mission.UnitExtendedInfo.opened", 0 );
			return 1
		end
	end

	if ( nMessageCode == 268435458 ) then		--FAKE SB button pressed
		AddMessage( 65536, 110, 0 )	--SET STATE 0
		return 1
	end

--	if ( nMessageCode == 2097264 ) then			--SHOW_STATUS_BAR from keyboard
--		local nShowSB = GetUserProfileVar( "Mission.UnitExtendedInfo.opened", 1 )
--		if ( bShowSB == 0 ) then
--			AddMessage( 65536, 110, 1 )	--SET STATE 1
--		else
--			AddMessage( 65536, 110, 0 )	--SET STATE 0
--		end
--		return 1
--	end

	if ( nMessageCode == 536936464 and nFirst == 40000 ) then	--ANIMATION_FINISHED
		AddMessage( 65568, 40000, 1 )
		return 1
	end
--ANIMATED STATUS BAR END

	return 0
end
