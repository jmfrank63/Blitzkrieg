#ifndef __STREAMIO_HELPER_H__
#define __STREAMIO_HELPER_H__
#pragma ONCE
class CStreamAccessor : public CPtr<IDataStream>
{
	template <class T1>
		void WriteString( const std::basic_string<T1> &str )
		{
			int nSize = str.size();
			int nCheck = (*this)->Write( &nSize, sizeof(nSize) );
			NI_ASSERT_SLOW_T( nCheck == sizeof(nSize), NStr::Format("%d bytes written instead of %d", nCheck, sizeof(nSize)) );
			if ( nSize != 0 )
			{
				nCheck = (*this)->Write( &(str[0]), nSize * sizeof(str[0]) );
				NI_ASSERT_SLOW_T( nCheck == nSize*sizeof(str[0]), NStr::Format("%d bytes written instead of %d", nCheck, nSize*sizeof(str[0])) );
			}
		}
	template <class T1>
		void ReadString( std::basic_string<T1> &str )
		{
			int nSize = 0;
			int nCheck = (*this)->Read( &nSize, sizeof(nSize) );
			NI_ASSERT_SLOW_T( nCheck == sizeof(nSize), NStr::Format("%d bytes read instead of %d", nCheck, sizeof(nSize)) );
			str.resize( nSize );
			if ( nSize != 0 )
			{
				nCheck = (*this)->Read( &(str[0]), nSize * sizeof(str[0]) );
				NI_ASSERT_SLOW_T( nCheck == nSize*sizeof(str[0]), NStr::Format("%d bytes read instead of %d", nCheck, nSize*sizeof(str[0])) );
			}
		}
public:
	CStreamAccessor() {  }
	CStreamAccessor( const CStreamAccessor &accessor ) : CPtr<IDataStream>( accessor ) {  }
	CStreamAccessor( const CPtr<IDataStream> &stream ) : CPtr<IDataStream>( stream ) {  }
	CStreamAccessor( IDataStream *_pStream ) : CPtr<IDataStream>( _pStream ) {  }
	template<class T>
		CStreamAccessor& operator>>( T &res )
		{
			const int nCheck = (*this)->Read( &res, sizeof(res) );
			NI_ASSERT_SLOW_T( nCheck == sizeof(res), NStr::Format("%d bytes read instead of %d", nCheck, sizeof(res)) );
			return *this;
		}
	template<class T>
		CStreamAccessor& operator<<( const T &res )
		{
			const int nCheck = (*this)->Write( &res, sizeof(res) );
			NI_ASSERT_SLOW_T( nCheck == sizeof(res), NStr::Format("%d bytes written instead of %d", nCheck, sizeof(res)) );
			return *this;
		}
	template<> CStreamAccessor& operator>>( std::string &str ) { ReadString( str ); return *this; }
	template<> CStreamAccessor& operator<<( const std::string &str ) { WriteString( str ); return *this; }
	template<> CStreamAccessor& operator>>( std::wstring &str ) { ReadString( str ); return *this; }
	template<> CStreamAccessor& operator<<( const std::wstring &str ) { WriteString( str ); return *this; }
};
#endif // __STREAMIO_HELPER_H__
