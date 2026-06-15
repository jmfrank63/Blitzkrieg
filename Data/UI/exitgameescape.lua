function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		if ( nFirst == 3001 or nFirst == 3002 ) then
			--Forward this message to parent
			AddMessage( 268435457, nFirst, nSecond )
			return 1
		end
	end

	return 0
end
