function EnterChapter( strChapterName )

-- adding units to depot
	AddBaseUpgrade( "M4A2_Sherman_Lend_Lease" );
	AddBaseUpgrade( "JSU_122" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\germany\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\ussr\\germany\\1");
	end;

--Medal Conditions(GoldMedal)
	if ((GetStatisticsValue(1, 0) >= 5000) and (HasMedal("Medals\\USSR\\GoldStar\\1") == 0)) then
		AddMedal("Medals\\USSR\\GoldStar\\1", 5);
	end;

-- end of campaign
	if ( strMissionName == "scenarios\scenariomissions\ussr\germany\1") then
		FinishCampaign();
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("JS_2");
		AddNewSlot ("JS_2");
		AddNewSlot ("JS_2");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("JSU_122");
		AddNewSlot ("JSU_122");
		AddNewSlot ("JSU_122");
	end;
end;
