#ifndef LINUX
#pragma once
#endif
#ifndef _MPxToolCommand
#define _MPxToolCommand

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MPxCommand.h>



class MString;
class MFileObject;
class MArgList;



/**
  The base class for interactive tool commands
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxToolCommand : public MPxCommand
{
public:
	MPxToolCommand(); // Called within a context

	virtual ~MPxToolCommand();

    virtual MStatus 	cancel();
	virtual MStatus 	finalize();

protected:
	MStatus				doFinalize( MArgList & command );

private:
	virtual const char*	className() const;

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxToolCommand */

