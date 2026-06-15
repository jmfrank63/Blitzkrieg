function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 and nFirst == 10000 ) then --OK
		nMessage = SetProcessedFlag( 3145729 )
		AddMessage( nMessage, 1, 1 )
		return 1
	end

	return 0
end
