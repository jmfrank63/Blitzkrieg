#ifndef __CUSTOMMESSAGEREACTION_H__
#define __CUSTOMMESSAGEREACTION_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCustomMessageReaction
{
	DECLARE_SERIALIZE;

	typedef void (*MESSAGE_REACTION)( class CInterfaceScreenBase *_pInterface );

	struct SReactionDescriptor
	{
		MESSAGE_REACTION pfnReaction;
		SReactionDescriptor() {  }
		SReactionDescriptor( MESSAGE_REACTION _pfnReaction )
			: pfnReaction( _pfnReaction )
		{
		}
	};

	typedef std::unordered_map<std::string, SReactionDescriptor> CReactions;
	CReactions reactions;
public:

	void Init();
	void Clear();
	void LaunchReaction( const std::string &szCutomReactionName, class CInterfaceScreenBase *_pInterface);
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __CUSTOMMESSAGEREACTION_H__
