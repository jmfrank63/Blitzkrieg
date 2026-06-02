#ifndef LINUX
#pragma once
#endif
#ifndef _MComputation
#define _MComputation

#if defined __cplusplus



#include <maya/MTypes.h>



/**
 An MComputation is used to monitor user-interupts during int computations.
*/

#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MComputation {

public:  
			MComputation();
	virtual	~MComputation();
    void	beginComputation();
    bool	isInterruptRequested();
    void	endComputation();

protected:

private:
    const char* className() const;
	void *f_data;
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MComputation */
