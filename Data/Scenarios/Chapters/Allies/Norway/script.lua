function EnterChapter( strChapterName )

	if ( GetUserProfileVar( "ScenarioTutorial.Passed", 0 ) == 1) then
		EnableMission("scenarios\\scenariomissions\\allies\\norway\\1");
	end;

	SetIGlobalVar( "Chapter.IsFirst", 1);

	EnableMission("scenarios\\scenariomissions\\allies\\intro\\1");

-- Adding start units
	AddNewSlot ("25pdr_QFH_GB");
	AddNewSlot ("25pdr_QFH_GB");
	AddNewSlot ("25pdr_QFH_GB");

	AddNewSlot ("Humber_MK1_GB");
	AddNewSlot ("Humber_MK1_GB");
	AddNewSlot ("Humber_MK1_GB");

-- adding depot upgrades
	AddBaseUpgrade( "2pdr_QFG_GB" );
	AddBaseUpgrade( "25pdr_QFH_GB" );
	AddBaseUpgrade( "Humber_MK1_GB" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\allies\\norway\\1.Finished", 0);

	if ( strMissionName == "scenarios\scenariomissions\allies\intro\1") then
		EnableMission("scenarios\\scenariomissions\\allies\\norway\\1");
-- allow scenario mission further
		SetUserProfileVar( "ScenarioTutorial.Passed", 1 );
	end;

	if ( strMissionName == "scenarios\scenariomissions\allies\norway\1") then
		EnableChapter("scenarios\\chapters\\allies\\france\\1");
-- Medal conditions
		AddMedal("Medals\\Allies\\Gmedal\\1", 0);
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("Humber_MK1_GB");
		AddNewSlot ("Humber_MK1_GB");
		AddNewSlot ("Humber_MK1_GB");
	end;
end;
