#include "StdAfx.h"
#include <iostream>
#include "ExcelExporter.h"
#include "..\Misc\FileUtils.h"

std::vector<SExtension> extensions;			//global

void ShowProgramInfo()
{
	cout << "Using:" << endl;
	cout << "Export mode: <*.project extensions> <folder with projects> <output excel file> [-i] [crap file] [-nr]" << endl;
	cout << "Import mode: <excel file>" << endl;
	cout << "Where [-nr] is non-recursive flag" << endl;
	cout << "[-i] means that strings in crap file will be ignored, else only that strings will be exported" << endl;
	cout << "[crap file] is a file with names of fields; meanings depends about presence of flag [-i]" << endl;
	cout << endl;
	cout << "Export example: export.exe *.msh c:\\a7\\data\\units\\humans\\german out.xls crap.txt" << endl;
	cout << "Import example: export.exe out.xls" << endl;
}

class CGetAllFiles
{
	vector<string> *pFileVector;
	string szMask;
public:
	CGetAllFiles( vector<string> *pFiles ) { pFileVector = pFiles; }
	~CGetAllFiles() {}

	void operator() ( const NFile::CFileIterator &it )
	{
		if ( !it.IsDirectory() )
		{
			pFileVector->push_back( it.GetFilePath() );
		}
	}
};

int main(int argc, char* argv[])
{
	extensions.push_back( SExtension( "*.bld", "desc" ) );
	extensions.push_back( SExtension( "*.obt", "desc" ) );
	extensions.push_back( SExtension( "*.eff", "effect" ) );
	
	if ( argc < 2 || argc > 6 )
	{
		cout << "Invalid number of params (too low or too high number)" << endl;
		ShowProgramInfo();
		return -1;
	}

	bool bImportMode = false;
	bool bRecursive = true;
	bool bIgnoreFields = false;
	string szExcelFileName;
	string szFolderName;
	string szFileMask;
	string szCrapFile;

	do
	{
		string szFirst = argv[1];
		string szExtention = szFirst.substr( szFirst.rfind('.') );
		if ( szExtention == ".txt" )
		{
			if ( argc != 2 )
			{
				cout << "Too many params for import mode" << endl;
				ShowProgramInfo();
				return -1;
			}

			szExcelFileName = argv[1];
			bImportMode = true;
			break;
		}
		else
		{
			if ( argc < 4 )
			{
				cout << "Too few params for export mode" << endl;
				ShowProgramInfo();
				return -1;
			}
			szFileMask = argv[1];
			szFolderName = argv[2];
			szExcelFileName = argv[3];

			for ( int i=4; i<argc; i++ )
			{
				string szVal = argv[i];

				if ( szVal == "-nr" )
					bRecursive = false;
				else if ( szVal == "-i" )
					bIgnoreFields = true;
				else
				{
					if ( _access( szVal.c_str(), 00 ) )
					{
						string szErr = NStr::Format( "Invalid parameter %s, such file does not exist", szVal.c_str() );
						cout << szErr.c_str() << endl;
						ShowProgramInfo();
						return -1;
					}
					szCrapFile = szVal;
				}
			}
		}
	}	while ( 0 );

	std::string szBaseNodeName = "RPG";
	int nExtensionsSize = sizeof( extensions ) / sizeof( extensions[0] );
	for ( int i=0; i<extensions.size(); i++ )
	{
		if ( extensions[i].szExtension == szFileMask )
		{
			szBaseNodeName = extensions[i].szBaseNode;
			break;
		}
	}

	if ( bImportMode )
	{
		CExcelExporter excelExporter;
		excelExporter.ConvertExcelToXMLFiles( szExcelFileName.c_str(), szBaseNodeName.c_str() );
	}
	else
	{
		vector<string> files, errorFiles;
		
		NFile::EnumerateFiles( szFolderName.c_str(), szFileMask.c_str(), CGetAllFiles( &files ), bRecursive );
		
		remove( szExcelFileName.c_str() );
		CExcelExporter excelExporter;
		excelExporter.ConvertFilesToExcel( files, szExcelFileName.c_str(), szCrapFile.c_str(), szBaseNodeName.c_str(), bIgnoreFields );
	}


/*
#ifdef MAIN_EXPORT
	{

		
		string szMask = "*.msh";
		vector<string> files, errorFiles;
		
		NFile::EnumerateFiles( argv[1], szMask.c_str(), CGetAllFiles( &files ), true );
		
		remove( "c:\\result.xls" );
		CExcelExporter excelExporter;
		excelExporter.ConvertFilesToExcel( files, "c:\\result.xls" );
	}
#else
	{
		CExcelExporter excelExporter;
		excelExporter.ConvertExcelToXMLFiles( "c:\\result.xls" );
	}
#endif		//MAIN_EXPORT
*/

	return 0;
}
