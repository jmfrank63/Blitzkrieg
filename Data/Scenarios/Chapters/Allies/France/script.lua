function EnterChapter( strChapterName )

-- adding depot upgrades
	AddBaseUpgrade( "S35_Somua_France" );
	AddBaseUpgrade( "4_5_inch_Gun_GB" );

	SetIGlobalVar("template.hunted.killed", 0);

	SetIGlobalVar( "RandomMissions.Count", 0);
end;

function MissionFinished( strMissionName )
local bScenario = GetIGlobalVar("Mission.scenarios\\scenariomissions\\allies\\france\\1.Finished", 0);
local num = GetIGlobalVar( "RandomMissions.Count", 0);

	if ( (GetIGlobalVar("Mission.Current.Random", 0) == 1) and (bScenario == 0)) then
		EnableMission("scenarios\\scenariomissions\\allies\\france\\1");
		num = num + 1;
		if ( num == 3) then
			SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 1);
		else
			SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 0);
		end;
		SetIGlobalVar( "RandomMissions.Count", num);
	end;

	if ( strMissionName == "scenarios\scenariomissions\allies\france\1") then
		EnableChapter("scenarios\\chapters\\allies\\tobruk\\1");
		SetIGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 0);
	end;
end;

function PlayerGainLevel( iLevel )
	if ( iLevel == 1) then
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
		AddNewSlot ("25pdr_QFH_GB");
	end;

	if ( iLevel == 2) then
		AddNewSlot ("S35_Somua_France");
		AddNewSlot ("S35_Somua_France");
		AddNewSlot ("S35_Somua_France");
	end;
end;
