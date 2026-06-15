function EnterChapter( strChapterName )
	ChangeCurrent ("Pz_38t_Ausf_A", "Pz_Kpfw_III_Ausf_E"); -- no Pz38t in Africa
	ChangeDefault ("Pz_38t_Ausf_A", "Pz_Kpfw_III_Ausf_E");
	ChangeCurrent ("Pz_Kpfw_II_Ausf_F", "Pz_Kpfw_III_Ausf_E"); -- no Pz II F in Africa
	ChangeDefault ("Pz_Kpfw_II_Ausf_F", "Pz_Kpfw_III_Ausf_E");

-- adding base upgrades
	RemoveBaseUpgrade( "Pz_38t_Ausf_A" ); -- no Pz38t in Africa
	RemoveBaseUpgrade( "Pz_Kpfw_II_Ausf_F" ); -- no Pz II F in Africa
	AddBaseUpgrade( "Pz_Kpfw_III_Ausf_E" );
	AddBaseUpgrade( "Sturmgeschutz_III_Ausf_A_B_C" );
	AddBaseUpgrade( "10_cm_ K_18" );
	AddBaseUpgrade( "8.8-cm FlaK18" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\german\\africa\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\german\\africa\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\german\africa\1") then
		EnableChapter("scenarios\\chapters\\german\\barbarossa\\1");
--Medal Conditions(Africa)
		AddMedal("Medals\\German\\Africa\\1", 1);
--Medal Conditions
		if (GetStatisticsValue(5, 0) >= 10) then -- artillery captured
			AddMedal("Medals\\German\\kvz1\\1", 0);
       		end;
	end;
--Medal Conditions(Iron Cross)
local var1 = GetStatisticsValue(1, 0);
	if ((var1 >= 1000) and (var1 < 2000) and (HasMedal("Medals\\German\\krest5\\1") == 0)) then
		AddMedal("Medals\\German\\krest5\\1", 3);
	else
	if ((var1 >= 2000) and (var1 < 3000) and (HasMedal("Medals\\German\\krest4\\1") == 0)) then
		AddMedal("Medals\\German\\krest4\\1", 3);
	else
	if ((var1 >= 3000) and (var1 < 4000) and (HasMedal("Medals\\German\\krest3\\1") == 0)) then
		AddMedal("Medals\\German\\krest3\\1", 3);
	else
	if ((var1 >= 4000) and (var1 < 5000) and (HasMedal("Medals\\German\\krest2\\1") == 0)) then
		AddMedal("Medals\\German\\krest2\\1", 3);
	else
	if ((var1 >= 5000) and (HasMedal("Medals\\German\\krest1\\1") == 0)) then
		AddMedal("Medals\\German\\krest1\\1", 3);
	end;
	end;
	end;
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
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 2) then
		AddNewSlot ("10_5mmLeFh18");
		AddNewSlot ("10_5mmLeFh18");
		AddNewSlot ("10_5mmLeFh18");
	end;

	if ( iLevel == 1) then
		AddNewSlot ("Pz_Kpfw_III_Ausf_E");
		AddNewSlot ("Pz_Kpfw_III_Ausf_E");
		AddNewSlot ("Pz_Kpfw_III_Ausf_E");
	end;
end;
