#ifndef __FMTSAVELOAD_H__
#define __FMTSAVELOAD_H__
#pragma ONCE
#include "../StreamIO/StreamIOHelper.h"
namespace NSaveLoad
{
struct SFileHeader
{
	enum { SIGNATURE = 0x00533741, VERSION = 4 };
	int nVersion;													// save version
	std::wstring szTitleName;							// title name (user-defined)
	std::string szChapterName;						// chapter, this save was made in
	std::string szMissionName;						// current mission name (empty, if we are not in mission) or template name, if this is a random mission
	bool bRandomMission;									// true, if we are in random mission
	int nPictureSizeX;										// picture's for this save width
	int nPictureSizeY;										// picture's for this save height
	SFileHeader()
		: nVersion( SFileHeader::VERSION ), bRandomMission( false ), nPictureSizeX( 0 ), nPictureSizeY( 0 ) {  }
};
struct SRandomHeader
{
	DWORD dwRandomDateTime;								// date and time of the random seed - to compare and, may be, restore maps and images
	std::string szChapterUnitsTableFileName; //���� � ���������� �������
	int nLevel;															 //���������
	std::string szGraphName;								 //��� �����
	int nAngle;															 //�������
	SRandomHeader()
		: dwRandomDateTime( 0 ), nLevel( 0 ), nAngle( 0 ) {}
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "DateTime", &dwRandomDateTime );
		saver.Add( "ChapterUnitsTableFileName", &szChapterUnitsTableFileName );
		saver.Add( "DifficultyLevel", &nLevel );
		saver.Add( "Graph", &szGraphName );
		saver.Add( "Angle", &nAngle );
		return 0;
	}
};
};
inline CStreamAccessor& operator>>( CStreamAccessor &stream, NSaveLoad::SFileHeader &hdr )
{
	stream >> hdr.nVersion;
	stream >> hdr.szTitleName;
	stream >> hdr.szChapterName;
	stream >> hdr.szMissionName;
	stream >> hdr.bRandomMission;
	stream >> hdr.nPictureSizeX;
	stream >> hdr.nPictureSizeY;
	return stream;
}
inline CStreamAccessor& operator<<( CStreamAccessor &stream, NSaveLoad::SFileHeader &hdr )
{
	stream << hdr.nVersion;
	stream << hdr.szTitleName;
	stream << hdr.szChapterName;
	stream << hdr.szMissionName;
	stream << hdr.bRandomMission;
	stream << hdr.nPictureSizeX;
	stream << hdr.nPictureSizeY;
	return stream;
}
inline CStreamAccessor& operator>>( CStreamAccessor &stream, NSaveLoad::SRandomHeader &hdr )
{
	stream >> hdr.dwRandomDateTime;
	stream >> hdr.szChapterUnitsTableFileName;
	stream >> hdr.nLevel;
	if ( hdr.nLevel & 0x00008000 ) 
	{
		hdr.nLevel &= 0xffff7fff;
		stream >>	hdr.szGraphName;
		stream >>	hdr.nAngle;
	}
	else
	{
		hdr.szGraphName.clear();
		hdr.nAngle = 0;
	}
	return stream;
}
inline CStreamAccessor& operator<<( CStreamAccessor &stream, NSaveLoad::SRandomHeader &hdr )
{
	stream << hdr.dwRandomDateTime;
	stream << hdr.szChapterUnitsTableFileName;
	stream << ( hdr.nLevel | 0x00008000 );
	stream <<	hdr.szGraphName;
	stream <<	hdr.nAngle;
	return stream;
}
#endif // __FMTSAVELOAD_H__
