function EnterChapter( strChapterName )

-- adding depot upgrades
	AddBaseUpgrade( "M8_Greyhound_USA" );
	AddBaseUpgrade( "M3_General_Stuart_USA" );
	AddBaseUpgrade( "M4A1_General_Sherman_USA" );
	AddBaseUpgrade( "Cruiser_MKVIII_Cromwell_GB" );
	AddBaseUpgrade( "M10_Wolverine_USA" );
	AddBaseUpgrade( "76mm_M1A1_USA" );
	AddBaseUpgrade( "105mm_M2A1_USA" );
	AddBaseUpgrade( "90mm_M2_USA" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\allies\\normandy\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\allies\\normandy\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\allies\normandy\1") then
		EnableChapter("scenarios\\chapters\\allies\\ardennes\\1");
--Medal Conditions(Medal of honour)
		if (GetStatisticsValue(19, 0) == 0) then -- objective failed
			AddMedal("Medals\\Allies\\HonorMedal\\1", 5);
		end;

--Medal Conditions(Silverstar)
	local kills = GetStatisticsValue (1, 0); -- number of units
	local looses = GetStatisticsValue (3, 0); -- number of units
		if (kills/(looses + 1) >= 3) then
			AddMedal("Medals\\Allies\\SilverStar\\1", 4);
		end;
	end;

end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("105mm_M2A1_USA");
		AddNewSlot ("105mm_M2A1_USA");
		AddNewSlot ("105mm_M2A1_USA");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("M4A1_General_Sherman_USA");
		AddNewSlot ("M4A1_General_Sherman_USA");
		AddNewSlot ("M4A1_General_Sherman_USA");
	end;
end;
