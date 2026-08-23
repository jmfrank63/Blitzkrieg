function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 ) then --NOTIFY STATE CHANGED
		-- The dialog buttons, the advanced toggle (10023), the per-row
		-- example-cycle buttons (4001..4007) and the scroll arrows
		-- (4100/4101): every clickable id the generic form renders.
		if ( nFirst == 10001 or nFirst == 10002 or nFirst == 10020 or nFirst == 10021 or nFirst == 10022 or nFirst == 10023
				or ( nFirst >= 4001 and nFirst <= 4007 ) or nFirst == 4100 or nFirst == 4101 ) then
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
