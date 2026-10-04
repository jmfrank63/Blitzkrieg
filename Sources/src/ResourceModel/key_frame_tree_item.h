#pragma once
// Portable mirror of Sources/src/editor/TreeItem.h's CKeyFrameTreeItem - the
// base the Particle sub-editor's twenty-plus curve items derive from (spin,
// weight, speed, size, opacity, texture-frame, random spin, complex source,
// rand life/speed/complex/c-random-speed, texture frame; see
// Sources/src/editor/ParticleTreeItem.{h,cpp}) plus the Effect sub-editor's
// animation/mesh/func/maya curves. Carries the authored frames list
// (list<pair<float,float>>) and the x/y clamp + resize-mode knobs. The MFC
// serialiser emits a single "Key_frames" child under the item (see MFC
// TreeItem.cpp CKeyFrameTreeItem::operator&); the port emits the same list
// under a `<frames>` child so a future per-item fixture round-trips through
// the typed path.
//
// For T03 the fixtures stay keyframe-free (minimal `<X_Composer_Project>` +
// `<fixture>` subtree) so the serialise/parse body is exercised only when
// authored data lands in a later task - but every ParticleTreeItem /
// EffectTreeItem subclass registered in factory.cpp inherits from this base
// so the slot is present on day one.

#include <string>
#include <utility>
#include <vector>

#include "items/stats_item.h"
#include "tree_item.h"

namespace NResourceModel
{

// A single (x,y) sample on an authored curve. list<pair<float,float>> in MFC;
// vector<FrameSample> here - the Qt port and the round-trip test both prefer
// random access over splice/insert costs that only mattered when MFC drove
// list manipulations off UI events.
struct FrameSample
{
	float x;
	float y;
};

// Base for every CKeyFrameTreeItem-derived item in the Particle/Effect
// sub-editors. Carries the knob set and (de)serialises it under `<frames>`
// so an authored curve survives a round-trip through the typed path. For
// roots the sub-editor passes an XML tag through to CStatsItem the same
// way the stats-only roots do.
class CKeyFrameTreeItem : public CStatsItem
{
public:
	std::vector<FrameSample> framesList;
	float fMinValX = 0.0f;
	float fMaxValX = 1.0f;
	float fStepX = 0.1f;
	float fMinValY = 0.0f;
	float fMaxValY = 1.0f;
	float fStepY = 0.1f;
	bool bResizeMode = false;

	// Default constructor keyed on ETIT_KEYFRAME_TREE_ITEM: the MFC factory
	// REGISTER_CLASSes CKeyFrameTreeItem itself (not just its derivatives),
	// so an instance appears via the factory on E_KEYFRAME_TREE_ITEM lookups.
	CKeyFrameTreeItem();
	explicit CKeyFrameTreeItem( int nType ) : CStatsItem( nType ) {}
	CKeyFrameTreeItem( int nType, std::string xmlTag )
		: CStatsItem( nType, std::move( xmlTag ) )
	{
	}

	// Emits one `<frames>` child containing `<f x="..." y="..."/>` samples +
	// the clamp/step/resize knobs as sibling children, then defers to the
	// base to walk any SProp slots a subclass has registered. parse() reverses
	// the walk and leaves framesList empty when the authoring file omits it.
	void parse( const NResourceXml::Node &node ) override;
	void serialise( NResourceXml::Node &node ) const override;
};

}
