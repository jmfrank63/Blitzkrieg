#ifndef LINUX
#pragma once
#endif
#ifndef _MEvent
#define _MEvent

#if defined __cplusplus



#include <maya/MStatus.h>



class MDeviceState;



/**

System event information class.

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MEvent  
{
public:

    enum ModifierType {
        shiftKey    = 1<<0,
        controlKey  = 1<<2
    };

	enum MouseButtonType {
		kLeftMouse		= 1<<6,
		kMiddleMouse	= kLeftMouse<<1
	};

public:
    MEvent();
    virtual ~MEvent();

    MStatus         getPosition( short& x_pos, short& y_pos ) const;
    MStatus         setPosition( short& x_pos, short& y_pos );
    MStatus         getWindowPosition( short& x_pos, short& y_pos ) const;
    MouseButtonType mouseButton( MStatus * ReturnStatus = NULL ) const;
    bool            isModifierKeyRelease( MStatus * ReturnStatus = NULL ) const;
    ModifierType    modifiers( MStatus * ReturnStatus = NULL ) const;
    MStatus         setModifiers( ModifierType& modType );
    bool            isModifierNone( MStatus * ReturnStatus = NULL ) const;
    bool            isModifierShift( MStatus * ReturnStatus = NULL ) const;
    bool            isModifierControl( MStatus * ReturnStatus = NULL ) const;
    bool            isModifierLeftMouseButton( MStatus * ReturnStatus = NULL )
					const;
    bool            isModifierMiddleMouseButton( MStatus * ReturnStatus = NULL )
					const;

protected:

private:
    static const char* className();




    MEvent( const void * );
    const void * fEventPtr;
    void * fModifier;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MEvent */
