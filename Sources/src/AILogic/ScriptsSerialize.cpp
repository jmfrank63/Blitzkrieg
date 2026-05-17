#include "stdafx.h"

#include "Scripts\Scripts.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CScripts *pScripts;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SScriptInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &period );
	saver.Add( 2, &lastUpdate );
	saver.Add( 3, &szName );
	saver.Add( 4, &nRepetitions );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::SReinforcementObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &mapObject );
	saver.Add( 2, &pStats );
	saver.Add( 3, &pScenarioUnit );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CScripts::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		pScripts = this;
		// �������� lua state
		script.Clear();
		script.Init();
	}

	if ( saver.IsReading() )
		pConsole = GetSingleton<IConsoleBuffer>();

	saver.Add( 3, &groups );
//	saver.Add( 7, &name2script );										// ��������� �� �����
	saver.Add( 8, &szScriptFile );

	// ��������� ���������� �� �������� ��������
	if ( !saver.IsReading() )
	{
		std::list<SScriptInfo> activeScriptsInfo;
		for ( std::unordered_map<int, SScriptInfo>::iterator iter = activeScripts.begin(); iter != activeScripts.end(); ++iter )
			activeScriptsInfo.push_back( iter->second );

		saver.Add( 9, &activeScriptsInfo );
	}
	else
	// ������������ ��������� lua	
	{
		std::list<SScriptInfo> activeScriptsInfo;
		saver.Add( 9, &activeScriptsInfo );

		activeScripts.clear();
		name2script.clear();

		if ( ReadScriptFile() )
		{
			for ( std::list<SScriptInfo>::iterator iter = activeScriptsInfo.begin(); iter != activeScriptsInfo.end(); ++iter )
			{
				script.GetGlobal( iter->szName.c_str() );
				const int nScriptRef = script.Ref( 1 );

				name2script[iter->szName] = nScriptRef;

				activeScripts[nScriptRef].szName = iter->szName;
				activeScripts[nScriptRef].period = iter->period;
				activeScripts[nScriptRef].lastUpdate = iter->lastUpdate;
				activeScripts[nScriptRef].nRepetitions = iter->nRepetitions;
			}
		}
	}

	saver.Add( 10, &areas );

	saver.Add( 11, &groupUnits );
	saver.Add( 12, &reinforcs );

	saver.Add( 13, &suspendedReinforcs );
	saver.Add( 14, &lastTimeToCheckSuspendedReinforcs );
	
	if ( saver.IsReading() )
		reinforcsIter = suspendedReinforcs.begin();

	saver.Add( 15, &reservePositions );
	saver.Add( 16, &bShowErrors );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
