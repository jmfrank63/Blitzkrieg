#include "StdAfx.h"

#include "UIInternal.h"
#include "UIInternalM.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IManipulator *CUIWindowSubState::GetManipulator()
{
	if ( !pManipulator )
	{
		CUIWindowSubStateManipulator *pSubStateManipulator = new CUIWindowSubStateManipulator;
		pSubStateManipulator->SetSubState( this );
		pManipulator = pSubStateManipulator;
	}
	
	return pManipulator;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIWindowSubState::CopyInternals( CUIWindowSubState *pSS ) const
{
	pSS->subRects = subRects;
	pSS->color = color;
	pSS->specular = specular;
	pSS->textColor = textColor;

	pSS->pTexture = pTexture;						// ������� ��� - ��������
	pSS->pMask = pMask;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIWindowSubState::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Color", &color );
	saver.Add( "Specular", &specular );
	saver.Add( "TextColor", &textColor );
	
	if ( saver.IsReading() )
		LoadTextureAndSubRects( &saver );
	else
		SaveTextureAndSubRects( &saver );

	if ( saver.IsReading() )
		LoadTileRects( &saver );


	if ( saver.IsReading() )
	{
		std::string szName;
		saver.Add( "Mask", &szName );
		if ( szName.size() == 0 )
			pMask = 0;
		else
			pMask = GetSingleton<IMaskManager>()->GetMask( szName.c_str() );
	}
	else
	{
		if ( pMask != 0 )
		{
			std::string szName = GetSingleton<IMaskManager>()->GetMaskName( pMask );
			szName.erase( szName.rfind( '.' ), -1 );
			saver.Add( "Mask", &szName );
		}
	}
	
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SWindowTempTileRect
{
	CTRect<float> rc;
	CVec2 vSize;
	CTRect<int> mapa;
	int operator&( IDataTree &ss );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SWindowTempTileRect::operator &( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "rect", &rc );
	saver.Add( "size", &vSize );
	saver.Add( "map", &mapa );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoadTileRectangles( CTreeAccessor *pFile, std::vector<SWindowSubRect> &subRects, DTChunkID sName, IGFXTexture *pTexture )
{
	NI_ASSERT_T( pFile->IsReading(), "Invalid call of LoadTileRectangles()" );
	
	std::vector<SWindowTempTileRect> temp;
	pFile->Add( sName, &temp );

	if ( temp.empty() )
		return;

	//����������� �������� �������������� � subRects
	if ( pTexture )
	{
		//����������� �� int ��������� � �������� �� ���������� ���������� float
		float fSizeX = pTexture->GetSizeX( 0 );
		float fSizeY = pTexture->GetSizeY( 0 );
		
		SWindowSubRect sub;
		for ( int i=0; i<temp.size(); i++ )
		{
			const SWindowTempTileRect &cur = temp[i];
			sub.mapa.y1 = ( cur.mapa.y1 + 0.5f ) / fSizeY;
			for ( int y=cur.rc.y1; y<cur.rc.y2; y+=cur.vSize.y )
			{
				sub.rc.y1 = y;
				if ( y + cur.vSize.y > cur.rc.y2 )
				{
					sub.rc.y2 = cur.rc.y2;
					float k = sub.rc.Height() / cur.vSize.y;
					sub.mapa.y2 = ( cur.mapa.y1 + k * cur.mapa.y2 + 0.5f ) / fSizeY;
				}
				else
				{
					sub.rc.y2 = y + cur.vSize.y;
					sub.mapa.y2 = ( cur.mapa.y1 + cur.mapa.y2 + 0.5f ) / fSizeY;
				}
				
				sub.mapa.x1 = ( cur.mapa.x1 + 0.5f ) / fSizeX;
				for ( int x=cur.rc.x1; x<cur.rc.x2; x+=cur.vSize.x )
				{
					sub.rc.x1 = x;
					
					if ( x + cur.vSize.x > cur.rc.x2 )
					{
						sub.rc.x2 = cur.rc.x2;
						float k = sub.rc.Width() / cur.vSize.x;
						sub.mapa.x2 = ( cur.mapa.x1 + k * cur.mapa.x2 + 0.5f ) / fSizeX;
					}
					else
					{
						sub.rc.x2 = x + cur.vSize.x;
						sub.mapa.x2 = ( cur.mapa.x1 + cur.mapa.x2 + 0.5f ) / fSizeX;
					}
					
					subRects.push_back( sub );
				}
			}
		}
	}
	else
	{
		//���� ��������, �� ���� �������
		SWindowSubRect sub;
		for ( int i=0; i<temp.size(); i++ )
		{
			sub.rc = temp[i].rc;
			
			sub.mapa.x1 = 0.0f;
			sub.mapa.x2 = 1.0f;
			sub.mapa.y1 = 0.0f;
			sub.mapa.y2 = 1.0f;
			subRects.push_back( sub );
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIWindowSubState::LoadTileRects( CTreeAccessor *pFile )
{
	NI_ASSERT_T( pFile->IsReading(), "Invalid call of LoadTileRects()" );
	
	std::vector<SWindowTempTileRect> temp;
	pFile->Add( "TileRects", &temp );

	if ( temp.empty() )
		return;

	//����������� �������� �������������� � subRects
	if ( pTexture )
	{
		//����������� �� int ��������� � �������� �� ���������� ���������� float
		float fSizeX = pTexture->GetSizeX( 0 );
		float fSizeY = pTexture->GetSizeY( 0 );
		
		SWindowSubRect sub;
		for ( int i=0; i<temp.size(); i++ )
		{
			const SWindowTempTileRect &cur = temp[i];
			sub.mapa.y1 = ( cur.mapa.y1 + 0.5f ) / fSizeY;
			for ( int y=cur.rc.y1; y<cur.rc.y2; y+=cur.vSize.y )
			{
				sub.rc.y1 = y;
				if ( y + cur.vSize.y > cur.rc.y2 )
				{
					sub.rc.y2 = cur.rc.y2;
					float k = sub.rc.Height() / cur.vSize.y;
					sub.mapa.y2 = ( cur.mapa.y1 + k * cur.mapa.y2 + 0.5f ) / fSizeY;
				}
				else
				{
					sub.rc.y2 = y + cur.vSize.y;
					sub.mapa.y2 = ( cur.mapa.y1 + cur.mapa.y2 + 0.5f ) / fSizeY;
				}

				sub.mapa.x1 = ( cur.mapa.x1 + 0.5f ) / fSizeX;
				for ( int x=cur.rc.x1; x<cur.rc.x2; x+=cur.vSize.x )
				{
					sub.rc.x1 = x;

					if ( x + cur.vSize.x > cur.rc.x2 )
					{
						sub.rc.x2 = cur.rc.x2;
						float k = sub.rc.Width() / cur.vSize.x;
						sub.mapa.x2 = ( cur.mapa.x1 + k * cur.mapa.x2 + 0.5f ) / fSizeX;
					}
					else
					{
						sub.rc.x2 = x + cur.vSize.x;
						sub.mapa.x2 = ( cur.mapa.x1 + cur.mapa.x2 + 0.5f ) / fSizeX;
					}

					subRects.push_back( sub );
				}
			}
		}
	}
	else
	{
		//���� ��������, �� ���� �������
		SWindowSubRect sub;
		for ( int i=0; i<temp.size(); i++ )
		{
			sub.rc = temp[i].rc;
			
			sub.mapa.x1 = 0.0f;
			sub.mapa.x2 = 1.0f;
			sub.mapa.y1 = 0.0f;
			sub.mapa.y2 = 1.0f;
			subRects.push_back( sub );
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SWindowTempSubRect
{
	CTRect<float> rc;
	CTRect<int> mapa;
	int operator&( IDataTree &ss );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SWindowTempSubRect::operator &( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "rect", &rc );
	saver.Add( "map", &mapa );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIWindowSubState::SaveTextureAndSubRects( CTreeAccessor *pFile )
{
	NI_ASSERT_T( !pFile->IsReading(), "Invalid call of SaveTextureAndSubRects()" );
	
	if ( pTexture == 0 )
	{
		std::vector<SWindowTempSubRect> temp;
		SWindowTempSubRect sub;
		for ( int i=0; i<subRects.size(); i++ )
		{
			SWindowTempSubRect sub;
			sub.rc = subRects[i].rc;
			sub.mapa.x1 = 0;
			sub.mapa.x2 = 128;
			sub.mapa.y1 = 0;
			sub.mapa.y2 = 128;
			temp.push_back( sub );
		}
		pFile->Add( "SubRects", &temp );
		return;
	}
	
	std::string szName = GetSingleton<ITextureManager>()->GetTextureName( pTexture );
	szName.erase( szName.rfind( '.' ), -1 );
	pFile->Add( "Texture", &szName );
	
	float fSizeX = pTexture->GetSizeX( 0 );
	float fSizeY = pTexture->GetSizeY( 0 );
	std::vector<SWindowTempSubRect> temp;
	SWindowTempSubRect sub;
	//��������� ������ ��������������� � int ������������
	for ( int i=0; i<subRects.size(); i++ )
	{
		sub.rc = subRects[i].rc;
		sub.mapa.x1 = subRects[i].mapa.x1 * fSizeX - 0.5f;
		sub.mapa.x2 = subRects[i].mapa.x2 * fSizeX - 0.5f - sub.mapa.x1;
		sub.mapa.y1 = subRects[i].mapa.y1 * fSizeY - 0.5f;
		sub.mapa.y2 = subRects[i].mapa.y2 * fSizeY - 0.5f - sub.mapa.y1;
		temp.push_back( sub );
	}
	pFile->Add( "SubRects", &temp );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIWindowSubState::LoadTextureAndSubRects( CTreeAccessor *pFile )
{
	NI_ASSERT_T( pFile->IsReading(), "Invalid call of LoadTextureAndSubRects()" );
	
	std::string szName;
	pFile->Add( "Texture", &szName );
	if ( szName.size() == 0 )
		pTexture = 0;
	else
		pTexture = GetSingleton<ITextureManager>()->GetTexture( szName.c_str() );
	
	std::vector<SWindowTempSubRect> temp;
	pFile->Add( "SubRects", &temp );

	//CRAP
	if ( temp.empty() )
	{
		//�������� ���� �� ������� ������ � �������� �� � ������ subRects
		if ( pTexture )
		{
			//����������� �� int ��������� � �������� �� ���������� ���������� float
			CTRect<int> rc( -1, -1, -1, -1 );
			pFile->Add( "Maps", &rc );

			if ( rc.x1 == -1 && rc.x2 == -1 && rc.y1 == -1 && rc.y2 == -1 )
				return;

			float fSizeX = pTexture->GetSizeX( 0 );
			float fSizeY = pTexture->GetSizeY( 0 );
			
			SWindowSubRect sub;

			sub.rc.x1 = sub.rc.y1 = -1.0f;
			sub.rc.x2 = sub.rc.y2 = -1.0f;
/*
			sub.rc.x1 = sub.rc.y1 = 0.0f;
			sub.rc.x2 = rc.Width();
			sub.rc.y2 = rc.Height();
*/
			sub.mapa.x1 = ( rc.x1 + 0.5f ) / fSizeX;
			sub.mapa.x2 = ( rc.x1 + rc.x2 + 0.5f ) / fSizeX;
			sub.mapa.y1 = ( rc.y1 + 0.5f ) / fSizeY;
			sub.mapa.y2 = ( rc.y1 + rc.y2 + 0.5f ) / fSizeY;

			subRects.push_back( sub );
		}
		else
		{
			SWindowSubRect sub;
			sub.rc.x1 = sub.rc.y1 = -1.0f;
			sub.rc.x2 = sub.rc.y2 = -1.0f;

			sub.mapa.x1 = 0.0f;
			sub.mapa.x2 = 1.0f;
			sub.mapa.y1 = 0.0f;
			sub.mapa.y2 = 1.0f;

			subRects.push_back( sub );
		}

		return;
	}
	//end of CRAP
	
	if ( pTexture )
	{
		//����������� �� int ��������� � �������� �� ���������� ���������� float
		float fSizeX = pTexture->GetSizeX( 0 );
		float fSizeY = pTexture->GetSizeY( 0 );
		
		SWindowSubRect sub;
		for ( int i=0; i<temp.size(); i++ )
		{
			sub.rc = temp[i].rc;

			sub.mapa.x1 = ( temp[i].mapa.x1 + 0.5f ) / fSizeX;
			sub.mapa.x2 = ( temp[i].mapa.x1 + temp[i].mapa.x2 + 0.5f ) / fSizeX;
			sub.mapa.y1 = ( temp[i].mapa.y1 + 0.5f ) / fSizeY;
			sub.mapa.y2 = ( temp[i].mapa.y1 + temp[i].mapa.y2 + 0.5f ) / fSizeY;
			subRects.push_back( sub );
		}
	}
	else
	{
		SWindowSubRect sub;
		for ( int i=0; i<subRects.size(); i++ )
		{
			sub.rc = temp[i].rc;

			sub.mapa.x1 = 0.0f;
			sub.mapa.x2 = 1.0f;
			sub.mapa.y1 = 0.0f;
			sub.mapa.y2 = 1.0f;
			subRects.push_back( sub );
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIWindowSubState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &color );
	saver.Add( 2, &specular );
	saver.Add( 3, &pTexture );
	saver.Add( 4, &subRects );
	saver.Add( 5, &textColor );
	saver.Add( 6, &pMask );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IManipulator *CWindowState::GetManipulator()
{
	if ( !pManipulator )
	{
		CWindowStateManipulator *pStateManipulator = new CWindowStateManipulator;
		pStateManipulator->SetState( this );
		pManipulator = pStateManipulator;
	}
	
	return pManipulator;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindowState::CopyInternals( CWindowState * pS ) const
{
	pS->szPushSound = szPushSound;
	pS->szClickSound = szClickSound;
	pS->szKey = szKey;
	pS->szToolKey = szToolKey;

	for ( int i = 0; i < 4; ++i )
		subStates[i].CopyInternals( &pS->subStates[i] ); 

	pS->InitDependentInfo();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CWindowState::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "AppearanceNormal", &subStates[0] );
	saver.Add( "AppearanceHighlighted", &subStates[1] );
	saver.Add( "AppearancePushed", &subStates[2] );
	saver.Add( "AppearanceDisabled", &subStates[3] );
	saver.Add( "PushSound", &szPushSound );
	saver.Add( "ClickSound", &szClickSound );
	saver.Add( "ToolTip", &szToolKey );
	//������ ���������� � CSimpleWindow::operator &()
	
	//�����
	saver.Add( "TextKey", &szKey );
	
	if ( saver.IsReading() )
	{
		InitDependentInfo();
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindowState::InitDependentInfo()
{
	pGfxText = CreateObject<IGFXText>( GFX_TEXT );
	ITextManager *pTextManager = GetSingleton<ITextManager>();
	IText *pText = pTextManager->GetString( szKey.c_str() );
	if ( !pText )
	{
		if ( !szKey.empty() )
			pText = pTextManager->GetDialog( szKey.c_str() );

		if ( !pText )
			pText = CreateObject<IText>( TEXT_STRING );
	}

	pGfxText->SetText( pText );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CWindowState::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &subStates[0] );
	saver.Add( 2, &subStates[1] );
	saver.Add( 3, &subStates[2] );
	saver.Add( 4, &subStates[3] );

	//�����
	saver.Add( 5, &szPushSound );
	saver.Add( 6, &szClickSound );

	//�����
	saver.Add( 7, &pGfxText );
	saver.Add( 8, &szKey );

	//tooltip
	saver.Add( 9, &szToolKey );
	saver.Add( 10, &pToolText );
	
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveTextureAndMap( CTreeAccessor *pFile, IGFXTexture *pTexture, DTChunkID tName, const CTRect<float> &maps, DTChunkID mName )
{
	NI_ASSERT_T( !pFile->IsReading(), "Invalid call of SaveTextureAndMap()" );

	if ( pTexture == 0 )
	{
		CTRect<int> rc;
		rc.x1 = 0;
		rc.x2 = 128;
		rc.y1 = 0;
		rc.y2 = 128;
		pFile->Add( mName, &rc );

		return;
	}

	std::string szName = GetSingleton<ITextureManager>()->GetTextureName( pTexture );
	szName.erase( szName.rfind( '.' ), -1 );
	pFile->Add( tName, &szName );
	
	if ( pTexture )
	{
		//����������� �� float ��������� � int ���������� ��������
		float fSizeX = pTexture->GetSizeX( 0 );
		float fSizeY = pTexture->GetSizeY( 0 );
		
		CTRect<int> rc;
		rc.x1 = maps.x1 * fSizeX - 0.5f;
		rc.x2 = maps.x2 * fSizeX - 0.5f - rc.x1;
		rc.y1 = maps.y1 * fSizeY - 0.5f;
		rc.y2 = maps.y2 * fSizeY - 0.5f - rc.y1;
		pFile->Add( mName, &rc );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoadTextureAndMap( CTreeAccessor *pFile, CPtr<IGFXTexture> *ppTexture, DTChunkID tName, CTRect<float> *pMaps, DTChunkID mName )
{
	NI_ASSERT_T( pFile->IsReading(), "Invalid call of LoadTextureAndMap()" );

	std::string szName;
	pFile->Add( tName, &szName );
	if ( szName.size() == 0 )
		*ppTexture = 0;
	else
		*ppTexture = GetSingleton<ITextureManager>()->GetTexture( szName.c_str() );
	
	if ( *ppTexture )
	{
		//����������� �� int ��������� � �������� �� ���������� ���������� float
		CTRect<int> rc;
		pFile->Add( mName, &rc );
		float fSizeX = (*ppTexture)->GetSizeX( 0 );
		float fSizeY = (*ppTexture)->GetSizeY( 0 );
		
		pMaps->x1 = ( rc.x1 + 0.5f ) / fSizeX;
		pMaps->x2 = ( rc.x1 + rc.x2 + 0.5f ) / fSizeX;
		pMaps->y1 = ( rc.y1 + 0.5f ) / fSizeY;
		pMaps->y2 = ( rc.y1 + rc.y2 + 0.5f ) / fSizeY;
	}
	else
	{
		pMaps->x1 = 0.0f;
		pMaps->x2 = 1.0f;
		pMaps->y1 = 0.0f;
		pMaps->y2 = 1.0f;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveSound( CTreeAccessor *pFile, ISound *pSound, DTChunkID sName )
{
	NI_ASSERT_T( !pFile->IsReading(), "Invalid call of SaveSound()" );
	
	if ( pSound == 0 )
	{
		pFile->Add( sName, "" );
		return;
	}
	
	std::string szName = GetSingleton<ISoundManager>()->GetSoundName( pSound );
	szName.erase( szName.rfind( '.' ), -1 );
	pFile->Add( sName, &szName );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LoadSound( CTreeAccessor *pFile, CPtr<ISound> *ppSound, DTChunkID sName )
{
	NI_ASSERT_T( pFile->IsReading(), "Invalid call of LoadSound()" );
	
	std::string szName;
	pFile->Add( sName, &szName );
	if ( szName.size() == 0 )
		*ppSound = 0;
	else
		*ppSound = GetSingleton<ISoundManager>()->GetSound2D( szName.c_str() );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
