bShowEscape = 0

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 10001 or ( nMessageCode >= 10003 and nMessageCode <= 10007 )  ) then
		AddMessage( 65537, nMessageCode, 1 )		--IMC_OK
		return 1
	end

	if ( nMessageCode == 536936451 and nFirst == 1000 ) then --selection changed
		local nMessage = SetProcessedFlag( 536936451 );
		AddMessage( nMessage, nFirst, nSecond );
		return 1
	end


	if ( nMessageCode == 536936448 and ( nFirst >= 10001 and nFirst <= 10007) ) then --OK
		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 0, 1 )
		return 1
	end

	if ( nMessageCode == 268435457 ) then				--OK or CANCEL in player's info pressed.
		if ( nFirst == 3006 or nFirst == 3007 ) then		--
			local nMessage = SetProcessedFlag( nFirst )
			AddMessage( nMessage, 0, 1 )			-- hide window
			AddMessage( 65552, 3000, 2)			--SHOW_WINDOW MINIMIZE
			AddMessage( 65552, 3000, 0)			--SHOW_WINDOW HIDE
			AddMessage( 65584, 3000, 0)			--reset modal flag

			return 1
		end
	end


	if ( nMessageCode == 536936480 ) then		--EDIT BOX return
		local nMessage = SetProcessedFlag( 536936480 );
		AddMessage( nMessage, nFirst, nSecond );
		return 1
	end

	if ( nMessageCode == 536936481 ) then		--EDIT BOX escape
		local nMessage = SetProcessedFlag( 536936481 );
		AddMessage( nMessage, nFirst, nSecond );
		return 1
	end

-- show player's info

	if ( nMessageCode == 8888 ) then				-- button "INFO" pressed
		AddMessage( 65552, 3000, 1 )	--SHOW_WINDOW show
		AddMessage( 65584, 3000, 1)	--set modal flag
		return 1
	end

	if ( nMessageCode == 8889 ) then				-- button "INFO" pressed
		AddMessage( 65552, 3000, 2)			--SHOW_WINDOW MINIMIZE
		AddMessage( 65552, 3000, 0)			--SHOW_WINDOW HIDE
		AddMessage( 65584, 3000, 0)			--reset modal flag
		return 1
	end

	return 0
end
