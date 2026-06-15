function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 and ( nFirst >= 10 and nFirst < 100 ) ) then --OK
		local nMessage = SetProcessedFlag( 2097920 )
		AddMessage( nMessage, (nFirst - 10)/2, 0 )
		return 1
	end

	return 0
end
