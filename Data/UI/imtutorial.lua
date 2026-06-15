function LuaProcessMessage( nMessageCode, nFirst, nSecond )
--	if ( nMessageCode == 10002 ) then
--		AddMessage( 65537, nMessageCode, 1 )		--NEXT_STATE
--		return 1
--	end

--	if ( nMessageCode == 536936448 and nFirst == 10002 ) then	--STATE_CHANGED
--		local nNewMessageCode = SetProcessedFlag( 31414 )
--		AddMessage( nNewMessageCode, 0, 0 )		--hide tutorial window
--		return 1
--	end

	return 0
end
