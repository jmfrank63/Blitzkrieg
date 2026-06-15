function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		if ( nFirst == 10006 ) then
			--Forward this message to parent
			AddMessage( 268435457, nFirst, nSecond )
			return 1
		end

		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 1, 1 )
		return 1
	end

	return 0
end
