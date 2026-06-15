function EnterChapter( strChapterName )

-- adding units to depot
	AddBaseUpgrade( "MKIII_Valentine_Lend_Lease" );
	AddBaseUpgrade( "T-34" );
	AddBaseUpgrade( "SU_76" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\ussr\\kursk\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\ussr\\kursk\\1");
	end;

local kills = GetStatisticsValue(2, 2); -- AIprice
local looses = GetStatisticsValue(4, 2); -- AIprice

	if ( strMissionName == "scenarios\scenariomissions\ussr\kursk\1") then
		EnableChapter("scenarios\\chapters\\ussr\\ukraine\\1");
--Medal Conditions(Suvorov)
		if ( (kills/(looses + 1)) >= 4) then
			AddMedal("Medals\\USSR\\Suv1\\1", 2);
		else
		if ( (kills/(looses + 1)) >= 2) then
			AddMedal("Medals\\USSR\\Suv2\\1", 2);
		else
		if ( (kills/(looses + 1)) >= 1) then
			AddMedal("Medals\\USSR\\Suv3\\1", 2);
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
