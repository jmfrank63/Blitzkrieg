#include "StdAfx.h"
#include "UINumberIndicator.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUINumberIndicator::SValueColor::operator &( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Value", &fVal );
	saver.Add( "Color", &dwColor );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUINumberIndicator::SValueColor::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &fVal );
	saver.Add( 2, &dwColor );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUINumberIndicator::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>(this) );

	saver.Add( "Value", &m_fVal );
	saver.Add( "Colors", &valueColors );

	if ( saver.IsReading() )
	{
		std::sort( valueColors.begin(), valueColors.end() );
//		valueColors.sort();
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUINumberIndicator::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CSimpleWindow*>(this) );
	saver.Add( 4, &m_fVal );
	saver.Add( 5, &valueColors );

	if ( saver.IsReading() )
	{
		std::sort( valueColors.begin(), valueColors.end() );
//		valueColors.sort();
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUINumberIndicator::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !IsVisible() )
		return;
	CSimpleWindow::Visit( pVisitor );
	
	// ������ ����������
	DWORD dwColor = 0xffffffff;
	if ( valueColors.size() >= 2 )
	{
		do
		{
			//������������� ����
			if ( m_fVal <= valueColors[0].fVal )
			{
				dwColor = valueColors[0].dwColor;
				break;
			}
			if ( m_fVal >= valueColors[valueColors.size()-1].fVal )
			{
				dwColor = valueColors[valueColors.size()-1].dwColor;
				break;
			}

			int i = 1;
			for ( ; i<valueColors.size(); i++ )
			{
				if ( m_fVal >= valueColors[i-1].fVal && m_fVal <= valueColors[i].fVal )
					break;
			}
			
			NI_ASSERT_T( i != valueColors.size(), "Wrong color values" );
			dwColor = 0xff000000;
			float fMul = (float) (m_fVal - valueColors[i-1].fVal) / (valueColors[i].fVal - valueColors[i-1].fVal);
			int a = valueColors[i-1].dwColor;
			int b = valueColors[i].dwColor;
			DWORD dwMask = 0x00ff0000;		//red
			int nTemp = (float) fMul * ((float) ((b & dwMask) >> 16) - ((a & dwMask) >> 16)) + ((a & dwMask) >> 16);
			dwColor |= nTemp << 16;
			dwMask = 0x0000ff00;					//green
			nTemp = (float) fMul * ((float) ((b & dwMask) >> 8) - ((a & dwMask) >> 8)) + ((a & dwMask) >> 8);
			dwColor |= nTemp << 8;
			dwMask = 0x000000ff;					//blue
			nTemp = (float) fMul * ((float) (b & dwMask) - (a & dwMask)) + (a & dwMask);
			dwColor |= nTemp;
		} while ( 0 );
	}
	//
	SGFXRect2 rc;
	rc.rect = wndRect;
	rc.rect.right = m_fVal * (wndRect.right - wndRect.left) + wndRect.left;
	rc.color = dwColor;
	rc.fZ = 0;
	pVisitor->VisitUIRects( 0, 3, &rc, 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUINumberIndicator::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;

	if ( !IsVisible() )
		return;
	CSimpleWindow::Draw( pGFX );
	
	//������ ����������
	pGFX->SetShadingEffect( 3 );
	DWORD dwColor = 0xffffffff;
	if ( valueColors.size() >= 2 )
	{
		do
		{
			//������������� ����
			if ( m_fVal <= valueColors[0].fVal )
			{
				dwColor = valueColors[0].dwColor;
				break;
			}
			if ( m_fVal >= valueColors[valueColors.size()-1].fVal )
			{
				dwColor = valueColors[valueColors.size()-1].dwColor;
				break;
			}

			int i = 1;
			for ( ; i<valueColors.size(); i++ )
			{
				if ( m_fVal >= valueColors[i-1].fVal && m_fVal <= valueColors[i].fVal )
					break;
			}
			
			NI_ASSERT_T( i != valueColors.size(), "Wrong color values" );
			dwColor = 0xff000000;
			float fMul = (float) (m_fVal - valueColors[i-1].fVal) / (valueColors[i].fVal - valueColors[i-1].fVal);
			int a = valueColors[i-1].dwColor;
			int b = valueColors[i].dwColor;
			DWORD dwMask = 0x00ff0000;		//red
			int nTemp = (float) fMul * ((float) ((b & dwMask) >> 16) - ((a & dwMask) >> 16)) + ((a & dwMask) >> 16);
			dwColor |= nTemp << 16;
			dwMask = 0x0000ff00;					//green
			nTemp = (float) fMul * ((float) ((b & dwMask) >> 8) - ((a & dwMask) >> 8)) + ((a & dwMask) >> 8);
			dwColor |= nTemp << 8;
			dwMask = 0x000000ff;					//blue
			nTemp = (float) fMul * ((float) (b & dwMask) - (a & dwMask)) + (a & dwMask);
			dwColor |= nTemp;
		} while ( 0 );
	}
	
	pGFX->SetTexture( 0, 0 );
	SGFXRect2 rc;
	rc.rect = wndRect;
	rc.rect.right = m_fVal * (wndRect.right - wndRect.left) + wndRect.left;
	rc.color = dwColor;
	rc.fZ = 0;
	pGFX->DrawRects( &rc, 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUINumberIndicator::SetColor( float fVal, DWORD dwColor )
{
	for ( std::vector<SValueColor>::iterator it=valueColors.begin(); it!=valueColors.end(); ++it )
	{
		if ( it->fVal > fVal )
		{
			SValueColor val;
			val.dwColor = dwColor;
			val.fVal = fVal;
			valueColors.insert( it, val );
			return;
		}
	}

	SValueColor val;
	val.dwColor = dwColor;
	val.fVal = fVal;
	valueColors.push_back( val );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUINumberIndicator::SetValue( float fVal )
{
	NI_ASSERT_T( !valueColors.empty(), "Empty CUINumberIndicator control, SetValue() error" );
	NI_ASSERT_T( fVal >= valueColors.front().fVal, "CUINumberIndicator error: attempt to set value lesser then min value of control" );
	NI_ASSERT_T( fVal <= valueColors.back().fVal, "CUINumberIndicator error: attempt to set value higher then max value of control" );

	if ( fVal < valueColors.front().fVal )
		m_fVal = valueColors.front().fVal;
	else if ( fVal > valueColors.back().fVal )
		m_fVal = valueColors.back().fVal;
	else
		m_fVal = fVal;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
