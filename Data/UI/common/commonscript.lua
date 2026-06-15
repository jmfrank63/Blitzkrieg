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
			AddMessage( 65536, i, 0 )		--CHANGE_STATE to 0 (clear state)
		end
		i = i + 1
	end
end

function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( bInit == 0 ) then
		nNumberOfButtons = InitCommonScript()
		bInit = 1
	end

	i = 1000
	while ( i <= 1000 + nNumberOfButtons ) do
		if ( nMessageCode == 536936448 and nFirst == i ) then
			if ( nSecond == 1 ) then
				nActiveButton = i
				ClearState( i )
				return 1
			end
			if ( nActiveButton == i and nSecond == 0 ) then
				AddMessage( 65536, i, 1 )	--SET_STATE
				return 1
			end
			return 0
		end
		i = i + 1
	end

	return 0
end
