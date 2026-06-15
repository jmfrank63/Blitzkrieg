function EnterChapter( strChapterName )

-- adding depot upgrades
	AddBaseUpgrade( "Bishop_GB" );
	AddBaseUpgrade( "Cruiser_MKVI_Crusader_GB" );
	AddBaseUpgrade( "Infantry_MKIII_Valentine_GB" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\allies\\tunis\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\allies\\tunis\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\allies\tunis\1") then
		EnableChapter("scenarios\\chapters\\allies\\italy\\1");
--Medal Conditions(Africa)
		AddMedal ("Medals\\Allies\\EAM_company\\1", 2);
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("Infantry_MKIII_Valentine_GB");
		AddNewSlot ("Infantry_MKIII_Valentine_GB");
		AddNewSlot ("Infantry_MKIII_Valentine_GB");
	end;
end;
