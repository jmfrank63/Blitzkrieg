#include "key_frame_tree_item.h"

#include <cstdlib>

#include "items/tree_item_types.h"
#include "mfc_value.h"

namespace NResourceModel
{

CKeyFrameTreeItem::CKeyFrameTreeItem() : CStatsItem( ETIT_KEYFRAME_TREE_ITEM ) {}

// A list the file does not carry leaves the frames as they are, as
// CTreeAccessor::Add does when StartContainerChunk finds no chunk.
void CKeyFrameTreeItem::ReadData( const NResourceXml::Node &node )
{
	CStatsItem::ReadData( node );
	const NResourceXml::Node *frames = FindElement( node, "Key_frames" );
	if ( !frames )
		return;
	framesList.clear();
	for ( const auto &entry : frames->children )
	{
		if ( entry.kind != NResourceXml::Node::Element || entry.name != "item" )
			continue;
		std::pair<float, float> frame( 0.0f, 0.0f );
		ReadFloat( entry, "first", frame.first );
		ReadFloat( entry, "second", frame.second );
		framesList.push_back( frame );
	}
}

void CKeyFrameTreeItem::WriteData( NResourceXml::Node &node ) const
{
	CStatsItem::WriteData( node );
	NResourceXml::Node frames;
	frames.kind = NResourceXml::Node::Element;
	frames.name = "Key_frames";
	for ( const auto &frame : framesList )
	{
		NResourceXml::Node entry;
		entry.kind = NResourceXml::Node::Element;
		entry.name = "item";
		SetAttr( entry, "first", MfcFloat( frame.first ) );
		SetAttr( entry, "second", MfcFloat( frame.second ) );
		frames.children.push_back( std::move( entry ) );
	}
	node.children.push_back( std::move( frames ) );
}

}
