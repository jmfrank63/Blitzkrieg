function EnterChapter( strChapterName )

-- adding units to depot
	AddBaseUpgrade( "37_mm_61_K" );
	AddBaseUpgrade( "BT-5" );
	AddBaseUpgrade( "BT-7" );

	SetIGlobalVar("template.hunted.killed", 0);

	SetIGlobalVar( "RandomMissions.Count", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\leningrad\\1.Finished", 0);
local num = GetIGlobalVar( "RandomMissions.Count", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\ussr\\leningrad\\1");
		num = num + 1;
		if ( num == 3) then
			SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 1);
		else
			SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 0);
		end;
		SetIGlobalVar( "RandomMissions.Count", num);
	end;

	if ( strMissionName == "scenarios\scenariomissions\ussr\leningrad\1") then
		EnableChapter("scenarios\\chapters\\ussr\\moscow\\1");
		SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 0);
	end;
--Medal Conditions(GoldMedal)
	if ((GetStatisticsValue(1, 0) >= 5000) and (HasMedal("Medals\\USSR\\GoldStar\\1") == 0)) then
		AddMedal("Medals\\USSR\\GoldStar\\1", 5);
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("BT-7");
		AddNewSlot ("BT-7");
		AddNewSlot ("BT-7");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("45_mm_M_37");
		AddNewSlot ("45_mm_M_37");
		AddNewSlot ("45_mm_M_37");
	end;
end;
