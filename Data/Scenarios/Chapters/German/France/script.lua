function EnterChapter( strChapterName )

-- adding base upgrades
	AddBaseUpgrade( "Sdkfz_231" );
	AddBaseUpgrade( "Pz_38t_Ausf_A" );

	SetIGlobalVar("template.hunted.killed", 0);

	SetIGlobalVar( "RandomMissions.Count", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\german\\france\\1.Finished", 0);
local num = GetIGlobalVar( "RandomMissions.Count", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\german\\france\\1");
		num = num + 1;
		if ( num == 3) then
			SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 1);
		else
			SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 0);
		end;
		SetIGlobalVar( "RandomMissions.Count", num);
	end;

	if ( strMissionName == "scenarios\scenariomissions\german\france\1") then
		EnableChapter("scenarios\\chapters\\german\\africa\\1");
		SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 0);
--Medal Conditions
		if (GetStatisticsValue(19, 0) == 0) then -- objectives failed
			AddMedal("Medals\\German\\kvz2\\1", 0);
		end;
	end;
--Medal Conditions(assault)
local var = GetNumMissions(0, 0);
	if ((var >= 40) and (HasMedal("Medals\\German\\as1\\1") == 0)) then
		AddMedal("Medals\\German\\as1\\1", 5);
	else
	if ((var >= 30) and (var < 40) and (HasMedal("Medals\\German\\as2\\1") == 0)) then
		AddMedal("Medals\\German\\as2\\1", 5);
	else
	if ((var >= 20) and (var < 30) and (HasMedal("Medals\\German\\as3\\1") == 0)) then
		AddMedal("Medals\\German\\as3\\1", 5);
	else
	if ((var >= 10) and (var < 20) and (HasMedal("Medals\\German\\as4\\1") == 0)) then
		AddMedal("Medals\\German\\as4\\1", 5);
	end;
	end;
	end;
	end;
--Medal Conditions(tank_assault)
	if ((GetStatisticsValue (3, 0) <= 200) and (HasMedal("Medals\\German\\tank_as4\\1") == 0)) then
		AddMedal("Medals\\German\\tank_as4\\1", 4);
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
