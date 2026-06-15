bShowQuestion = 0

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 268435457 ) then
		if ( nFirst == 3001 ) then		--NO
			bShowQuestion = 0
			AddMessage( 65552, 3000, 2 )	--SHOW_WINDOW MINIMIZE
			AddMessage( 65552, 3000, bShowQuestion )	--SHOW_WINDOW HIDE
			AddMessage( 65584, 3000, bShowQuestion )	--CHANGE MODAL FLAG
			return 1
		end

		if ( nFirst == 3002 ) then		--YES
			bShowQuestion = 0
			AddMessage( 65552, 3000, 2 )	--SHOW_WINDOW MINIMIZE
			AddMessage( 65552, 3000, bShowQuestion )	--SHOW_WINDOW HIDE
			AddMessage( 65584, 3000, bShowQuestion )	--CHANGE MODAL FLAG

			local nMessage = SetProcessedFlag( 8889 )
			AddMessage( nMessage, 0, 0 )			--SWITCH TO THE NEXT CHAPTER
			return 1
		end
	end

	if ( nMessageCode == 10001 and bShowQuestion == 1 ) then	--ESCAPE in dialog box
		--Hide dialog box
		bShowQuestion = 0
		AddMessage( 65552, 3000, bShowQuestion )	--SHOW_WINDOW SHOW
		AddMessage( 65584, 3000, bShowQuestion )	--CHANGE MODAL FLAG
		return 1
	end

	if ( nMessageCode == 10002 and bShowQuestion == 1 ) then	--ENTER in dialog box
		bShowQuestion = 0
		AddMessage( 65552, 3000, 2 )	--SHOW_WINDOW MINIMIZE
		AddMessage( 65552, 3000, bShowQuestion )	--SHOW_WINDOW HIDE
		AddMessage( 65584, 3000, bShowQuestion )	--CHANGE MODAL FLAG

		local nMessage = SetProcessedFlag( 8889 )
		AddMessage( nMessage, 0, 0 )			--SWITCH TO THE NEXT CHAPTER
		return 1
	end

	if ( nMessageCode >= 10001 and nMessageCode <= 10006 ) then
		AddMessage( 65537, nMessageCode, 1 )		--NEXT_STATE
		return 1
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 10001 and nFirst <= 10006 ) ) then
		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 0, 1 )
		return 1
	end

--	if ( nMessageCode == 8888 ) then
--		--Show dialog box
--		bShowQuestion = 1
--		AddMessage( 65552, 3000, bShowQuestion )	--SHOW_WINDOW SHOW
--		AddMessage( 65584, 3000, bShowQuestion )	--CHANGE MODAL FLAG
--		return 1
--	end

	return 0
end
