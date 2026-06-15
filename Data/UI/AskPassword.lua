function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		if ( nFirst == 10001 or nFirst == 10002 ) then
			--Forward this message to parent
			local nMessage = SetProcessedFlag( nFirst )
			AddMessage( nMessage, 0,0 );

--			AddMessage( 268435457, nFirst, nSecond )
			return 1
		end
	end

-- if keyboard pressed, press specific button



	if ( nMessageCode == 10002 ) then
		AddMessage( 65537,10002, 1 )		-- button pressed
		return 1
	end

	if ( nMessageCode == 10001 ) then
		AddMessage( 65537,10001, 1 )		-- button pressed
		return 1
	end


	if ( nMessageCode == 536936482 or nMessageCode == 536936480 or nMessageCode == 536936481 ) then --NOTIFY TEXTEDIT
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
