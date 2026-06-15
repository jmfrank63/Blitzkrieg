nWindowID = 0

function LuaInit( ... )
	OutputValue( "LuaInit called()", arg[1] )
	nWindowID = arg[1]
end

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode >= 20000 and nMessageCode < 29000 ) then
		AddMessage( 65537, nMessageCode, 1 )		--? encyclopedia reference
		return 1
	end

	if ( nMessageCode == 268435457 ) then
		OutputValue( "268435457 message, nFirst = ", nFirst )
		if ( nFirst >= 20000 and nFirst < 29000 ) then
			local nMessage = SetProcessedFlag( nFirst )
			AddMessage( nMessage, nWindowID, 1 )
			return 1
		end
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 20000 and nFirst < 29000 ) ) then --?
		AddMessage( 268435457, nFirst, 1 )
		return 1
	end

	return 0
end
