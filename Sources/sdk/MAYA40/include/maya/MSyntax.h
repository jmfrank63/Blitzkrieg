#ifndef LINUX
#pragma once
#endif
#ifndef _MSyntax
#define _MSyntax

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>





/**
This class is used to specify flags and arguments passed to commands.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32


class OPENMAYA_EXPORT MSyntax {
public:

	enum MArgType {
		kInvalidArgType,
		kNoArg,
		kBoolean,
		kLong,
		kDouble,
		kString,
		kUnsigned,
		kDistance,
		kAngle,
		kTime,
		kSelectionItem,
		kLastArgType
	};

	enum MObjectFormat { 
		kInvalidObjectFormat,
		kNone,
		kStringObjects,
		kSelectionList,
		kLastObjectFormat
	};

		    MSyntax ();
		    MSyntax ( const MSyntax& other );
	virtual ~MSyntax();
	MSyntax &operator=(const MSyntax &rhs);

	MStatus	addFlag					(const char *shortName,
									 const char *longName,
									 MArgType argType1 = kNoArg,
									 MArgType argType2 = kNoArg,
									 MArgType argType3 = kNoArg,
									 MArgType argType4 = kNoArg,
									 MArgType argType5 = kNoArg,
									 MArgType argType6 = kNoArg);

	MStatus	makeFlagMultiUse		(const char *flag);

	MStatus	addArg					(MArgType arg);
	
	void	useSelectionAsDefault	(bool useSelectionList = false);

	MStatus	setObjectType			(MObjectFormat objectFormat, 
									 unsigned minimumObjects = 0);
	MStatus	setObjectType			(MObjectFormat objectFormat, 
									 unsigned minimumObjects, 
									 unsigned maximumObjects);

	void	setMinObjects			(unsigned minimumObjectCount);
	void	setMaxObjects			(unsigned maximumObjectCount);
	void	enableQuery				(bool supportsQuery = true);
	void	enableEdit				(bool supportsEdit = true);

	unsigned	minObjects			() const;
	unsigned	maxObjects			() const;
	bool		canQuery			() const;
	bool		canEdit				() const;

protected:

private:
	const char *className() const;



	MSyntax(void *);
	MSyntax(const void *);
	bool fOwn;

	void * apiData;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MSyntax */
