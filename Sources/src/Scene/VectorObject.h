#ifndef __VECTOR_OBJECT_H__
#define __VECTOR_OBJECT_H__
#pragma ONCE
#include "..\Formats\fmtVSO.h"
typedef SGFXLVertex STVOVertex;
struct STVOLayer
{
	struct SPatch
	{
		DWORD dwPatch;											// patch coords
		std::vector<int> points;						// points, which belong to this patch
		SPatch() : dwPatch( -1 ) {  }
		int operator&( IStructureSaver &ss );
	};
	std::vector<STVOVertex> allvertices;	// all vertices
	int nNumVertsPerLine;									// number of vertices per line
	std::vector< CPtr<IGFXTexture> > textures;	// textures (>1 for animated or 1)
	std::vector<SPatch> patches;					// all patches
	std::vector<STVOVertex> vertices;			// vertices, selected for current visualization
	std::vector<WORD> indices;						// indices, selected for current visualization
	STVOLayer() : nNumVertsPerLine( -1 ) {  }
	void SelectPatches( const std::vector<DWORD> &sels, const int nNumBasePoints );
	int operator&( IStructureSaver &ss );
};
class CTerrainVectorObject
{
	SVectorStripeObject desc;
protected:
	void Init( const SVectorStripeObject &_desc );
	const SVectorStripeObject& GetDesc() const { return desc; }
public:
	virtual int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &desc );
		return 0;
	}
	const int GetID() const { return desc.nID; }
};
#endif // __VECTOR_OBJECT_H__