#ifndef __DTHELPER_H__
#define __DTHELPER_H__
#include "StructureSaver.h"
#ifndef ZDATA_
#define ZDATA_(a)
#endif // ZDATA_

#ifndef ZDATA__
#define ZDATA__(a, b)
#endif // ZDATA__

#ifndef ZDATA___
#define ZDATA___(a, b, c)
#endif // ZDATA___

#ifndef ZDATA
#define ZDATA
#endif // ZDATA

#ifndef ZEND
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#define ZEND
#endif // ZEND

#ifndef ZSKIP
#define ZSKIP
#endif // ZSKIP
template <class T>
inline char operator&( T &c, IDataTree &ss ) { return 0; }
class CTreeAccessor
{
	CPtr<IDataTree> pSS;
	char __cdecl TestDataPath( ... ) { return 0; }
	template <class T1, class T2>
		int __cdecl TestDataPath( std::pair<T1, T2> * ) { return 0; }
	template <class T1, class T2>
		int __cdecl TestDataPath( CPtrBase<T1, T2> * ) { return 0; }
	template <class T1>
		int __cdecl TestDataPath( CGDBPtr<T1> * ) { return 0; }
	template <class T1>
		int __cdecl TestDataPath( std::basic_string<T1> * ) { return 0; }
	template <class T1, class T2>
		int __cdecl TestDataPath( std::vector<T1, T2> * ) { return 0; }
	template <class T1>
		int __cdecl TestDataPath( CArray2D<T1> * ) { return 0; }
	template <class T1, class T2>
		int __cdecl TestDataPath( std::list<T1, T2> * ) { return 0; }
	template <class T1, class T2, class T3, class T4>
		int __cdecl TestDataPath( std::map<T1, T2, T3, T4> * ) { return 0; }
	template <class T1, class T2, class T3, class T4, class T5>
		int __cdecl TestDataPath( std::unordered_map<T1, T2, T3, T4, T5> * ) { return 0; }
	template <class T1, class T2, class T3, class T4, class T5>
		int __cdecl TestDataPath( std::unordered_multimap<T1, T2, T3, T4, T5> * ) { return 0; }
	template <class T1, class T2, class T3, class T4>
		int __cdecl TestDataPath( std::unordered_set<T1, T2, T3, T4> * ) { return 0; }
	template <class T1, class T2, class T3>
		int __cdecl TestDataPath( std::set<T1, T2, T3> * ) { return 0; }
	template <class T1, class T2, class T3, class T4, class T5>
		int __cdecl TestDataPath( std::priority_queue<T1, T2, T3> * ) { return 0; }
	template <class TYPE>
		void AddIntData( DTChunkID idChunk, TYPE *pData ) 
		{ 
			int nData = *pData;
			if ( idChunk[0] == '\0' )
				pSS->DataChunk( "data", &nData );
			else
				pSS->DataChunk( idChunk, &nData );
			if ( IsReading() )
				*pData = (TYPE)nData;
		}
	template <class TYPE>
		void AddFPData( DTChunkID idChunk, TYPE *pData ) 
		{ 
			double fData = *pData;
			if ( idChunk[0] == '\0' )
				pSS->DataChunk( "data", &fData );
			else
				pSS->DataChunk( idChunk, &fData );
			if ( IsReading() )
				*pData = (TYPE)fData;
		}
	template <class T>
		void __cdecl AddStringData( std::basic_string<T> *pData )
		{
			if ( IsReading() )
			{
				const int nSize = pSS->GetChunkSize();
				pData->resize( nSize + 1 );
				if ( sizeof(T) == sizeof(char) )
					pSS->StringData( reinterpret_cast<char*>( &(*pData)[0] ) );
				else
					pSS->StringData( reinterpret_cast<WORD*>( &(*pData)[0] ) );
				pData->resize( std::char_traits<T>::length( pData->c_str() ) );
			}
			else if ( pData->empty() )
			{
				T szEmpty[1] = { 0 };
				if ( sizeof(T) == sizeof(char) )
					pSS->StringData( reinterpret_cast<char*>( szEmpty ) );
				else
					pSS->StringData( reinterpret_cast<WORD*>( szEmpty ) );
			}
			else
			{
				if ( sizeof(T) == sizeof(char) )
					pSS->StringData( reinterpret_cast<char*>( const_cast<T*>( pData->c_str() ) ) );
				else
					pSS->StringData( reinterpret_cast<WORD*>( const_cast<T*>( pData->c_str() ) ) );
			}
		}
	template <class T>
		void __cdecl CallObjectSerialize( const DTChunkID idChunk, T *pData, ... )
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			pData->operator&( *pSS );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T>
		void __cdecl CallObjectSerialize( const DTChunkID idChunk, T *pData, SGenericNumber<1> *pp )
		{
			NI_ASSERT_T( sizeof(T) <= 4, "Complex object has no serialization operator" );
			AddRawData( idChunk, const_cast<void*>(static_cast<const void*>(pData)), sizeof(T) );
		}
	template <> 
		void __cdecl CallObjectSerialize<bool>( const DTChunkID idChunk, bool *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<char>( const DTChunkID idChunk, char *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<unsigned char>( const DTChunkID idChunk, unsigned char *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<signed char>( const DTChunkID idChunk, signed char *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<short>( const DTChunkID idChunk, short *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<unsigned short>( const DTChunkID idChunk, unsigned short *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<long>( const DTChunkID idChunk, long *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<unsigned long>( const DTChunkID idChunk, unsigned long *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<int>( const DTChunkID idChunk, int *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<unsigned int>( const DTChunkID idChunk, unsigned int *pData, SGenericNumber<1> *pp )
		{
			AddIntData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<float>( const DTChunkID idChunk, float *pData, SGenericNumber<1> *pp )
		{
			AddFPData( idChunk, pData );
		}
	template <> 
		void __cdecl CallObjectSerialize<double>( const DTChunkID idChunk, double *pData, SGenericNumber<1> *pp )
		{
			AddFPData( idChunk, pData );
		}
	template <class T>
		void __cdecl AddInternal( const DTChunkID idChunk, T *pData, ... )
		{
			const int N_HAS_SERIALIZE_TEST = sizeof( (*pData) & (*pSS) );
			SGenericNumber<N_HAS_SERIALIZE_TEST> separator;
			CallObjectSerialize( idChunk, pData, &separator );
		}

	template <class T1, class T2>
		void __cdecl AddInternal( const DTChunkID idChunk, T1 *p, std::basic_string<T2> *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
				AddStringData( pData );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T, class T1, class T2>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::vector<T1, T2> *pData ) 
		{
			if ( pSS->StartContainerChunk( idChunk ) == 0 )
				return;
			const int nSize = pSS->CountChunks( idChunk );
			DoVector( *pData, nSize );
			pSS->FinishContainerChunk();
		}
	template <class T, class T1>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, CArray2D<T1> *pData ) 
		{
			if ( pSS->StartContainerChunk( idChunk ) == 0 )
				return;
			const int nSize = pSS->CountChunks( idChunk );
			if ( sizeof( TestDataPath( &(*pData)[0][0] ) ) == 1 && sizeof( (*pData)[0][0] & (*pSS) ) == 1 )
				Do2DArrayData( *pData, nSize );
			else
				Do2DArray( *pData, nSize );
			pSS->FinishContainerChunk();
		}
	template <class T, class T1, class T2>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::list<T1, T2> *pData ) 
		{
			if ( pSS->StartContainerChunk( idChunk ) == 0 )
				return;
			const int nSize = pSS->CountChunks( idChunk );
			if ( IsReading() )
			{
				pData->clear();
				pData->insert( pData->begin(), nSize, T1() );
			}
			int i = 0;
			for ( typename std::list<T1, T2>::iterator it = pData->begin(); it != pData->end(); ++it, ++i )
			{
				pSS->SetChunkCounter( i );
				Add( "", &(*it) );
			}
			pSS->FinishContainerChunk();
		}
	template <class T, class T1, class T2, class T3, class T4, class T5>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::unordered_map<T1, T2, T3, T4, T5> *pData ) 
		{
			if ( pSS->StartContainerChunk( idChunk ) == 0 )
				return;
			const int nSize = pSS->CountChunks( idChunk );
			DoHashMap( *pData, nSize );
			pSS->FinishContainerChunk();
		}
	template <class T, class T1, class T2, class T3, class T4, class T5>
		void __cdecl AddInternal( const DTChunkID idChunk, T* p, std::unordered_multimap<T1, T2, T3, T4, T5> *pData )
		{
			if ( pSS->StartContainerChunk( idChunk ) == 0 )
				return;
			const int nSize = pSS->CountChunks( idChunk );
			DoHashMultiMap( *pData, nSize );
			pSS->FinishContainerChunk();
		}
	template <class T, class T1, class T2, class T3, class T4>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::map<T1, T2, T3, T4> *pData ) 
		{
			if ( pSS->StartContainerChunk( idChunk ) == 0 )
				return;
			const int nSize = pSS->CountChunks( idChunk );
			DoMap( *pData, nSize );
			pSS->FinishContainerChunk();
		}
	template <class T, class T1, class T2, class T3>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::priority_queue<T1, T2, T3> *pData ) 
		{
			std::vector<T1> elements;
			std::priority_queue<T1, T2, T3> &data = *pData;
			if ( IsReading() )
			{
				while ( !data.empty() )
					data.pop();
			}
			else
			{
				int nSize = data.size();
				elements.reserve( nSize );
				while ( !data.empty() )
				{
					elements.push_back( data.top() );
					data.pop();
				}
			}
			Add( idChunk, &elements );
			for ( std::vector<T1>::iterator it = elements.begin(); it != elements.end(); ++it )
				data.push( *it );
		}
	template <class T, class T1, class T2, class T3, class T4>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::unordered_set<T1, T2, T3, T4> *pData ) 
		{
			std::vector<T1> elements;
			if ( !IsReading() )
			{
				elements.reserve( pData->size() );
			for ( std::unordered_set<T1, T2, T3, T4>::iterator it = pData->begin(); it != pData->end(); ++it )
					elements.push_back( *it );
			}
			Add( idChunk, &elements );
			if ( IsReading() )
			{
				pData->clear();
				for ( std::vector<T1>::iterator it = elements.begin(); it != elements.end(); ++it )
					pData->insert( *it );
			}
		}
	template <class T, class T1, class T2, class T3>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::set<T1, T2, T3> *pData ) 
		{
			std::list<T1> elements;
			if ( !IsReading() )
			{
				for ( std::set<T1, T2, T3>::iterator it = pData->begin(); it != pData->end(); ++it )
					elements.push_back( *it );
			}
			Add( idChunk, &elements );
			if ( IsReading() )
			{
				pData->clear();
				for ( std::list<T1>::iterator it = elements.begin(); it != elements.end(); ++it )
					pData->insert( *it );
			}
		}
	template <class T>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, CVec2 *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "x", &pData->x );
			Add( "y", &pData->y );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, CVec3 *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "x", &pData->x );
			Add( "y", &pData->y );
			Add( "z", &pData->z );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, CVec4 *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "x", &pData->x );
			Add( "y", &pData->y );
			Add( "z", &pData->z );
			Add( "w", &pData->w );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T, class T1>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, CTRect<T1> *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "x1", &pData->x1 );
			Add( "y1", &pData->y1 );
			Add( "x2", &pData->x2 );
			Add( "y2", &pData->y2 );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, RECT *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "x1", &pData->left );
			Add( "y1", &pData->top );
			Add( "x2", &pData->right );
			Add( "y2", &pData->bottom );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T, class T1>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, CTPoint<T1> *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "x", &pData->x );
			Add( "y", &pData->y );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, POINT *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "x", &pData->x );
			Add( "y", &pData->y );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T, class T1, class T2>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, std::pair<T1, T2> *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			Add( "first", &( pData->first ) );
			Add( "second", &( pData->second ) );
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T, class T1, class T2>
		void __cdecl AddInternal( const DTChunkID idChunk, T *p, CPtrBase<T1, T2> *pData )
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			if ( IsReading() )
			{
				int nTypeID = -1;
				Add( "ClassTypeID", &nTypeID );
				if ( nTypeID == -1 )
					Add( "type", &nTypeID );
				*pData = static_cast<T1*>( GetCommonFactory()->CreateObject( nTypeID ) );
				Add( "", pData->GetPtr() );
			}
			else
			{
				int nTypeID = GetCommonFactory()->GetObjectTypeID( pData->GetPtr() );
				Add( "ClassTypeID", &nTypeID );
				Add( "", pData->GetPtr() );
			}
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T, class T1> 
		void AddInternal( const DTChunkID idChunk, T *p, CGDBPtr<T1> *pData ) 
		{
			int nVal = pSS->StartChunk( idChunk );
			if ( nVal == 0 )
				return;
			if ( IsReading() ) 
			{
				if ( IObjectsDB *pGDB = GetSingleton<IObjectsDB>() )
				{
					std::string szParent, szName;
					Add( "ParentName", &szParent );
					Add( "Name", &szName );
					if ( !szParent.empty() && !szName.empty() )
						*pData = static_cast<const T1*>( pGDB->Get( szName.c_str(), szParent.c_str() ) );
					else
						*pData = 0;
				}
				else
					*pData = 0;
			}
			else
			{
				if ( pData->GetPtr() != 0 )
				{
					std::string szString = (*pData)->GetParentName();
					Add( "ParentName", &szString );
					szString = (*pData)->GetName();
					Add( "Name", &szString );
				}
			}
			if ( nVal != -1 )
				pSS->FinishChunk();
		}
	template <class T1, class T2> 
		void DoVector( std::vector<T1, T2> &data, const int nExtSize )
		{
			int nSize = nExtSize;
			if ( IsReading() )
			{
				data.clear();
				data.resize( nSize );
			}
			else
				nSize = data.size();
			for ( int i = 0; i < nSize; ++i )
			{
				pSS->SetChunkCounter( i );
				Add( "", &data[i] );
			}
		}
	template <class T1, class T2, class T3, class T4, class T5> 
	void DoHashMap( std::unordered_map<T1, T2, T3, T4, T5> &data, const int nExtSize )
		{
			if ( IsReading() )
			{
				data.clear();
				for ( int i = 0; i < nExtSize; ++i )
				{
					pSS->SetChunkCounter( i );
					T1 idx;
					Add( "key", &idx );
					Add( "data", &data[idx] );
				}
			}
			else
			{
				int i = 0;
			for ( typename std::unordered_map<T1, T2, T3, T4, T5>::iterator it = data.begin(); it != data.end(); ++it, ++i )
				{
					pSS->SetChunkCounter( i );
					T1 idx = it->first;
					Add( "key", &idx );
					Add( "data", &it->second );
				}
			}
		}
	template <class T1, class T2, class T3, class T4, class T5>
		void DoHashMultiMap( std::unordered_multimap<T1, T2, T3, T4, T5> &data, const int nExtSize )
		{
			if ( IsReading() )
			{
				data.clear();

				for ( int i = 0; i < nExtSize; ++i )
				{
					pSS->SetChunkCounter( i );
					T1 idx;
					Add( "key", &idx );
					T2 value;
					Add( "data", &value );

					data.insert( std::unordered_multimap<T1, T2, T3, T4, T5>::value_type( idx, value ) );
				}
			}
			else
			{
				int i = 0;
				for ( std::unordered_multimap<T1, T2, T3, T4, T5>::iterator it = data.begin(); it != data.end(); ++it, ++i )
				{
					pSS->SetChunkCounter( i );
					T1 idx = it->first;
					Add( "key", &idx );
					Add( "data", &(it->second ) );
				}
			}
		}
	template <class T1, class T2, class T3, class T4> 
		void DoMap( std::map<T1, T2, T3, T4> &data, const int nExtSize )
		{
			if ( IsReading() )
			{
				data.clear();
				for ( int i = 0; i < nExtSize; ++i )
				{
					pSS->SetChunkCounter( i );
					T1 idx;
					Add( "key", &idx );
					Add( "data", &data[idx] );
				}
			}
			else
			{
				int i = 0;
				for ( std::map<T1, T2, T3, T4>::iterator it = data.begin(); it != data.end(); ++it, ++i )
				{
					pSS->SetChunkCounter( i );
					T1 idx = it->first;
					Add( "key", &idx );
					Add( "data", &it->second );
				}
			}
		}
	template <class T> 
		void Do2DArray( CArray2D<T> &data, const int nExtSize )
		{
			if ( IsReading() && (nExtSize <= 1) )
				return;
			int nSizeX = data.GetSizeX(), nSizeY = data.GetSizeY();
			pSS->SetChunkCounter( 0 );
			Add( "size_x", &nSizeX );
			Add( "size_y", &nSizeY );
			if ( IsReading() )
			{
				data.Clear();
				data.SetSizes( nSizeX, nSizeY );
			}
			for ( int i=0; i<nSizeX*nSizeY; ++i )
			{
				pSS->SetChunkCounter( i + 1 );
				Add( "", &( data[i/nSizeX][i%nSizeX] ) );
			}
		}
	template <class T> 
		void Do2DArrayData( CArray2D<T> &data, const int nExtSize )
		{
			if ( IsReading() && (nExtSize <= 1) )
				return;
			int nSizeX = data.GetSizeX(), nSizeY = data.GetSizeY();
			pSS->SetChunkCounter( 0 );
			Add( "size_x", &nSizeX );
			Add( "size_y", &nSizeY );
			if ( IsReading() )
			{
				data.Clear();
				data.SetSizes( nSizeX, nSizeY );
			}
			for ( int i=0; i<nSizeY; ++i )
			{
				pSS->SetChunkCounter( i + 1 );
				pSS->RawData( &( data[i][0] ), nSizeX * sizeof(T) );
			}
		}
public:
	CTreeAccessor() {  }
	CTreeAccessor( const CTreeAccessor &accessor ) 
		: pSS( accessor.pSS ) {  }
	CTreeAccessor( IDataTree *_pSS ) 
		: pSS( _pSS ) {  }
	CTreeAccessor( const CPtr<IDataTree> &_pSS )
		: pSS( _pSS ) {  }
	const CTreeAccessor& operator=( IDataTree *_pSS ) { pSS = _pSS; return *this; }
	const CTreeAccessor& operator=( const CPtr<IDataTree> &_pSS ) { pSS = _pSS; return *this; }
	const CTreeAccessor& operator=( const CTreeAccessor &accessor ) { pSS = accessor.pSS; return *this; }
	operator IDataTree*() const { return pSS; }
	IDataTree* operator->() const { return pSS; }
	bool operator==( const CTreeAccessor &ptr ) const { return ( pSS.GetPtr() == ptr.pSS.GetPtr() ); }
	bool operator==( IDataTree *pNewObject ) const { return ( pSS.GetPtr() == pNewObject ); }
	bool operator!=( const CTreeAccessor &ptr ) const { return ( pSS.GetPtr() != ptr.pSS.GetPtr() ); }
	bool operator!=( IDataTree *pNewObject ) const { return ( pSS.GetPtr() != pNewObject ); }
	bool IsReading() const { return pSS->IsReading(); }
	void AddRawData( const DTChunkID idChunk, void *pData, int nSize ) 
	{ 
		int nVal = pSS->StartChunk( idChunk );
		if ( nVal == 0 )
			return;
		(*this)->RawData( pData, nSize );
		if ( nVal != -1 )
			(*this)->FinishChunk();
	}
	template <class T>
		void Add( const DTChunkID idChunk, T *p ) { AddInternal( idChunk, p, p ); }
	template <class T>
		void AddTypedSuper( T *pData ) { pData->T::operator&( *pSS ); }
};
#endif // __DTHELPER_H__
