#include "StdAfx.h"
#include <mmsystem.h>

#include "RandomGenInternal.h"
#include "..\Misc\FileUtils.h"
#include "..\StreamIO\StreamIOHelper.h"
CStreamAccessor& operator>>( CStreamAccessor &stream, SRandData &rnd )
{
	stream >> rnd.randcnt;
	stream->Read( &(rnd.randrsl[0]), sizeof(rnd.randrsl) );
	stream->Read( &(rnd.randmem[0]), sizeof(rnd.randmem) );
	stream >> rnd.randa;
	stream >> rnd.randb;
	stream >> rnd.randc;
	return stream;
}
CStreamAccessor& operator<<( CStreamAccessor &stream, SRandData &rnd )
{
	stream << rnd.randcnt;
	stream->Write( &(rnd.randrsl[0]), sizeof(rnd.randrsl) );
	stream->Write( &(rnd.randmem[0]), sizeof(rnd.randmem) );
	stream << rnd.randa;
	stream << rnd.randb;
	stream << rnd.randc;
	return stream;
}
#define ind(mm,x)  (*(unsigned _int32 *)(( unsigned _int8 *)(mm) + ((x) & ((RANDSIZ-1)<<2))))
#define rngstep(mix,a,b,mm,m,m2,r,x) \
{ \
  x = *m;  \
  a = (a ^ (mix)) + *(m2++); \
  *(m++) = y = ind( mm, x ) + a + b; \
  *(r++) = b = ind( mm, y >> RANDSIZL ) + x; \
}
#define mix(a,b,c,d,e,f,g,h) \
{ \
   a ^= b << 11; d += a; b += c; \
   b ^= c >> 2;  e += b; c += d; \
   c ^= d << 8;  f += c; d += e; \
   d ^= e >> 16; g += d; e += f; \
   e ^= f << 10; h += e; f += g; \
   f ^= g >> 4;  a += f; g += h; \
   g ^= h << 8;  b += g; h += a; \
   h ^= a >> 9;  c += h; a += b; \
}
void Isaac( SRandData *pRnd )
{
	unsigned _int32 a, b, x, y, *m, *mm, *m2, *r, *mend;
	mm = pRnd->randmem; 
	r = pRnd->randrsl;
	a = pRnd->randa; 
	b = pRnd->randb + ( ++pRnd->randc );
	for ( m = mm, mend = m2 = m+(RANDSIZ/2); m<mend; )
	{
		rngstep( a<<13, a, b, mm, m, m2, r, x );
		rngstep( a>>6 , a, b, mm, m, m2, r, x );
		rngstep( a<<2 , a, b, mm, m, m2, r, x );
		rngstep( a>>16, a, b, mm, m, m2, r, x );
	}
	for ( m2 = mm; m2<mend; )
	{
		rngstep( a<<13, a, b, mm, m, m2, r, x );
		rngstep( a>>6 , a, b, mm, m, m2, r, x );
		rngstep( a<<2 , a, b, mm, m, m2, r, x );
		rngstep( a>>16, a, b, mm, m, m2, r, x );
	}
	pRnd->randb = b; 
	pRnd->randa = a;
}
void CRandomGenerator::Init()
{
	if ( bIsReady )
		return;

	CPtr<CRandomGenSeed> pSeed = new CRandomGenSeed();
	pSeed->Init();
	SetSeed( pSeed );
	bIsReady = TRUE;
}
void CRandomGenerator::SetSeed( IRandomGenSeed *pSeed )
{
	if ( CRandomGenSeed *pRGS = dynamic_cast<CRandomGenSeed*>(pSeed) )
		rnd = pRGS->GetRandData();
	else
	{
		NI_ASSERT_T( false, "Wrong class as a random seed" );
	}
}
IRandomGenSeed* CRandomGenerator::GetSeed()
{
	CRandomGenSeed *pSeed = new CRandomGenSeed();
	pSeed->SetRandData( rnd );
	return pSeed;
}
int CRandomGenerator::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bIsReady );
	saver.Add( 2, &rnd.randcnt );
	saver.AddRawData( 3, &(rnd.randrsl[0]), sizeof(rnd.randrsl) );
	saver.AddRawData( 4, &(rnd.randmem[0]), sizeof(rnd.randmem) );
	saver.Add( 5, &rnd.randa );
	saver.Add( 6, &rnd.randb );
	saver.Add( 7, &rnd.randc );
	return 0;
}
void CRandomGenerator::Store( IDataStream *pStream )
{
	CStreamAccessor stream = pStream;
	stream << bIsReady;
	stream << rnd;
}
void CRandomGenerator::Restore( IDataStream *pStream )
{
	CStreamAccessor stream = pStream;
	stream >> bIsReady;
	stream >> rnd;
}
void CRandomGenSeed::InitVariables()
{
	rnd.randa = rnd.randb = rnd.randc = 0;
	unsigned _int32 *m = rnd.randmem;
	unsigned _int32 *r = rnd.randrsl;
	unsigned _int32 a, b, c, d, e, f, g, h;
	a = b = c = d = e = f = g = h = 0x9e3779b9;  // the golden ratio
	for ( int i = 0; i < 4; ++i )
		mix( a, b, c, d, e, f, g, h );
	for ( int i = 0; i < RANDSIZ; i += 8 )
	{
		a+=r[i  ]; b+=r[i+1]; c+=r[i+2]; d+=r[i+3];
		e+=r[i+4]; f+=r[i+5]; g+=r[i+6]; h+=r[i+7];
		mix( a, b, c, d, e, f, g, h );
		m[i  ]=a; m[i+1]=b; m[i+2]=c; m[i+3]=d;
		m[i+4]=e; m[i+5]=f; m[i+6]=g; m[i+7]=h;
	}
	for ( int i = 0; i < RANDSIZ; i += 8 )
	{
		a+=m[i  ]; b+=m[i+1]; c+=m[i+2]; d+=m[i+3];
		e+=m[i+4]; f+=m[i+5]; g+=m[i+6]; h+=m[i+7];
		mix( a, b, c, d, e, f, g, h );
		m[i  ]=a; m[i+1]=b; m[i+2]=c; m[i+3]=d;
		m[i+4]=e; m[i+5]=f; m[i+6]=g; m[i+7]=h;
	}
	Isaac( &rnd );
	rnd.randcnt = RANDSIZ;		
}
void CRandomGenSeed::Init()
{
	if ( GetGlobalVar( "fixrandom", 0 ) != 0 )
		Zero( rnd.randrsl );
	else
		FillRandRsl();

	InitVariables();
}
void CRandomGenSeed::InitByZeroSeed()
{
	Zero( rnd.randrsl );
	InitVariables();
}
const int N_FROM_START = 1024;
bool CRandomGenSeed::RecFindFile( LPSTR pszFindedName, LPCSTR pszBaseMask, int nToFind, int* pnTotFinded )
{
	for ( NFile::CFileIterator fileFind( pszBaseMask ); !fileFind.IsEnd(); ++fileFind )
	{
		if ( fileFind.IsDots() || fileFind.IsSystem() || fileFind.IsHidden() )
			continue;
		if ( fileFind.IsDirectory() )
		{
			if ( RecFindFile( pszFindedName, (fileFind.GetFilePath() + "\\*.*").c_str(), nToFind, pnTotFinded ) == TRUE )
				return true;
			continue;
		}
		if ( *pnTotFinded >= nToFind )
		{
			if ( fileFind.GetLength() >= N_FROM_START + sizeof(rnd.randrsl) )
			{
				strcpy( pszFindedName, fileFind.GetFilePath().c_str() );
				return true;
			}
			( *pnTotFinded )--;
		}
		( *pnTotFinded )++;
	}
	return false;
}
void CRandomGenSeed::FillRandRsl()
{
	char buf[1024];
	char pszMaskToFindFiles[256];
	int nSize = GetLogicalDriveStrings( sizeof(buf), buf );
	int i;
	for ( i = 0; i < nSize; )
	{
		char* pDrive = buf + i;
		i += strlen( pDrive ) + 1;
		if ( GetDriveType(pDrive) == DRIVE_FIXED || GetDriveType(pDrive) == DRIVE_REMOTE )
		{
			strcpy( pszMaskToFindFiles, pDrive );
			strcat( pszMaskToFindFiles, "*.*" );
			break;
		}
	}
	if ( i == nSize )
		return; // cannot find any hd, run without initialization
	srand( timeGetTime() );
	char pszFindedName[256];
	BOOL bSuccess = FALSE;
	
	while ( !bSuccess )
	{
		int nTotFinded = 0;
		if ( !RecFindFile( pszFindedName, pszMaskToFindFiles, rand() % 512 + 1, &nTotFinded ) )
		{
			int nToFind = rand() % ( nTotFinded - 1 ) + 1;
			nTotFinded = 0;
			if ( !RecFindFile( pszFindedName, pszMaskToFindFiles, nToFind, &nTotFinded ) )
				continue;
		}
		bSuccess = TRUE;
		OFSTRUCT ofStruct;
		Zero( ofStruct );
		ofStruct.cBytes = sizeof( ofStruct );
		HFILE hFile = OpenFile( pszFindedName, &ofStruct, OF_READ | OF_SHARE_DENY_NONE );
		if ( hFile != HFILE_ERROR )
		{
			srand( timeGetTime() );
			SetFilePointer( HANDLE( (INT_PTR)hFile ), N_FROM_START - rand() % ( N_FROM_START - 512 ), 0, FILE_BEGIN );
			DWORD dwReadBytes = 0;
			if ( ReadFile( HANDLE( (INT_PTR)hFile ), rnd.randrsl, sizeof(rnd.randrsl), &dwReadBytes, 0 ) != TRUE || (dwReadBytes != sizeof(rnd.randrsl)) )
				bSuccess = FALSE;
			CloseHandle( HANDLE( (INT_PTR)hFile ) );
		}
		else
			bSuccess = FALSE;
		BOOL bHaveNotZero = FALSE;
		for ( int i = 0; i < RANDSIZ; i++ )
		{
			if ( rnd.randrsl[i] )
			{
				bHaveNotZero = TRUE;
				break;
			}
		}
		if ( bHaveNotZero == FALSE )
		{
			bSuccess = FALSE;
			Sleep( 10 );
		}
		for ( i = 0; i < RANDSIZ; i++ )
			rnd.randrsl[i] ^= rand();
	}
}
int CRandomGenSeed::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &rnd.randcnt );
	saver.AddRawData( 2, &(rnd.randrsl[0]), sizeof(rnd.randrsl) );
	saver.AddRawData( 3, &(rnd.randmem[0]), sizeof(rnd.randmem) );
	saver.Add( 4, &rnd.randa );
	saver.Add( 5, &rnd.randb );
	saver.Add( 6, &rnd.randc );
	return 0;
}
int CRandomGenSeed::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "RandCounter", &rnd.randcnt );
	saver.Add( "RandA", &rnd.randa );
	saver.Add( "RandB", &rnd.randb );
	saver.Add( "RandC", &rnd.randc );
	saver.AddRawData( "RandRSL", &(rnd.randrsl[0]), sizeof(rnd.randrsl) );
	saver.AddRawData( "RandMem", &(rnd.randmem[0]), sizeof(rnd.randmem) );
	return 0;
}
void CRandomGenSeed::Store( IDataStream *pStream )
{
	CStreamAccessor stream = pStream;
	stream << rnd;
}
void CRandomGenSeed::Restore( IDataStream *pStream )
{
	CStreamAccessor stream = pStream;
	stream >> rnd;
}
