function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		if ( nFirst == 3006 or nFirst == 3007 ) then
			--Forward this message to parent
			AddMessage( 268435457, nFirst, nSecond )
			return 1
		end
	end

-- if keyboard pressed, press specific button


	if ( nMessageCode == 10002 ) then
		AddMessage( 65537, 3006, 1 )		-- button pressed
		return 1
	end

	if ( nMessageCode == 10001 ) then
		AddMessage( 65537, 3007, 1 )		-- button pressed
		return 1
	end

	return 0
end
