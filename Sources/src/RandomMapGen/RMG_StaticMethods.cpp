#include "stdafx.h"

#include "RMG_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SRMPlacedPatch::GetAndRemoveClosestVSOPoints( int nType,
																									 SRMPlacedPatch &rBeginPlacedPatch,
																									 SRMPlacedPatch &rEndPlacedPatch,
																									 const std::string &rVSODescFileName,
																									 int nPointSelectionType,
																									 SVSOPoint *pBeginVSOPoint,
																									 SVSOPoint *pEndVSOPoint )
{
	NI_ASSERT_TF( pBeginVSOPoint != 0,
							  NStr::Format( "SRMPlacedPatch::GetAndRemoveClosestVSOPoints, Invalid parameter: pBeginVSOPoint: %x\n", pBeginVSOPoint ),
							  return false );
	NI_ASSERT_TF( pEndVSOPoint != 0,
							  NStr::Format( "SRMPlacedPatch::GetAndRemoveClosestVSOPoints, Invalid parameter: pEndVSOPoint: %x\n", pEndVSOPoint ),
							  return false );

	if ( ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rBeginPlacedPatch.riversPoints.empty() : rBeginPlacedPatch.roadsPoints.empty() ) ||
			 ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rEndPlacedPatch.riversPoints.empty() : rEndPlacedPatch.roadsPoints.empty() ) )
	{
		return false;
	}
	
	bool bOneMismatch = false;
	
	//����� �� ����� �����
	std::list<std::list<SVSOPoint>::iterator> beginIterators;
	for ( std::list<SVSOPoint>::iterator pointIterator = ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rBeginPlacedPatch.riversPoints.begin() : rBeginPlacedPatch.roadsPoints.begin() );
				pointIterator != ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rBeginPlacedPatch.riversPoints.end() : rBeginPlacedPatch.roadsPoints.end() );
				++pointIterator )
	{
		std::string szStringToCompare0 = pointIterator->szVSODescFileName;
		std::string szStringToCompare1 = rVSODescFileName;
		NStr::ToLower( szStringToCompare0 );
		NStr::ToLower( szStringToCompare1 );
		if ( szStringToCompare0 == szStringToCompare1 )
		{
			beginIterators.push_back( pointIterator );
		}
	}
	if ( beginIterators.empty() )
	{
		if ( nPointSelectionType == PST_TWO )
		{
			return false;
		}
		else
		{
			bOneMismatch = true;
			for ( std::list<SVSOPoint>::iterator pointIterator = ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rBeginPlacedPatch.riversPoints.begin() : rBeginPlacedPatch.roadsPoints.begin() );
						pointIterator != ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rBeginPlacedPatch.riversPoints.end() : rBeginPlacedPatch.roadsPoints.end() );
						++pointIterator )
			{
				beginIterators.push_back( pointIterator );
			}
		}
	}
	
	//����� �� ������ �����
	std::list<std::list<SVSOPoint>::iterator> endIterators;
	for ( std::list<SVSOPoint>::iterator pointIterator = ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rEndPlacedPatch.riversPoints.begin() : rEndPlacedPatch.roadsPoints.begin() );
				pointIterator != ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rEndPlacedPatch.riversPoints.end() : rEndPlacedPatch.roadsPoints.end() );
				++pointIterator )
	{
		std::string szStringToCompare0 = pointIterator->szVSODescFileName;
		std::string szStringToCompare1 = rVSODescFileName;
		NStr::ToLower( szStringToCompare0 );
		NStr::ToLower( szStringToCompare1 );
		if ( szStringToCompare0 == szStringToCompare1 )
		{
			endIterators.push_back( pointIterator );
		}
	}
	if ( endIterators.empty() )
	{
		if ( nPointSelectionType == PST_TWO )
		{
			return false;
		}
		else if ( ( nPointSelectionType == PST_ONE ) && bOneMismatch )
		{
			return false;
		}
		else
		{
			bOneMismatch = true;
			for ( std::list<SVSOPoint>::iterator pointIterator = ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rEndPlacedPatch.riversPoints.begin() : rEndPlacedPatch.roadsPoints.begin() );
						pointIterator != ( ( nType != SRMGraphLink::TYPE_ROAD ) ? rEndPlacedPatch.riversPoints.end() : rEndPlacedPatch.roadsPoints.end() );
						++pointIterator )
			{
				endIterators.push_back( pointIterator );
			}
		}
	}
	
	NI_ASSERT_T( ( !beginIterators.empty() ) && ( ! endIterators.empty() ),
							 NStr::Format( "SRMPlacedPatch::GetAndRemoveClosestVSOPoints, Can't get points, desc: %s", rVSODescFileName.c_str() ) );

	//������� ��������� �����
	std::list<SVSOPoint>::iterator beginIteratorToReturn = beginIterators.front();
	std::list<SVSOPoint>::iterator endIteratorToReturn = endIterators.front();
	for ( std::list<std::list<SVSOPoint>::iterator>::iterator beginIterator = beginIterators.begin(); beginIterator != beginIterators.end(); ++beginIterator )
	{
		for ( std::list<std::list<SVSOPoint>::iterator>::iterator endIterator = endIterators.begin(); endIterator != endIterators.end(); ++endIterator )
		{
			if ( fabs2( beginIteratorToReturn->vPos - endIteratorToReturn->vPos ) > fabs2( ( *beginIterator )->vPos - ( *endIterator )->vPos ) )
			{
				beginIteratorToReturn = ( *beginIterator );
				endIteratorToReturn = ( *endIterator );
			}
		}		
	}
	
	//�������� ������������ ��� �����
	( *pBeginVSOPoint ) = ( *beginIteratorToReturn );
	( *pEndVSOPoint ) = ( *endIteratorToReturn );
	
	//������� �� �� ����������� ������������
	//������� �� �� ����������� ������������
	if( nType != SRMGraphLink::TYPE_ROAD )
	{
		rBeginPlacedPatch.riversPoints.erase( beginIteratorToReturn );
		rEndPlacedPatch.riversPoints.erase( endIteratorToReturn );
	}
	else
	{
		rBeginPlacedPatch.roadsPoints.erase( beginIteratorToReturn );
		rEndPlacedPatch.roadsPoints.erase( endIteratorToReturn );
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string SRMTemplateUnitsTable::GetUnitRPGMnemonic( DWORD nUnitRPGType )
{
	int nUnitRPGTypeIndex = UnitRPGTypeToIndex( nUnitRPGType );
	if ( nUnitRPGTypeIndex < 0 )
	{
		return std::string( "" );
	}
	else
	{
		return UNIT_RPG_MNEMONICS[nUnitRPGTypeIndex];
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD SRMTemplateUnitsTable::GetUnitRPGType( const std::string &rszUnitRPGMnenonic )
{
	std::string szToCompare = rszUnitRPGMnenonic;
	NStr::ToLower( szToCompare );
	int nUnitRPGTypeIndex = UnitRPGMnemonicToIndex( szToCompare );
	if ( nUnitRPGTypeIndex < 0 )
	{
		return INVALID_UNIT_RPG_TYPE;
	}	
	else
	{
		return UNIT_RPG_TYPES[nUnitRPGTypeIndex];
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTemplateUnitsTable::UnitRPGTypeToIndex( DWORD nUnitRPGType )
{
	if ( unitRPGTypeToIndex.empty() )
	{
		for ( int nUnitRPGTypeIndex = 0; nUnitRPGTypeIndex < UNIT_RPG_TYPE_COUNT; ++nUnitRPGTypeIndex )
		{
			unitRPGTypeToIndex[UNIT_RPG_TYPES[nUnitRPGTypeIndex] ] = nUnitRPGTypeIndex;
		}
	}
	
	std::unordered_map<DWORD, int>::const_iterator indexIterator = unitRPGTypeToIndex.find( nUnitRPGType );
	if ( indexIterator == unitRPGTypeToIndex.end() )
	{
		return -1;
	}
	else
	{
		return indexIterator->second;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTemplateUnitsTable::UnitRPGMnemonicToIndex( const std::string &rszUnitRPGMnenonic )
{
	if ( unitRPGMnemonicToIndex.empty() )
	{
		for ( int nUnitRPGTypeIndex = 0; nUnitRPGTypeIndex < UNIT_RPG_TYPE_COUNT; ++nUnitRPGTypeIndex )
		{
			unitRPGMnemonicToIndex[UNIT_RPG_MNEMONICS[nUnitRPGTypeIndex] ] = nUnitRPGTypeIndex;
		}
	}
	
	std::unordered_map<std::string, int>::const_iterator indexIterator = unitRPGMnemonicToIndex.find( rszUnitRPGMnenonic );
	if ( indexIterator == unitRPGMnemonicToIndex.end() )
	{
		return -1;
	}
	else
	{
		return indexIterator->second;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

