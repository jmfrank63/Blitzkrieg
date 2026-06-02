#ifndef LINUX
#pragma once
#endif
#ifndef _MDeviceState
#define _MDeviceState

#if defined __cplusplus



#include <maya/MTypes.h>



class MString;
class THeventInputDevice;



/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MDeviceState  
{
public:
	virtual ~MDeviceState();

	int		devicePosition( const unsigned short int axis) const;
	int		devicePosition( const MString & axisName ) const;
	void	setDevicePosition( const int position, 
							   const unsigned short int axis);
	void	setDevicePosition( const int position, 
							   const MString & axisName );

	bool	buttonState( const unsigned short int button ) const;
	bool	buttonState( const MString & buttonName ) const;
	void	setButtonState( const bool state, 
							const unsigned short int button );
	void	setButtonState( const bool state, 
							const MString & buttonName );

	int		maxAxis() const;

	bool	isNull();

protected:

private:



	MDeviceState( void * );
	void *   data();
	void * api_stateData;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MDeviceState */
