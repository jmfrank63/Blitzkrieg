#include "stdafx.h"

#include "..\\Image\\Image.h"
#include "AsyncImageList.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//class CIndicesHolder
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CIndicesHolder::INVALID_INDEX = ( -1 );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//AsyncImageListThreadFunc( PVOID pvParam )
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI AsyncImageListThreadFunc( PVOID pvParam )
{
	CAsyncImageList *pAsyncImageList = reinterpret_cast<CAsyncImageList*>( pvParam );
	if ( pAsyncImageList )
	{
		return pAsyncImageList->Fill();
	}
	return ERROR_INVALID_PARAMETER;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//class CAsyncImageList
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CAsyncImageList::DEFAULT_ICON_NUMBER = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAsyncImageList::Clear()
{
	if ( hImageList )
	{
		::ImageList_Destroy( hImageList );
		hImageList = 0;
	}
	if ( hSmallImageList )
	{
		::ImageList_Destroy( hSmallImageList );
		hSmallImageList = 0;
	}
	indicesHolder.Clear();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAsyncImageList::CAsyncImageList()
	: nForceStartImageIndex( CIndicesHolder::INVALID_INDEX ), hImageList( 0 ), hSmallImageList( 0 ), hThread( 0 ), hExitEvent( 0 )
{
	::InitializeCriticalSection( &criticalSection );
	hExitEvent = ::CreateEvent( NULL, TRUE, FALSE, NULL );
	hForceStartEvent = ::CreateEvent( NULL, TRUE, FALSE, NULL );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CAsyncImageList::~CAsyncImageList()
{
	StopFilling();
	Clear();
	::CloseHandle( hExitEvent );
	::CloseHandle( hForceStartEvent );
	::DeleteCriticalSection( &criticalSection );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CAsyncImageList::Fill()
{
	::EnterCriticalSection( &criticalSection );
	int nImageCount = ImageList_GetImageCount( hImageList );
	::LeaveCriticalSection( &criticalSection );

	int nImageIndex = indicesHolder.GetStartIndex( 0 );
	while ( nImageIndex != CIndicesHolder::INVALID_INDEX )
	{
		DWORD dwStatus = Fill( nImageIndex );
			
		::EnterCriticalSection( &criticalSection );
		for ( std::unordered_map<int, IAsyncImageListCallback* >::iterator callbackIterator = callbackHashMap.begin(); callbackIterator != callbackHashMap.end(); ++callbackIterator )
		{
			callbackIterator->second->Callback( nImageIndex, dwStatus );
		}
		::LeaveCriticalSection( &criticalSection );
			
		if ( dwStatus != ERROR_SUCCESS )
		{
			return dwStatus;
		}

		if ( WaitForSingleObject( hExitEvent, 0 ) == WAIT_OBJECT_0 )
		{
			return ERROR_CANCELLED;
		}

		if ( WaitForSingleObject( hForceStartEvent, 0 ) == WAIT_OBJECT_0 )
		{
			::ResetEvent( hForceStartEvent );
			::EnterCriticalSection( &criticalSection );
			if ( ( nForceStartImageIndex >= 0 ) && ( nForceStartImageIndex < nImageCount ) )
			{
				nImageIndex = indicesHolder.GetStartIndex( nForceStartImageIndex );
			}
			else
			{
				nImageIndex = indicesHolder.GetNextIndex();
			}
			::LeaveCriticalSection( &criticalSection );
		}
		else
		{
			nImageIndex = indicesHolder.GetNextIndex();
		}
	}
	return ERROR_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAsyncImageList::Create( int nImageCount, const CTPoint<int> &rImageSize )
{
	Clear();
	NI_ASSERT_TF( ( nImageCount > 0 ) && ( ( rImageSize.x * rImageSize.y ) > 0 ),
							  NStr::Format( "Wrong parameters: %d, (%d, %d)\n", nImageCount, rImageSize.x, rImageSize.y ),
							  return false );
  IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get ImageProcessor\n" ),
							  return false );

	hImageList = ::ImageList_Create( rImageSize.x, rImageSize.y, ILC_COLOR24, nImageCount, nImageCount );
	hSmallImageList = ::ImageList_Create( ::GetSystemMetrics( SM_CXSMICON ), ::GetSystemMetrics( SM_CYSMICON ), ILC_COLOR24, nImageCount, nImageCount );
	if ( !hImageList || !hSmallImageList )
	{
		Clear();
		return false;
	}

	SColor whiteColor( 0xFF, 0xFF, 0xFF, 0xFF );
		
	DWORD dwTime = GetTickCount();

	CPtr<IImage> pImage = pImageProcessor->CreateImage( rImageSize.x , rImageSize.y );
	CPtr<IImage> pSmallImage = pImageProcessor->CreateImage( ::GetSystemMetrics( SM_CXSMICON ), ::GetSystemMetrics( SM_CYSMICON ) );
	pImage->Set( whiteColor );
	pSmallImage->Set( whiteColor );
	
	dwTime = GetTickCount() - dwTime;
	//NStr::DebugTrace( "CAsyncImageList::Create(): image %d\n", dwTime );
	dwTime = GetTickCount();

	HDC hDC = ::GetDC( ::GetDesktopWindow() );
	HBITMAP hBitmap = CreateCompatibleBitmap( hDC, rImageSize.x, rImageSize.y );
	HBITMAP hSmallBitmap = CreateCompatibleBitmap( hDC, ::GetSystemMetrics( SM_CXSMICON ), ::GetSystemMetrics( SM_CYSMICON ) );

	dwTime = GetTickCount() - dwTime;
	//NStr::DebugTrace( "CAsyncImageList::Create(): bitmap1 %d\n", dwTime );
	dwTime = GetTickCount();

	//������� HBITMAP ����� ���������� � � image list
	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize  = sizeof( bitmapInfo.bmiHeader );
	bitmapInfo.bmiHeader.biWidth  = rImageSize.x;
	bitmapInfo.bmiHeader.biHeight = -rImageSize.y;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	::SetDIBits( hDC, hBitmap, 0, rImageSize.y, pImage->GetLFB(), &bitmapInfo, DIB_RGB_COLORS );
	bitmapInfo.bmiHeader.biWidth  = ::GetSystemMetrics( SM_CXSMICON );
	bitmapInfo.bmiHeader.biHeight = -::GetSystemMetrics( SM_CYSMICON );
	::SetDIBits( hDC, hBitmap, 0, rImageSize.y, pSmallImage->GetLFB(), &bitmapInfo, DIB_RGB_COLORS );
	
	::ReleaseDC( ::GetDesktopWindow(), hDC );

	dwTime = GetTickCount() - dwTime;
	//NStr::DebugTrace( "CAsyncImageList::Create(): bitmap2 %d\n", dwTime );
	dwTime = GetTickCount();

	for ( int nImageIndex = 0; nImageIndex < nImageCount; ++nImageIndex )
	{
		
		int nResult0 = ImageList_Add( hImageList, hBitmap, 0 );
		int nResult1 = ImageList_Add( hSmallImageList, hSmallBitmap, 0 );

		NI_ASSERT_T( ( nResult0 >= 0 ) && ( nResult1 >= 0 ), 
								 NStr::Format("Can't add image to imagelist: %d to %d\n", nImageIndex, nImageCount ) );
	}

	dwTime = GetTickCount() - dwTime;
	//NStr::DebugTrace( "CAsyncImageList::Create(): imageList %d\n", dwTime );

	::DeleteObject( hBitmap );
	::DeleteObject( hSmallBitmap );

	indicesHolder.Create( CTPoint<int>( 0, nImageCount ) );

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAsyncImageList::AsyncFill()
{
	if ( IsFilling() )
	{
		//return ERROR_ALREADY_INITIALIZED;
		return true;
	}
	::ResetEvent( hExitEvent );
	DWORD dwThreadId;
	hThread = ::CreateThread( NULL, 0, AsyncImageListThreadFunc, reinterpret_cast<PVOID>( this ), CREATE_SUSPENDED, &dwThreadId );
	if ( hThread == 0 )
	{
		//return ERROR_INVALID_HANDLE;
		return false;
	}
	::ResumeThread( hThread );
	//return ERROR_SUCCESS;
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAsyncImageList::IsFilling()
{
	if ( !hThread )
	{
		return false;
	}
	DWORD dwStatus = ERROR_SUCCESS;
	::GetExitCodeThread( hThread, &dwStatus );
	return ( dwStatus == STILL_ACTIVE );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAsyncImageList::StopFilling()
{
	DWORD dwStatus = ERROR_SUCCESS;
	if ( IsFilling() )
	{
		::SetEvent( hExitEvent );
		::WaitForSingleObject( hThread , INFINITE );
	}
	if ( hThread )
	{
		::GetExitCodeThread( hThread, &dwStatus );
		::CloseHandle( hThread );
		hThread = 0;
	}
	//return dwStatus;
	return ( dwStatus == ERROR_SUCCESS );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CAsyncImageList::AddCallback( IAsyncImageListCallback *pCallback )
{
	NI_ASSERT_TF( pCallback != 0,
								NStr::Format( "Wrong parameter: %x,\n", pCallback ),
								return -1 );
	
	int nKey = rand();

	::EnterCriticalSection( &criticalSection );
	while( callbackHashMap.find( nKey ) != callbackHashMap.end() )
	{
		nKey = rand();
	}
	callbackHashMap[nKey] = pCallback;
	::LeaveCriticalSection( &criticalSection );
	
	return nKey;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAsyncImageList::RemoveCallback( int nKey )
{
	::EnterCriticalSection( &criticalSection );
	if ( callbackHashMap.find( nKey ) != callbackHashMap.end() )
	{
		callbackHashMap.erase( nKey );
	}
	::LeaveCriticalSection( &criticalSection );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAsyncImageList::ForceFillFrom( int nImageIndex )
{
	if ( IsFilling() )
	{
		::EnterCriticalSection( &criticalSection );
		nForceStartImageIndex = nImageIndex;
		::LeaveCriticalSection( &criticalSection );
		::SetEvent( hForceStartEvent );
		return true;
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
