#ifndef LINUX
#pragma once
#endif
#ifndef _MAnimCurveClipboardItemArray
#define _MAnimCurveClipboardItemArray

#if defined __cplusplus



#include <maya/MAnimCurveClipboardItem.h>
#include <maya/MStatus.h>





/**
  Implement an array of MAnimCurveClipboardItem data type.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAANIM_EXPORT MAnimCurveClipboardItemArray  
{

public:
					MAnimCurveClipboardItemArray();
					MAnimCurveClipboardItemArray(
									const MAnimCurveClipboardItemArray& other );
					MAnimCurveClipboardItemArray(
									const MAnimCurveClipboardItem src[],
									unsigned count );
					~MAnimCurveClipboardItemArray();
 	const MAnimCurveClipboardItem&		operator[]( unsigned index ) const;
 	MAnimCurveClipboardItem&			operator[]( unsigned index ); 
 	MStatus			set( const MAnimCurveClipboardItem& element,
						 unsigned index ); 
 	unsigned		length() const;
 	MStatus			remove( unsigned index );
 	MStatus			insert( const MAnimCurveClipboardItem & element,
							unsigned index );
 	MStatus			append( const MAnimCurveClipboardItem & element );
 	MStatus			clear();
	MStatus			get( MAnimCurveClipboardItem array[] ) const;
	void			setSizeIncrement ( unsigned newIncrement );
	unsigned		sizeIncrement () const;
	bool			isValid( unsigned & failedIndex ) const;

protected:

private:

	bool							validate( unsigned int & index,
											  unsigned int rowCount ) const;
 	MAnimCurveClipboardItemArray&	operator = (
										const MAnimCurveClipboardItemArray&)
										const;
 	MAnimCurveClipboardItemArray&	operator = (
										const MAnimCurveClipboardItemArray&);
 	void*							fArray;
	static const char*				className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAnimCurveClipboardItemArray */
