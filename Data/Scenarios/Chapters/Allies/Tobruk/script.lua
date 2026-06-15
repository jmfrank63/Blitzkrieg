function EnterChapter( strChapterName )
	ChangeCurrent( "S35_Somua_France", "Infantry_MKII_Matilda_GB");
	ChangeDefault( "S35_Somua_France", "Infantry_MKII_Matilda_GB");

-- adding depot upgrades
	RemoveBaseUpgrade( "S35_Somua_France" );
	AddBaseUpgrade( "40mm_Bofors_GB" );
	AddBaseUpgrade( "6pdr_QFG_GB" );
	AddBaseUpgrade( "Infantry_MKII_Matilda_GB" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\allies\\tobruk\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\allies\\tobruk\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\allies\tobruk\1") then
		EnableChapter("scenarios\\chapters\\allies\\tunis\\1");
--Medal Conditions(Gcross)
		if (GetStatisticsValue(3, 0) <= 400) then
			AddMedal("Medals\\Allies\\Gcross\\1", 1);
		end;
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("Infantry_MKII_Matilda_GB");
		AddNewSlot ("Infantry_MKII_Matilda_GB");
		AddNewSlot ("Infantry_MKII_Matilda_GB");
	end;
end;
