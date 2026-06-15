function EnterChapter( strChapterName )

	if ( GetUserProfileVar( "ScenarioTutorial.Passed", 0 ) == 1) then
		EnableMission("scenarios\\scenariomissions\\german\\poland\\1");
	end;

	SetIGlobalVar( "Chapter.IsFirst", 1);

	EnableMission("scenarios\\scenariomissions\\german\\intro\\1");

-- Adding start units
	AddNewSlot ("Sdkfz_222");
	AddNewSlot ("Pz_Kpfw_II_Ausf_F");
	AddNewSlot ("Pz_Kpfw_II_Ausf_F");

	AddNewSlot ("10_5mmLeFh18");
	AddNewSlot ("10_5mmLeFh18");
	AddNewSlot ("10_5mmLeFh18");

-- adding base upgrades
	AddBaseUpgrade( "5_cm_Pak38" );
	AddBaseUpgrade( "10_5mmLeFh18" );
	AddBaseUpgrade( "Pz_Kpfw_II_Ausf_F" );
	AddBaseUpgrade( "Sdkfz_222" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\german\\poland\\1.Finished", 0);

	if ( strMissionName == "scenarios\scenariomissions\german\intro\1") then
		EnableMission("scenarios\\scenariomissions\\german\\poland\\1");
-- allow scenario mission further
		SetUserProfileVar( "ScenarioTutorial.Passed", 1 );
	end;

	if ( strMissionName == "scenarios\scenariomissions\german\poland\1") then
		EnableChapter("scenarios\\chapters\\german\\france\\1");
--Medal Conditions
		AddMedal("Medals\\German\\kvz3\\1", 0);
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 2) then
		AddNewSlot ("10_5mmLeFh18");
		AddNewSlot ("10_5mmLeFh18");
		AddNewSlot ("10_5mmLeFh18");
	end;

	if ( iLevel == 1) then
		AddNewSlot ("Pz_Kpfw_II_Ausf_F");
		AddNewSlot ("Pz_Kpfw_II_Ausf_F");
		AddNewSlot ("Pz_Kpfw_II_Ausf_F");
	end;
end;
