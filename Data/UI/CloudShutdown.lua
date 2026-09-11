function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		if ( nFirst == 3104 ) then --skip and exit now
			local nMessage = SetProcessedFlag( nFirst )
			AddMessage( nMessage, nFirst, 1 )
			return 1
		end
		return 0
	end
	return 0
end
