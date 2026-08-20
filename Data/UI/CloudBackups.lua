function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		if ( nFirst == 10001 ) then
			local nMessage = SetProcessedFlag( nFirst )
			AddMessage( nMessage, nFirst, 1 )
			return 1
		end
		return 0
	end

	if ( nMessageCode == 536936451 and nFirst == 2100 ) then --LIST SELECTION CHANGED
		local nMessage = SetProcessedFlag( 2100 )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
