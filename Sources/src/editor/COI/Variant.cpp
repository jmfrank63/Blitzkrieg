
#include "StdAfx.h"
#include "Variant.h"
#include <algorithm>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define FLOAT_EPSILON 0.0000001

int64 MyHexStrTo64( const char *pszStr )
{
	int64 res = 0;
	int nLen = strlen( pszStr );
	for ( int i=0; i<nLen; i++ )
	{
		res *= 16;
		char c = pszStr[i];
		if ( c >= '0' && c <= '9' )
			res += pszStr[i] - '0';
		else if ( c >= 'a' && c <= 'f' )
			res += pszStr[i] - 'a' + 10;
		else if ( c >= 'A' && c <= 'F' )
			res += pszStr[i] - 'A' + 10;
		else
			return 0;
	}
	return res;
}

int HexStrToByte( const std::string &szValue )
{
	int res = 0;
	for ( int i = 0; i < 2; ++i )
	{
		char c = szValue[i];
		res *= 16;
		if ( c >= '0' && c <= '9' )
			res += c - '0';
		else if ( c >= 'a' && c <= 'f' )
			res += c - 'a' + 10;
		else if ( c >= 'A' && c <= 'F' )
			res += c - 'A' + 10;
	}
	return res;
}

int HexStrToInt( const std::string &szValue )
{
	std::string szArg = "00000000";
	int nLen = szValue.size();
	if ( nLen >= 8 )
		szArg = szValue.substr( nLen - 8 );
	else
		szArg.replace( 8 - nLen, nLen, szValue );
	int nResult = 0;
	for ( int nOffset = 0; nOffset < 4; ++nOffset )
	{
		nResult |= ( HexStrToByte( szArg.substr( nOffset * 2, 2 ) ) << nOffset * 8 );
	}
	return nResult;
}

std::string ByteToHexStr( int nValue )
{
	std::string szRes;
	for ( int i = 0; i < 2; ++i )
	{
		int nSymb = nValue & 0x0000000f;
		std::string szChar;
		if ( nSymb < 10 )
			szChar = '0' + nSymb;
		else
			szChar = 'A' + nSymb - 10;
		szRes = szChar + szRes;
		nValue = nValue >> 4;
	}
	return szRes;
}

std::string IntToHexStr( int nValue )
{
	std::string szRes;
	for ( int nOffset = 0; nOffset < 4; ++nOffset )
	{
		szRes += ByteToHexStr( (nValue >> (nOffset * 8)) & 0x000000ff );
	}
	return szRes;
}

void CVariant::OptimizeInt() const
{
	if ( !HasFlag(VT_INT) || !HasFlag( VT_INT32 ) )
	{
		switch(m_eType)
		{
			case VT_FLOAT:
				m_intVal = int(m_floatVal);
				break;
			case VT_STR:
				m_intVal = atoi(m_strVal.c_str());
				break;
		}
		AddFlag(VT_INT);
	}
}

void CVariant::OptimizeFloat() const
{
	if ( !HasFlag(VT_FLOAT) )
	{
		switch(m_eType)
		{
			case VT_BOOL:
			case VT_INT:
			case VT_INT32:
				m_floatVal = float(m_intVal);
				break;
			case VT_STR:
				m_floatVal = atof(m_strVal.c_str());
				break;
		}
		AddFlag(VT_FLOAT);
	}
}

void CVariant::OptimizeStr() const
{
	if ( !HasFlag(VT_STR) )
	{
		CString szTemp;
		switch(m_eType)
		{
			case VT_BOOL:
			case VT_INT:
				szTemp.Format("%i", m_intVal );
				m_strVal = szTemp;
				break;
			case VT_FLOAT:
				szTemp.Format("%g", m_floatVal );
				m_strVal = szTemp;
				break;
			case VT_INT64:
				{
					char temp[255];
					_ui64toa( m_int64Val, temp, 16 );
					m_strVal = temp;
					break;
				}
			case VT_INT32:
				m_strVal = IntToHexStr( m_intVal );
				break;
		}
		AddFlag(VT_STR);
	}
}

void CVariant::OptimizeBool() const
{
	if ( !HasFlag(VT_BOOL) )
	{
		switch(m_eType)
		{
		case VT_STR:
			{
				string str;
				std::transform( m_strVal.begin(), m_strVal.end(), str.begin(), tolower );
				m_intVal = ( str == "yes" || str == "true" );
			}
			break;
		default:
			OptimizeInt();
			break;
		}
		AddFlag(VT_BOOL);
	}
}

void CVariant::OptimizeInt64() const
{
	if ( !HasFlag( VT_INT64 ) )
	{
		switch(m_eType)
		{
		case VT_STR:
			m_int64Val = MyHexStrTo64( m_strVal.c_str() );
			break;

		default:
			NI_ASSERT( 0 );
		}
		AddFlag( VT_INT64 );
	}
}

bool CVariant::operator != ( const CVariant &var ) const
{
  if ( m_eType != var.m_eType )
    return true;
  switch( m_eType )
  {
  case VT_BOOL:
  case VT_INT:
  case VT_INT32:
    return m_intVal != var.m_intVal;
  case VT_FLOAT:
		return fabs( m_floatVal - var.m_floatVal) > FLOAT_EPSILON;
  case VT_STR:
    return m_strVal != var.m_strVal;
  case VT_INT64:
    return m_int64Val != var.m_int64Val;
  case VT_NULL:
    return false;
  }
  return true;
}

void CVariant::SetNewValue( const string strVal )
{
  m_flagsOptimized = m_eType;

  switch( m_eType )
  {
  case VT_BOOL:
  case VT_INT:
    m_intVal = atoi( strVal.c_str() );
    break;
  case VT_FLOAT:
    m_floatVal = atof( strVal.c_str() );
    break;
	case VT_INT64:
		m_int64Val = MyHexStrTo64( strVal.c_str() );
	case VT_INT32:
		m_intVal = HexStrToInt( strVal.c_str() );
		break;
  case VT_STR:
    m_strVal = strVal;
    break;
  }
}

void CVariant::SetType( EVarialeType tip )		//RR
{
	if ( m_eType == tip )
		return;
	
	switch ( tip )
	{
		case VT_FLOAT:
			if ( m_eType == VT_INT || m_eType == VT_BOOL || m_eType == VT_INT32 )
				m_floatVal = (float) m_intVal;
			else
				m_floatVal = 0.0f;
			break;
		case VT_INT32:			
		case VT_INT:
		case VT_BOOL:
			if ( m_eType == VT_FLOAT )
				m_intVal = (int) m_floatVal;
			else if ( m_eType != VT_INT && m_eType != VT_INT32 && m_eType != VT_BOOL )
				m_intVal = 0;
			break;
			
		case VT_STR:
			if ( m_eType == VT_BOOL )
				break;
			if ( m_eType == VT_INT64 )
			{
				char temp[255];
				_ui64toa( m_int64Val, temp, 16 );
				m_strVal = temp;
				break;
			}
			if ( m_eType == VT_INT32 )
			{
				m_strVal = IntToHexStr( m_intVal );
				break;
			}
		case VT_INT64:
			if ( m_eType == VT_INT || m_eType == VT_INT32 )
			{
				m_int64Val = 0;
				m_int64Val |= m_intVal;
				break;
			}
		default:
			NI_ASSERT( 0 );			//what is the fucking conversion?
	}

	m_eType = tip;
}

int CVariant::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		int nType = 0;
		saver.Add( "type", &nType );
		m_eType = (CVariant::EVarialeType) nType;
	}
	else
	{
		int nType = m_eType;
		saver.Add( "type", &nType );
	}

	saver.Add( "flag", &m_flagsOptimized );
	saver.Add( "float_value", &m_floatVal );
	saver.Add( "int_value", &m_intVal );
	saver.Add( "string_value", &m_strVal );

	if ( !saver.IsReading() )
	{
		DWORD dwLow = m_int64Val;
		DWORD dwHigh = m_int64Val >> 32;
		saver.Add( "int64low", &dwLow );
		saver.Add( "int64high", &dwHigh );
	}
	else
	{
		DWORD dwLow, dwHigh;
		saver.Add( "int64low", &dwLow );
		saver.Add( "int64high", &dwHigh );
		m_int64Val = ((int64) dwHigh << 32) + dwLow;
	}
	return 0;
}
