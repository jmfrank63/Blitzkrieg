#if !defined(__AsyncImageList__)
#define __AsyncImageList__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CRAP{��������� �� ����� �������� � ������� ������ ��������������
class CIndicesHolder
{
	CTPoint<int> bounds;
	BYTE* pIndices;

	int nCurrentIndex;

	int ProceedIndex( int nStartIndex )
	{
		while ( ( nStartIndex < bounds.max ) && ( pIndices[nStartIndex - bounds.min] > 0 ) )
		{
			++nStartIndex;
		}

		if ( nStartIndex >= bounds.max ) 
		{
			return INVALID_INDEX;
		}
		return nStartIndex;
	}
public:
	static const int INVALID_INDEX;

	CIndicesHolder()
		: bounds( CTPoint<int>( 0, 0 ) ), nCurrentIndex( INVALID_INDEX ), pIndices( 0 ) {}
	~CIndicesHolder()
	{
		Clear();
	}

	void Create( const CTPoint<int> &rBounds )
	{
		NI_ASSERT_T( rBounds.min < rBounds.max,
								 NStr::Format("Invalid bounds: (%d, %d)\n", rBounds.min, rBounds.max ) );
		Clear();
		bounds = rBounds;
		pIndices = new BYTE[bounds.max - bounds.min];
		::ZeroMemory( pIndices, ( bounds.max - bounds.min ) * sizeof( BYTE ) );
	}

	void Clear()
	{
		if ( pIndices )
		{
			delete[] pIndices;
			pIndices = 0;
		}
		bounds = CTPoint<int>( 0, 0 );
		nCurrentIndex = INVALID_INDEX;
	}

	int CIndicesHolder::GetStartIndex( int nStartIndex )
	{
		NI_ASSERT_T( pIndices != 0,
								 NStr::Format( "Not created." ) );

		if ( ( nStartIndex < bounds.min ) || ( nStartIndex >= bounds.max ) ) 
		{
			return INVALID_INDEX;
		}
		nCurrentIndex = ProceedIndex( nStartIndex );
		if ( nCurrentIndex == INVALID_INDEX ) 
		{
			nCurrentIndex = ProceedIndex( bounds.min );
		}		
		return nCurrentIndex;
	}

	int CIndicesHolder::GetNextIndex()
	{
		NI_ASSERT_T( pIndices != 0,
								 NStr::Format( "Not created." ) );
		
		if ( ( nCurrentIndex < bounds.min ) || ( nCurrentIndex >= bounds.max ) ) 
		{
			nCurrentIndex = bounds.min;
		}
		else
		{
			pIndices[nCurrentIndex - bounds.min] = 1;
			nCurrentIndex = ProceedIndex( ++nCurrentIndex );
		}
		if ( nCurrentIndex == INVALID_INDEX ) 
		{
			nCurrentIndex = ProceedIndex( bounds.min );
		}		
		return nCurrentIndex;
	}
};
//CRAP}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//pvParam - ��������� �� ������ ������ CAsyncImageList, ��������������� � PVOID
DWORD WINAPI AsyncImageListThreadFunc( PVOID pvParam );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IAsyncImageListCallback
{
	//����������� � ����������� ������
	virtual DWORD Callback( int nImageIndex, DWORD dwResult ) = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAsyncImageList
{
	static const int DEFAULT_ICON_NUMBER;

	int nForceStartImageIndex;
	HIMAGELIST hImageList;
	HIMAGELIST hSmallImageList;

	CRITICAL_SECTION criticalSection;
	HANDLE hThread;
	HANDLE hExitEvent;
	HANDLE hForceStartEvent;

	std::unordered_map<int, IAsyncImageListCallback* > callbackHashMap;
	CIndicesHolder indicesHolder;

	void Clear();
	//��������� ������� ���������� �� AsyncFill
	DWORD Fill();
	//�� ��������������, ��������� �������� �� ���������� ������
	//� HIMAGELIST ���������� ����� ����������� ������, ������� ���������� � ������ ������
	virtual DWORD Fill( int nImageIndex ) = 0;
	
	friend DWORD WINAPI AsyncImageListThreadFunc( PVOID pvParam );
public:
	CAsyncImageList();
	~CAsyncImageList();

	//���������� ����� ����������� ������
	HIMAGELIST GetImageList() { return hImageList; }
	HIMAGELIST GetSmallImageList() { return hSmallImageList; }
	//����������� ������
	CRITICAL_SECTION* GetCriticalSection() { return &criticalSection; }

	//������� imageList �������� ������, ��������� �������
	bool Create( int nImageCount, const CTPoint<int> &rImageSize );
	//����� ������������ ���������� imageList
	bool AsyncFill();
	//�������� �� ������������� �������� ����������
	bool IsFilling();
	//�������������� ���������� �������� ����������
	bool StopFilling();

	//���������� callback �������
	int AddCallback( IAsyncImageListCallback *pCallback );
	//�������� callback ������� �� �����
	void RemoveCallback( int nKey );

	//���������� ���������� � ���������� ��������
	bool ForceFillFrom( int nImageIndex );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // !defined(__AsyncImageList__)
