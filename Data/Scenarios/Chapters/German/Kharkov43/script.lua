function EnterChapter( strChapterName )

-- adding base upgrades
	AddBaseUpgrade( "6-barrel 15cm Nebelwerfer 41" );
	AddBaseUpgrade( "Wespe_SdKfz124" );
	AddBaseUpgrade( "Panzerjager_Nashorn_Sdkfz164" );
	AddBaseUpgrade( "15cmK39" );

	SetIGlobalVar("template.hunted.killed", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\german\\kharkov43\\1.Finished", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\german\\kharkov43\\1");
	end;

	if ( strMissionName == "scenarios\scenariomissions\german\kharkov43\1") then
		EnableChapter("scenarios\\chapters\\german\\ardennes\\1");
--Medal Conditions
		AddMedal("Medals\\German\\Kuban\\1", 2);
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
	if ((GetStatisticsValue (3, 0) <= 800) and (HasMedal("Medals\\German\\tank_as1\\1") == 0)) then
		AddMedal("Medals\\German\\tank_as1\\1", 4);
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
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 2) then
		AddNewSlot ("15_cm_sFH18");
		AddNewSlot ("15_cm_sFH18");
		AddNewSlot ("15_cm_sFH18");
	end;

	if ( iLevel == 1) then
		AddNewSlot ("Pz_Kpfw_IV_Ausf_G");
		AddNewSlot ("Pz_Kpfw_IV_Ausf_G");
		AddNewSlot ("Pz_Kpfw_IV_Ausf_G");
	end;
end;
