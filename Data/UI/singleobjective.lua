function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 and nFirst == 10002 ) then	--STATE_CHANGED
		local nNewMessageCode = SetProcessedFlag( 2097253 )
		AddMessage( nNewMessageCode, 0, 0 )		--HIDE_ESCAPE_MENU, when UI process it will hide objective screen
		return 1
	end

	return 0
end
