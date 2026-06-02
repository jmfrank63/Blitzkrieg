#ifndef LINUX
#pragma once
#endif
#ifndef _MArgDatabase
#define _MArgDatabase

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>
#include <maya/MArgParser.h>



class MSyntax;
class MSelectionList;
class MStringArray;
class MArgList;
class MDistance;
class MAngle;
class MTime;



/**
This class parses argument lists based on a syntax object (MSyntax) 
which describes the format for a command.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MArgDatabase : public MArgParser
{
public:
			MArgDatabase		();
			MArgDatabase		(const MSyntax &syntax,
								 const MArgList &argList,
								 MStatus *ReturnStatus = NULL);
	virtual ~MArgDatabase();

	MStatus getFlagArgument		(const char *flag, unsigned index, 
								 MSelectionList &result) const;
	MStatus	getCommandArgument	(unsigned index, MSelectionList &result) const;

	MStatus	getObjects			(MSelectionList &result) const;

	MStatus	getFlagArgument		(const char *flag, unsigned index, 
								 bool &result) const;
	MStatus	getFlagArgument		(const char *flag, unsigned index,
								 int &result) const;
	MStatus	getFlagArgument		(const char *flag, unsigned index, 
								 double &result) const;
	MStatus	getFlagArgument		(const char *flag, unsigned index, 
								 MString &result) const;
	MStatus getFlagArgument		(const char *flag, unsigned index, 
								 unsigned &result) const;
	MStatus	getFlagArgument		(const char *flag, unsigned index, 
								 MDistance &result) const;
	MStatus	getFlagArgument		(const char *flag, unsigned index, 
								 MAngle &result) const;
	MStatus	getFlagArgument		(const char *flag, unsigned index, 
								 MTime &result) const;
	MStatus	getCommandArgument	(unsigned index, bool &result) const;
	MStatus	getCommandArgument	(unsigned index, int &result) const;
	MStatus	getCommandArgument	(unsigned index, double &result) const;
	MStatus	getCommandArgument	(unsigned index, MString &result) const;
	MStatus	getCommandArgument	(unsigned index, MDistance &result) const;
	MStatus	getCommandArgument	(unsigned index, MAngle &result) const;
	MStatus	getCommandArgument	(unsigned index, MTime &result) const;
	MStatus	getObjects			(MStringArray &result) const;

protected:

private:
	const char *className() const;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MArgDatabase */

