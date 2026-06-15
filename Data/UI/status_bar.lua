function LuaProcessMessage( nMessageCode, nFirst, nSecond )
	if ( nMessageCode == 536936448 and nFirst == 110110 ) then 
--		AddMessage( 65568, 40000, 0 )	--ENABLE WINDOW (disable)
--		AddMessage( 65552, 40000, 0 )	--SHOW_WINDOW UI_SW_HIDE
--		SetUserProfileVar( "Mission.UnitExtendedInfo.opened", 0 );
		AddMessage( 268435458, 0, 0 );
		return 1
	end

	return 0
end
