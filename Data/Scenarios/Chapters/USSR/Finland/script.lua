function EnterChapter( strChapterName )

	if ( GetUserProfileVar( "ScenarioTutorial.Passed", 0 ) == 1) then
		EnableMission("scenarios\\scenariomissions\\ussr\\finland\\1");
	end;

	SetIGlobalVar( "Chapter.IsFirst", 1);

	EnableMission("scenarios\\scenariomissions\\ussr\\intro\\1");
-- adding units to start with
	AddNewSlot ("BA10");
	AddNewSlot ("T-26");
	AddNewSlot ("T-26");

	AddNewSlot ("122-mm_M-30");
	AddNewSlot ("122-mm_M-30");
	AddNewSlot ("122-mm_M-30");

-- adding units to depot
	AddBaseUpgrade( "45_mm_M_37" );
	AddBaseUpgrade( "122-mm_M-30" );
	AddBaseUpgrade( "T-26" );
	AddBaseUpgrade( "BA10" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\finland\\1.Finished", 0);

	if ( strMissionName == "scenarios\scenariomissions\ussr\intro\1") then
		EnableMission("scenarios\\scenariomissions\\ussr\\finland\\1");
-- allow scenario mission further
		SetUserProfileVar( "ScenarioTutorial.Passed", 1 );
	end;

	if ( strMissionName == "scenarios\scenariomissions\ussr\finland\1") then
		EnableChapter("scenarios\\chapters\\ussr\\leningrad\\1");
-- Medal Conditions(Redbanner)
		AddMedal("Medals\\USSR\\redflag\\1", 0);
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("T-26");
		AddNewSlot ("T-26");
		AddNewSlot ("T-26");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("45_mm_M_37");
		AddNewSlot ("45_mm_M_37");
		AddNewSlot ("45_mm_M_37");
	end;
end;
