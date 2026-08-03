#ifndef __TEXTURE_MANAGER_H__
#define __TEXTURE_MANAGER_H__
#pragma ONCE
#include "../Misc/BasicShare.h"
#include "TextSystem.h"
#include "TextObject.h"
class CTextStringShare : public CBasicShare<std::string, CTextString, TEXT_STRING>
{
protected:
	virtual CTextString* Create( const std::string &key ) { return 0; }
public:
	CTextStringShare() : CBasicShare<std::string, CTextString, TEXT_STRING>( 111 ) {  }
};
BASIC_SHARE_DECLARE( CTextDialogShare, std::string, CTextDialog, TEXT_DIALOG, 112, ".txt" );
class CTextManager : public ITextManager
{
	OBJECT_COMPLETE_METHODS( CTextManager );
	DECLARE_SERIALIZE;
	CTextStringShare shareString;
	CTextDialogShare shareDialog;
public:
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) { shareDialog.SetSerialMode( eSerialMode ); }
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) { shareDialog.SetShareMode( eShareMode ); }
	virtual void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	virtual bool STDCALL Init();
	virtual bool STDCALL AddTextFile( const char *pszFileName );
	virtual IText* STDCALL GetString( const char *pszKey );
	virtual IText* STDCALL GetDialog( const char *pszKey ) { return shareDialog.Get( pszKey ); }
	virtual const char* STDCALL GetTextName( IText *pText )
	{
		if ( CTextDialog *pDlg = dynamic_cast<CTextDialog*>(pText) )
		{
			const std::string *pName = shareDialog.GetKey( pDlg );
			return pName != 0 ? pName->c_str() : "";
		}
		else if ( CTextString *pStr = dynamic_cast<CTextString*>(pText) )
		{
			const std::string *pName = shareString.GetKey( pStr );
			return pName != 0 ? pName->c_str() : "";
		}
		else
			return "";
	}
};
#endif		//__TEXTURE_MANAGER_H__
