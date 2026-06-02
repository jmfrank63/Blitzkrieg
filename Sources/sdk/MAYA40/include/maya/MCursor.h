#ifndef LINUX
#pragma once
#endif
#ifndef _MCursor
#define _MCursor

#if defined __cplusplus




#include <maya/MTypes.h>





/**
 Implement a cursor.
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MCursor  
{
public:
#ifdef _WIN32
				MCursor( LPCSTR pszResource );
#else
				MCursor(short width,
						short height,
						short hotSpotX,
						short hotSpotY,
						unsigned char * bits,
						unsigned char * mask );
#endif
				MCursor(const MCursor& other);
	        	~MCursor();

	MCursor &	operator=(const MCursor &);
	bool		operator==(const MCursor &) const;
	bool		operator!=(const MCursor &) const;
	static		MCursor	defaultCursor;
	static		MCursor	crossHairCursor;
	static		MCursor	doubleCrossHairCursor;
	static		MCursor	editCursor;
	static		MCursor	pencilCursor;
	static		MCursor	handCursor;

protected:

private:


				MCursor();
				MCursor(const void *);
	const void*	apiData;
	const void* apiData2;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32


#endif /* __cplusplus */
#endif /* _MCursor */
