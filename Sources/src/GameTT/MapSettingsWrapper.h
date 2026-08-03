#ifndef __MAPSETTINGSWRAPPER_H__
#define __MAPSETTINGSWRAPPER_H__
#pragma ONCE
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\StreamIO\OptionSystem.h"
#include "MultiplayerCommandManager.h"
#include "..\UI\UI.h"
class COptionsListWrapper;
class CMapSettingsWrapper : public IRefCount
{
	OBJECT_COMPLETE_METHODS(CMapSettingsWrapper);

public:

	struct CFakeOptionSystem : public IOptionSystem
	{
		OBJECT_COMPLETE_METHODS(CFakeOptionSystem);
	private:
		bool bServer;											//server stores options.
	public:
		SMultiplayerGameSettings settings;		// copy of settings.
		
		CFakeOptionSystem() {  }
		CFakeOptionSystem( const bool _bIsServer ) 
			: bServer( _bIsServer )
		{  }

		virtual bool STDCALL Set( const std::string &szVarName, const variant_t &var );
		virtual bool STDCALL Get( const std::string &szVarName, variant_t *pVar ) const;
		
		virtual const SOptionDesc* STDCALL GetDesc( const std::string &szVarName ) const { return GetSingleton<IOptionSystem>()->GetDesc( szVarName ); }
		virtual const std::vector<SOptionDropListValue>& STDCALL GetDropValues( const std::string &szVarName ) const { return GetSingleton<IOptionSystem>()->GetDropValues( szVarName ); }
		virtual IOptionSystemIterator* STDCALL CreateIterator( const DWORD dwMask = 0xffffffff ) { return GetSingleton<IOptionSystem>()->CreateIterator( dwMask ); }
		virtual bool STDCALL SerializeConfig( IDataTree *pSS ) { return GetSingleton<IOptionSystem>()->SerializeConfig( pSS ); }
		virtual bool STDCALL Remove( const std::string &szVarName ) { return GetSingleton<IOptionSystem>()->Remove( szVarName ); }
		virtual bool STDCALL RemoveByMatch( const std::string &szVarMatch ) { return GetSingleton<IOptionSystem>()->RemoveByMatch( szVarMatch ); }
		virtual bool STDCALL ChangeSerialize( const std::string &szVarMatch, bool bInclude ) { return GetSingleton<IOptionSystem>()->ChangeSerialize( szVarMatch, bInclude ); } 
		virtual bool STDCALL IsChanged() const { return GetSingleton<IOptionSystem>()->IsChanged(); }
		virtual void STDCALL Init() {}
		virtual void STDCALL Repair( IDataTree *pSS, const bool bToDefault ) {  }
	};

private:
	CPtr<IUIListControl> pList;
	CPtr<IUIStatic> pGameType;
	CPtr<CFakeOptionSystem> pOptionSystem;
	CPtr<COptionsListWrapper> pListWrapper;	
	bool bCanChange;													// is 
	int nFlag;																// options flag

		
public:
	CMapSettingsWrapper() { }
	CMapSettingsWrapper ( const bool bCanChange, const int bFlag );
	const SMultiplayerGameSettings & GetSettings();
	const SMultiplayerGameSettings & GetSettingsWOApply() const;
	
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	void Init( IUIListControl *_pList, IUIStatic *_pGameType );
	void Init( const SMultiplayerGameSettings &_settings );
	
	void SetGameType ( const int /*SQuickLoadMapInfo::EMultiplayerMapType*/ nGameType );
	void Apply();
	
};
#endif // __MAPSETTINGSWRAPPER_H__
