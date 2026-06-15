function EnterChapter( strChapterName )

-- adding units to depot
	AddBaseUpgrade( "76_2mm_F22" );
	AddBaseUpgrade( "T_34_76" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\moscow\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\ussr\\moscow\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\ussr\moscow\1") then
		EnableChapter("scenarios\\chapters\\ussr\\stalingrad\\1");
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
		AddNewSlot ("76_2mm_F22");
		AddNewSlot ("76_2mm_F22");
		AddNewSlot ("76_2mm_F22");
	end;
end;
