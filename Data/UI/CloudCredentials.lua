function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		if ( nFirst == 10001 or nFirst == 10002 or nFirst == 10020 or nFirst == 10021 or nFirst == 10022 ) then
			local nMessage = SetProcessedFlag( nFirst )
			AddMessage( nMessage, nFirst, 1 )
			return 1
		end
		return 0
	end

	if ( nMessageCode == 536936482 or nMessageCode == 536936480 or nMessageCode == 536936481 ) then --NOTIFY TEXTEDIT
		--nFirst is the edit box id; the dialog branches on it.
		local nMessage = SetProcessedFlag( nMessageCode )
		AddMessage( nMessage, nFirst, nSecond )
		return 1
	end

	return 0
end
