#include "StdAfx.h"

#include "anim_names.h"

#include "../Anim/Animation.h"

namespace NResourceModel
{

namespace NCompose
{

int GetActionFromName( const std::string &szAnimName, bool *pbKnown )
{
	if ( pbKnown != 0 )
		*pbKnown = true;
	std::string szName = szAnimName;
	NStr::ToLower( szName );
	if ( szName == "idle" )
		return ANIMATION_IDLE;
	else if ( szName == "idle down" )
		return ANIMATION_IDLE_DOWN;
	else if ( szName == "aiming" )
		return ANIMATION_AIMING;
	else if ( szName == "aiming down" )
		return ANIMATION_AIMING_DOWN;
	else if ( szName == "aiming trench" )
		return ANIMATION_AIMING_TRENCH;
	else if ( szName == "run" )
		return ANIMATION_MOVE;
	else if ( szName == "crawl" )
		return ANIMATION_CRAWL;
	else if ( szName == "shoot" )
		return ANIMATION_SHOOT;
	else if ( szName == "shoot down" )
		return ANIMATION_SHOOT_DOWN;
	else if ( szName == "shoot trench" )
		return ANIMATION_SHOOT_TRENCH;
	else if ( szName == "throw" )
		return ANIMATION_THROW;
	else if ( szName == "throw trench" )
		return ANIMATION_THROW_TRENCH;
	else if ( szName == "death" )
		return ANIMATION_DEATH;
	else if ( szName == "death down" )
		return ANIMATION_DEATH_DOWN;
	else if ( szName == "use up" )
		return ANIMATION_USE;
	else if ( szName == "use down" )
		return ANIMATION_USE_DOWN;
	else if ( szName == "pointing" )
		return ANIMATION_POINTING;
	else if ( szName == "binoculars" )
		return ANIMATION_BINOCULARS;
	else if ( szName == "radio" )
		return ANIMATION_RADIO;
	else if ( ( szName == "default" ) || ( szName == "effect" ) )
		return ANIMATION_IDLE;
	else if ( szName == "install" )
		return ANIMATION_INSTALL;
	else if ( szName == "uninstall" )
		return ANIMATION_UNINSTALL;
	else if ( szName == "stand to lie cross" )
		return ANIMATION_LIE;
	else if ( szName == "lie to stand cross" )
		return ANIMATION_STAND;
	else if ( szName == "throw down" )
		return ANIMATION_THROW_DOWN;
	else if ( szName == "idle2" )
		return ANIMATION_IDLE2;
	else if ( szName == "prisoning" )
		return ANIMATION_PRISONING;

	if ( pbKnown != 0 )
		*pbKnown = false;
	return 0;
}

}

}
