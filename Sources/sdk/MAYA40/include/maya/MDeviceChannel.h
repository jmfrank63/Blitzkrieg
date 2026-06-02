#ifndef LINUX
#pragma once
#endif
#ifndef _MDeviceChannel
#define _MDeviceChannel

#if defined __cplusplus



#include <maya/MStatus.h>



class MDeviceState;
class MString;



/**


*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MDeviceChannel
{
public:
	MDeviceChannel( const MString&, MDeviceChannel* = NULL, int = -1 );

	~MDeviceChannel();


	MString 			name() const;
	MString 			longName() const;

	int				axisIndex() const;

	bool     			hasChildren() const;
	MDeviceChannel	 	parent() const;
	MDeviceChannel 		childByIndex( int );
	int					numChildren() const;

protected:

private:




	friend class MPxMidiInputDevice;
	MDeviceChannel( void * );
	void * data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDeviceChannel */
