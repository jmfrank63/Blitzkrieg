nLastActiveButton = 0
p1 = 0
p2 = 0
p3 = 0
p4 = 0			--priority of bottom row buttons

--p1:   SUPPRESS 40, DEPLOY ARTILLERY 35, UNLOAD 30, BUILD 20, FORMATION 10
--p2:	RANGING 40, RESUPPLY 30, REPAIR 20, BINOCULARS 10
--p3:	SHELL TYPE 40, MOBILIZE 30, SET MINE 20, SQUAD- 10, SQUAD+ 5
--p4:	DEMINE 20

gActiveFormations = { 0, 0, 0, 0, 0 }
gActiveAviations = { 0, 0, 0, 0, 0 }
gActiveShellTypes = { 0, 0, 0, }	--DAMAGE, AGITATION, SMOKE

gButtons = {
	{  1, 63, 17,  2,  3,  6, 10, 16, -1, -1, -1, -1 },		--USUAL
	{ 23, 29, 24, 28, -1, -1, -1, -1, -1, -1, -1,  0 },		--BUILD
	{ 20, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0 },		--MINE
	{ 50, 53, 52, 51, 54, -1, -1, -1, -1, -1, -1,  0 },		--FORMATION
	{ 32, 30, 31, 35, 33, -1, -1, -1, -1, -1, -1,  0 },		--AVIA
--	{ 55, 56, 57, -1, -1, -1, -1, -1, -1, -1, -1,  0 },		--CHOOSE SHELL TYPE
	{ 55, 57, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0 },		--CHOOSE SHELL TYPE  without morale shell
}

E_USUAL_BUTTONS			= 1
E_BUILD_BUTTONS			= 2
E_MINE_BUTTONS			= 3
E_FORMATION_BUTTONS		= 4
E_AVIA_BUTTONS			= 5
E_SHELL_BUTTONS			= 6
nActivePanel = E_USUAL_BUTTONS

function LuaLoad( ... )
	local i = 1
	if ( arg.n < 10 ) then
		CallNullScriptError()		--THERE IS NO SUCH FUNCTION
		return
	end

	while ( i <= 5 ) do
		gActiveFormations[i] = arg[i*2]
		i = i + 1
	end

	while ( i <= 10 ) do
		gActiveAviations[i - 5] = arg[i*2]
		i = i + 1
	end

	nActivePanel = arg[22]
	nLastActiveButton = arg[24]
	gButtons[1][9] = arg[26]
	gButtons[1][10] = arg[28]
	gButtons[1][11] = arg[30]
	gButtons[1][12] = arg[32]

	gActiveShellTypes[1] = arg[34]
	gActiveShellTypes[2] = arg[36]
	gActiveShellTypes[3] = arg[38]
end

function LuaSave()
	local i = 1
	while ( i <= 5 ) do
		SaveLuaValue( i, gActiveFormations[i] )
		i = i + 1
	end

	while ( i <= 10 ) do
		SaveLuaValue( i, gActiveAviations[i - 5] )
		i = i + 1
	end

	SaveLuaValue( 11, nActivePanel )
	SaveLuaValue( 12, nLastActiveButton )
	SaveLuaValue( 13, gButtons[1][9] )
	SaveLuaValue( 14, gButtons[1][10] )
	SaveLuaValue( 15, gButtons[1][11] )
	SaveLuaValue( 16, gButtons[1][12] )

	SaveLuaValue( 17, gActiveShellTypes[1] )
	SaveLuaValue( 18, gActiveShellTypes[2] )
	SaveLuaValue( 19, gActiveShellTypes[3] )
end

function IsActiveButton( nIndex )
	if ( nIndex == 1 or nIndex == 2 or nIndex == 3 ) then
		return 1
	end

	if ( nIndex == 5 or nIndex == 6 ) then
		return 1
	end

	if ( nIndex == 10 or nIndex == 12 or nIndex == 13 or nIndex == 16 or nIndex == 17 ) then
		return 1
	end

	if ( nIndex == 22 or nIndex == 26 or nIndex == 27 or nIndex == 34 or nIndex == 36) then
		return 1
	end

	if ( nIndex == 42 or nIndex == 61 or nIndex == 62 or nIndex == 63 ) then
		return 1
	end
	
	return 0
end

function IsSingleStateButton( nIndex )
	if ( nIndex == 10 or nIndex == 16 or nIndex == 17 ) then
		return 1
	end

	if ( nIndex == 61 or nIndex == 62 or nIndex == 63 ) then
		return 1
	end

	return 0
end

function ProcessButtonsMessage( nIndex )
	local nID = gButtons[ nActivePanel ][ nIndex ]
	if ( nID == -1 ) then
		return
	end

	if ( nActivePanel == E_USUAL_BUTTONS ) then
		local temp = IsActiveButton( nID )
		if ( temp ~= 0 ) then
			AddMessage( 65537, 20000 + nID, 1 )	--NEXT_STATE
		else
			if ( nID == 70 or nID == 71 or nID == 72 or nID == 73 ) then
				AddMessage( 65537, 20000 + nID, 1 )	--NEXT_STATE
			end
		end
		return
	end

	if ( nIndex == 12 ) then						--RETURN
		AddMessage( 65537, 22011, 1 )			--NEXT_STATE
		return
	end

	local bSendMessage = 1
	if ( nActivePanel == E_AVIA_BUTTONS ) then
		if ( gActiveAviations[ nIndex ] == 0 ) then
			--this avia unit is disabled
			--clear active avia button
			if ( nLastActiveButton ~= 0 ) then
				AddMessage( 65536, nLastActiveButton, 0 )	--SET_STATE
				nLastActiveButton = 0
			end

			bSendMessage = 0
		end
	end

	if ( nActivePanel == E_FORMATION_BUTTONS ) then
		if ( nIndex == 4 ) then
			nIndex = 2
		else
			if ( nIndex == 2 ) then
				nIndex = 4
			end
		end
		if ( gActiveFormations[ nIndex ] == 0 ) then
			--this formation is disabled
			bSendMessage = 0
		end
	end

--	if ( nActivePanel == E_SHELL_BUTTONS ) then
--		OutputValue( "disabled check", nIndex );
--		if ( gActiveShellTypes[ nIndex ] == 0 ) then
--			OutputValue( "disabled", nIndex );
--			--this shell type is disabled
--			bSendMessage = 0
--		end
--	end

	if ( bSendMessage ~= 0 ) then
		AddMessage( 65537, 20000 + nID, 1 )	--NEXT_STATE
	end
end

function HideActivePanel()
	if ( nActivePanel == E_BUILD_BUTTONS ) then
		ShowBuildButtons( 0 )
	end
	if ( nActivePanel == E_MINE_BUTTONS ) then
		ShowMineButtons( 0 )
	end
	if ( nActivePanel == E_FORMATION_BUTTONS ) then
		ShowFormationButtons( 0 )
	end
	if ( nActivePanel == E_AVIA_BUTTONS ) then
		ShowAviaButtons( 0 )
	end
	if ( nActivePanel == E_SHELL_BUTTONS ) then
		ShowShellButtons( 0 )
	end

--	nActivePanel = E_USUAL_BUTTONS
	return
end

function ShowBuildButtons( bShow )
	if ( bShow ~= 0 ) then
		nActivePanel = E_BUILD_BUTTONS
	else
		nActivePanel = E_USUAL_BUTTONS
	end

	AddMessage( 65552, 20023, bShow )
	AddMessage( 65552, 20029, bShow )
	AddMessage( 65552, 20024, bShow )
	AddMessage( 65552, 20028, bShow )

	local i = 4
	while ( i < 12 ) do
		AddMessage( 65552, 22000+i, bShow )
		i = i + 1
	end
end

function ShowMineButtons( bShow )
	if ( bShow ~= 0 ) then
		nActivePanel = E_MINE_BUTTONS
	else
		nActivePanel = E_USUAL_BUTTONS
	end

	AddMessage( 65552, 20020, bShow )
	AddMessage( 65552, 20021, bShow )

	local i = 2
	while ( i < 12 ) do
		AddMessage( 65552, 22000+i, bShow )
		i = i + 1
	end
end

function ShowFormationButtons( bShow )
	if ( bShow ~= 0 ) then
		nActivePanel = E_FORMATION_BUTTONS
	else
		nActivePanel = E_USUAL_BUTTONS
	end

	AddMessage( 65552, 20050, bShow )
	AddMessage( 65552, 20051, bShow )
	AddMessage( 65552, 20052, bShow )
	AddMessage( 65552, 20053, bShow )
	AddMessage( 65552, 20054, bShow )

	local i = 5
	while ( i < 12 ) do
		AddMessage( 65552, 22000+i, bShow )
		i = i + 1
	end
end

function ShowAviaButtons( bShow )
	if ( bShow ~= 0 ) then
		nActivePanel = E_AVIA_BUTTONS
	else
		nActivePanel = E_USUAL_BUTTONS
	end

	AddMessage( 65552, 20032, bShow )		--SHOW WINDOW
	AddMessage( 65552, 20030, bShow )		--SHOW WINDOW
	AddMessage( 65552, 20031, bShow )		--SHOW WINDOW
	AddMessage( 65552, 20035, bShow )		--SHOW WINDOW
	AddMessage( 65552, 20033, bShow )		--SHOW WINDOW

	local i = 5
	while ( i < 12 ) do
		AddMessage( 65552, 22000+i, bShow )
		i = i + 1
	end
end

function ShowShellButtons( bShow )
	if ( bShow ~= 0 ) then
		nActivePanel = E_SHELL_BUTTONS
	else
		nActivePanel = E_USUAL_BUTTONS
	end

	AddMessage( 65552, 20055, bShow )		--SHOW WINDOW
--	AddMessage( 65552, 20056, bShow )		--SHOW WINDOW
	AddMessage( 65552, 20057, bShow )		--SHOW WINDOW

--	local i = 3
	local i = 2
	while ( i < 12 ) do
		AddMessage( 65552, 22000+i, bShow )
		i = i + 1
	end
end

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 2097664 ) then
		return 0					--for faster perfomance
	end
	if ( nMessageCode == 1048579 ) then
		return 0					--for faster perfomance
	end

	if ( nMessageCode >= 205 and nMessageCode <= 214 ) then
		--AVIATION DISABLE -- ENABLE
--		OutputValue( "Avia message", nMessageCode )
		local temp = nMessageCode - 205
		if ( temp == 0 or temp == 1 ) then
			gActiveAviations[1] = IsActiveBit( temp + 1, 0 )
			AddMessage( 65568, 20032, gActiveAviations[1] )
		end
		if ( temp == 2 or temp == 3 ) then
			gActiveAviations[2] = IsActiveBit( temp + 1, 0 )
			AddMessage( 65568, 20030, gActiveAviations[2] )
		end
		if ( temp == 4 or temp == 5 ) then
			gActiveAviations[3] = IsActiveBit( temp + 1, 0 )
			AddMessage( 65568, 20031, gActiveAviations[3] )
		end
		if ( temp == 6 or temp == 7 ) then
			gActiveAviations[4] = IsActiveBit( temp + 1, 0 )
			AddMessage( 65568, 20035, gActiveAviations[4] )
		end
		if ( temp == 8 or temp == 9 ) then
			gActiveAviations[5] = IsActiveBit( temp + 1, 0 )
			AddMessage( 65568, 20033, gActiveAviations[5] )
		end

		--these messages need to be forwarded to the game
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	if ( nMessageCode == 524 ) then
		--local bCanProcess = IsGameButtonProcessing();
--		if ( bCanProcess ~= 0 ) then 
			--Show aviation buttons
			HideActivePanel()
			ShowAviaButtons( 1 )
			OutputValue( "**** aviation enabled", 1 );
--		end
		return 1
	end

	if ( nMessageCode == 525 ) then
		--Show SHELL TYPE buttons
		HideActivePanel()
		ShowShellButtons( 1 )
		return 1
	end

	-- process 'change actions set' message
	if ( nMessageCode == 201 ) then
		HideActivePanel()

		p1 = 0
		p2 = 0
		p3 = 0
		p4 = 0

		--show empty buttons (bottom row of buttons)
		AddMessage( 65552, 22090, 1 )
		AddMessage( 65552, 22091, 1 )
		AddMessage( 65552, 22092, 1 )
		AddMessage( 65552, 22093, 1 )
		gButtons[1][9]  = -1
		gButtons[1][10] = -1
		gButtons[1][11] = -1
		gButtons[1][12] = -1

		local i = 1
		while ( i < 32 ) do
			local temp = IsActiveButton( i )
			if ( temp ~= 0 ) then
				nRes = IsActiveBit( nFirst, i )

				if ( nRes ~= 0 ) then
					AddMessage( 65568, 20000 + i, 1 )		--ENABLE WINDOW
					local nRes = IsSingleStateButton( i )
					if ( nRes == 0 ) then
						AddMessage( 65536, 20000 + i, 0 )	--SET STATE 0
					end
				else
					AddMessage( 65568, 20000 + i, 0 )		--ENABLE WINDOW (DISABLE)
				end
			end
			i = i + 1
		end

		nRes = IsActiveBit( nFirst, 5 )			--5, UNLOAD
		if ( nRes ~= 0 ) then
			p1 = 40
			AddMessage( 65552, 20005, 1 )
			gButtons[1][9] = 5
		end

		nRes = IsActiveBit( nFirst, 13 )		--13, SUPRESSIVE FIRE
		if ( nRes ~= 0 and p1 < 30 ) then
			p1 = 30
			AddMessage( 65552, 20013, 1 )
			gButtons[1][9] = 13
		end

		nRes = IsActiveBit( nFirst, 23 )		--23 is BUILD ANTY PERSON
		if ( nRes ~= 0 and p1 < 20 ) then
			p1 = 20
			AddMessage( 65552, 20070, 1 )
			gButtons[1][9] = 70
		end

		nRes = IsActiveBit( nFirst, 11 )		--11 is MAKE FORMATION
		if ( nRes ~= 0 and p1 < 10 ) then
			p1 = 10
			AddMessage( 65552, 20072, 1 )
			gButtons[1][9] = 72
		end

		nRes = IsActiveBit( nFirst, 12 )		--12, RANGING
		if ( nRes ~= 0 ) then
			p2 = 40
			AddMessage( 65552, 20012, 1 )
			gButtons[1][10] = 12
		end

		nRes = IsActiveBit( nFirst, 27 )		--27, RESUPPLY
		if ( nRes ~= 0 and p2 < 30 ) then
			p2 = 30
			AddMessage( 65552, 20027, 1 )
			gButtons[1][10] = 27
		end

		nRes = IsActiveBit( nFirst, 26 )		--26, REPAIR
		if ( nRes ~= 0 and p2 < 20 ) then
			p2 = 20
			AddMessage( 65552, 20026, 1 )
			gButtons[1][10] = 26
		end

		nRes = IsActiveBit( nFirst, 15 )		--15, CHOOSE FIRE SHELL TYPE
		if ( nRes ~= 0 ) then
			p3 = 40
			AddMessage( 65552, 20073, 1 )
			gButtons[1][11] = 73
		end

		nRes = IsActiveBit( nFirst, 20 )		--20 is SET MINE
		if ( nRes ~= 0 and p3 < 20 ) then
			p3 = 20
			AddMessage( 65552, 20071, 1 )
			gButtons[1][11] = 71
		end

		nRes = IsActiveBit( nFirst, 22 )		--22, DEMINE
		if ( nRes ~= 0 and p4 < 20 ) then
			p4 = 20
			AddMessage( 65552, 20022, 1 )
			gButtons[1][12] = 22
		end

		return 1
	end

	if ( nMessageCode == 202 ) then
		i = 0
		while ( i < 32 ) do
			temp = IsActiveButton( i + 32 )
			if ( temp ~= 0 ) then
				nRes = IsActiveBit( nFirst, i )
				if ( nRes ~= 0 ) then
					AddMessage( 65568, 20032 + i, 1 )		--ENABLE WINDOW
					local nRes = IsSingleStateButton( i + 32 )
					if ( nRes == 0 ) then
						AddMessage( 65536, 20032 + i, 0 )	--SET STATE 0
					end
				else
					AddMessage( 65568, 20032 + i, 0 )		--ENABLE WINDOW (DISABLE)
				end
			else
				if ( i >= 18 and i <= 22 ) then
					nRes = IsActiveBit( nFirst, i )
					if ( nRes ~= 0 ) then
						gActiveFormations[i - 17] = 1
						AddMessage( 65568, 20032 + i, 1 )	--ENABLE WINDOW
					else
						gActiveFormations[i - 17] = 0
						AddMessage( 65568, 20032 + i, 0 )	--ENABLE WINDOW (DISABLE)
					end
				end

				if ( i >= 23 and i <= 25 ) then
					nRes = IsActiveBit( nFirst, i )
					if ( nRes ~= 0 ) then
						gActiveShellTypes[i - 22] = 1
						AddMessage( 65568, 20032 + i, 1 )	--ENABLE WINDOW
					else
						gActiveShellTypes[i - 22] = 0
						AddMessage( 65568, 20032 + i, 0 )	--ENABLE WINDOW (DISABLE)
					end
				end
			end

			i = i + 1
		end

		nRes = IsActiveBit( nFirst, 10 )		--42, DEPLOY ARTILLERY
		if ( nRes ~= 0 and p1 < 35 ) then
			p1 = 35
			OutputValue( "++++++++++++Gotcha++++++++++~~~~~+++", 1 );
			AddMessage( 65552, 20042, 1 )
			gButtons[1][9] = 42
		end

		nRes = IsActiveBit( nFirst, 2 )			--34, BINOCULARS
		if ( nRes ~= 0 and p2 < 10 ) then
			p2 = 10
			AddMessage( 65552, 20034, 1 )
			gButtons[1][10] = 34
		end

		nRes = IsActiveBit( nFirst, 4 )			--36, MOBILIZE (HUMAN RESUPPLY)
		if ( nRes ~= 0 and p3 < 30 ) then
			p3 = 30
			AddMessage( 65552, 20036, 1 )
			gButtons[1][11] = 36
		end

		nRes = IsActiveBit( nFirst, 29 )		--61, SQUAD-
		if ( nRes ~= 0 and p3 < 10 ) then
			p3 = 10
			AddMessage( 65552, 20061, 1 )
			gButtons[1][11] = 61
		end

		nRes = IsActiveBit( nFirst, 30 )		--62, SQUAD+
		if ( nRes ~= 0 and p3 < 5 ) then
			p3 = 5
			AddMessage( 65552, 20062, 1 )
			gButtons[1][11] = 62
		end

		return 1
	end


--	event come from outside (game)
--	if ( nMessageCode < 64 ) then
--		if ( IsActiveButton( nMessageCode ) ~= 0 ) then
--			AddMessage( 65537, 20000 + nMessageCode, 1 )	--NEXT_STATE
--			return 1
--		end
--		return 0
--	end

	if ( nMessageCode >= 4194561 and nMessageCode <= 4194572 ) then
		ProcessButtonsMessage( nMessageCode - 4194560 )
		return 1
	end

	if ( nMessageCode > 256 and nMessageCode < 320 ) then
		--Command was processed by the game, we need to clear interface button
		local temp = IsActiveButton( nMessageCode - 256 )
		if ( temp ~= 0 ) then
			local nRes = IsSingleStateButton( nMessageCode - 256 )
			if ( nRes == 0 ) then
				AddMessage( 65536, 20000 - 256 + nMessageCode, 0 )	--CHANGE_STATE to 0 (clear state)
			end
			return 1
		end

		if ( nMessageCode == 23+256 or nMessageCode == 29+256 or nMessageCode == 24+256 or nMessageCode == 28+256 ) then
			if ( nMessageCode - 256 == nLastActiveButton - 20000 ) then
				--BUILD ACTION DONE
				nLastActiveButton = 0
				AddMessage( 65536, 20000 - 256 + nMessageCode, 0 )	--CHANGE_STATE to 0 (clear state)
				ShowBuildButtons( 0 )
			end

			return 1
		end

		if ( nMessageCode == 20+256 or nMessageCode == 21+256 ) then
			if ( nMessageCode - 256 == nLastActiveButton - 20000 ) then
				--MINE WAS PLACED
				nLastActiveButton = 0
				AddMessage( 65536, 20000 - 256 + nMessageCode, 0 )	--CHANGE_STATE to 0 (clear state)
				ShowMineButtons( 0 )
			end

			return 1
		end

		if ( (nMessageCode >= 30+256 and nMessageCode <= 33+256) or nMessageCode == 35+256 ) then
			if ( nMessageCode - 256 == nLastActiveButton - 20000 ) then
				--AVIA WAS CALLED
				nLastActiveButton = 0
				AddMessage( 65536, 20000 - 256 + nMessageCode, 0 )	--CHANGE_STATE to 0 (clear state)
				ShowAviaButtons( 0 )
			end

			return 1
		end

		return 0
	end
	
	-- in-UI messages about state changing
	if ( nMessageCode == 536936448 and nFirst > 20000 and nFirst < 20064 ) then	--NOTIFY_CHANGE_STATE
		local nRes = IsSingleStateButton( nFirst - 20000 )
		if ( nRes ~= 0 ) then
			--these buttons have only one state
			if ( nLastActiveButton ~= 0 ) then
				AddMessage( 65536, nLastActiveButton, 0 )	--SET_STATE
				nLastActiveButton = 0
			else
				AddMessage( 65616, nFirst, 100 );
			end
			local nButtonID = nFirst - 20000;
			local nNewMessageCode = SetProcessedFlag( nFirst - 20000 )
			AddMessage( nNewMessageCode, 0, 0 )
			OutputValue( "button code", nButtonID );
			return 1
		end

		if ( nFirst - 20000 >= 50 and nFirst - 20000 <= 54 ) then			--FORMATIONS
			--these buttons have only one state
			if ( nLastActiveButton ~= 0 ) then
				AddMessage( 65536, nLastActiveButton, 0 )	--SET_STATE
				nLastActiveButton = 0
			end

			local nNewMessageCode = SetProcessedFlag( nFirst - 20000 )
			AddMessage( nNewMessageCode, 0, 0 )

			ShowFormationButtons( 0 )
			return 1
		end

		if ( nFirst - 20000 >= 55 and nFirst - 20000 <= 57 ) then			--CHOOSE SHELLS TYPE
			--these buttons have only one state
			local nButtonID = nFirst - 20000;
			OutputValue( "button code", nButtonID );
			if ( nLastActiveButton ~= 0 ) then
				AddMessage( 65536, nLastActiveButton, 0 )	--SET_STATE
				nLastActiveButton = 0
			end

			local nNewMessageCode = SetProcessedFlag( nFirst - 20000 )
			AddMessage( nNewMessageCode, 0, 0 )

			ShowShellButtons( 0 )
			return 1
		end

		if ( nSecond == 1 ) then
			if ( nLastActiveButton ~= 0 and nLastActiveButton ~=  nFirst ) then  -- this button isn't active button 
				OutputValue("don't allow to actovate button", nFirst );
				OutputValue("because current active is ", nLastActiveButton );
				AddMessage( 65536, nLastActiveButton, 0 )	--SET_STATE
			end
			nLastActiveButton = nFirst

			local nNewMessageCode = SetProcessedFlag( nFirst - 20000 )
			AddMessage( nNewMessageCode, 0, 0 )

			return 1
		end
		if ( nSecond == 0 ) then
			if ( nLastActiveButton == nFirst ) then
				OutputValue("deactivate button", nFirst );
				nLastActiveButton = 0
			end

			local nNewMessageCode = SetProcessedFlag( nFirst + 256 - 20000 )	--RESET_ACTION_xx
			AddMessage( nNewMessageCode, 0, 0 )
			return 1
		end
	end

--	if ( nMessageCode == 536936448 and nFirst >= 20080 and nFirst <= 20083 ) then	--CHOOSE FORMATION
--		local nNewMessageCode = SetProcessedFlag( 11 )
--		AddMessage( nNewMessageCode, nFirst - 20080, 0 )
--		return 1
--	end

	if ( nMessageCode == 536936448 and nFirst == 22011 ) then	--RETURN
		HideActivePanel()
		if ( nLastActiveButton ~= 0 ) then
			AddMessage( 65536, nLastActiveButton, 0 )	--CHANGE_STATE to 0 (clear state)
			nLastActiveButton = 0
		end

		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 20070 ) then	--BUILD
		HideActivePanel()
		ShowBuildButtons( 1 )
		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 20071 ) then	--SET MINE
		HideActivePanel()
		ShowMineButtons( 1 )

		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 20072 ) then	--CHOOSE FORMATION
		HideActivePanel()
		ShowFormationButtons( 1 )

		return 1
	end

	if ( nMessageCode == 536936448 and nFirst == 20073 ) then	--SHELL TYPE
		HideActivePanel()
		ShowShellButtons( 1 )

		return 1
	end

	if ( nMessageCode == 536936448 and nFirst >= 22030 and nFirst <= 22035 and nFirst ~= 22034 ) then
		--diable avia button pressed
		--clear active avia button
		if ( nLastActiveButton ~= 0 ) then
			AddMessage( 65536, nLastActiveButton, 0 )	--SET_STATE
			nLastActiveButton = 0
		end
		return 1
	end

	return 0
end
