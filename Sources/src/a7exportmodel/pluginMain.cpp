#include "StdAfx.h"

#include <maya/MFnPlugin.h>

#include "A7ExportModel.h"

#ifdef __MAYA4__
#define EXPORT __declspec(dllexport)
#define VERSION "4.0"
#else
#define EXPORT
#define VERSION "3.0"
#endif // __MAYA4__

EXPORT MStatus initializePlugin( MObject obj )
{ 
	MFnPlugin plugin( obj, "Nival Interactive", VERSION, "Any" );

	return plugin.registerFileTranslator( "A7ExportModel", "none", CA7ExportModel::creator, "", "skeleton=1;" );
}

EXPORT MStatus uninitializePlugin( MObject obj )
{
	MFnPlugin plugin( obj );

	return plugin.deregisterFileTranslator( "A7ExportModel" );
}
