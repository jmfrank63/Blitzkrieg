function EnterChapter( strChapterName )

-- adding units to depot
	AddBaseUpgrade( "SU_85" );
	AddBaseUpgrade( "SU_152" );
	AddBaseUpgrade( "BM_8-48" );
	AddBaseUpgrade( "KV_1C" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\ukraine\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\ussr\\ukraine\\1");
	end;

local iStat = GetStatisticsValue(17, 2); -- time elapsed

	if ( strMissionName == "scenarios\scenariomissions\ussr\ukraine\1") then
		EnableChapter("scenarios\\chapters\\ussr\\rumania\\1");
--Medal Conditions(Kutuzov)
		if (( (iStat <= 60) and (iStat >= 40)) and (HasMedal ("Medals\\USSR\\Kut3\\1") == 0)) then
			AddMedal("Medals\\USSR\\Kut3\\1", 3);
		else
		if (( (iStat <= 39) and (iStat >= 20)) and (HasMedal ("Medals\\USSR\\Kut2\\1") == 0)) then
			AddMedal("Medals\\USSR\\Kut2\\1", 3);
		else
		if (( (iStat <= 19) and (iStat >= 0)) and (HasMedal ("Medals\\USSR\\Kut1\\1") == 0)) then
			AddMedal("Medals\\USSR\\Kut1\\1", 3);
		end;
		end;
		end;
	end;

--Medal Conditions(GoldMedal)
	if ((GetStatisticsValue(1, 0) >= 5000) and (HasMedal("Medals\\USSR\\GoldStar\\1") == 0)) then
		AddMedal("Medals\\USSR\\GoldStar\\1", 5);
	end;

end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("T-34");
		AddNewSlot ("T-34");
		AddNewSlot ("T-34");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("122-mm_M-30");
		AddNewSlot ("122-mm_M-30");
		AddNewSlot ("122-mm_M-30");
	end;
end;
