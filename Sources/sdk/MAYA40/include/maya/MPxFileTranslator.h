#ifndef LINUX
#pragma once
#endif
#ifndef _MPxFileTranslator
#define _MPxFileTranslator

#if defined __cplusplus




#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTypes.h>
#include <maya/MFileObject.h>





/**
  This class provides the connection to Maya by which user written file
  translators can be added as plug-ins.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MPxFileTranslator  
{

public:
	enum MFileKind {
		kIsMyFileType,
		kCouldBeMyFileType,
		kNotMyFileType
	};
	enum FileAccessMode {
		kUnknownAccessMode,
		kOpenAccessMode,
		kImportAccessMode,
		kSaveAccessMode,
		kExportAccessMode,
		kExportActiveAccessMode,
	};
						MPxFileTranslator ();
	virtual				~MPxFileTranslator ();	
	virtual MStatus		reader ( const MFileObject& file,
								 const MString& optionsString,
								 FileAccessMode mode);
	virtual MStatus		writer ( const MFileObject& file,
								 const MString& optionsString,
								 FileAccessMode mode);
	virtual bool		haveReadMethod () const;
	virtual bool		haveWriteMethod () const;
	virtual MString     defaultExtension () const;
	virtual bool        canBeOpened () const;
	virtual MPxFileTranslator::MFileKind identifyFile (	const MFileObject& file,
														const char* buffer,
														short size) const;

protected:

private:

	void*				data;

};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxFileTranslator */
