#ifndef __LISTS_SET_H__
#define __LISTS_SET_H__
#pragma ONCE
template< class T >
struct SElInfo
{ 
	DECLARE_SERIALIZE;
	public:

	int id; 
	T value;

	SElInfo() { }	
	SElInfo( const int _id, const T &_value ) : id( _id ), value( _value ) { }
};
template< class T >
class CListsSet
{
	DECLARE_SERIALIZE;
	
	std::vector<int> nexts;
	std::vector<int> preds;
	std::vector<T> values;

	std::vector<int> fronts;
	std::vector<int> sizes;

	int freePtr;

	int GetFreePos();
	void AddToFree( int pos )
	{	
		NI_ASSERT_T( pos < nexts.size(), NStr::Format( "Wrong pos (%d)", pos ) );
		nexts[pos] = freePtr;
		freePtr = pos;
	}
public:
	CListsSet() { Init( SConsts::AI_START_VECTOR_SIZE ); }
	explicit CListsSet( const int nLists ) { Init( nLists ); }

	void Init( const int nFronts );
	void Clear();

	const int GetSize( const int nList ) const { if ( nList >= sizes.size() ) return 0; else return sizes[nList]; }

	void IncreaseListsNum( const int nSize ) { if ( fronts.size() <= nSize * 1.5 ) { fronts.resize( nSize * 1.5 ); sizes.resize( nSize * 1.5 ); } }
	const int GetListsNum() const { return fronts.size(); }
	
	typedef int tEnumerator;
	const int Add( const int listNum, const T &value );
	int InsertAfter( const int listNum, const int nPos, const T &value );
	
	const int Erase( const int listNum, const int pos );
	
	const int begin( const int listNum )	const	{ if ( listNum >= fronts.size() ) return 0; else return fronts[listNum]; }
	const int end( )		const		{ return 0; }
	const int GetNext( const tEnumerator pos ) const { return nexts[pos]; }
	const int GetPred( const tEnumerator pos ) const { return preds[pos]; }
	
	T& GetEl ( int pos )
	{ 
		NI_ASSERT_T( pos != 0, "Invalid enumerator" );
		return values[pos];
	}

	const T& GetEl ( int pos ) const
	{ 
		NI_ASSERT_T( pos != 0, "Invalid enumerator" );
		return values[pos];
	}
	
	void MoveFrontToPosition( const int listNum, const tEnumerator pos ) 
	{
		if ( fronts[listNum] != pos )
		{
			nexts[preds[pos]] = freePtr;
			freePtr = fronts[listNum];

			fronts[listNum] = pos;
			preds[pos] = 0;

			sizes[listNum] = 0;
			for ( int iter = begin( listNum ); iter != end(); iter = GetNext( iter ) )
				++sizes[listNum];
		}
	}

	void DelList( const int listNum, const tEnumerator lastPos )
	{
		if ( lastPos != 0 )
		{
			nexts[lastPos] = freePtr;
			freePtr = fronts[listNum];

			fronts[listNum] = 0;
			sizes[listNum] = 0;
		}
	}
};
template< class T >
void CListsSet<T>::Init( const int nFronts )
{
	nexts.resize( SConsts::AI_START_VECTOR_SIZE );
	preds.resize( SConsts::AI_START_VECTOR_SIZE );
	values.resize( SConsts::AI_START_VECTOR_SIZE );

	fronts.resize( nFronts );
	sizes.resize( nFronts );
	
	freePtr = 1;
}
template< class T >
void CListsSet<T>::Clear()
{
	nexts.clear();
	preds.clear();
	values.clear();
	fronts.clear();
	sizes.clear();

	Init( SConsts::AI_START_VECTOR_SIZE );
}
template< class T >
const int CListsSet<T>::Add( int listNum, const T &value )
{
#ifdef _DEBUG	
	int cnt = 0;
	for ( int i = begin( listNum ); i != end(); i = GetNext( i ) )
		++cnt;

	NI_ASSERT_T( cnt == GetSize( listNum ), NStr::Format( "Wrong size of list %d", listNum ) );
#endif
	
	const int newPos = GetFreePos();

	nexts[newPos] = fronts[listNum];
	preds[newPos] = 0;
	
	preds[fronts[listNum]] = newPos;
	fronts[listNum] = newPos;
	++sizes[listNum];
	
	values[newPos] = value;

	return newPos;
}
template< class T >
int CListsSet<T>::InsertAfter( const int listNum, const int nPos, const T &value )
{
#ifdef _DEBUG	
	int cnt = 0;
	for ( int i = begin( listNum ); i != end(); i = GetNext( i ) )
		++cnt;

	NI_ASSERT_T( cnt == GetSize( listNum ), NStr::Format( "Wrong size of list %d", listNum ) );
#endif
	
	const int newPos = GetFreePos();
	preds[newPos] = nPos;

	if ( nPos != 0 )
	{
		nexts[newPos] = nexts[nPos];		
		nexts[nPos] = newPos;
	}
	else
	{
		nexts[newPos] = fronts[listNum];
		if ( fronts[listNum] != 0 )
			preds[fronts[listNum]] = newPos;
		fronts[listNum] = newPos;
	}
	++sizes[listNum];

	values[newPos] = value;

	return newPos;
}
template< class T >
const int CListsSet<T>::Erase( int listNum, tEnumerator pos )
{
#ifdef _DEBUG
	int i;
	for ( i = begin( listNum ); i != end() && i!= pos; i = GetNext( i ) );
	NI_ASSERT_T( i != end(), "There isn't such position in the list" );

	int cnt = 0;
	for ( int i = begin( listNum ); i != end(); i = GetNext( i ) )
		++cnt;

	NI_ASSERT_T( cnt == GetSize( listNum ), NStr::Format( "Wrong size of list %d", listNum ) );
#endif
	
	nexts[preds[pos]] = nexts[pos];
	preds[nexts[pos]] = preds[pos];

	if ( fronts[listNum] == pos )
		fronts[listNum] = nexts[pos];
	
	AddToFree( pos );

	const int pred = preds[pos];
	preds[pos] = 0;

	--sizes[listNum];

	return pred;
}
template< class T >
int CListsSet<T>::GetFreePos()
{
	int result = freePtr;

	if ( nexts[freePtr] == 0 )
	{
		++freePtr;

		if ( freePtr == nexts.size() )
		{
			int nSize = nexts.size() * 1.5;
			
			nexts.resize( nSize );
			preds.resize( nSize );
			values.resize( nSize );
		}
	}		
	else
		freePtr = nexts[freePtr];
	
	return result;
}
template< class T >
class CQueuesSet
{
	DECLARE_SERIALIZE;
protected:
	CListsSet<T> cListsSet;
	std::vector<int> currentPos;
public:
	CQueuesSet() { Init( SConsts::AI_START_VECTOR_SIZE ); }
	explicit CQueuesSet( const int queuesNum ) { Init( queuesNum ); }

	void Init( const int nQueues );
	void Clear();

	const int GetSize( const int nQueueNum ) const { return cListsSet.GetSize( nQueueNum ); }
	
	void IncreaseQueuesNum( const int nSize ) 
	{ 
		NI_ASSERT_T( currentPos.size() == cListsSet.GetListsNum(), "Wrong sizes" );
		cListsSet.IncreaseListsNum( nSize ); 
		if ( cListsSet.GetListsNum() != currentPos.size() )
			currentPos.resize( cListsSet.GetListsNum() );
	}
	const int GetQueuesNum() const { return cListsSet.GetListsNum(); }
	
	inline int Push( const int queueNum, const T &el );
	inline int PushAndEvict( const int queueNum, const T &el );
	
	inline void Pop( const int queueNum );
	inline T& Peek( const int queueNum );
	inline void Erase( const int queueNum, const int nPos );

	inline const T& Peek( const int queueNum ) const;
	const T& GetEl( const int nPos ) const { return cListsSet.GetEl( nPos ); }
	T& GetEl(  const int nPos ) { return cListsSet.GetEl( nPos ); }
	
	inline const bool IsLast( const int queueNum ) const;
	inline const bool IsEmpty( const int queueNum ) const;
	
	inline void DelQueue( const int queueNum );

	const int begin( const int queueNum )	const	{ return IsEmpty(queueNum) ? end() : currentPos[queueNum]; }
	const int end( ) const { return 0; }
	const int last( const int queueNum ) const { return cListsSet.begin( queueNum ); }
	const int GetNext( const int id ) const	{ return cListsSet.GetPred( id ); }
	const int GetPred( const int id ) const	{ return cListsSet.GetNext( id ); }
};
template< class T >
void CQueuesSet<T>::Init( const int nQueues )
{
	cListsSet.Init( nQueues ); 
	currentPos.resize( nQueues );
}
template< class T >
void CQueuesSet<T>::Clear()
{
	cListsSet.Clear();
	currentPos.clear();

	Init( SConsts::AI_START_VECTOR_SIZE );
}
template< class T >
inline int CQueuesSet<T>::Push( const int queueNum, const T &el )
{
	CListsSet<T>::tEnumerator pos = cListsSet.Add( queueNum, el );

	if ( cListsSet.GetNext( pos ) == cListsSet.end() )
		currentPos[queueNum] = pos;

	return pos;
}
template< class T >
inline int CQueuesSet<T>::PushAndEvict( const int queueNum, const T &el )
{
	cListsSet.MoveFrontToPosition( queueNum, currentPos[queueNum] );
	return Push( queueNum, el );
}
template< class T >
inline void CQueuesSet<T>::Pop( const int queueNum )
{
	NI_ASSERT_T( !IsEmpty( queueNum ), "The queue is empty" );
	
	currentPos[queueNum] = cListsSet.Erase( queueNum, currentPos[queueNum] );
}
template< class T >
inline void CQueuesSet<T>::Erase( const int queueNum, const int nPos )
{
	NI_ASSERT_T( !IsEmpty( queueNum ), "The queue is empty" );

	if ( currentPos[queueNum] == nPos )
		currentPos[queueNum] = cListsSet.Erase( queueNum, nPos );
	else
		cListsSet.Erase( queueNum, nPos );
}
template< class T >
inline T& CQueuesSet<T>::Peek( const int queueNum )
{
	NI_ASSERT_T( !IsEmpty( queueNum ), "The queue is empty" );	
	return cListsSet.GetEl( currentPos[queueNum] );
}
template< class T >
inline const T& CQueuesSet<T>::Peek( const int queueNum ) const
{
	NI_ASSERT_T( !IsEmpty( queueNum ), "The queue is empty" );	
	return cListsSet.GetEl( currentPos[queueNum] );
}
template< class T >
inline const bool CQueuesSet<T>::IsEmpty( const int queueNum ) const
{
	return cListsSet.begin( queueNum ) == cListsSet.end();
}
template< class T >
inline const bool CQueuesSet<T>::IsLast( const int queueNum ) const
{
	return cListsSet.begin( queueNum ) == currentPos[queueNum];
}
template< class T >
inline void CQueuesSet<T>::DelQueue( const int queueNum )
{
	if ( queueNum < currentPos.size() && currentPos[queueNum] != 0 )
	{
		cListsSet.DelList( queueNum, currentPos[queueNum] );
		currentPos[queueNum] = 0;
	}
}
template< class T >
class CDecksSet : public CQueuesSet<T>
{
	DECLARE_SERIALIZE;
public:
	CDecksSet() { Init( SConsts::AI_START_VECTOR_SIZE ); }
	explicit CDecksSet( const int decksNum ) { Init( decksNum ); }

	void Init( const int nDecks );
	void Clear();
	
	void IncreaseDecksNum( const int nSize ) { IncreaseQueuesNum( nSize ); }
	const int GetDecksNum() const { return GetQueuesNum(); }
	void DelDeck( const int deckNum ) { DelQueue( deckNum ); }
	
	void PushFront( const int deckNum, const T &el );

	T& GetLastEl( const int nDeckNum );
	const T& GetLastEl( const int nDeckNum ) const;
};
template< class T >
void CDecksSet<T>::Init( const int nDecks )
{
	CQueuesSet<T>::Init( nDecks );
}
template< class T >
void CDecksSet<T>::Clear()
{
	CQueuesSet<T>::Clear();
	Init( SConsts::AI_START_VECTOR_SIZE );
}
template< class T >
void CDecksSet<T>::PushFront( const int deckNum, const T &el )
{
	currentPos[deckNum] = cListsSet.InsertAfter( deckNum, currentPos[deckNum], el );
}
template< class T >
T& CDecksSet<T>::GetLastEl( const int nDeckNum )
{
	return cListsSet.GetEl( cListsSet.begin( nDeckNum ) );
}
template< class T >
const T& CDecksSet<T>::GetLastEl( const int nDeckNum ) const
{
	return cListsSet.GetEl( cListsSet.begin( nDeckNum ) );
}
template <class T>
inline int SElInfo<T>::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;	
	
	saver.Add( 1, &id );
	saver.Add( 2, &value );

	return 0;
}
template <>
inline int SElInfo<WORD>::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;		
	
	saver.Add( 1, &id );
	saver.Add( 2, &value );

	return 0;
}
template < class T >
inline int CListsSet<T>::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nexts );
	saver.Add( 2, &preds );
	saver.Add( 3, &values );
	saver.Add( 4, &fronts );
	saver.Add( 5, &freePtr );
	saver.Add( 6, &sizes );

	return 0;
}
template < class T >
int CQueuesSet<T>::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &cListsSet );
	saver.Add( 2, &currentPos );

	return 0;
}
template < class T >
int CDecksSet<T>::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CQueuesSet<T>*>(this) ); 

	return 0;
}
#endif // __LISTS_SET_H__
