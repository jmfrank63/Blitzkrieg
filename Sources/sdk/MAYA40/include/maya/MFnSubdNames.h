#ifndef LINUX
#pragma once
#endif
#ifndef _MFnSubdNames
#define _MFnSubdNames

#if defined __cplusplus



#include <maya/MStatus.h>
#include <maya/MTypes.h>




/**

*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MFnSubdNames
{
public:
					MFnSubdNames();
					~MFnSubdNames();
	static int		base( MUint64 id );
	static int		first( MUint64 id );
	static int		level( MUint64 id );
	static int		path( MUint64 id );
	static int		corner( MUint64 id );

	static MStatus	fromMUint64( MUint64 id, int& base, int& first,
								 int& level, int& path, int& corner );
	static MStatus	toMUint64( MUint64& id, int base, int first,
								 int level, int path, int corner );

	static MUint64	baseFaceId( MUint64 id );
	static long		baseFaceIndex( MUint64 id );
	static unsigned	baseFaceIndexFromId( MUint64 id );
	static MUint64	levelOneFaceId( MUint64 id );
	static long		levelOneFaceAsLong( MUint64 id );
	static unsigned	levelOneFaceIndexFromId( MUint64 id );

	static MUint64	levelOneFaceIdFromLong( long one );
	static MUint64	levelOneFaceIdFromIndex( unsigned index );

	static MUint64	baseFaceIdFromLong( long base );
	static MUint64	baseFaceIdFromIndex( unsigned index );

	static MUint64	parentFaceId( MUint64 id );

	static MStatus	nonBaseFaceVertices( MUint64 id,
										 MUint64& vertex1, MUint64& vertex2,
										 MUint64& vertex3, MUint64& vertex4 );
	static MStatus	nonBaseFaceEdges( MUint64 id,
									  MUint64& edge1, MUint64& edge2,
									  MUint64& edge3, MUint64& edge4);


	static MStatus	fromSelectionIndices( MUint64& id,
										  unsigned int firstIndex,
										  unsigned int secondIndex );

	static MStatus	toSelectionIndices( MUint64 id,
										unsigned int& firstIndex,
										unsigned int& secondIndex );

protected:

private:
	static const char* className();
};

#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MFnSubd */
