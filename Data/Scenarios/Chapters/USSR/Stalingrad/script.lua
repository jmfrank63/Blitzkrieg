function EnterChapter( strChapterName )

-- adding units to depot
	AddBaseUpgrade( "57-mm ZIS-2" );
	AddBaseUpgrade( "76-mm ZIS-3" );
	AddBaseUpgrade( "85-mm_52-K" );
	AddBaseUpgrade( "BM_13" );
	AddBaseUpgrade( "T-70" );
	AddBaseUpgrade( "MKII_Matilda_Lend_Lease" );
	AddBaseUpgrade( "122-mm_A-19" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\stalingrad\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\ussr\\stalingrad\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\ussr\stalingrad\1") then
		EnableChapter("scenarios\\chapters\\ussr\\kursk\\1");
--Medal conditions(for Stalingrad)
		AddMedal("Medals\\USSR\\Stalingrad\\1", 1);
	end;
--Medal Conditions(GoldMedal)
	if ((GetStatisticsValue(1, 0) >= 5000) and (HasMedal("Medals\\USSR\\GoldStar\\1") == 0)) then
		AddMedal("Medals\\USSR\\GoldStar\\1", 5);
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("T_34_76");
		AddNewSlot ("T_34_76");
		AddNewSlot ("T_34_76");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("122-mm_M-30");
		AddNewSlot ("122-mm_M-30");
		AddNewSlot ("122-mm_M-30");
	end;
end;
