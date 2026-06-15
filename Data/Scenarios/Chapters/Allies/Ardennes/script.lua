function EnterChapter( strChapterName )

-- adding depot upgrades
	AddBaseUpgrade( "T34_Calliope_USA" );
	AddBaseUpgrade( "M13_GMC_USA" );
	AddBaseUpgrade( "M4A3_General_Sherman_USA" );
	AddBaseUpgrade( "M5_General_Stuart_USA" );
	AddBaseUpgrade( "155mm_M1_USA" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\allies\\ardennes\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\allies\\ardennes\\1");
	end;

-- end of campaign
	if ( strMissionName == "scenarios\scenariomissions\allies\ardennes\1") then
		FinishCampaign();
	end;

end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("4_5_inch_Gun_GB");
		AddNewSlot ("4_5_inch_Gun_GB");
		AddNewSlot ("4_5_inch_Gun_GB");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("M4A3_General_Sherman_USA");
		AddNewSlot ("M4A3_General_Sherman_USA");
		AddNewSlot ("M4A3_General_Sherman_USA");
	end;
end;
