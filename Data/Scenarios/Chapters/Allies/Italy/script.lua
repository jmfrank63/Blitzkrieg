function EnterChapter( strChapterName )
	ChangeCurrent ("M3_Grant_GB", "Cruiser_MKVIII_Cromwell_GB");
	ChangeDefault ("M3_Grant_GB", "Cruiser_MKVIII_Cromwell_GB");

-- adding depot upgrades
	AddBaseUpgrade( "Crusader_AA_GB" );
	AddBaseUpgrade( "Sexton_II_GB" );
	AddBaseUpgrade( "M7_Priest_USA" );
	AddBaseUpgrade( "Cruiser_MKVIII_Cromwell_GB" );
	AddBaseUpgrade( "Infantry_MKIV_Churchill_GB" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\allies\\italy\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\allies\\italy\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\allies\italy\1") then
		EnableChapter("scenarios\\chapters\\allies\\normandy\\1");
--Medal Conditions(Bronzestar)
		if (GetStatisticsValue(1, 0) >= 3000) then
			AddMedal ("Medals\\Allies\\BronzeStar\\1", 3);
		end;
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("4_5_inch_Gun_GB");
		AddNewSlot ("4_5_inch_Gun_GB");
		AddNewSlot ("4_5_inch_Gun_GB");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("Cruiser_MKVIII_Cromwell_GB");
		AddNewSlot ("Cruiser_MKVIII_Cromwell_GB");
		AddNewSlot ("Cruiser_MKVIII_Cromwell_GB");
	end;
end;
