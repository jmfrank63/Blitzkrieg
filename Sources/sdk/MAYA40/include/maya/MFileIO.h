#ifndef LINUX
#pragma once
#endif
#ifndef _MFileIO
#define _MFileIO

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MTypes.h>





/**
 Methods for opening, saving, importing, exporting and referencing files.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFileIO  
{

public:
						MFileIO ();
						MFileIO ( const MString& fileName );
	virtual				~MFileIO ();
	static MString		currentFile ();
	static MStatus		setCurrentFile ( const MString& fileName );
	static MString		fileType();
	static MStatus		getFileTypes ( MStringArray& types );
	static MStatus		newFile ( bool force = false );
	static MStatus		open ( const MString& fileName,
							   const char* type = NULL,
							   bool force = false );
	static MStatus		save ( bool force = false );
	static MStatus		saveAs ( const MString& fileName,
							     const char* type = NULL,
								 bool force = false );
	static MStatus		import ( const MString& fileName,
								 const char* type = NULL);
	static MStatus		exportSelected ( const MString& fileName,
								 const char* type = NULL);
	static MStatus		exportAll ( const MString& fileName,
								 const char* type = NULL);
	static MStatus		getReferences ( MStringArray& references );
	static MStatus		getReferenceNodes ( const MString &fileName,
											MStringArray& nodes );
	static MStatus		getReferenceConnectionsMade ( const MString &fileName,
													  MStringArray& connections );
	static MStatus		getReferenceConnectionsBroken ( const MString &fileName,
														MStringArray& connections );
	static MStatus		reference ( const MString& fileName );
	static MStatus		removeReference ( const MString& fileName );
	static bool			isReadingFile();
	static bool			isWritingFile();

protected:

private:

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFileIO */
