nNumberOfButtons = 0
nActiveButton = 0
bInit = 0

--the variable nNumberOfButtons is initilized from GlobalVar
--so that we do not need to write explicit LUA serialize code to save it
--when chapter or campaign screen is loading, the game serialize all GlobalVars
--and script is initializing after all global variables are loaded

function ClearState( nControl )
	i = 1000
	while ( i <= 1000 + nNumberOfButtons ) do
		if ( i ~= nControl ) then
			AddMessage( 65538, i, 0 )		--UI_SET_STATE_WO_NOTIFY to  0 (clear state)
		end
		i = i + 1
	end
end

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( bInit == 0 ) then
		nNumberOfButtons = InitCommonScript()
		bInit = 1
	end

	if ( nMessageCode == 536936448 and ( nFirst >= 1000 and nFirst <= 1000+nNumberOfButtons  ) ) then -- chapter button pressed
		if ( nSecond == 0 ) then
			AddMessage( 65536, nFirst, 1 )	--SET_STATE back to 1 (cannot deselect)
			return 1
		end
		
		if ( nSecond  == 1 ) then
			local nMessage = SetProcessedFlag( nFirst )
			AddMessage( nMessage, 0, 1 )
			ClearState( nFirst )
			return 1;
		end

		return 1
	end
end
