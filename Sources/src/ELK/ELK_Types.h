#if !defined(__ELK_TYPES__)
#define __ELK_TYPES__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\RandomMapGen\Registry_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\Formats\fmtFont.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SELKTextProperty
{
	enum STATE
	{
		STATE_NOT_TRANSLATED		= 0,
		STATE_OUTDATED					= 1,
		STATE_TRANSLATED				= 2,
		STATE_APPROVED					= 3,
		STATE_COUNT							= 4,
	};
	static LPCTSTR STATE_LABELS[STATE_COUNT];
	static LPCTSTR STATE_NAMES[STATE_COUNT];
	int nState;
	bool bTranslated;
	
	SELKTextProperty() : nState( STATE_NOT_TRANSLATED ), bTranslated( false ) {}
	SELKTextProperty( const SELKTextProperty &rELKTextProperty ) : nState( rELKTextProperty.nState ), bTranslated( rELKTextProperty.bTranslated ) {}
	SELKTextProperty& operator=( const SELKTextProperty &rELKTextProperty )
	{
		if( &rELKTextProperty != this )
		{
			nState = rELKTextProperty.nState;
			bTranslated = rELKTextProperty.bTranslated;
		}
		return *this;
	}	
	
	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};

struct SELKDescription
{
	std::string szName;					// ��� � ������
	std::string szPAKName;			// ������������� ��� �����, ���� ���������� ������������� ������������ ����
	std::string szUPDPrefix;		// ������� � ������ UPD ������
	bool bGenerateFonts;				// �������� ��� ��� �����

	SELKDescription() : bGenerateFonts( false ) {}
	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SELKElement
{
public:
	static const TCHAR DATA_BASE_FOLDER[];
	static const TCHAR DATA_BASE_RESERVE_FOLDER[];

	//�������� ������� � ����� �� ����������
	static void GetDataBaseFolder( const std::string &rszELKPath, std::string *pszDataBaseFolder );
	static void GetDataBaseReserveFolder( const std::string &rszELKPath, std::string *pszDataBaseReserveFolder );
	void GetDataBaseFolder( std::string *pszDataBaseFolder ) const;
	void GetDataBaseReserveFolder( std::string *pszDataBaseReserveFolder ) const;

	SELKDescription description;
	std::string szPath;					//��� ����������!
	std::string szVersion;
	int nLastUpdateNumber;

	SELKElement() : nLastUpdateNumber( -2 ){}
	SELKElement( const SELKElement &rELKElement )
		: description( rELKElement.description ),
			szPath( rELKElement.szPath ),
			szVersion( rELKElement.szVersion ),
			nLastUpdateNumber( rELKElement.nLastUpdateNumber ) {}
	SELKElement& operator=( const SELKElement &rELKElement )
	{
		if( &rELKElement != this )
		{
			description = rELKElement.description;
			szPath = rELKElement.szPath;
			szVersion = rELKElement.szVersion;
			nLastUpdateNumber = rELKElement.nLastUpdateNumber;
		}
		return *this;
	}	

	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SELKElementStatistic
{
	struct SState 
	{
		int nTextsCount;
		int nWordsCount;
		int nWordSymbolsCount;
		int nSymbolsCount;

		SState() : nTextsCount( 0 ), nWordsCount( 0 ), nWordSymbolsCount( 0 ), nSymbolsCount( 0 ) {}
		SState( const SState &rState )
			: nTextsCount( rState.nTextsCount ),
				nWordsCount( rState.nWordsCount ),
				nWordSymbolsCount( rState.nWordSymbolsCount ),
				nSymbolsCount( rState.nSymbolsCount ) {}
		SState& operator=( const SState &rState )
		{
			if( &rState != this )
			{
				nTextsCount = rState.nTextsCount;
				nWordsCount = rState.nWordsCount;
				nWordSymbolsCount = rState.nWordSymbolsCount;
				nSymbolsCount = rState.nSymbolsCount;
			}
			return *this;
		}	

		virtual int STDCALL operator&( IStructureSaver &ss );
		virtual int STDCALL operator&( IDataTree &ss );
	};

	std::vector<SState> states; //�� �������

	SELKElementStatistic()
	{
		states.resize( SELKTextProperty::STATE_COUNT );
	}
	SELKElementStatistic( const SELKElementStatistic &rELKElementStatistic )
		: states( rELKElementStatistic.states )
	{
		states.resize( SELKTextProperty::STATE_COUNT );
	}
	SELKElementStatistic& operator=( const SELKElementStatistic &rELKElementStatistic )
	{
		if( &rELKElementStatistic != this )
		{
			states = rELKElementStatistic.states;
			states.resize( SELKTextProperty::STATE_COUNT );
		}
		return *this;
	}	

	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SELKStatistic
{
	std::vector<SELKElementStatistic> original; //�� ���������
	std::vector<SELKElementStatistic> translation; //�� ���������
	bool bValid;

	SELKStatistic() : bValid( false ) {}
	SELKStatistic( const SELKStatistic &rELKStatistic )
		: original( rELKStatistic.original ),
			translation( rELKStatistic.translation ),
			bValid( rELKStatistic.bValid ) {}
	SELKStatistic& operator=( const SELKStatistic &rELKStatistic )
	{
		if( &rELKStatistic != this )
		{
			original = rELKStatistic.original;
			translation = rELKStatistic.translation;
			bValid = rELKStatistic.bValid;
		}
		return *this;
	}	

	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );

	void Clear()
	{
		original.clear();
		translation.clear();
		bValid = false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CELK
{
public:
	static void ToText( const std::vector<BYTE> &rBuffer, CString *pstrText, int nCodePage, bool bRemove_0D = false );
	static void FromText( const CString &rstrText, std::vector<BYTE> *pBuffer, int nCodePage, bool bAdd_0D = false );

public:
	static const int DEFAULT_CODE_PAGE;
	static const TCHAR	PAK_FILE_NAME[];
	static const TCHAR	ELK_FILE_NAME[];
	static const TCHAR FOLDER_DESC_FILE_NAME[];

	static const TCHAR GAME_REGISTRY_FOLDER[];
	static const TCHAR GAME_REGISTRY_KEY[];
	static const TCHAR TEXTS_PAK_FILE_NAME[];
	static const TCHAR GAME_FILE_NAME[];
	static const TCHAR GAME_PARAMETERS[];
	static const TCHAR TEMP_FOLDER[];
	static const TCHAR GAME_DATA_FOLDER[];

	static const TCHAR PAK_DESCRIPTION[];

	static const TCHAR PAK_EXTENTION[];
	static const TCHAR UPD_EXTENTION[];
	static const TCHAR XML_EXTENTION[];
	static const TCHAR ELK_EXTENTION[];
	static const TCHAR TXT_EXTENTION[];
	static const TCHAR DSC_EXTENTION[];
	static const TCHAR PAK_DESCRIPTION_EXTENTION[];

	static const TCHAR ZIP_EXE[];
	static const TCHAR ELK_CHM[];

public:
	std::vector<SELKElement> elements;
	std::unordered_map<std::string, int> elementNames; //�� �������������, ����������� � Open
	std::string szPath;	//� �����������
	SELKStatistic statistic; //���������� ���������� �� ����� ����.
	SELKStatistic previousStatistic; //���������� ����������
	SEnumFolderStructureParameter enumFolderStructureParameter;
	
public:
	//������ �� �����������
	bool IsOpened() { return ( !szPath.empty() ); }
	bool Open( const std::string &rszELKPath, bool bEnumFiles );
	bool Save();
	void Close();
	
	//���������� ���������� ��������!!
	//std::string szFileName = szFileName.substr( 0, szFileName.rfind( '.' ) );
	//�������� ��������� ������ � UNICODE
	static void GetOriginalText  ( const std::string &rszTextPath, CString *pstrText, int nCodePage, bool bRemove_0D = false );
	static void GetTranslatedText( const std::string &rszTextPath, CString *pstrText, int nCodePage, bool bRemove_0D = false );
	static void GetDescription   ( const std::string &rszTextPath, CString *pstrText, int nCodePage, bool bRemove_0D = false );
	static int GetState( const std::string &rszTextPath, bool *pbTranslated );

	//���������� ���������� ��������!!
	//std::string szFileName = szFileName.substr( 0, szFileName.rfind( '.' ) );
	//���������� ��������� ������ � UNICODE
	static void SetTranslatedText( const std::string &rszTextPath, const CString &rstrText, int nCodePage, bool bAdd_0D = false );
	static int SetState( const std::string &rszTextPath, int nState, bool *pbTranslated ); //���������� ���������� �����
	
	//������� PAK ����
	static bool CreatePAK( const std::string &rszGamePath, const std::string &rszFilePath, const std::string &rszZIPToolPath, class CProgressDialog* pwndProgressDialog = 0 );
	//���������� ������������ ������ ( APPROVED )
	//������ ��� SELKElement
	static bool ExportToPAK( const std::string &rszELKPath,
													 const std::string &rszPAKPath,
													 const std::string &rszZIPToolPath,
													 class CELKTreeWindow *pwndELKTreeWindow,
													 bool bOnlyFilled,
													 bool bGenerateFonts,
													 const CString &rstrFontName,
													 DWORD dwNormalFontSize,
													 DWORD dwLargeFontSize,
													 int nCodePage,
													 class CProgressDialog* pwndProgressDialog = 0,
													 const struct SSimpleFilter *pELKFilter = 0 );
	static bool ImportFromPAK( const std::string &rszPAKPath, const std::string &rszELKPath, bool bAbsolute, std::string *pszNewVersion, class CProgressDialog* pwndProgressDialog = 0 );

	//����������� PAK � ELK c �������� ���������
	//��� ����� CELK ( ������� �������� ����� SELKElement )
	static bool ExportToXLS( const CELK &rELK, const std::string &rszXLSPath, class CELKTreeWindow *pwndELKTreeWindow, int nCodePage, class CProgressDialog* pwndProgressDialog = 0 );
	static bool ImportFromXLS( const CELK &rELK, const std::string &rszXLSPath, std::string *pszNewVersion, int nCodePage, class CProgressDialog* pwndProgressDialog = 0 );

	static bool CreateStatistic( SELKStatistic *pStatistic, class CELKTreeWindow *pwndELKTreeWindow, int nCodePage, class CProgressDialog* pwndProgressDialog = 0 );

	//����� � ������������ ��� ����, ������� � ����������
	static bool UpdateELK( const std::string &rszPath, const std::string &rszPAKFileName, class CProgressDialog* pwndProgressDialog = 0 );
	//����� ���� �� ���������� ����� � �������� �� � ����, ��� ���� ��������� ���� ��� ���
	static void UpdateGame( const CELK &rELK,
													const std::string &rszZIPToolPath,
													class CELKTreeWindow *pwndELKTreeWindow,
													bool bRunGame,
													const CString &rstrFontName,
													DWORD dwNormalFontSize,
													DWORD dwLargeFontSize,
													int nCodePage,
													class CProgressDialog* pwndProgressDialog = 0 );

	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::list<std::string> TSimpleFilterItem;
typedef std::list<TSimpleFilterItem> TSimpleFilter;
struct SSimpleFilter
{
	TSimpleFilter filter;
	bool bTranslated;
	std::vector<int> states;

	SSimpleFilter() : bTranslated( false )
	{
		states.resize( SELKTextProperty::STATE_COUNT, true );
	}
	SSimpleFilter( const SSimpleFilter &rSimpleFilter ) : filter( rSimpleFilter.filter ), bTranslated( rSimpleFilter.bTranslated ), states( rSimpleFilter.states )
	{
		states.resize( SELKTextProperty::STATE_COUNT, true );
	}
	SSimpleFilter& operator=( const SSimpleFilter &rSimpleFilter )
	{
		if( &rSimpleFilter != this )
		{
			filter = rSimpleFilter.filter;
			bTranslated = rSimpleFilter.bTranslated;
			states = rSimpleFilter.states;
			states.resize( SELKTextProperty::STATE_COUNT, true );
		}
		return *this;
	}	

	bool Check( const std::string &rszFolder, bool _bTranslated, int nState ) const;

	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};
typedef std::unordered_map<std::string, SSimpleFilter> TFilterHashMap;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CFontInfo
//      This class stores information about the currently loaded font.
//      This includes TEXTMETRIC, ABCs, Kerning pairs and estimated texture size for the font letters image
struct SFontInfo
{
  HFONT hFont;													// HFONT used to draw with this font
  TEXTMETRIC tm;												// text metrics, e.g. character height
	std::vector<ABC> abc;									// character ABC widths
	std::vector<KERNINGPAIR> kps;					// kernging pairs
	int nTextureSizeX, nTextureSizeY;			// estimated texture size
	std::unordered_map<WORD, WORD> translate;	// ANSI => UNICODE translation table
	//
	WORD Translate( WORD code ) const
	{
		std::unordered_map<WORD, WORD>::const_iterator pos = translate.find( code );
		ASSERT( pos != translate.end() );
		return pos->second;
	}

  //
  SFontInfo() : hFont( 0 ), nTextureSizeX( 0 ), nTextureSizeY( 0 ) {  }
  virtual ~SFontInfo() { if ( hFont ) DeleteObject( hFont ); }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SKPZeroFunctional
{
  const std::unordered_map<WORD, WORD> *pTranslate;
	SKPZeroFunctional() : pTranslate( 0 )	{}
	SKPZeroFunctional( const std::unordered_map<WORD, WORD> *_pTranslate )
	{
		pTranslate = _pTranslate;
	}

	bool operator()( const KERNINGPAIR &kp ) const
	{ 
		if ( pTranslate->find( kp.wFirst ) == pTranslate->end() )
		{
			return true;
		}
		if ( pTranslate->find( kp.wSecond ) == pTranslate->end() )
		{
			return true;
		}
		return kp.iKernAmount == 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define FONTS_COUNT ( 4 )
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFontGen
{
public:
	static const int LEADING_PIXELS;
	static const TCHAR FONT_NAME[];
	static const TCHAR FONT_FILE_NAME[];
	static const TCHAR FONT_FILE_EXTENTION[];

	static const DWORD FONTS_SIZE[];
	static LPCTSTR FONTS_FOLDER[];

public:
	// estimate, is requested number of chars fit in the selected texture
	static bool IsFit( const SFontInfo &fi, DWORD dwNumChars, DWORD dwSizeX, DWORD dwSizeY );
	// estimate optimal texture size
	static bool EstimateTextureSize( SFontInfo &fi, DWORD dwNumChars );
	// Fills SFontInfo fi with text metrics, char widths and kerning pairs
	// -> hdc: HDC that the font is currently selected into
	static void MeasureFont( HDC hdc, SFontInfo &fi, std::vector<WORD> &chars, bool bSingleByte, int nCodePage );
	// create font object and estimate metrics and texture size
	static void LoadFont( HWND hWnd,
												SFontInfo &fi,
												int nHeight,
												int nWeight,
												bool bItalic,
												int nCodePage, 
												bool bAntialias,
												DWORD dwPitch,
												const CString &strFaceName,
												bool bSingleByte,
												std::vector<WORD> &chars );
	// draw font in the DC
	static bool DrawFont( HDC hdc, const SFontInfo &fi, const std::vector<WORD> &chars );
	// draw font in the DC, extract bitmap from the DC and convert it to the Image
	static IImage* CreateFontImage( const SFontInfo &fi, const std::vector<WORD> &chars );
	
	// create and fill SFontFormat with the complete data for the font
	static SFontFormat* CreateFontFormat( const std::string &szFaceName, const SFontInfo &fi, const std::vector<WORD> &chars );

	static bool GenerateFont( const std::string &rszFolder,
														const std::set<WORD> &rSymbols,
														bool bSingleByte,
														const CString &rstrFontName,
														DWORD dwNormalFontSize,
														DWORD dwLargeFontSize,
														int nCodePage );

	static int GetFonts( DWORD dwCharacterSet, std::set<CString> *pFontsList );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMainFrameParams
{
	struct SSearchParam
	{
		enum WINDOW_TYPE
		{
			WT_ORIGINAL			= 0,
			WT_DESCRIPTION	= 1,
			WT_TRANSLATION	= 2,
			WT_COUNT				= 3,
		};

		bool bFindDown;
		bool bFindMatchCase;
		bool bFindWholeWord;

		CString strFindText;

		int nWindowType;
		int nPosition;

		SSearchParam() : bFindDown( true ), bFindMatchCase( false ), bFindWholeWord( false ), nWindowType( WT_ORIGINAL ), nPosition( 0 ) {}

		// serializing...
		virtual int STDCALL operator&( IStructureSaver &ss );
		virtual int STDCALL operator&( IDataTree &ss );
	};
	
	static int INVALID_FILTER_NUMBER;	
	
	std::string szCurrentFolder;
	std::string szZIPToolPath;
	std::string szHelpFilePath;
	bool bCollapseItem;

	std::string szLastOpenedELKName;
	std::string szLastOpenedPAKName;
	std::string szLastOpenedXLSName;
	std::string szPreviousPath;
	std::string szLastPath;
	int nLastELKElement;
	std::list<std::string> recentList;

	TFilterHashMap filters;
	std::string szCurrentFilterName;

	int nCodePage;

	bool bFullScreen;
	CTRect<int> rect;

	std::string szLastOpenedPAKShortName;
	std::string szGameFolder;

	CString strFontName;
	DWORD dwNormalFontSize;
	DWORD dwLargeFontSize;
	
	SSearchParam searchParam;

	SMainFrameParams()
		: nLastELKElement( 0 ),
			bCollapseItem( false ),
			nCodePage( CELK::DEFAULT_CODE_PAGE ),
			bFullScreen( false ),
			strFontName( CFontGen::FONT_NAME ),
			dwNormalFontSize( CFontGen::FONTS_SIZE[2] ),
			dwLargeFontSize( CFontGen::FONTS_SIZE[3] )
	{}

	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );

	void ValidatePath( std::string *pszPath, bool bFolder );
	void LoadFromRegistry( const std::string &rszRegistryKey, bool bShortApperence );
	void SaveToRegistry( const std::string &rszRegistryKey, bool bShortApperence );

	const SSimpleFilter* GetCurrentFilter() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // !defined(__ELK_TYPES__)
