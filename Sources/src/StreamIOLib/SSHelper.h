#ifndef __SSHELPER_H__
#define __SSHELPER_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef ZDATA_
#define ZDATA_(a)
#endif // ZDATA_

#ifndef ZDATA
#define ZDATA
#endif // ZDATA

#ifndef ZEND
#define ZEND
#endif // ZEND

#ifndef ZSKIP
#define ZSKIP
#endif // ZSKIP
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline char operator&( T &c, IStructureSaver &ss ) { return 0; }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaverAccessor
{
	IStructureSaver *pSS;
	// test data path for different objects
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
	template <class T1, class T2, class T3>
		int __cdecl TestDataPath( std::set<T1, T2, T3> * ) { return 0; }
	template <class T1, class T2, class T3, class T4, class T5>
		int __cdecl TestDataPath( std::unordered_map<T1, T2, T3, T4, T5> * ) { return 0; }
	template <class T1, class T2, class T3, class T4>
		int __cdecl TestDataPath( std::unordered_set<T1, T2, T3, T4> * ) { return 0; }
	template <class T1, class T2, class T3, class T4, class T5>
		int __cdecl TestDataPath( std::priority_queue<T1, T2, T3> * ) { return 0; }
	// call serialize from object or raw data
	template <class T>
		void __cdecl CallObjectSerialize( const SSChunkID idChunk, T *pData, ... )
		{
			if ( !pSS->StartChunk(idChunk) )
				return;
			pData->operator&( *pSS );
			pSS->FinishChunk();
		}
	template <class T>
		void __cdecl CallObjectSerialize( const SSChunkID idChunk, T *pData, SGenericNumber<1> *pp )
		{
			pSS->DataChunk( idChunk, pData, sizeof(T) );
		}
	// 'add internal' functions series for data storing
	template <class T>
		void __cdecl AddInternal( const SSChunkID idChunk, T *pData, ... )
		{
			const int N_HAS_SERIALIZE_TEST = sizeof( (*pData) & (*pSS) );
			SGenericNumber<N_HAS_SERIALIZE_TEST> separator;
			CallObjectSerialize( idChunk, pData, &separator );
		}
	// for unknown reason, IRefCount derivatives cannot be serialized directly - we need IRefCount specialization
	template <class T>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, IRefCount *pData )
		{
			SGenericNumber<4> separator;
			CallObjectSerialize( idChunk, pData, &separator );
		}
	// other specializations
	template <class T1, class T2>
		void __cdecl AddInternal( const SSChunkID idChunk, T1 *p, std::basic_string<T2> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			//
			int nSize = IsReading() ? 0 : pData->size();
			Add( 1, &nSize );
			if ( IsReading() )
				pData->resize( nSize );
			AddRawData( 2, const_cast<T2*>( pData->c_str() ), sizeof(T2) * nSize );
			//
			pSS->FinishChunk();
		}
	template <class T, class T1, class T2>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, std::vector<T1, T2> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			if ( sizeof( TestDataPath( &(*pData)[0] ) ) == 1 && sizeof( (*pData)[0] & (*pSS) ) == 1 )
				DoDataVector( *pData );
			else
				DoVector( *pData );
			pSS->FinishChunk();
		}
	template <class T, class T1>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, CArray2D<T1> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			if ( sizeof( TestDataPath( &(*pData)[0][0] ) ) == 1 && sizeof( (*pData)[0][0] & (*pSS) ) == 1 )
				Do2DArrayData( *pData );
			else
				Do2DArray( *pData );
			pSS->FinishChunk();
		}
	template <class T, class T1, class T2>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, std::list<T1, T2> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			std::list<T1, T2> &data = *pData;
			if ( IsReading() )
			{
				data.clear();
				data.insert( data.begin(), pSS->CountChunks( 1 ), T1() );
			}
			int i = 1;
			for ( std::list<T1, T2>::iterator k = data.begin(); k != data.end(); ++k, ++i )
			{
				pSS->SetChunkCounter( i );
				Add( 1, &(*k) );
			}
			pSS->FinishChunk();
		}
	template <class T, class T1, class T2, class T3, class T4, class T5>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, std::unordered_map<T1, T2, T3, T4, T5> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			DoHashMap( *pData );
			pSS->FinishChunk();
		}
	template <class T, class T1, class T2, class T3, class T4>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, std::map<T1, T2, T3, T4> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			DoMap( *pData );
			pSS->FinishChunk();
		}
	template <class T, class T1, class T2, class T3, class T4>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, std::unordered_set<T1, T2, T3, T4> *pData ) 
		{
			std::list<T1> elements;
			// hash_set => list
			if ( !IsReading() )
			{
				for ( std::unordered_set<T1, T2, T3, T4>::iterator it = pData->begin(); it != pData->end(); ++it )
					elements.push_back( *it );
			}
			// add container
			Add( idChunk, &elements );
			// list => hash_set
			if ( IsReading() )
			{
				pData->clear();
				for ( std::list<T1>::iterator it = elements.begin(); it != elements.end(); ++it )
					pData->insert( *it );
			}
		}
	template <class T, class T1, class T2, class T3>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, std::set<T1, T2, T3> *pData ) 
		{
			std::list<T1> elements;
			// hash_set => list
			if ( !IsReading() )
			{
				for ( std::set<T1, T2, T3>::iterator it = pData->begin(); it != pData->end(); ++it )
					elements.push_back( *it );
			}
			// add container
			Add( idChunk, &elements );
			// list => hash_set
			if ( IsReading() )
			{
				pData->clear();
				for ( std::list<T1>::iterator it = elements.begin(); it != elements.end(); ++it )
					pData->insert( *it );
			}
		}
	template <class T, class T1, class T2, class T3>
		void __cdecl AddInternal( const SSChunkID idChunk, T *p, std::priority_queue<T1, T2, T3> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			DoPriorityQueue( *pData );
			pSS->FinishChunk();
		}
	template <class T, class T1, class T2> 
		void AddInternal( const SSChunkID idChunk, T *p, std::pair<T1, T2> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			Add( 1, &( pData->first ) );
			Add( 2, &( pData->second ) );
			pSS->FinishChunk();
		}
	// smart pointers specialization
	// general smart ptr/obj
	template <class T, class T1, class T2> 
		void AddInternal( const SSChunkID idChunk, T *p, CPtrBase<T1, T2> *pData ) 
		{
			if ( !pSS->StartChunk( idChunk ) )
				return;
			if ( IsReading() ) 
				*pData = CastToUserObject( pSS->LoadObject(), (T1*)0 ); 
			else 
				pSS->StoreObject( CastToRefCount(pData->GetPtr()) );
			pSS->FinishChunk();
		}
	// database ptr
	template <class T, class T1> 
		void AddInternal( const SSChunkID idChunk, T *p, CGDBPtr<T1> *pData ) 
	{
		if ( !pSS->StartChunk( idChunk ) )
			return;
		if ( IsReading() ) 
		{
			if ( IGDB *pGDB = pSS->GetGDB() )
			{
				std::string szParent, szName;
				Add( 1, &szParent );
				Add( 2, &szName );
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
			//NI_ASSERT_SLOW_TF( pData->GetPtr() != 0, "Can't store NULL database object", return );
			if ( pData->GetPtr() != 0 )
			{
				std::string szString = (*pData)->GetParentName();
				Add( 1, &szString );
				szString = (*pData)->GetName();
				Add( 2, &szString );
			}
		}
		pSS->FinishChunk();
	}
	//
	// 'Do...' functions
	// vector
	template <class T1, class T2> 
		void DoVector( std::vector<T1, T2> &data )
		{
			int i, nSize;
			if ( IsReading() )
			{
				data.clear();
				data.resize( nSize = pSS->CountChunks( 1 ) );
			}
			else
				nSize = data.size();
			for ( i = 0; i < nSize; i++ )
			{
				pSS->SetChunkCounter( i + 1 );
				Add( 1, &data[i] );
			}
		}
	template <class T1, class T2> 
		void DoDataVector( std::vector<T1, T2> &data )
		{
			int nSize = data.size();
			Add( 1, &nSize );
			if ( IsReading() )
			{
				data.clear();
				data.resize( nSize );
			}
			if ( nSize > 0 )
				AddRawData( 2, &data[0], sizeof(T1) * nSize );
		}
	// hash_map
	template <class T1, class T2, class T3, class T4, class T5> 
		void DoHashMap( std::unordered_map<T1, T2, T3, T4, T5> &data )
		{
			if ( IsReading() )
			{
				data.clear();
				int nSize = pSS->CountChunks( 1 ), i;
				std::vector<T1> indices;
				indices.resize( nSize );
				for ( i = 0; i < nSize; ++i )
				{
					pSS->SetChunkCounter( i + 1 );
					Add( 1, &indices[i] );
				}
				for ( i = 0; i < nSize; ++i )
				{
					pSS->SetChunkCounter( i + 1 );
					Add( 2, &data[ indices[i] ] );
				}
			}
			else
			{
				for ( std::unordered_map<T1, T2, T3, T4, T5>::iterator pos = data.begin(); pos != data.end(); ++pos )
				{
					T1 idx = pos->first;
					Add( 1, &idx );
					Add( 2, &pos->second );
				}
			}
		}
	// map
	template <class T1, class T2, class T3, class T4> 
		void DoMap( std::map<T1, T2, T3, T4> &data )
		{
			if ( IsReading() )
			{
				data.clear();
				int nSize = pSS->CountChunks( 1 ), i;
				std::vector<T1> indices;
				indices.resize( nSize );
				for ( i = 0; i < nSize; ++i )
				{
					pSS->SetChunkCounter( i + 1 );
					Add( 1, &indices[i] );
				}
				for ( i = 0; i < nSize; ++i )
				{
					pSS->SetChunkCounter( i + 1 );
					Add( 2, &data[ indices[i] ] );
				}
			}
			else
			{
				for ( std::map<T1, T2, T3, T4>::iterator pos = data.begin(); pos != data.end(); ++pos )
				{
					T1 idx = pos->first;
					Add( 1, &idx );
					Add( 2, &pos->second );
				}
			}
		}
	//
	template <class T> 
		void Do2DArray( CArray2D<T> &a )
		{
			int nSizeX = a.GetSizeX(), nSizeY = a.GetSizeY();
			Add( 1, &nSizeX );
			Add( 2, &nSizeY );
			if ( IsReading() )
				a.SetSizes( nSizeX, nSizeY );
			for ( int i = 0; i < nSizeX * nSizeY; i++ )
			{
				pSS->SetChunkCounter( i + 1 );
				Add( 3, &a[i/nSizeX][i%nSizeX] );
			}
		}
	template <class T> 
		void Do2DArrayData( CArray2D<T> &a )
		{
			int nSizeX = a.GetSizeX(), nSizeY = a.GetSizeY();
			Add( 1, &nSizeX );
			Add( 2, &nSizeY );
			if ( IsReading() )
				a.SetSizes( nSizeX, nSizeY );
			if ( nSizeX * nSizeY > 0 )
				AddRawData( 3, &a[0][0], sizeof(T) * nSizeX * nSizeY );
		}
	// priority_queueu
	template <class T1, class T2, class T3>
		void DoPriorityQueue( std::priority_queue<T1, T2, T3> &data )
		{
			std::vector<T1> elements;
			//
			if ( IsReading() )
			{
				// clear container
				while ( !data.empty() )
					data.pop();
			}
			else
			{
				int nSize = data.size();
				// queue => vector translation (with queue clearing)
				elements.reserve( nSize );
				while ( !data.empty() )
				{
					elements.push_back( data.top() );
					data.pop();
				}
			}
			// serialize
			Add( 1, &elements );
			// vector => queue translation
			for ( std::vector<T1>::iterator it = elements.begin(); it != elements.end(); ++it )
				data.push( *it );
		}
public:
	CSaverAccessor() {  }
	CSaverAccessor( const CSaverAccessor &accessor ) 
		: pSS( accessor.pSS ) {  }
	CSaverAccessor( IStructureSaver *_pSS ) 
		: pSS( _pSS ) {  }
	// stream assigning and extracting
	const CSaverAccessor& operator=( IStructureSaver *_pSS ) { pSS = _pSS; return *this; }
	const CSaverAccessor& operator=( const CSaverAccessor &accessor ) { pSS = accessor.pSS; return *this; }
	operator IStructureSaver*() const { return pSS; }
	IStructureSaver* operator->() const { return pSS; }
	// comparison operators
	bool operator==( const CSaverAccessor &ptr ) const { return ( pSS == ptr.pSS ); }
	bool operator==( IStructureSaver *pNewObject ) const { return ( pSS == pNewObject ); }
	bool operator!=( const CSaverAccessor &ptr ) const { return ( pSS != ptr.pSS ); }
	bool operator!=( IStructureSaver *pNewObject ) const { return ( pSS != pNewObject ); }
	// are we in reading state?
	bool IsReading() const { return pSS->IsReading(); }
	// add raw data of specified size (in bytes)
	void AddRawData( const SSChunkID idChunk, void *pData, int nSize ) { pSS->DataChunk( idChunk, pData, nSize ); }
	// main add function - add all structures/classes/datas through it
	template <class T>
		void Add( const SSChunkID idChunk, T *p ) { AddInternal( idChunk, p, p ); }
	// adding typed super class - use it only for super class members serialization
	template <class T>
		void AddTypedSuper( const SSChunkID idChunk, T *pData )
		{
			if ( !pSS->StartChunk(idChunk) )
				return;
			pData->T::operator&( *pSS );
			pSS->FinishChunk();
		}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SSHELPER_H__