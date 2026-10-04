#include "key_frame_tree_item.h"

#include <cstdlib>
#include <string>

#include "items/tree_item_types.h"

namespace NResourceModel
{

// Match the forward declaration in the header. Keyed on ETIT_KEYFRAME_TREE_ITEM
// so a factory-created plain CKeyFrameTreeItem is self-identifying before
// Load() runs.
CKeyFrameTreeItem::CKeyFrameTreeItem() : CStatsItem( ETIT_KEYFRAME_TREE_ITEM ) {}

namespace
{

std::string FloatToString( float v )
{
	// MFC rendered keyframe knobs through CString::Format("%g"); std::to_string
	// is close enough for the FutureBlob-dominated T03 fixtures and gets replaced
	// by a %g-equivalent writer when a non-FutureBlob keyframe fixture lands.
	return std::to_string( v );
}

float ParseFloat( const std::string &s )
{
	if ( s.empty() ) return 0.0f;
	return std::strtof( s.c_str(), nullptr );
}

const std::string *AttrValue( const NResourceXml::Node &n, const char *name )
{
	for ( const auto &kv : n.attrs )
		if ( kv.first == name )
			return &kv.second;
	return nullptr;
}

}

void CKeyFrameTreeItem::parse( const NResourceXml::Node &node )
{
	CStatsItem::parse( node );

	framesList.clear();
	const NResourceXml::Node *frames = NResourceXml::FindChild( node, "frames" );
	if ( !frames )
		return;

	if ( const auto *v = AttrValue( *frames, "min_x" ) )  fMinValX = ParseFloat( *v );
	if ( const auto *v = AttrValue( *frames, "max_x" ) )  fMaxValX = ParseFloat( *v );
	if ( const auto *v = AttrValue( *frames, "step_x" ) ) fStepX   = ParseFloat( *v );
	if ( const auto *v = AttrValue( *frames, "min_y" ) )  fMinValY = ParseFloat( *v );
	if ( const auto *v = AttrValue( *frames, "max_y" ) )  fMaxValY = ParseFloat( *v );
	if ( const auto *v = AttrValue( *frames, "step_y" ) ) fStepY   = ParseFloat( *v );
	if ( const auto *v = AttrValue( *frames, "resize" ) ) bResizeMode = ( *v == "1" || *v == "true" );

	for ( const auto &child : frames->children )
	{
		if ( child.kind != NResourceXml::Node::Element || child.name != "f" )
			continue;
		FrameSample sample{ 0.0f, 0.0f };
		if ( const auto *v = AttrValue( child, "x" ) ) sample.x = ParseFloat( *v );
		if ( const auto *v = AttrValue( child, "y" ) ) sample.y = ParseFloat( *v );
		framesList.push_back( sample );
	}
}

void CKeyFrameTreeItem::serialise( NResourceXml::Node &node ) const
{
	CStatsItem::serialise( node );

	NResourceXml::Node frames;
	frames.kind = NResourceXml::Node::Element;
	frames.name = "frames";
	frames.attrs.push_back( { "min_x",  FloatToString( fMinValX ) } );
	frames.attrs.push_back( { "max_x",  FloatToString( fMaxValX ) } );
	frames.attrs.push_back( { "step_x", FloatToString( fStepX ) } );
	frames.attrs.push_back( { "min_y",  FloatToString( fMinValY ) } );
	frames.attrs.push_back( { "max_y",  FloatToString( fMaxValY ) } );
	frames.attrs.push_back( { "step_y", FloatToString( fStepY ) } );
	frames.attrs.push_back( { "resize", bResizeMode ? "1" : "0" } );

	for ( const FrameSample &sample : framesList )
	{
		NResourceXml::Node f;
		f.kind = NResourceXml::Node::Element;
		f.name = "f";
		f.attrs.push_back( { "x", FloatToString( sample.x ) } );
		f.attrs.push_back( { "y", FloatToString( sample.y ) } );
		frames.children.push_back( std::move( f ) );
	}
	node.children.push_back( std::move( frames ) );
}

}
