bShowEscape = 0 
bShowHelp = 0
bShowObjectives = 0
bShowSingleObjective = 0
bShowSB = 0
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
	bShowSB = arg[8]

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
	SaveLuaValue( 4, bShowSB )
	SaveLuaValue( 5, bPause )
	SaveLuaValue( 6, bShowSingleObjective )
	SaveLuaValue( 7, bShowWin )
	SaveLuaValue( 8, bShowLose )
end

function LuaProcessMessage( nMessageCode, nFirst, nSecond )

--++++++++++++++++++++++++++++++++++
-- BY VESELOV: ESCAPE MENU HANDLE 
--++++++++++++++++++++++++++++++++++

	--+++++++++++++++++++++++++
	-- multiplayer
	if ( nMessageCode == 2098196 ) then -- MC_ESCAPE_MENU_MP
		AddMessage( 65552, 99998, 1 )	--SHOW_WINDOW
		AddMessage( 65584, 99998, 1 )	--CHANGE MODAL FLAG
	end

	if ( nMessageCode == 2098199 ) then -- hide escape menu
		AddMessage( 65552, 99998, 0 )	--SHOW_WINDOW
		AddMessage( 65584, 99998, 0 )	--CHANGE MODAL FLAG
	end


	--+++++++++++++++++++++++++
	-- single player
	if ( nMessageCode == 2098196 ) then -- MC_ESCAPE_MENU_MP
		AddMessage( 65552, 99998, 1 )	--SHOW_WINDOW
		AddMessage( 65584, 99998, 1 )	--CHANGE MODAL FLAG
	end

	if ( nMessageCode == 2098200 ) then -- hide escape menu
		AddMessage( 65552, 99999, 0 )	--SHOW_WINDOW
		AddMessage( 65584, 99999, 0 )	--CHANGE MODAL FLAG
	end

--++++++++++++++++++++++++++++++++++
--END BY VESELOV
--++++++++++++++++++++++++++++++++++
	if ( nMessageCode == 3145729 ) then
		local nMessageCode = SetProcessedFlag( 1048593 )
		AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME

		AddMessage( 65552, 1000, 1 )		--SHOW_WINDOW WIN DIALOG
		AddMessage( 65584, 1000, 1 )		--SET MODAL FLAG
		bShowWin = 1
		return 1
	end

	if ( nMessageCode == 3145730 ) then
		local nMessageCode = SetProcessedFlag( 1048593 )
		AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME

		AddMessage( 65552, 1001, 1 )		--SHOW_WINDOW LOSE DIALOG
		AddMessage( 65584, 1001, 1 )		--SET MODAL FLAG
		return 1
	end

	if ( nMessageCode == 3145731 ) then			--draw
		local nMessageCode = SetProcessedFlag( 1048593 )
		AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME

		AddMessage( 65552, 1003, 1 )		--SHOW_WINDOW DRAW DIALOG
		AddMessage( 65584, 1003, 1 )		--SET MODAL FLAG
		return 1
	end

	if ( nMessageCode == 2097233 and bShowWin == 1 ) then	--ENTER in WIN dialog
		nMessage = SetProcessedFlag( 3145729 )
		AddMessage( nMessage, 1, 1 )
		return 1
	end

	if ( nMessageCode == 2097233 and bShowLose == 1 ) then	--ENTER in LOSE dialog
		nMessage = SetProcessedFlag( 3145730 )
		AddMessage( nMessage, 1, 1 )
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

		if ( bShowObjectives == 1 ) then
			bShowObjectives = 0
			AddMessage( 65552, 4000, bShowObjectives )		--SHOW_WINDOW
			AddMessage( 65584, 4000, 0 )		--REMOVE MODAL FLAG

			local nMessageCode = SetProcessedFlag( 1048594 )
			AddMessage( nMessageCode, 0, 0 )		--UNPAUSE_GAME
			bPause = 0

			return 1
		end

		if ( bShowHelp == 1 ) then
			bShowHelp = 0
			AddMessage( 65552, 3000, bShowHelp )	--SHOW_WINDOW HELP
			AddMessage( 65584, 3000, 0 )		--REMOVE MODAL FLAG

			local nMessageCode = SetProcessedFlag( 1048594 )
			AddMessage( nMessageCode, 0, 0 )		--UNPAUSE_GAME
			bPause = 0

			return 1
		end

		if ( bShowSingleObjective == 1 ) then
			bShowSingleObjective = 0
			AddMessage( 65552, 6000, bShowSingleObjective )	--SHOW_WINDOW SINGLE_OBJECTIVE
			local nMessageCode = SetProcessedFlag( 4194838 )
			AddMessage( nMessageCode, 0, 0 )		--SEND NOTIFY ABOUT CLOSING OBJECTIVE TO THE GAME 

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
		if ( bShowObjectives ~= 0 ) then
			bShow = 1
			bShowObjectives = 0
			AddMessage( 65552, 4000, bShowObjectives )		--SHOW_WINDOW
			AddMessage( 65584, 4000, 0 )		--REMOVE MODAL FLAG
		end

		if ( bShowSingleObjective ~= 0 ) then
			bShow = 1
		end

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

	if ( ( nMessageCode == 2097248 or nMessageCode == 2097257 ) and bShowObjectives == 1 ) then	--ESC or TAB
		bShowObjectives = 0
		AddMessage( 65552, 4000, bShowObjectives )		--SHOW_WINDOW
		AddMessage( 65584, 4000, 0 )		--REMOVE MODAL FLAG

		local nMessageCode = SetProcessedFlag( 1048594 )
		AddMessage( nMessageCode, 0, 0 )		--UNPAUSE_GAME
		bPause = 0

		return 1
	end

	if ( nMessageCode == 2097248 and bShowSingleObjective == 1 ) then	--ESC
		bShowSingleObjective = 0
		AddMessage( 65552, 6000, bShowSingleObjective )		--SHOW_WINDOW
		local nMessageCode = SetProcessedFlag( 4194838 )
		AddMessage( nMessageCode, 0, 0 )		--SEND NOTIFY ABOUT CLOSING OBJECTIVE TO THE GAME 

		return 1
	end

	if ( nMessageCode == 2097248 ) then				--ESC
		bShowEscape = 1 - bShowEscape
		AddMessage( 65552, 2000, bShowEscape )	--SHOW_WINDOW
		AddMessage( 65584, 2000, bShowEscape )	--CHANGE MODAL FLAG

		nMessageCode = SetProcessedFlag( 1048594 - bShowEscape )
		AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME/UNPAUSE
		bPause = bShowEscape

		return 1
	end

	if ( nMessageCode == 2097265 ) then				--HIDE ESC WITHOUT UNPAUSE
		bShowEscape = 0
		AddMessage( 65552, 2000, bShowEscape )	--SHOW_WINDOW
		AddMessage( 65584, 2000, bShowEscape )	--CHANGE MODAL FLAG
		bPause = bShowEscape

		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 6001 ) then	--SHOW SINGLE OBJECTIVE BUTTON
		local nMessageCode = SetProcessedFlag( 4194839 )
		AddMessage( nMessageCode, 0, 0 )		--FORWARD TO THE GAME
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

		if ( bShowSingleObjective ~= 0 ) then
			bShow = 1
--			bShowSingleObjective = 0
--			AddMessage( 65552, 6000, bShowSingleObjective )	--SHOW_WINDOW SINGLE_OBJECTIVE
--			local nMessageCode = SetProcessedFlag( 4194838 )
--			AddMessage( nMessageCode, 0, 0 )		--SEND NOTIFY ABOUT CLOSING OBJECTIVE TO THE GAME 
		end

		if ( bShow == 0 ) then
			local nMessageCode = SetProcessedFlag( 1048593 )
			AddMessage( nMessageCode, 0, 0 )		--PAUSE_GAME
			bPause = 1
		end

		bShowEscape = 0
		AddMessage( 65552, 2000, bShowEscape )			--SHOW_WINDOW (HIDE)

		bShowObjectives = 1
		AddMessage( 65552, 4000, bShowObjectives )		--SHOW_WINDOW
		AddMessage( 65584, 4000, 1 )		--SET MODAL FLAG
		return 1
	end

	if ( nMessageCode == 2097266 ) then				--SHOW_SINGLE_OBJECTIVE
		local bShow = 0
		if ( bShowHelp ~= 0 ) then
			bShow = 1
			bShowHelp = 0
			AddMessage( 65552, 3000, bShowHelp )	--SHOW_WINDOW HELP
			AddMessage( 65584, 3000, 0 )		--REMOVE MODAL FLAG
		end

		if ( bShowObjectives ~= 0 ) then
			bShow = 1
			bShowObjectives = 0
			AddMessage( 65552, 4000, bShowObjectives )	--SHOW_WINDOW OBJECTIVES
			AddMessage( 65584, 4000, 0 )		--REMOVE MODAL FLAG
		end

		bShowEscape = 0
		AddMessage( 65552, 2000, bShowEscape )			--SHOW_WINDOW (HIDE)

		bShowSingleObjective = 1
		AddMessage( 65552, 6000, bShowSingleObjective )		--SHOW_WINDOW
		AddMessage( 65584, 6000, 0 )		--REMOVE MODAL FLAG
		local nMessageCode = SetProcessedFlag( 1048594 )
		AddMessage( nMessageCode, 0, 0 )		--UNPAUSE_GAME
		return 1
	end

--ANIMATED STATUS BAR BEGIN
	if ( nMessageCode == 536936448 and nFirst == 110 ) then	--STATE_CHANGED
		if ( nSecond == 1 ) then
			bShowSB = 1
			AddMessage( 65568, 40000, 0 )	--ENABLE WINDOW (disable)
			AddMessage( 65552, 40000, 2 )	--SHOW_WINDOW UI_SW_LAST
			return 1
		else
			bShowSB = 0
			AddMessage( 65568, 40000, 0 )	--ENABLE WINDOW (disable)
			AddMessage( 65552, 40000, 0 )	--SHOW_WINDOW UI_SW_HIDE
			return 1
		end
	end

	if ( nMessageCode == 268435458 ) then		--FAKE SB button pressed
		AddMessage( 65536, 110, 0 )	--SET STATE 0
		return 1
	end

	if ( nMessageCode == 2097264 ) then			--SHOW_STATUS_BAR from keyboard
		if ( bShowSB == 0 ) then
			AddMessage( 65536, 110, 1 )	--SET STATE 1
		else
			AddMessage( 65536, 110, 0 )	--SET STATE 0
		end
		return 1
	end

	if ( nMessageCode == 536936464 and nFirst == 40000 ) then	--ANIMATION_FINISHED
		AddMessage( 65568, 40000, 1 )
		return 1
	end
--ANIMATED STATUS BAR END

	return 0
end
