#ifndef LINUX
#pragma once
#endif
#ifndef _MPxContextCommand
#define _MPxContextCommand

#if defined __cplusplus



#include <maya/MTypes.h>
#include <maya/MStatus.h>



class MPxContext;
class MArgParser;



/**
  The base class for context creation commands.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYAUI_EXPORT MPxContextCommand
{
public:
							MPxContextCommand	();
	virtual					~MPxContextCommand	();
	virtual MStatus			doEditFlags			();
	virtual MStatus			doQueryFlags		();
	virtual MPxContext *	makeObj				();
	virtual MStatus			appendSyntax		();
	MStatus					setResult			(bool result);
	MStatus					setResult			(int result);
	MStatus					setResult			(double result);
	MStatus					setResult			(const MString &result);

protected:
	MSyntax	syntax(MStatus *ReturnStatus = NULL) const;
	MArgParser parser(MStatus *ReturnStatus = NULL) const;

private:
	const char *className() const;

	void setData(void *ptr);

	void *instance;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MPxContextCommand */
