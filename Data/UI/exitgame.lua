bShowEscape = 0

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 268435457 ) then				--EXIT GAME BUTTON
		if ( nFirst == 10006 ) then
			bShowEscape = 1
			AddMessage( 65552, 3000, bShowEscape )	--SHOW_WINDOW
--			AddMessage( 65584, 3000, bShowEscape )	--CHANGE MODAL FLAG
			return 1
		end

		if ( nFirst == 3001 ) then		--YES
			local nMessage = SetProcessedFlag( 10006 )
			AddMessage( nMessage, 0, 0 )			--EXIT GAME
			return 1
		end

		if ( nFirst == 3002 ) then		--NO
			bShowEscape = 0
			AddMessage( 65552, 3000, 2 )	--SHOW_WINDOW MINIMIZE
			AddMessage( 65552, 3000, bShowEscape )	--SHOW_WINDOW HIDE
--			AddMessage( 65584, 3000, bShowEscape )	--CHANGE MODAL FLAG
			return 1
		end
	end

	if ( nMessageCode == 8888 ) then				--ESC
		bShowEscape = 1 - bShowEscape
		if ( bShowEscape == 0 ) then
			AddMessage( 65552, 3000, 2 )	--SHOW_WINDOW MINIMIZE
		end
		AddMessage( 65552, 3000, bShowEscape )	--SHOW_WINDOW HIDE
--		AddMessage( 65584, 3000, bShowEscape )	--CHANGE MODAL FLAG

		return 1
	end

	if ( nMessageCode == 10002 ) then				--ENTER
		if ( bShowEscape == 1 ) then
			local nMessage = SetProcessedFlag( 10006 )
			AddMessage( nMessage, 0, 0 )			--EXIT GAME
			return 1
		else
			--game will process this message (forward to interface NEW_GAME)
			return 0
		end
	end

	return 0
end
