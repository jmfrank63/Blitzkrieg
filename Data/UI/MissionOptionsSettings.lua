function LuaProcessMessage( nMessageCode, nFirst, nSecond )
--	if ( nMessageCode == 536936451 ) then --selection changed
--		local nMessage = SetProcessedFlag( 7777 );
--		AddMessage( nMessage, 7777, nSecond );
--		return 1
--	end

	if ( nMessageCode == 10001 or ( nMessageCode >= 10003 and nMessageCode <= 10012 )) then
		AddMessage( 65537, nMessageCode, 1 )		--IMC_OK
		return 1
	end

	if ( nMessageCode == 536936451 and nFirst == 1000 ) then  -- selection changed
		local nMessage = SetProcessedFlag( 536936451 );
		AddMessage( nMessage, nFirst, nSecond );
		return 1
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 10001 and nFirst <= 10012 ) ) then
		local nMessage = SetProcessedFlag( nFirst )
		AddMessage( nMessage, 0, 1 )
		return 1
	end

	return 0
end
