function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	local nTemp = ProcessMessageWithLink( nMessageCode, nFirst );
	if ( nTemp ~= 0 ) then
		return 1
	end

	if ( nMessageCode == 536936451 or nMessageCode == 536936452 ) then	--NOTIFY SELECTION CHANGED
		--forward these messages to the game
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
