#ifndef LINUX
#pragma once
#endif
#ifndef _MFeedbackLine
#define _MFeedbackLine

#if defined __cplusplus



#include <maya/MTypes.h>



class MStatus;
class MString;


/**
This class provides methods for displaying information to the user.
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32


class OPENMAYAUI_EXPORT MFeedbackLine {
public:
	static MStatus	setFormat		(const MString &format);
	static MStatus	setTitle		(const MString &title);
	static MStatus	setValue		(short index, double value);
	static void		clear			();
	static bool		showFeedback	();
	static void		setShowFeedback	(bool showFeedback);

private:
	static const char *className();
};


#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFeedbackLine */
