#ifndef LINUX
#pragma once
#endif
#ifndef _MFnPlugin
#define _MFnPlugin

#if defined __cplusplus



#include <maya/MFnBase.h>
#include <maya/MApiVersion.h>
#include <maya/MPxNode.h>
#include <maya/MPxData.h>

#ifdef NT_PLUGIN
#include <maya/MTypes.h>
HINSTANCE MhInstPlugin;

extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	MhInstPlugin = hInstance;
	return 1;
}
#endif // NT_PLUGIN



class MString;
class MFileObject;
class MTypeId;



/**
  Register plug-in supplied, commands, dependency nodes, etc. with Maya
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnPlugin : public MFnBase 
{
public:
					MFnPlugin();
					MFnPlugin( MObject& object,
							   const char* vendor = "Unknown",
							   const char* version = "Unknown",
							   const char* requiredApiVersion = "Any",
							   MStatus* ReturnStatus = 0L );
	virtual			~MFnPlugin();
	virtual			MFn::Type type() const; 

	MString			vendor( MStatus* ReturnStatus=NULL ) const;
	MString			version( MStatus* ReturnStatus=NULL ) const;
	MString			apiVersion( MStatus* ReturnStatus=NULL ) const;
	MString			name( MStatus* ReturnStatus=NULL ) const;
	MString			loadPath( MStatus* ReturnStatus=NULL ) const;
	MStatus			setName( const MString& newName,
							 bool allowRename = true );

	MStatus			registerCommand(const MString& commandName,
									MCreatorFunction creatorFunction,
									MCreateSyntaxFunction 
									    createSyntaxFunction = NULL);
	MStatus			deregisterCommand(	const MString& commandName );
    MStatus         registerContextCommand( const MString& commandName,
											MCreatorFunction creatorFunction);

    MStatus         registerContextCommand( const MString& commandName,
											MCreatorFunction creatorFunction,
											const MString& toolCmdName,
											MCreatorFunction toolCmdCreator,
											MCreateSyntaxFunction
												toolCmdSyntax = NULL
											);

    MStatus         deregisterContextCommand( const MString& commandName );
    MStatus         deregisterContextCommand( const MString& commandName,
											  const MString& toolCmdName );
	MStatus			registerNode(	const MString& typeName,
									const MTypeId& typeId,
									MCreatorFunction creatorFunction,
									MInitializeFunction initFunction,
									MPxNode::Type type = MPxNode::kDependNode,
									const MString* classification = NULL); 
	MStatus			deregisterNode(	const MTypeId& typeId );
	MStatus			registerShape(	const MString& typeName,
									const MTypeId& typeId,
									MCreatorFunction creatorFunction,
									MInitializeFunction initFunction,
									MCreatorFunction uiCreatorFunction,
									const MString* classification = NULL); 
	MStatus			registerData(	const MString& typeName,
									const MTypeId& typeId,
									MCreatorFunction creatorFunction,
									MPxData::Type type = MPxData::kData );
	MStatus			deregisterData(	const MTypeId& typeId );
	MStatus         registerDevice( const MString& deviceName,
									MCreatorFunction creatorFunction );
	MStatus         deregisterDevice( const MString& deviceName );
	MStatus			registerFileTranslator( const MString& translatorName,
										char* pixmapName,
										MCreatorFunction creatorFunction,
										char* optionsScriptName = NULL,
										char* defaultOptionsString = NULL,
										bool requiresFullMel = NULL );
	MStatus			deregisterFileTranslator( const MString& translatorName );
	MStatus			registerIkSolver( const MString& ikSolverName,
										MCreatorFunction creatorFunction );
	MStatus			deregisterIkSolver( const MString& ikSolverName );
	MStatus			registerUI(const MString & creationProc,
							   const MString & deletionProc);
protected:
	virtual const char* className() const;

private:
					MFnPlugin( const MObject& object,
							   const char* vendor = "Unknown",
							   const char* version = "Unknown",
							   const char* requiredApiVersion = "Any",
							   MStatus* ReturnStatus = 0L );
	MFnPlugin&		operator=( const MFnPlugin & ) const;
	MFnPlugin&		operator=( const MFnPlugin & );
	MFnPlugin*		operator& () const;
	MFnPlugin*		operator& ();
};
#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnPlugin */
