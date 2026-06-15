function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	local nTemp = ProcessMessageWithLink( nMessageCode, nFirst );
	if ( nTemp ~= 0 ) then
		return 1
	end

return 0
end
