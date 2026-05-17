#ifndef __VARSYSTEMINTERNAL_H__
#define __VARSYSTEMINTERNAL_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSerialVariantT : public variant_t
{
	SSerialVariantT() {  }
	SSerialVariantT( const variant_t &var ) : variant_t( var ) {  }
	//
	void Set( const variant_t &var ) { *(static_cast<variant_t*>(this)) = var; }
	const variant_t& Get() const { return *(static_cast<const variant_t*>(this)); }
	variant_t& Get() { return *(static_cast<variant_t*>(this)); }
	//
	virtual int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		const VARTYPE oldvt = vt;
		saver.Add( 1, &vt );
		switch ( vt ) 
		{
		case VT_EMPTY:
			break;
			case VT_BSTR:
				if ( saver.IsReading() ) 
				{
					if ( oldvt == VT_BSTR ) 
						SysFreeString( bstrVal );
					int nLen = 0;
					saver.Add( 3, &nLen );
					if ( nLen > 0 ) 
					{
						std::vector<OLECHAR> buffer( nLen );
						saver.AddRawData( 2, &(buffer[0]), nLen * sizeof(OLECHAR) );
						bstrVal = SysAllocString( &(buffer[0]) );
					}
				}
				else
				{
					int nLen = SysStringLen( bstrVal );
					saver.Add( 3, &nLen );
					if ( nLen > 0 ) 
						saver.AddRawData( 2, bstrVal, nLen * sizeof(OLECHAR) );
				}
				break;
			case VT_UI1:
				saver.Add( 2, &bVal );
				break;
			case VT_I2:
				saver.Add( 2, &iVal );
				break;
			case VT_I4:
				saver.Add( 2, &lVal );
				break;
			case VT_R4:
				saver.Add( 2, &fltVal );
				break;
			case VT_R8:
				saver.Add( 2, &dblVal );
				break;
			case VT_BOOL:
				saver.Add( 2, &boolVal );
				break;
			case VT_UI8:
				saver.Add( 2, &ulVal );
				break;
			default:
				NI_ASSERT_T( false, NStr::Format("Unsupported variant type (%d) to serialize", vt) );
		}
		return 0;
	}
	virtual int STDCALL operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		const VARTYPE oldvt = vt;
		saver.Add( "Type", &vt );
		switch ( vt ) 
		{
			case VT_EMPTY:
				break;
			case VT_BSTR:
				if ( saver.IsReading() ) 
				{
					if ( oldvt == VT_BSTR ) 
						SysFreeString( bstrVal );
					std::wstring str;
					saver.Add( "Var", &str );
					bstrVal = SysAllocString( str.c_str() );
				}
				else
				{
					std::wstring str = bstr_t( bstrVal );
					NI_ASSERT_T( str.size() == SysStringLen(bstrVal), "Unsupported BSTR value" );
					saver.Add( "Var", &str );
				}
				break;
			case VT_UI1:
				saver.Add( "Var", &bVal );
				break;
			case VT_I2:
				saver.Add( "Var", &iVal );
				break;
			case VT_I4:
				saver.Add( "Var", &lVal );
				break;
			case VT_R4:
				saver.Add( "Var", &fltVal );
				break;
			case VT_R8:
				saver.Add( "Var", &dblVal );
				break;
			case VT_BOOL:
				saver.Add( "Var", &boolVal );
				break;
			default:
				NI_ASSERT_T( false, NStr::Format("Unsupported variant type (%d) to serialize", vt) );
		}
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** helper functionals
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMatchNameFunctional
{
	const std::string &szMatchName;
public:
	CMatchNameFunctional( const std::string _szMatchName ) : szMatchName( _szMatchName ) {  }
	//
	bool operator()( const std::string &szVarName ) const
	{
		return szVarName.compare(0, szMatchName.size(), szMatchName) == 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMatchInListFunctional
{
	const std::list<std::string> &serialIncludes;
	const std::list<std::string> &serialExcludes;
	//
	bool IsMatchName( const std::list<std::string> &lst, const std::string &szVarName ) const
	{
		for ( std::list<std::string>::const_iterator it = lst.begin(); it != lst.end(); ++it )
		{
			if ( szVarName.compare(0, it->size(), *it) == 0 )
				return true;
		}
		return false;
	}
	bool IsInIncludes( const std::string &szVarName ) const { return IsMatchName( serialIncludes, szVarName ); }
	bool IsInExcludes( const std::string &szVarName ) const { return IsMatchName( serialExcludes, szVarName ); }
public:
	CMatchInListFunctional( const std::list<std::string> &_serialIncludes, const std::list<std::string> &_serialExcludes )
		: serialIncludes( _serialIncludes ), serialExcludes( _serialExcludes ) {  }
	//
	bool operator()( const std::string &szVarName ) const
	{
		if ( serialIncludes.empty() ) 
			return !IsInExcludes( szVarName );
		else
			return IsInIncludes( szVarName );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVar>
struct SSerialVarEqFunctional
{
	bool operator()( const TVar &v1, const TVar &v2 ) const { return v1.szKeyName < v2.szKeyName; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVarSystem>
struct SEmptySorter
{
	void Sort( std::list<typename TVarSystem::CVarsMap::const_iterator> &lst ) const {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** main var system
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVar, class TBase>
class CTVarSystem : public TBase
{
public:
	typedef std::unordered_map<std::string, TVar> CVarsMap;
	typedef TVar CVar;
private:
	struct SSerialVar : public TVar
	{
		std::string szKeyName;
		//
		const TVar& Get() const { return *( static_cast<const TVar*>(this) ); }
		void Set( const TVar &var ) { *( static_cast<TVar*>(this) ) = var; }
		//
		virtual int STDCALL operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;
			saver.AddTypedSuper( static_cast<TVar*>(this) );
			saver.Add( "KeyName", &szKeyName );
			return 0;
		}
	};
	//
	CVarsMap variables;										// variables map
	std::list<std::string> serialIncludes;// vars to include in serialization
	std::list<std::string> serialExcludes;// vars to exclude from serialization
	bool bVarsChanged;										// vars was changed from serialization
	//
	//
	template <class TCheck>
		bool RemoveVars( CVarsMap &vars, TCheck check )
		{
			std::list<std::string> toRemove;
			// find all vars to remove
			for ( CVarsMap::const_iterator it = vars.begin(); it != vars.end(); ++it )
			{
				if ( check(it->first) )
					toRemove.push_back( it->first );
			}
			// remove found vars
			const bool bVarsRemoved = !toRemove.empty();
			for ( std::list<std::string>::const_iterator it = toRemove.begin(); it != toRemove.end(); ++it )
				vars.erase( *it );
			return bVarsRemoved;
		}
protected:
	const TVar* GetVar( const std::string &szVarName ) const
	{
		CVarsMap::const_iterator pos = variables.find( szVarName );
		if ( pos == variables.end() ) 
			return 0;
		return &( pos->second );
	}
	void SetVar( const std::string &szVarName, const TVar *rVar )
	{
		variables[szVarName] = *rVar;
	};
public:
	CTVarSystem() : bVarsChanged( false ) {  }
	virtual ~CTVarSystem() {  }
	//
	virtual bool STDCALL IsChanged() const { return bVarsChanged; }
	// get/set variable by name
	virtual bool STDCALL Get( const std::string &szVarName, variant_t *pVar ) const
	{
		CVarsMap::const_iterator pos = variables.find( szVarName );
		if ( pos == variables.end() ) 
			return false;
		*pVar = pos->second.Get();
		return true;
	}
	virtual bool STDCALL Set( const std::string &szVarName, const variant_t &var )
	{
		CVarsMap::iterator pos = variables.find( szVarName );
		if ( pos == variables.end() )
		{
			variables[szVarName].Set( var );
			bVarsChanged = true;
		}
		else
		{
			bVarsChanged = bVarsChanged || ( pos->second.Get() == var );
			pos->second.Set( var );
		}
		return true;
	}
	// remove variable by name or by match
	virtual bool STDCALL Remove( const std::string &szVarName )
	{
		CVarsMap::iterator pos = variables.find( szVarName );
		if ( pos == variables.end() ) 
			return false;
		variables.erase( pos );
		bVarsChanged = true;
		return true;
	}
	virtual bool STDCALL RemoveByMatch( const std::string &szVarMatch )
	{
		if ( RemoveVars(variables, CMatchNameFunctional(szVarMatch)) )
			bVarsChanged = true;
		return true;
	}
	// include/exclude variable by match to serialize
	virtual bool STDCALL ChangeSerialize( const std::string &szVarMatch, bool bInclude )
	{
		if ( bInclude ) 
			serialIncludes.push_back( szVarMatch );
		else
			serialExcludes.push_back( szVarMatch );
		return true;
	}
	// serialization
	virtual int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		if ( saver.IsReading() ) 
		{
			CVarsMap newvars;
			saver.Add( 1, &newvars );
			// remove all 'include' and 'non-exclude' variables from vars map
			RemoveVars( variables, CMatchInListFunctional(serialIncludes, serialExcludes) );
			// add new vars
			for ( CVarsMap::const_iterator it = newvars.begin(); it != newvars.end(); ++it )
				variables[it->first] = it->second;
			bVarsChanged = false;
		}
		else
		{
			CVarsMap temp = variables;
			// remove all 'include' and 'non-exclude' variables from vars map
			RemoveVars( temp, CMatchInListFunctional(serialIncludes, serialExcludes) );
			//
			saver.Add( 1, &temp );
		}
		return 0;
	}
	virtual int STDCALL operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		if ( saver.IsReading() ) 
		{
			std::list<SSerialVar> vars;
			saver.Add( "Vars", &vars );
			// remove all 'include' and 'non-exclude' variables from vars map
			RemoveVars( variables, CMatchInListFunctional(serialIncludes, serialExcludes) );
			// add new vars
			for ( std::list<SSerialVar>::const_iterator it = vars.begin(); it != vars.end(); ++it )
				variables[it->szKeyName] = it->Get();
			bVarsChanged = false;
		}
		else
		{
			CVarsMap temp = variables;
			// remove all 'include' and 'non-exclude' variables from vars map
			RemoveVars( temp, CMatchInListFunctional(serialIncludes, serialExcludes) );
			//
			std::vector<SSerialVar> vars;
			vars.reserve( temp.size() );
			for ( CVarsMap::const_iterator it = temp.begin(); it != temp.end(); ++it )
			{
				SSerialVar var;
				var.Set( it->second );
				var.szKeyName = it->first;
				vars.push_back( var );
			}
			std::sort( vars.begin(), vars.end(), SSerialVarEqFunctional<SSerialVar>() );
			saver.Add( "Vars", &vars );
		}
		return 0;
	}
	//
	typename CVarsMap::const_iterator begin() const { return variables.begin(); }
	typename CVarsMap::const_iterator end() const { return variables.end(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVar>
struct SVarAccepter 
{ 
	const bool operator()( const TVar &var ) const { return true; } 
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template < class TVarSystem, class TBase, class TSorter = SEmptySorter<TVarSystem>, class TVarAccepter = SVarAccepter<typename TVarSystem::CVar> >
class CTVarSystemIterator : public TBase
{
public:
	typedef std::list<typename TVarSystem::CVarsMap::const_iterator> CPosList;
private:
	CPosList positions;										// all positions, sorted by some criterion
	typename CPosList::const_iterator pos;					// current iteration position
protected:
	const typename TVarSystem::CVarsMap::const_iterator& GetIt() const { return *pos; }
public:
	CTVarSystemIterator( const TVarSystem *pVS, TVarAccepter &accepter )
	{
		TSorter sorter;
		for ( typename TVarSystem::CVarsMap::const_iterator it = pVS->begin(); it != pVS->end(); ++it )
		{
			if ( accepter(it->second) ) 
				positions.push_back( it );
		}
		sorter.Sort( positions );
		pos = positions.begin();
	}
	virtual ~CTVarSystemIterator() {  }
	// go to the next item
	virtual bool STDCALL Next() { ++pos; return !IsEnd(); }
	// was all items already iterated?
	virtual bool STDCALL IsEnd() const { return pos == positions.end(); }
	// get current variable
	virtual bool STDCALL Get( variant_t *pVarName, variant_t *pVar = 0 ) const
	{
		if ( IsEnd() ) 
			return false;
		//
		if ( pVarName ) 
			*pVarName = (*pos)->first.c_str();
		if ( pVar ) 
			*pVar = (*pos)->second.Get();
		//
		return true;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __VARSYSTEMINTERNAL_H__
