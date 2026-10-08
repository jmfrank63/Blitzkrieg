#pragma once
// Portable mirror of Sources/src/editor/TreeItem.h's CKeyFrameTreeItem, the
// base of the Particle sub-editor's curve items (ParticleTreeItem.h). Each
// curve is a list of (x, y) key frames plus the clamp, step and resize-mode
// knobs the curve control uses; the derived InitDefaultValues set them, as
// in MFC.
//
// Serialisation is CKeyFrameTreeItem::operator&( IDataTree & ): the CTreeItem
// fields, then the frames as the container "Key_frames", one <item> per
// pair with first and second as attributes (DTHelper.h list and pair). The
// knobs are not stored; a read item keeps the ones its constructor set.

#include <list>
#include <string>
#include <utility>

#include "items/stats_item.h"
#include "tree_item.h"

namespace NResourceModel
{

using CFramesList = std::list<std::pair<float, float>>;

class CKeyFrameTreeItem : public CStatsItem
{
public:
	CFramesList framesList;
	float fMinValX = 0.0f;
	float fMaxValX = 1.0f;
	float fStepX = 0.1f;
	float fMinValY = 0.0f;
	float fMaxValY = 1.0f;
	float fStepY = 0.1f;
	bool bResizeMode = false;

	// The MFC factory registers CKeyFrameTreeItem itself under
	// E_KEYFRAME_TREE_ITEM, so the plain class is a factory product too.
	CKeyFrameTreeItem();
	explicit CKeyFrameTreeItem( int nType ) : CStatsItem( nType ) {}

	void SetFramesList( const CFramesList &frames ) { framesList = frames; }

protected:
	void ReadData( const NResourceXml::Node &node ) override;
	void WriteData( NResourceXml::Node &node ) const override;
	bool OwnsField( const std::string &name ) const override { return name == "Key_frames" || CStatsItem::OwnsField( name ); }
};

}
