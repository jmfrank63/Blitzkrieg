function EnterChapter( strChapterName )

-- adding units to depot
	AddBaseUpgrade( "152_mm_D1" );
	AddBaseUpgrade( "T-34-85" );
	AddBaseUpgrade( "BM_31_12" );
	AddBaseUpgrade( "152-mm_ML-20" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\rumania\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\ussr\\rumania\\1");
	end;

local iStat = GetStatisticsValue(17, 2); -- time elapsed

	if ( strMissionName == "scenarios\scenariomissions\ussr\rumania\1") then
		EnableChapter("scenarios\\chapters\\ussr\\german\\1");
--Medal conditions(Redstar)
		AddMedal("Medals\\USSR\\Redstar\\1", 4);
--Medal Conditions(Kutuzov)
		if (( (iStat <= 60) and (iStat >= 40)) and (HasMedal ("Medals\\USSR\\Kut3\\1") == 0) and (HasMedal ("Medals\\USSR\\Kut2\\1") == 0) and
		  (HasMedal ("Medals\\USSR\\Kut1\\1") == 0)) then
			AddMedal("Medals\\USSR\\Kut3\\1", 3);
		else
		if (( (iStat <= 39) and (iStat >= 20)) and (HasMedal ("Medals\\USSR\\Kut1\\1") == 0) and (HasMedal ("Medals\\USSR\\Kut2\\1") == 0)) then
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
		AddNewSlot ("T-34-85");
		AddNewSlot ("T-34-85");
		AddNewSlot ("T-34-85");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("152_mm_D1");
		AddNewSlot ("152_mm_D1");
		AddNewSlot ("152_mm_D1");
	end;
end;
