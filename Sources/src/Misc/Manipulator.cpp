#include "StdAfx.h"

#include "Manipulator.h"

#include <comdef.h>
bool CPropertiesRegister::AddProperty( const std::string &szRegister, const std::string &szName, SBaseProperty *pPropertyDesc )
{
	NProperty::SProperties &properties = registers[szRegister];
	NI_ASSERT_SLOW_TF( properties.GetProperty(szName) == 0, NStr::Format("property \"%s\" already exist in the register \"%s\"", szRegister.c_str(), szName.c_str()), return false );
	properties.propMap[szName] = pPropertyDesc;
	properties.propList.push_back( NProperty::SPropDesc(szName, pPropertyDesc) );
	return true;
}
SBaseProperty* CPropertiesRegister::GetProperty( const std::string &szRegister, const std::string &szName )
{
	NProperty::SProperties *pRegister = GetProperties( szRegister );
	NI_ASSERT_SLOW_TF( pRegister != 0, NStr::Format("Can't find register \"%s\"", szRegister.c_str()), return 0 );

	SBaseProperty *pProp = pRegister->GetProperty( szName );

	return pProp;
}
bool CManipulator::DoWeNeedFillProps() const
{
	NI_ASSERT_TF( pRegister != 0, "set register first!!!", return false );
	return !pRegister->HasRegister( szRegister );
}
const SPropertyDesc* CManipulator::FillTempProps( const char *pszName, const SBaseProperty *pProp )
{
	if ( (pszName == 0) || (pProp == 0) )
		return 0;

	tempPropDesc.pszName = pszName;
	tempPropDesc.nType = pProp->nType;
	tempPropDesc.values = pProp->values;
	tempPropDesc.ePropType = pProp->ePropType;
	return &tempPropDesc;
}
const SPropertyDesc* STDCALL CManipulator::GetPropertyDesc( const char *pszName ) 
{
	std::string szRestName;
	int nIndex = -1;
	const SBaseProperty *pProperty = GetProperty( pszName, &szRestName, &nIndex );
	NI_ASSERT_TF( pProperty != 0, NStr::Format("Can't find property \"%s\" in the register \"%s\" during direct call", pszName, szRegister.c_str()), return 0 );
	if ( pProperty->nodeType != SBaseProperty::LEAF )
	{
		CPtr<IManipulator> pMan = GetPropertyAsManipulator( const_cast<SBaseProperty*>(pProperty), nIndex );
		if ( pMan != 0 )
		{
			if ( const SPropertyDesc *pDesc = pMan->GetPropertyDesc(szRestName.c_str()) )
			{
				tempPropDesc = *pDesc;
				return &tempPropDesc;
			}
		}
		return 0;
	}
	else
		return FillTempProps( pszName, pProperty );
}
SBaseProperty* CManipulator::GetProperty( const std::string &szFullName, std::string *pRestName, int *pnIndex )
{
	std::string &szRest = *pRestName;
	std::string szName;

	szRest = szFullName;
	SBaseProperty *pProp = 0;
	while ( 1 )
	{
		int nPos = szRest.find_first_of( ".[" );
		szName += szRest.substr( 0, nPos );

		pProp = GetProperty( szName );

		if ( nPos == std::string::npos )
		{
			szRest.clear();
			break;
		}
		szRest = szRest.substr( nPos + 1 );
		if ( pProp != 0 ) 
			break;

		szName += '.';
	}
	if ( pProp == 0 )
		return 0;
	if ( pProp->nodeType == SBaseProperty::VECTOR )
	{
		int nPos = szRest.find( ']' );
		NI_ASSERT_TF( nPos != std::string::npos, NStr::Format("VECTOR property \"%s\" must have an index in the form of '[x]'", szName.c_str()), return false );
		std::string szIndex = szRest.substr( 0, nPos );
		NI_ASSERT_TF( NStr::IsDecNumber( szIndex ), NStr::Format("index for VECTOR property \"%s\" must be a decimal number", szName.c_str()), return false );
		int nIndex = NStr::ToInt( szIndex );
		nPos = szRest.find( '.' );
		NI_ASSERT_TF( nPos != std::string::npos, NStr::Format("VECTOR property \"%s\" must have rest name, separated by '.'", szName.c_str()), return false );
		szRest = szRest.substr( nPos + 1 );
		*pnIndex = nIndex;
	}
	else if ( pProp->nodeType == SBaseProperty::SINGLE )
		*pnIndex = -1;
	return pProp;
}
IManipulator* CManipulator::GetPropertyAsManipulator( SBaseProperty *pProp, int nIndex )
{
	variant_t varMan;
	if ( pProp->Get( this, &varMan, nIndex ) == false )
	{
		NI_ASSERT_TF( false, "Can't find manipulator for property", return 0 );
		varMan.vt = VT_EMPTY;
		return 0;
	}
	else
	{
		IManipulator* pMan = reinterpret_cast<IManipulator*>( varMan.byref );
		NI_ASSERT_TF( pMan != 0 && dynamic_cast<IManipulator*>(pMan) != 0, "returned value for property is not a manipulator", return 0 );
		varMan.vt = VT_EMPTY;
		return pMan;
	}
}
bool CManipulator::GetValue( const char *pszValueName, variant_t *pValue )
{
	NI_ASSERT_T( pszValueName != 0, "Value name must be != 0" );
	NI_ASSERT_T( strlen(pszValueName) > 0, "length of the value name must be > 0" );

	std::string szRest;
	int nIndex;
	SBaseProperty *pProp = GetProperty( pszValueName, &szRest, &nIndex );
	
	if ( pProp == 0 )
		return false;
	
	if ( pProp->nodeType == SBaseProperty::LEAF )
		return pProp->Get( this, pValue );
	else
	{
		CPtr<IManipulator> pMan = GetPropertyAsManipulator( pProp, nIndex );
		return pMan != 0 ? pMan->GetValue( szRest.c_str(), pValue ) : 0;
	}
}
bool CManipulator::SetValue( const char *pszValueName, const variant_t &value )
{
	NI_ASSERT_T( pszValueName != 0, "Value name must be != 0" );
	NI_ASSERT_T( strlen(pszValueName) > 0, "length of the value name must be > 0" );

	std::string szRest;
	int nIndex;
	SBaseProperty *pProp = GetProperty( pszValueName, &szRest, &nIndex );

	if ( pProp == 0 )
		return false;
	
	if ( pProp->nodeType == SBaseProperty::LEAF )
		return pProp->Set( this, value );
	else
	{
		CPtr<IManipulator> pMan = GetPropertyAsManipulator( pProp, nIndex );
		return pMan != 0 ? pMan->SetValue( szRest.c_str(), value ) : 0;
	}
}

IManipulatorIterator* CManipulator::Iterate()
{
	return new CManipulatorIterator( this );
}
const SPropertyDesc* CManipulatorIterator::GetFirst()
{
	if ( !pPropLocal )
		return 0;
	if ( (pPropLocal->nodeType == SBaseProperty::SINGLE) && (nPropIndexLocal != -1) )
		return 0;
	if ( (pPropLocal->nodeType == SBaseProperty::VECTOR) && (nPropIndexLocal < 0) )
		return 0;
	CPtr<IManipulator> pMan;
	{
		variant_t var;
		pPropLocal->Get( pManipulator, &var, nPropIndexLocal );
		pMan = reinterpret_cast<IManipulator*>( var.byref );
		var.vt = VT_EMPTY;
	}
	if ( pMan == 0 )
		return 0;
	pItLocal = pMan->Iterate();
	if ( pItLocal->IsEnd() )
		return GetNext();
	else
	{
		const SPropertyDesc *pDesc = pItLocal->GetPropertyDesc();
		propDesc = *pDesc;
		szPropName = szBaseName + (pPropLocal->nodeType == SBaseProperty::VECTOR ? NStr::Format("[%d].", nPropIndexLocal) : ".") + pDesc->pszName;
		propDesc.pszName = szPropName.c_str();
		return &propDesc;
	}
}
const SPropertyDesc* CManipulatorIterator::GetNext()
{
	if ( pItLocal->IsEnd() || pItLocal->Next() ==  false )
	{
		++nPropIndexLocal;
		pItLocal = 0;
		return GetFirst();
	}
	else
	{
		const SPropertyDesc *pDesc = pItLocal->GetPropertyDesc();
		propDesc = *pDesc;
		szPropName = szBaseName + (pPropLocal->nodeType == SBaseProperty::VECTOR ? NStr::Format("[%d].", nPropIndexLocal) : ".") + pDesc->pszName;
		propDesc.pszName = szPropName.c_str();
		return &propDesc;
	}
}
bool CManipulatorIterator::GetNextLocal()
{
	if ( (pProperties == 0) || (itProps == pProperties->end()) )
		return false;
	++itProps;
	if ( IsEnd() )
	{
		pPropLocal = 0;
		szBaseName.clear();
		return false;
	}
	FillLocals();
	return true;
}
const SPropertyDesc* CManipulatorIterator::GetFirstProperty()
{
	if ( pPropLocal == 0 )
		return 0;
	if ( pPropLocal->nodeType == SBaseProperty::LEAF )
	{
		szPropName = itProps->szName;
		propDesc.pszName = szPropName.c_str();
		propDesc.ePropType = itProps->pProperty->ePropType;
		propDesc.nType = itProps->pProperty->nType;
		propDesc.values = itProps->pProperty->values;
		return &propDesc;
	}
	else
	{
		nPropIndexLocal = pPropLocal->nodeType == SBaseProperty::SINGLE ? -1 : 0;
		return GetFirst();
	}
}
const SPropertyDesc* CManipulatorIterator::GetNextProperty()
{
	if ( pItLocal )
	{
		const SPropertyDesc *pDesc = GetNext();
		if ( pDesc == 0 )
		{
			nPropIndexLocal = -1;
			pItLocal = 0;
			pPropLocal = 0;
			szBaseName.clear();
			return GetNextProperty();
		}
		else
			return pDesc;
	}
	else
	{
		if ( GetNextLocal() == false )
			return 0;
		if ( pPropLocal->nodeType == SBaseProperty::LEAF )
		{
			szPropName = itProps->szName;
			propDesc.pszName = szPropName.c_str();
			propDesc.ePropType = itProps->pProperty->ePropType;
			propDesc.nType = itProps->pProperty->nType;
			propDesc.values = itProps->pProperty->values;
			return &propDesc;
		}
		else
		{
			nPropIndexLocal = pPropLocal->nodeType == SBaseProperty::SINGLE ? -1 : 0;
			return GetFirst();
		}
	}
}
struct SVariantCmpEq
{
	bool operator()( const VARIANT &var1, const VARIANT &var2 ) const
	{
		if ( &var1 == &var2 )
			return true;
		if ( var1.vt != var2.vt )
			return false;
		switch ( var1.vt )
		{
			case VT_EMPTY:
			case VT_NULL:
				return true;
			case VT_BOOL:
				return V_BOOL(&var1) == V_BOOL(&var2);
			case VT_UI1:
				return var1.bVal == var2.bVal;
			case VT_INT:
				return var1.intVal == var2.intVal;

			case VT_I2:
				return var1.iVal == var2.iVal;
			case VT_I4:
				return var1.lVal == var2.lVal;
			case VT_CY:
				return (var1.cyVal.Hi == var2.cyVal.Hi && var1.cyVal.Lo == var2.cyVal.Lo);
			case VT_R4:
				return var1.fltVal == var2.fltVal;
			case VT_R8:
				return var1.dblVal == var2.dblVal;
			case VT_DATE:
				return var1.date == var2.date;
			case VT_BSTR:
				return SysStringByteLen(var1.bstrVal) == SysStringByteLen(var2.bstrVal) && 
					     memcmp(var1.bstrVal, var2.bstrVal, SysStringByteLen(var1.bstrVal)) == 0;
			case VT_ERROR:
				return var1.scode == var2.scode;
			case VT_DISPATCH:
			case VT_UNKNOWN:
				return var1.punkVal == var2.punkVal;
			default:
				NI_ASSERT_T( false, "Unsupported variant type comparison" );
				return false;
		}
		return false;
	}
};
struct SVariantCmpEq1
{
	SVariantCmpEq cmp;
	VARIANT variant;
	explicit SVariantCmpEq1( const VARIANT &var )
	{
		variant = var;
	}
	bool operator()( const VARIANT &var ) const { return cmp( variant, var ); }
};
void CMultiManipulator::AddManipulator( IManipulator *pMan )
{
	CPtr<IManipulator> pTempMan = pMan;
	bPropsAlreadyBuilt = false;
	int nOrder = 0;
	for ( CPtr<IManipulatorIterator> pIt = pMan->Iterate(); !pIt->IsEnd(); pIt->Next(), ++nOrder )
	{
		const SPropertyDesc *pDesc = pIt->GetPropertyDesc();
		if ( pDesc )
		{
			SMultiManipulatorProperty &prop = propsMap[pDesc->pszName];
			++prop.nCounter;
			prop.nOrderIndex = Max( prop.nOrderIndex, nOrder );
			if ( prop.nCounter == 1 )
			{
				prop.ePropType = pDesc->ePropType;
				prop.values = pDesc->values;
				prop.szName = pDesc->pszName;
			}
			else
			{
				if ( prop.ePropType != pDesc->ePropType )
					--prop.nCounter;
				if ( !prop.values.empty() )
				{
					for ( std::vector<variant_t>::iterator it = prop.values.begin(); it != prop.values.end(); )
					{
						if ( std::find_if( pDesc->values.begin(), pDesc->values.end(), SVariantCmpEq1(*it) ) == pDesc->values.end() )
							it = prop.values.erase( it );
						else
							++it;
					}
				}
			}
		}
	}
	manipulators.push_back( pMan );
}
struct SMultiManPropCmpLess
{
	bool operator()( const SMultiManipulatorProperty *pProp1, const SMultiManipulatorProperty *pProp2 ) const
	{
		return pProp1->nOrderIndex < pProp2->nOrderIndex;
	}
};
void CMultiManipulator::BuildProps()
{
	if ( bPropsAlreadyBuilt )
		return;
	int nMans = manipulators.size();
	for ( CPropsMap::iterator it = propsMap.begin(); it != propsMap.end(); ++it )
	{
		if ( it->second.nCounter == nMans )
			propsList.push_back( &(it->second) );
	}
	propsList.sort( SMultiManPropCmpLess() );
	bPropsAlreadyBuilt = true;
}
bool CMultiManipulator::GetValue( const char *pszValueName, variant_t *pValue )
{
	BuildProps();
	SVariantCmpEq comparator;
	variant_t var, tmpvar;
	var.vt = VT_EMPTY;
	int nPropsCounter = 0;
	for ( CManipulatorsList::iterator it = manipulators.begin(); it != manipulators.end(); ++it )
	{
		if ( (*it)->GetValue( pszValueName, &tmpvar ) == false )
			continue;
		++nPropsCounter;
		if ( var.vt == VT_EMPTY )
			var = tmpvar;
		else
		{
			if ( comparator(var, tmpvar) == false )
			{
				pValue->vt = VT_BSTR;
				_bstr_t tmpVal = "...";
				pValue->bstrVal = tmpVal;
				return true;
			}
		}
	}
	if ( nPropsCounter > 0 )
	{
		*pValue = var;
		return true;
	}
	return false;
}
IManipulatorIterator* CMultiManipulator::Iterate() 
{ 
	BuildProps();
	return new CMultiManipulatorIterator( this ); 
}
