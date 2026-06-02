#ifndef LINUX
#pragma once
#endif
#ifndef _MPxMidiInputDevice
#define _MPxMidiInputDevice

#if defined __cplusplus



#ifdef SGI
#include <dmedia/midi.h>
#endif // SGI
#include <maya/MStatus.h>
#include <maya/MTypes.h>



class MDeviceState;
class MDeviceChannel;
class MString;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxMidiInputDevice
{
public:
	MPxMidiInputDevice();
	virtual ~MPxMidiInputDevice();
	
	virtual	MStatus			openDevice();
	virtual	void			closeDevice();

	virtual	void			nameAxes();
	virtual void			nameButtons();
	virtual	MDeviceState* 	deviceState();
#ifdef SGI
	virtual	MDeviceState* 	deviceState( MDevent& );
#endif // SGI

	virtual MStatus 		sendMessage(	const char* const messageType,
											const char* const messageParams );
	virtual char* 			getMessage(	const char* const messageType,
										char* messageResponse );

	virtual void 			doButtonEvents( bool = true );
	virtual void 			doMovementEvents( bool = true );

	MPxMidiInputDevice( void * init );

protected:
	MStatus		setNamedButton( MString &, unsigned short );
	MStatus		addChannel( MDeviceChannel & );
	MStatus 	setDegreesOfFreedom( int freedom );
	MStatus 	setNumberOfButtons( int buttons );
	
private:

	void   setData( void* );
	virtual const char*	className() const;
	void * 	data;
	int 	degreesOfFreedom;
	int 	numberOfButtons;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxMidiInputDevice */
