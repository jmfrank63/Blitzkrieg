
#include "StdAfx.h"
#include "editor.h"
#include "RoadDrawState.h"
#include "PropertieDialog.h"
#include "InputState.h"

#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "../GFX/GFX.h"
#include "../GFX/GFXHelper.h"
#include "../Scene/Terrain.h"
#include "../Image/Image.h"
#include "../Scene/Scene.h"
#include "../Anim/Animation.h"
#include "IUndoRedoCmd.h"
#include "DrawingTools.h"
#include "ObjectPlacerState.h"
#include "../RandomMapGen/VSO_Types.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

void CRoadDrawState::Enter()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}

void CRoadDrawState::Leave()
{
	if ( CTemplateEditorFrame *frame = g_frameManager.GetTemplateEditorFrame() )
	{
		if ( GetSingleton<IAIEditor>() != 0 )
		{
			if ( tmpSpan )
			{
				GetSingleton<IAIEditor>()->DeleteObject( tmpSpan->pAIObj );	
				tmpSpan = 0;
			}
		}
		if ( frame->m_currentFences.size() )
		{
			for( std::vector<IVisObj*>::iterator it = frame->m_currentFences.begin(); it != frame->m_currentFences.end(); ++it )
			{
				frame->RemoveObject( *it );
			}
			frame->m_currentFences.clear();
		} 
		m_pointForTrench.clear();
		frame->RedrawWindow();
	}
}

void CRoadDrawState::Update()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}


std::vector<CVec2> CRoadDrawState::GetPointsForBridge( CVec2 &begin, CVec2 &end, CTemplateEditorFrame *frame )
{
	std::vector<CVec2> retVal;
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc* pDesc = pGDB->GetDesc( frame->m_mapEditorBarPtr->GetBridgeWnd()->GetBridgeName().c_str() );
	if ( pDesc )
	{
		const SBridgeRPGStats *pStats = static_cast<const SBridgeRPGStats*>( pGDB->GetRPGStats( pDesc ) );
		if ( pStats )
		{
			float fLength = pStats->GetSpanStats(pStats->states[0].lines[0]).fLength * fWorldCellSize / 2.0f;
			if ( pStats->direction == SBridgeRPGStats::EDirection::HORIZONTAL )
			{
				if ( begin.x > end.x )
					std::swap( begin, end );
				CVec3 vBeginPoint( begin.x, begin.y, 0.0f );
				FitVisOrigin2AIGrid( &vBeginPoint, pStats->GetOrigin( pStats->GetRandomBeginIndex() ) );
				int nParts = ( end.x - begin.x ) / fLength;
				retVal.push_back( CVec2( vBeginPoint.x - 0.1f, vBeginPoint.y ) );
				for ( int nIndex = 0; nIndex < nParts; ++nIndex )
					retVal.push_back( CVec2( vBeginPoint.x + ( nIndex + 0.5f ) * fLength , vBeginPoint.y ) );
				retVal.push_back( CVec2( vBeginPoint.x + nParts * fLength, vBeginPoint.y ) );
			}
			if ( pStats->direction == SBridgeRPGStats::EDirection::VERTICAL )
			{
				if ( begin.y > end.y )
					std::swap( begin, end );
				CVec3 vBeginPoint( begin.x, begin.y, 0.0f );
				FitVisOrigin2AIGrid( &vBeginPoint, pStats->GetOrigin( pStats->GetRandomBeginIndex() ) );
				int nParts = ( end.y - begin.y ) / fLength;
				retVal.push_back( CVec2( vBeginPoint.x, vBeginPoint.y ) );
				for ( int nIndex = 0; nIndex < nParts; ++nIndex )
					retVal.push_back( CVec2( vBeginPoint.x, vBeginPoint.y + ( nIndex + 0.5f ) * fLength ) );
				retVal.push_back( CVec2( vBeginPoint.x, vBeginPoint.y + nParts * fLength + 0.1f ) );
			}
		}
	}	
	return retVal;  
}


int GetVToolsState( CTemplateEditorFrame* frame )
{
		 return frame->m_mapEditorBarPtr->GetRoadEditorState();	
}
struct APointHelper
{
	std::vector<GPoint> m_points;
	APointHelper() {}
	bool operator() ( long x, long y ) { m_points.push_back( GPoint( x, y ) );return true; }
};
template <class Fn>
bool a_dirLine(long X1, long Y1, long X2, long Y2, Fn &pixf)
{
	long t,e,DeltaX,DeltaY,dirX,dirY,denom;
	dirY = 1;
	DeltaY = Y2-Y1;
	if (DeltaY<0) {
		dirY = -1;
		DeltaY = -DeltaY;
	}
	dirX = 1;
	DeltaX = X2-X1;
	if (DeltaX<0) {
		dirX = -1;
		DeltaX = -DeltaX;
	}
	if (DeltaX==0) {
		for (;;) {
			if (!pixf(X1,Y1)) return false;
			if (Y1==Y2) return true;
			Y1+=dirY;
		}
	}
	if (DeltaY==0) {
		for (;;) {
			if (!pixf(X1,Y1)) return false;
			if (X1==X2) return true;
			X1+=dirX;
		}
	}
	if (DeltaX>=DeltaY) {
		e = -DeltaX;
		denom = DeltaX*2;
		t = DeltaY*2;
		for (;;) {
			if (!pixf(X1,Y1)) return false;
			if (X1==X2) return true;
			X1+=dirX;
			e+=t;
			if (e>=0) {
				Y1+=dirY;
				e-=denom;
			}
		}
	} else {
		e = -DeltaY;
		denom = DeltaY*2;
		t = DeltaX*2;
		for (;;) {
			if (!pixf(X1,Y1)) return false;
			if (Y1==Y2) return true;
			Y1+=dirY;
			e+=t;
			if (e>=0) {
				X1+=dirX;
				e-=denom;
			}
		}
	}
	return true;
}

class CAngle
{
	float value;
public:
		CAngle() {}
		CAngle( float val ) { value = val;}
		CAngle( const CAngle &val ) { value = val.value; }
		
		void operator += ( const float val ) 
		{
			value += val;
			value = fmod( value, FP_2PI );
			value = value < 0 ? FP_2PI + value : value;
		}
		
		void operator -= ( const float val ) 
		{
			value -= val;
			value = fmod( value, FP_2PI );
			value = value < 0 ? FP_2PI + value : value;
		}
		
		void operator += ( const CAngle val ) 
		{
			value += val.value;
			value = fmod( value, FP_2PI );
			value = value < 0 ? FP_2PI + value : value;
		}
		
		void operator -= ( const CAngle val ) 
		{
			value -= val.value;
			value = fmod( value, FP_2PI );
			value = value < 0 ? FP_2PI + value : value;
		}	
		
		CAngle operator + ( const CAngle &val ) 
		{
			CAngle t( *this );  
			t += val; 
			return t;
		}
		
		CAngle operator - ( const CAngle &val ) 
		{
			CAngle t( *this );  
			t -= val; 
			return t;
		}	
		operator float() const
		{
			return value;
		}	
};
float GetLineAngle( const CVec2 &vBegin, const CVec2 &vEnd )
{
	CVec2 vTmp = vEnd - vBegin;  
	Normalize( &vTmp );
	float fAngle = acos ( ( vTmp.x )/ fabs( vTmp.x, vTmp.y ) );
	if ( vTmp.y < 0 )
		fAngle = FP_2PI - fAngle;
	return fAngle;
}
float GetTrenchWidth( int nType )// 0 - секция , 1 - поворот
{
	IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
	const SGDBObjectDesc *pDesc = pGDB->GetDesc( "Entrenchment" );
	const SEntrenchmentRPGStats *pRPG = static_cast<const SEntrenchmentRPGStats*>( pGDB->GetRPGStats( pDesc ) );
	int nFrameIndex;
	if ( nType == 0 )
		nFrameIndex = pRPG->GetLineIndex();
	if ( nType == 1 )
		nFrameIndex = pRPG->GetArcIndex();

	return pRPG->segments[nFrameIndex].GetVisAABBHalfSize().x * 2;
}
std::vector<GPoint> SplitLineToSegrments( CVec2 vBegin, CVec2 vEnd, float TRENCHWIDTH )
{
	GPoint currentPoint = GPoint( vBegin.x, vBegin.y) ;
	std::vector<GPoint> vPoints; 

	if( vBegin == vEnd )
		return vPoints;
	
	CAngle angle = GetLineAngle( vBegin, vEnd );
	float allLength = fabs( vEnd.x - vBegin.x, vEnd.y - vBegin.y );
	CVec2 vAddSegment;
	vAddSegment.x = cos( angle ) * TRENCHWIDTH ;
	vAddSegment.y = sin( angle ) * TRENCHWIDTH ;
	vAddSegment.x += currentPoint.x;
	vAddSegment.y += currentPoint.y;
	if( fabs( vAddSegment.x - currentPoint.x, vAddSegment.y - currentPoint.y ) < allLength )
	{
	 	vPoints.push_back( GPoint( currentPoint.x, currentPoint.y ) );
	}
 
	while( fabs( vBegin.x - vAddSegment.x, vBegin.y - vAddSegment.y ) < allLength )
	{
		vPoints.push_back( GPoint( vAddSegment.x, vAddSegment.y ) );
		currentPoint = GPoint( vAddSegment.x, vAddSegment.y );
		vAddSegment.x = cos( angle ) * TRENCHWIDTH ;
		vAddSegment.y = sin( angle ) * TRENCHWIDTH ;
		vAddSegment.x += currentPoint.x;
		vAddSegment.y += currentPoint.y;
	}	
	return vPoints;
}


class CConnector
{
	float m_beginDir; //начальный угол
	float m_endDir;		//конечный угол 
public:
	std::vector<GPoint> m_points;
	
	CConnector( GPoint begPos, float beginAngle, float endAngle )
	{
		float SEGMENTLENGHT = GetTrenchWidth( 1 );
		if ( fabs( endAngle - beginAngle) < ( FP_PI / 6.0f ) )
		{
		}
		else 
		{
			std::vector<GPoint> m_pointsAntiClockWise;
			std::vector<GPoint> m_pointsClockWise;
			
			{// по часовой стрелке будем достраивать кусочки
				CAngle tempAngle = beginAngle ;
				while ( fabs( endAngle - tempAngle ) > ( FP_PI / 6.0f ) )
				{
					GPoint p;
					if ( m_pointsClockWise.empty() )
					{
						tempAngle += ( FP_PI / 12.0f );
					}
					else
					{
						tempAngle += ( FP_PI / 6.0f );
					}

					p.x = SEGMENTLENGHT * cos ( tempAngle );
					p.y = SEGMENTLENGHT * sin ( tempAngle );
					if ( !m_pointsClockWise.size() )
						p += begPos; // для первого сегмента коннектора в начальную точку
					else
						p += m_pointsClockWise[ m_pointsClockWise.size() - 1 ] ; // для всех последующих надо двигать к предудущей точке
					m_pointsClockWise.push_back( p );					
				}			
			}
			{// против  часовой стрелки будем достраивать кусочки
				CAngle tempAngle = beginAngle ;
				while ( fabs( endAngle - tempAngle ) > ( FP_PI / 6.0f ) )
				{
					GPoint p;
					if( m_pointsAntiClockWise.empty() )
					{
						tempAngle -= ( FP_PI / 12.0f );
					}
					else
					{
						tempAngle -= ( FP_PI / 6.0f );
					}

					p.x = SEGMENTLENGHT * cos ( tempAngle );
					p.y = SEGMENTLENGHT * sin ( tempAngle );
					if ( !m_pointsAntiClockWise.size() )
						p += begPos; // для первого сегмента коннектора в начальную точку
					else
						p += m_pointsAntiClockWise[ m_pointsAntiClockWise.size() - 1 ] ; // для всех последующих надо двигать к предудущей точке
					m_pointsAntiClockWise.push_back( p );  
					
				}
			}
			if ( m_pointsAntiClockWise.size() > m_pointsClockWise.size() )
				m_points = m_pointsClockWise;
			else
				m_points = m_pointsAntiClockWise;
		}
	}
};
/**
void DrawHelpRect( DWORD color, CVec3 pos, float fR, CSceneDrawTool *pTool )
{
	pTool->DrawLine( CVec3( pos.x - fR, pos.y - fR, 0 ), CVec3( pos.x + fR, pos.y - fR, 0 ), color );
	pTool->DrawLine( CVec3( pos.x + fR, pos.y - fR, 0 ), CVec3( pos.x + fR, pos.y + fR, 0 ), color );
	pTool->DrawLine( CVec3( pos.x + fR, pos.y + fR, 0 ), CVec3( pos.x - fR, pos.y + fR, 0 ), color );
	pTool->DrawLine( CVec3( pos.x - fR, pos.y + fR, 0 ), CVec3( pos.x - fR, pos.y - fR, 0 ), color );
}
/**/

/**
bool IsPointInPolygon( const CVec3 *points, const int *indices, const int nNumIndices, const CVec2 &vPoint )
{
	bool bInside = false;
	
	for ( int i=0; i<nNumIndices; ++i )
	{
		const CVec3 &vBeg = points[ indices[i] ];
		const CVec3 &vEnd = points[ indices[(i + 1) % nNumIndices] ];
		if ( (vEnd.y <= vPoint.y) && (vPoint.y < vBeg.y) && ( (vBeg.y - vEnd.y) * (vPoint.x - vEnd.x) < (vPoint.y - vEnd.y) * (vBeg.x - vEnd.x) ) )
			bInside = !bInside;
		else if ( (vBeg.y <= vPoint.y) && (vPoint.y < vEnd.y) && ( (vBeg.y - vEnd.y) * (vPoint.x - vEnd.x) > (vPoint.y - vEnd.y) * (vBeg.x - vEnd.x) ) )
			bInside = !bInside;
	}
	
	return bInside;
}
/**/

/**
bool IsPointInSimplePolygonEdge( const std::vector<CVec2> &points, const int nNumPoints, const CVec2 &vPoint, float fEdge )
{
	bool isEdge = false;
	for ( int index = 0; index < nNumPoints; ++index )
	{
		CLine2 side( points[index], points[index + 1] );
		if ( fabs( side.DistToPoint( vPoint ) ) < fEdge )
		{
			bool isProjectPointInSide = true;
			CVec2 projectPointOnSide( 0, 0 );
			side.ProjectPoint( vPoint, &projectPointOnSide );
			if ( points[index].x > points[index + 1].x )
			{
				if ( ( points[index].x < projectPointOnSide.x ) || 
						 ( points[index + 1].x > projectPointOnSide.x ) )
				{
					isProjectPointInSide = false;
				}
			}
			else if ( points[index].x < points[index + 1].x )
			{
				if ( ( points[index + 1].x < projectPointOnSide.x ) || 
						 ( points[index].x > projectPointOnSide.x ) )
				{
					isProjectPointInSide = false;
				}
			}
			if ( points[index].y >  points[index + 1].y )
			{
				if ( ( points[index].y < projectPointOnSide.y ) || 
						 ( points[index + 1].y > projectPointOnSide.y ) )
				{
					isProjectPointInSide = false;
				}
			}
			else if ( points[index].y < points[index + 1].y )
			{
				if ( ( points[index + 1].y < projectPointOnSide.y ) || 
						 ( points[index].y > projectPointOnSide.y ) )
				{
					isProjectPointInSide = false;
				}
			}
			if ( isProjectPointInSide )
			{
				isEdge = true;
				break;
			}
		}
	}
	return isEdge;
}
/**/

/**
bool IsPointInSimplePolygon( const std::vector<CVec3> &points, const CVec2 &vPoint )
{
	bool bInside = false;
	
	for ( int index = 0; index < points.size(); ++index )
	{
		const CVec3 &vBeg = points[index];
		const CVec3 &vEnd = points[( index + 1 ) % points.size()];
		if ( (vEnd.y <= vPoint.y) && (vPoint.y < vBeg.y) && ( (vBeg.y - vEnd.y) * (vPoint.x - vEnd.x) < (vPoint.y - vEnd.y) * (vBeg.x - vEnd.x) ) )
			bInside = !bInside;
		else if ( (vBeg.y <= vPoint.y) && (vPoint.y < vEnd.y) && ( (vBeg.y - vEnd.y) * (vPoint.x - vEnd.x) > (vPoint.y - vEnd.y) * (vBeg.x - vEnd.x) ) )
			bInside = !bInside;
	}
	
	return bInside;
}
/**/

int  GetTrenchIndex( IVisObj* obj, CTemplateEditorFrame* frame )
{
	IRefCount *pTmp = frame->FindByVis( obj )->pAIObj;
	for( int i = 0; i !=  frame->m_entrenchmentsAI.size() ; ++i )
	{
			for( int i2 = 0; i2 !=  frame->m_entrenchmentsAI[i].size(); ++i2 )
			{
				for( int i3 = 0; i3 !=  frame->m_entrenchmentsAI[i][i2].size(); ++i3 )
				{
					if( pTmp == frame->m_entrenchmentsAI[i][i2][i3] )
						return i;
				}
			}
	}
		
	return -1;
}

void 	CRoadDrawState::OnMouseMove(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* frame)
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, frame ) )
	{
		return;
	}
	if ( !stateParameter.LastPointInTerrain() )
	{
		return;
	}

	CPoint point( rMousePoint.x, rMousePoint.y );
	
	m_lastScreenMouse.x = point.x;
	m_lastScreenMouse.y = point.y;
	RECT r;
	g_frameManager.GetGameWnd()->GetClientRect( &r );
	point.x -= r.left;
	point.y -= r.top;
	CVec3 v;
	CVec2 p ( point.x , point.y );
	GetSingleton<IScene>()->GetPos3( &v, p );
	frame->m_lastPoint.x = v.x;
	frame->m_lastPoint.y = v.y;

	if( tmpSpan ) // удалим tmp мост 
	{
		GetSingleton<IAIEditor>()->DeleteObject( tmpSpan->pAIObj );	
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		int time = pTimer->GetGameTime( );
		frame->Update( time );
		tmpSpan = 0;
	}
	if ( nFlags & MK_LBUTTON )
	{
		if ( ( GetVToolsState( frame ) == roadStateConsts::nFence ) ||
				 ( GetVToolsState( frame ) == roadStateConsts::nBridge ) )
		{
			if ( frame->GetCurrentDirection() == 0 )
				frame->m_lastPoint.x = frame->m_firstPoint.x;
			else
				frame->m_lastPoint.y = frame->m_firstPoint.y;
		}	
		ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
		if ( terra )
		{
			
			APointHelper hlp;
			int x1,x2,y1,y2;		
			(dynamic_cast< ITerrainEditor* >(terra))->GetTileIndex( CVec3( frame->m_firstPoint.x , frame->m_firstPoint.y , 0.0f ), &x1, &y1, true );
			(dynamic_cast< ITerrainEditor* >(terra))->GetTileIndex( CVec3( frame->m_lastPoint.x , frame->m_lastPoint.y , 0.0f ), &x2, &y2, true );
			const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
			GRect terrainRect( 0, 0 , terrainInfo.patches.GetSizeX() * 16 - 1, terrainInfo.patches.GetSizeY() * 16 - 1);
			if ( ! ( terrainRect.contains( x1, y1 ) && terrainRect.contains( x2, y2 )
				&& terrainRect.contains( x1 , y1 ) && terrainRect.contains( x2 , y2 ) ) 
				)
			{ 
				return;
			}
			a_dirLine( x1, y1, x2, y2, hlp );
			if ( hlp.m_points.size() )
			{
				std::vector< CTPoint<int> > pointsForUpdate;

				if ( GetVToolsState( frame ) == roadStateConsts::nFence ) 
				{
					if ( frame->m_currentFences.size() )		
					{
						for( std::vector<IVisObj*>::iterator it = frame->m_currentFences.begin(); it != frame->m_currentFences.end(); ++it )
						{
							frame->RemoveObject( *it );
						}
						frame->m_currentFences.clear();
					} 
					
					APointHelper hlpFence;
					int fenceX1,fenceX2,fenceY1,fenceY2;		
					(dynamic_cast< ITerrainEditor* >(terra))->GetAITileIndex( CVec3( frame->m_firstPoint.x, frame->m_firstPoint.y, 0.0f ), &fenceX1, &fenceY1 );
					(dynamic_cast< ITerrainEditor* >(terra))->GetAITileIndex( CVec3( frame->m_lastPoint.x, frame->m_lastPoint.y, 0.0f ), &fenceX2, &fenceY2 );
					const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
					GRect terrainRect( 0, 0 , terrainInfo.patches.GetSizeX() * 16 - 1, terrainInfo.patches.GetSizeY() * 16 - 1);
					if ( ! ( terrainRect.contains( fenceX1 >> 1, ( fenceY1 >> 1 ) - 1 ) && terrainRect.contains( fenceX2 >> 1, ( fenceY2 >> 1 )- 1)
						&& terrainRect.contains( fenceX1 >> 1 , ( fenceY1 >> 1 ) + 1 ) && terrainRect.contains( fenceX2 >> 1 , ( fenceY2>> 1 ) + 1 ) ) )
					{  
					}
					a_dirLine( fenceX1, fenceY1, fenceX2, fenceY2, hlpFence );
					
					for ( std::vector<GPoint>::iterator it = hlpFence.m_points.begin(); it != hlpFence.m_points.end();   )
					{
						IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
						const SGDBObjectDesc* pDesc = pGDB->GetDesc( frame->m_mapEditorBarPtr->GetFenceWnd()->GetFenceName().c_str() );
						const SFenceRPGStats *pStats = static_cast<const SFenceRPGStats*>( pGDB->GetRPGStats( pDesc ) );
	
						IVisObj *pVisObj = frame->AddObject( *pDesc, 0, true );
						if ( pVisObj )
						{
							const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
							GRect terrainRect( 0, 0 , terrainInfo.patches.GetBoundX() * 16 - 1, terrainInfo.patches.GetBoundY() * 16 - 1);
							CVec3 pos( it->x * SAIConsts::TILE_SIZE, it->y * SAIConsts::TILE_SIZE, 0 );
							AI2Vis( &pos );
							pVisObj->SetPlacement( pos, 0 );
							
							ISpriteAnimation *pAnim = static_cast<ISpriteAnimation*>( static_cast<IObjVisObj*>( pVisObj )->GetAnimation() );
							if ( frame->GetCurrentDirection() == 1 )
							{
								int nAnimFrame = frame->m_firstPoint.x > frame->m_lastPoint.x ? pStats->GetCenterIndex( 1 ) : pStats->GetCenterIndex( 3 );
								pAnim->SetFrameIndex( nAnimFrame );
								if ( frame->m_firstPoint.x < frame->m_lastPoint.x  )
								{
									CVec3 pos2(  ( it->x + 2 )* SAIConsts::TILE_SIZE, ( it->y )* SAIConsts::TILE_SIZE, 0 ); 
									AI2Vis( &pos2 );
									pVisObj->SetPlacement( pos2, 0 );
									
								}
							}
							if ( frame->GetCurrentDirection() == 0 )
							{
								int nAnimFrame = frame->m_firstPoint.y > frame->m_lastPoint.y ? pStats->GetCenterIndex( 0 ) :  pStats->GetCenterIndex( 2 ) ;
								pAnim->SetFrameIndex( nAnimFrame );
								if ( frame->m_firstPoint.y > frame->m_lastPoint.y  )
								{
									CVec3 pos2(  ( ( it->x )  )* SAIConsts::TILE_SIZE, ( it->y - 2 )* SAIConsts::TILE_SIZE, 0 ); 
									AI2Vis( &pos2 );
									pVisObj->SetPlacement( pos2, 0 );
								}
							}
							CVec3 vTmpPos = pVisObj->GetPosition();
							FitVisOrigin2AIGrid( &vTmpPos, pStats->GetOrigin( pAnim->GetFrameIndex() ) );	
							CVSOBuilder::UpdateZ( terrainInfo.altitudes, &vTmpPos );
							pVisObj->SetPlacement( vTmpPos, 0 );
							
							GetSingleton<IScene>()->AddObject( pVisObj, pDesc->eGameType, pDesc );
							frame->m_currentFences.push_back( pVisObj );
							
							it = it != hlpFence.m_points.end() ? ++it : it;
							it = it != hlpFence.m_points.end() ? ++it : it;
						}
					}				
				}

				if ( GetVToolsState( frame ) == roadStateConsts::nBridge ) 
				{
					if( tmpSpan )
					{	
						GetSingleton<IAIEditor>()->DeleteObject( tmpSpan->pAIObj );				
						tmpSpan = 0;
					}
					if ( frame->m_tempSpans.size() )		
					{
						for( std::vector< CPtr<SBridgeSpanObject> >::iterator it = frame->m_tempSpans.begin(); it != frame->m_tempSpans.end(); ++it )
						{
							frame->RemoveFromScene( *it );
						}
						frame->m_tempSpans.clear();
					} 
					
					IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
					const SGDBObjectDesc* pDesc = pGDB->GetDesc( frame->m_mapEditorBarPtr->GetBridgeWnd()->GetBridgeName().c_str() );
					const SBridgeRPGStats *pStats = static_cast<const SBridgeRPGStats*>( pGDB->GetRPGStats( pDesc ) );
					std::vector< CVec2 > vForBridge = GetPointsForBridge( CVec2( frame->m_firstPoint.x, frame->m_firstPoint.y ), CVec2( v.x, v.y), frame );
					for( int i = 0; i != vForBridge.size(); ++i )
					{
						int nId;	
						if( i == 0 )
						{
							nId = pStats->GetRandomBeginIndex();								
						}
						if( i == vForBridge.size() - 1 )
						{
							nId = pStats->GetRandomEndIndex();								
						}
						if( i != vForBridge.size() - 1 && i != 0 )
						{
							nId = pStats->GetRandomLineIndex();								
						}
						SBridgeSpanObject* tmpBridge = frame->CreateSpanObject( pGDB->GetIndex( frame->m_mapEditorBarPtr->GetBridgeWnd()->GetBridgeName().c_str() ), nId, pStats->fMaxHP );
						tmpBridge->SetPlacement( CVec3( vForBridge[i], 0.0f ) , 0 );
						frame->AddToScene( tmpBridge );	
						frame->m_tempSpans.push_back( tmpBridge );
					}
				}
				if ( pointsForUpdate.size() )			
					dynamic_cast<ITerrainEditor*>(terra)->SetMarker( &pointsForUpdate[0], pointsForUpdate.size() );
			}
		}
	}

	if ( /*frame->m_ifMouseDown && */GetVToolsState( frame ) == roadStateConsts::nTrench )
	{
		CSceneDrawTool drawTool;
		DWORD color = 0xffFF0000;
		if ( m_pointForTrench.size() > 1 )
		{
			DWORD connectorColor = 0xffffff00;
			DWORD firstConnectorColor = 0xffff00ff;
			for( int i = 0; i != m_pointForTrench.size() - 1; ++i )
			{
				drawTool.DrawLine( CVec2( m_pointForTrench[i].x, m_pointForTrench[i].y ), CVec2( m_pointForTrench[i + 1].x, m_pointForTrench[i + 1].y ), color );
			}
			
			int lastIndex = m_pointForTrench.size() - 1;
			CAngle fLastAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex - 1].x, m_pointForTrench[lastIndex - 1].y  )
				,  CVec2(m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y  ));
			CAngle fMouseAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex ].y  )
				,  CVec2( frame->m_lastPoint.x, frame->m_lastPoint.y  ) );
			CConnector connector( GPoint( m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex ].y ), fLastAngle, fMouseAngle );	
			if ( !connector.m_points.size() ) // если поворачивать не надо то берем последнее направление от m_pointForTrench  
			{
				CVec2 vAddSegment;
				vAddSegment.x = cos( fLastAngle ) * fabs( frame->m_lastPoint.x - frame->m_firstPoint.x, frame->m_lastPoint.y - frame->m_firstPoint.y );
				vAddSegment.y = sin( fLastAngle ) * fabs( frame->m_lastPoint.x - frame->m_firstPoint.x, frame->m_lastPoint.y - frame->m_firstPoint.y );
				vAddSegment.x += m_pointForTrench[lastIndex].x;
				vAddSegment.y += m_pointForTrench[lastIndex].y;

				std::vector<GPoint> points = SplitLineToSegrments( CVec2( m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y ),
					CVec2( vAddSegment.x, vAddSegment.y ), GetTrenchWidth( 0 ) );
				if( points.size() )
				{	
					for( int i = 0; i != points.size() - 1; ++i )
					{
						drawTool.DrawLine( CVec2( points[i].x, points[i].y ), CVec2( points[i + 1].x, points[i + 1].y ), color );
					}
				}
			}
			else // если же коннеткор не пуcт то надо взять напрвление от последней точки коннектора
			{	
				int nLastConnectorIndex = connector.m_points.size() - 1;
				if( fabs( frame->m_lastPoint.x - connector.m_points[0].x, frame->m_lastPoint.y - connector.m_points[0].y )
					>= fabs( frame->m_lastPoint.x - connector.m_points[nLastConnectorIndex].x, frame->m_lastPoint.y - connector.m_points[nLastConnectorIndex].y ))
				{
					CAngle fLastConnectorAngle;
					if( connector.m_points.size() == 1 )	
					{
						fLastConnectorAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex].x ,m_pointForTrench[lastIndex].y ),
							CVec2(connector.m_points[nLastConnectorIndex].x, connector.m_points[nLastConnectorIndex].y  )
						);
					}
					else
					{
						fLastConnectorAngle = GetLineAngle( CVec2(connector.m_points[nLastConnectorIndex - 1].x, connector.m_points[nLastConnectorIndex - 1].y  )
					,  CVec2(connector.m_points[nLastConnectorIndex].x, connector.m_points[nLastConnectorIndex].y  ));
					}

					CAngle fFirstConnectorAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y  )
					,  CVec2(connector.m_points[0].x, connector.m_points[0].y  ));
					if( ( fFirstConnectorAngle - fLastAngle ) < FP_PI2 )
						fLastConnectorAngle += FP_PI / 12.0f ;
					else
						fLastConnectorAngle -= FP_PI / 12.0f ;

					CVec2 vAddSegment;
					vAddSegment.x = cos( fLastConnectorAngle ) * fabs( frame->m_lastPoint.x - connector.m_points[nLastConnectorIndex].x, frame->m_lastPoint.y - connector.m_points[nLastConnectorIndex].y );
					vAddSegment.y = sin( fLastConnectorAngle ) * fabs( frame->m_lastPoint.x - connector.m_points[nLastConnectorIndex].x, frame->m_lastPoint.y - connector.m_points[nLastConnectorIndex].y );
					vAddSegment.x += connector.m_points[nLastConnectorIndex].x;
					vAddSegment.y += connector.m_points[nLastConnectorIndex].y;\
			
					std::vector<GPoint> points = SplitLineToSegrments( CVec2( connector.m_points[nLastConnectorIndex].x, connector.m_points[nLastConnectorIndex].y ),
						CVec2( vAddSegment.x, vAddSegment.y ), GetTrenchWidth( 0 ) );
					if( points.size() )
					{
						for( int i = 0; i != points.size() - 1; ++i )
						{
							drawTool.DrawLine( CVec2( points[i].x, points[i].y ), CVec2( points[i + 1].x, points[i + 1].y ), color );
						}
						for( int i = 1; i != points.size(); ++i )
						{
							CVec2 vNorm = CVec2( points[i - 1].y - points[i].y, points[i].x - points[i - 1].x );
							vNorm = ( vNorm * fWorldCellSize ) / ( 4.0f * sqrt( vNorm.x * vNorm.x + vNorm.y * vNorm.y ) );
							drawTool.DrawLine( CVec2( points[i - 1].x, points[i - 1].y ), CVec2( points[i - 1].x + vNorm.x, points[i - 1].y +  + vNorm.y ), color );
							drawTool.DrawLine( CVec2( points[i].x, points[i].y ), CVec2( points[i].x + vNorm.x, points[i].y +  + vNorm.y ), color );
						}
					}
				}
					
				for( int i = 0; i != connector.m_points.size() - 1; ++i )
				{
					drawTool.DrawLine( CVec2( connector.m_points[i].x, connector.m_points[i].y ), CVec2( connector.m_points[i + 1].x, connector.m_points[i + 1].y ), connectorColor );
				}
				drawTool.DrawLine( CVec2( m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y ), CVec2( connector.m_points[0].x, connector.m_points[0].y ), firstConnectorColor );
			}
		}
		else
		{
			std::vector<GPoint> points = SplitLineToSegrments( CVec2( frame->m_firstPoint.x, frame->m_firstPoint.y ),
				 CVec2( frame->m_lastPoint.x, frame->m_lastPoint.y ), GetTrenchWidth( 0 ) );
			if( points.size() && m_pointForTrench.size() )
			{
				for( int i = 0; i != points.size() - 1; ++i )
				{
					drawTool.DrawLine( CVec2( points[i].x, points[i].y ), CVec2( points[i + 1].x, points[i + 1].y ), color );
				}
				for( int i = 1; i != points.size(); ++i )
				{
					CVec2 vNorm = CVec2( points[i - 1].y - points[i].y, points[i].x - points[i - 1].x );
					vNorm = ( vNorm * fWorldCellSize ) / ( 4.0f * sqrt( vNorm.x * vNorm.x + vNorm.y * vNorm.y ) );
					drawTool.DrawLine( CVec2( points[i - 1].x, points[i - 1].y ), CVec2( points[i - 1].x + vNorm.x, points[i - 1].y +  + vNorm.y ), color );
					drawTool.DrawLine( CVec2( points[i].x, points[i].y ), CVec2( points[i].x + vNorm.x, points[i].y +  + vNorm.y ), color );
				}
			}
		}
		for( int i = 1; i < m_pointForTrench.size(); ++i )
		{
			CVec2 vNorm = CVec2( m_pointForTrench[i - 1].y - m_pointForTrench[i].y, m_pointForTrench[i].x - m_pointForTrench[i - 1].x );
			vNorm = ( vNorm * fWorldCellSize ) / ( 4.0f * sqrt( vNorm.x * vNorm.x + vNorm.y * vNorm.y ) );
			drawTool.DrawLine( CVec2( m_pointForTrench[i - 1].x, m_pointForTrench[i - 1].y ), CVec2( m_pointForTrench[i - 1].x + vNorm.x, m_pointForTrench[i - 1].y +  + vNorm.y ), color );
			drawTool.DrawLine( CVec2( m_pointForTrench[i].x, m_pointForTrench[i].y ), CVec2( m_pointForTrench[i].x + vNorm.x, m_pointForTrench[i].y +  + vNorm.y ), color );
		}
		
		drawTool.DrawToScene();

		if ( m_pointForTrench.empty() )
		{
			if( m_highlightedObjects.size() )
			{
				for( std::vector< interface IVisObj*> ::iterator it = m_highlightedObjects.begin();
				it != m_highlightedObjects.end(); ++ it )
				{
					(*it)->Select( SGVOSS_UNSELECTED );
				}
				m_highlightedObjects.clear();
			}

			CVec2 p( point.x, point.y ) ;
			std::pair<IVisObj*, CVec2> *pObjects;
			int num;
			GetSingleton<IScene>()->Pick( p, &pObjects, &num, SGVOGT_UNKNOWN );
			IVisObj *tmpTrench = 0;
			for ( int i = 0; i != num; ++i )
			{
				if ( frame->IsExistByVis( pObjects[i].first ) && frame->FindByVis( pObjects[i].first )->pDesc->eGameType == SGVOGT_ENTRENCHMENT )
				{
					tmpTrench = pObjects[i].first;
					break;
				}
			}
			
			if( tmpTrench )
			{
				int nTrenchIndex = GetTrenchIndex( tmpTrench, frame );
				if( nTrenchIndex != -1 )
				{
					for( int i = 0; i != frame->m_entrenchmentsAI[nTrenchIndex ].size(); ++i )
					{
						for( int i2 = 0; i2 != frame->m_entrenchmentsAI[nTrenchIndex ][i].size(); ++i2 )
						{
							frame->FindByAI( frame->m_entrenchmentsAI [nTrenchIndex][i][i2] )->pVisObj->Select( SGVOSS_SELECTED );;
							m_highlightedObjects.push_back( frame->FindByAI( frame->m_entrenchmentsAI [nTrenchIndex][i][i2] )->pVisObj );
						}
					}
						
				}
			}

		}
	}
	if ( !( nFlags & MK_LBUTTON ) && GetVToolsState( frame ) == roadStateConsts::nFence  )
	{
		CVec2 p ( point.x , point.y );
		GetSingleton<IScene>()->GetPos3( &v, p );
		if ( frame->m_currentFences.size() )		
		{
			for( std::vector<IVisObj*>::iterator it = frame->m_currentFences.begin(); it != frame->m_currentFences.end(); ++it )
			{
				frame->RemoveObject( *it );
			}
			frame->m_currentFences.clear();
		} 
		int fenceX,fenceY;		
		ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
		(dynamic_cast< ITerrainEditor* >(terra))->GetAITileIndex( CVec3( v.x, v.y, 0.0f ), &fenceX, &fenceY );
		IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
		const SGDBObjectDesc* pDesc = pGDB->GetDesc( frame->m_mapEditorBarPtr->GetFenceWnd()->GetFenceName().c_str() );
		const SFenceRPGStats *pStats = static_cast<const SFenceRPGStats*>( pGDB->GetRPGStats( pDesc ) );
		IVisObj *pVisObj = frame->AddObject( *pDesc, 0, true );
		if ( pVisObj ) 
		{
			const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
			GRect	terrainRect ( 0, 0 , terrainInfo.tiles.GetSizeX() * fWorldCellSize - fWorldCellSize, terrainInfo.tiles.GetSizeY() * fWorldCellSize - fWorldCellSize
				);		
			CVec3 pos( fenceX * SAIConsts::TILE_SIZE, fenceY* SAIConsts::TILE_SIZE, 0 );
			if( terrainRect.contains( v.x, v.y ) )
			{
				AI2Vis( &pos );
				pVisObj->SetPlacement( pos, 0 );
				
				ISpriteAnimation *pAnim = static_cast<ISpriteAnimation*>( static_cast<IObjVisObj*>( pVisObj )->GetAnimation() );
				int nDir = 0;
				if(  ( GetAsyncKeyState( VK_CONTROL ) & 32768 ) )
				{
					nDir = 1;
				}
				
				if ( nDir == 1 )
				{
					int nAnimFrame = pStats->GetCenterIndex( 1 ) ;
					pAnim->SetFrameIndex( nAnimFrame );
				}
				else
				{
					int nAnimFrame = pStats->GetCenterIndex( 0 ) ;
					pAnim->SetFrameIndex( nAnimFrame );
				}

				CVec3 vTmpPos = pVisObj->GetPosition();
				FitVisOrigin2AIGrid( &vTmpPos, pStats->GetOrigin( pAnim->GetFrameIndex() ) );	
				CVSOBuilder::UpdateZ( terrainInfo.altitudes, &vTmpPos );
				pVisObj->SetPlacement( vTmpPos, 0 );
				
				GetSingleton<IScene>()->AddObject( pVisObj, pDesc->eGameType, pDesc );
				frame->m_currentFences.push_back( pVisObj );	
			}
		}
		
	}
	if ( !( nFlags & MK_LBUTTON ) && GetVToolsState( frame ) == roadStateConsts::nBridge  )
	{
	

		ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
		GRect terrainRect( 0, 0 , 0, 0 );
		if( terra )
		{
			const STerrainInfo	&info = dynamic_cast<ITerrainEditor *>(terra)->GetTerrainInfo();
			terrainRect = GRect( 0, 0 , info.tiles.GetSizeX() * fWorldCellSize - 1, info.tiles.GetSizeY() * fWorldCellSize - 1 );		
		}
		if( !tmpSpan && terrainRect.contains( frame->m_lastPoint.x, frame->m_lastPoint.y) )
		{

			IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
			std::string szBridgeType = frame->m_mapEditorBarPtr->GetBridgeWnd()->GetBridgeName();
			if ( !szBridgeType.empty() )
			{
				const SGDBObjectDesc* pDesc = pGDB->GetDesc( szBridgeType.c_str() );
				const SBridgeRPGStats *pStats = static_cast<const SBridgeRPGStats*>( pGDB->GetRPGStats( pDesc ) );
				int nId = pStats->GetRandomBeginIndex();								
				SMapObjectInfo info;
				info.nScriptID = -1;
				info.vPos = CVec3( frame->m_lastPoint.x,frame->m_lastPoint.y, 0.0f ) ;
				Vis2AI( &info.vPos );
				info.nDir = 0;
				info.szName = pDesc->szKey;
				info.nPlayer = 0;
				info.fHP = 1.0f;
				info.nFrameIndex = nId; 
				IRefCount *pAiObject = 0;
				GetSingleton<IAIEditor>()->AddNewObject( info, &pAiObject );
				if ( pAiObject != 0 )
				{
					IGameTimer *pTimer = GetSingleton<IGameTimer>();
					int time = pTimer->GetGameTime( );
					frame->Update( time );
					tmpSpan = frame->FindSpanByAI( pAiObject );
					tmpSpan->SetOpacity( 128 );
				}
			}
		}
	}
	frame->RedrawWindow();
	
}
void 	CRoadDrawState::OnLButtonUp(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame *frame )
{
	if ( !stateParameter.Update( CInputStateParameter::ISE_LBUTTONUP, rMousePoint, frame ) )
	{
		return;
	}
	if ( !stateParameter.LastPointInTerrain() )
	{
		return;
	}

	CPoint point( rMousePoint.x, rMousePoint.y );
	
	ITerrain *terra = GetSingleton<IScene>()->GetTerrain();

	if ( frame->m_tempSpans.size() )		
	{
		for( std::vector< CPtr<SBridgeSpanObject> >::iterator it = frame->m_tempSpans.begin(); it != frame->m_tempSpans.end(); ++it )
		{
			frame->RemoveFromScene( *it );
		}
		frame->m_tempSpans.clear();
	} 
	if ( terra && GetVToolsState( frame ) == roadStateConsts::nBridge )
	{
		IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
		const SGDBObjectDesc* pDesc = pGDB->GetDesc( frame->m_mapEditorBarPtr->GetBridgeWnd()->GetBridgeName().c_str() );
		const SBridgeRPGStats *pStats = static_cast<const SBridgeRPGStats*>( pGDB->GetRPGStats( pDesc ) );
		std::vector< CVec2 > vForBridge = GetPointsForBridge( CVec2( frame->m_firstPoint.x, frame->m_firstPoint.y ), CVec2( frame->m_lastPoint.x, frame->m_lastPoint.y ), frame );
	
		bool bErrorExists = false;
		std::vector< CPtr<SBridgeSpanObject> > tmpBridgeForAdd;
		for( int i = 0; i != vForBridge.size(); ++i )
		{
			int nId;	
			if( i == 0 )
			{
				nId = pStats->GetRandomBeginIndex();								
			}
			if( i == vForBridge.size() - 1 )
			{
				nId = pStats->GetRandomEndIndex();								
			}
			if( i != vForBridge.size() - 1 && i != 0 )
			{
				nId = pStats->GetRandomLineIndex();								
			}
			SMapObjectInfo info;

			info.nScriptID = -1;
			info.vPos = CVec3( vForBridge[i], 0.0f ) ;
			Vis2AI( &info.vPos );
			if ( ( i == 0 ) && ( pStats->direction == SBridgeRPGStats::EDirection::HORIZONTAL ) )
			{
				info.vPos.x -= 0.1f;
			}
			else if ( ( i == ( vForBridge.size() -1 ) ) && ( pStats->direction == SBridgeRPGStats::EDirection::VERTICAL ) )
			{
				info.vPos.y += 0.1f;
			}

			info.nDir = 0;
			info.szName = pDesc->szKey;
			info.nPlayer = 0;
			info.fHP = 1.0f;
			info.nFrameIndex = nId; 
			IRefCount *pAiObject = 0;
			GetSingleton<IAIEditor>()->AddNewObject( info, &pAiObject );
			if ( pAiObject != 0 )
			{
				IGameTimer *pTimer = GetSingleton<IGameTimer>();
				int time = pTimer->GetGameTime( );
				frame->Update( time );
				tmpBridgeForAdd.push_back( frame->FindSpanByAI( pAiObject ) );
			}
			else
			{
				bErrorExists = true;
				break;
			}
		}
		if ( !bErrorExists )
		{
			frame->m_Spans.push_back( tmpBridgeForAdd );
			frame->SetMapModified();
		}
	}
	
	if ( terra && GetVToolsState( frame ) == roadStateConsts::nFence )
	{
		
		if ( frame->m_currentFences.size() )		
		{
			for( std::vector<IVisObj*>::iterator it = frame->m_currentFences.begin(); it != frame->m_currentFences.end(); ++it )
			{
				frame->RemoveObject( *it );
			}
			frame->m_currentFences.clear();
		} 
		
		APointHelper hlpFence;
		int fenceX1,fenceX2,fenceY1,fenceY2;		
		(dynamic_cast< ITerrainEditor* >(terra))->GetAITileIndex( CVec3( frame->m_firstPoint.x, frame->m_firstPoint.y, 0.0f ), &fenceX1, &fenceY1 );
		(dynamic_cast< ITerrainEditor* >(terra))->GetAITileIndex( CVec3( frame->m_lastPoint.x, frame->m_lastPoint.y, 0.0f ), &fenceX2, &fenceY2 );
		const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
		GRect terrainRect( 0, 0 , terrainInfo.patches.GetSizeX() * 16 - 1, terrainInfo.patches.GetSizeY() * 16 - 1);

		if ( !terrainRect.contains( fenceX1 >> 1, ( fenceY1 >> 1 ) ) && terrainRect.contains( fenceX2 >> 1, ( fenceY2 >> 1 ) ) )
		{ 
			return;
		}

		a_dirLine( fenceX1, fenceY1, fenceX2, fenceY2, hlpFence );
		
		for ( std::vector<GPoint>::iterator it = hlpFence.m_points.begin(); it != hlpFence.m_points.end();   )
		{
			IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
			const SGDBObjectDesc* pDesc = pGDB->GetDesc( frame->m_mapEditorBarPtr->GetFenceWnd()->GetFenceName().c_str() );
			const SFenceRPGStats *pStats = static_cast<const SFenceRPGStats*>( pGDB->GetRPGStats( pDesc ) );
			{
				const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
				GRect terrainRect( 0, 0 , terrainInfo.patches.GetSizeX() * 16 - 1, terrainInfo.patches.GetSizeY() * 16 - 1);
				CVec3 pos(  ( it->x ) * SAIConsts::TILE_SIZE, ( it->y )* SAIConsts::TILE_SIZE, 0 ); 		
				AI2Vis( &pos );
				CVec3 fensePosition	= pos;
				SMapObjectInfo info;
				info.szName = pDesc->szKey;
				info.nDir = 0 ;
				info.nPlayer = 0;
				info.fHP = 1.0f;			
				if ( frame->GetCurrentDirection() == 1 )
				{
					info.nFrameIndex = frame->m_firstPoint.x > frame->m_lastPoint.x ? pStats->GetCenterIndex( 1 ) : pStats->GetCenterIndex( 3 );
					if ( frame->m_firstPoint.x < frame->m_lastPoint.x  )
					{
						fensePosition = CVec3( ( it->x + 2 )* SAIConsts::TILE_SIZE, ( it->y )* SAIConsts::TILE_SIZE, 0.0f ); 
						AI2Vis( &fensePosition );
					}
				}
				if ( frame->GetCurrentDirection() == 0 )
				{
					info.nFrameIndex = frame->m_firstPoint.y > frame->m_lastPoint.y ? pStats->GetCenterIndex( 0 ) :  pStats->GetCenterIndex( 2 ) ;
					if ( frame->m_firstPoint.y > frame->m_lastPoint.y  )
					{
						fensePosition = CVec3 (  it->x * SAIConsts::TILE_SIZE, ( it->y - 2 )* SAIConsts::TILE_SIZE, 0 ); 
						AI2Vis( &fensePosition );
					}
				}
				
				info.vPos = fensePosition;
				
				FitVisOrigin2AIGrid( &info.vPos, pStats->GetOrigin( info.nFrameIndex ) );	
				
				Vis2AI( &info.vPos );
				frame->AddObjectByAI( info );	
				
				it = it != hlpFence.m_points.end() ? ++it : it;
				it = it != hlpFence.m_points.end() ? ++it : it;
			}
		}
		frame->SetMapModified();
	}
	frame->RedrawWindow();
}
void 	CRoadDrawState::OnLButtonDown(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* frame)
{
		if ( !stateParameter.Update( CInputStateParameter::ISE_LBUTTONDOWN, rMousePoint, frame ) )
		{
			return;
		}
		if ( !stateParameter.LastPointInTerrain() )
		{
			return;
		}

		CPoint point( rMousePoint.x, rMousePoint.y );
		RECT r;
		g_frameManager.GetGameWnd()->GetClientRect( &r );
		point.x -= r.left;
		point.y -= r.top;
		CVec3 v;
		CVec2 p ( point.x , point.y );
		GetSingleton<IScene>()->GetPos3( &v, p );
		frame->m_firstPoint.x = frame->m_lastPoint.x = v.x;
		frame->m_firstPoint.y = frame->m_lastPoint.y = v.y;

		if ( ( GetVToolsState( frame ) == roadStateConsts::nTrench ) && ( m_pointForTrench.size() > 1 ) )
		{
			int lastIndex = m_pointForTrench.size() - 1;
			CAngle fLastAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex - 1].x, m_pointForTrench[lastIndex - 1].y  )
				,  CVec2(m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y  ));
			CAngle fMouseAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex ].y  )
				,  CVec2( frame->m_lastPoint.x, frame->m_lastPoint.y  ) );
			CConnector connector( GPoint( m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex ].y ), fLastAngle, fMouseAngle );	
			if ( !connector.m_points.size() ) // если поворачивать не надо то берем последнее направление от m_pointForTrench  
			{
				
				CVec2 vAddSegment;
				vAddSegment.x = cos( fLastAngle ) * fabs( m_pointForTrench[lastIndex].x - frame->m_firstPoint.x, m_pointForTrench[lastIndex].y - frame->m_firstPoint.y );
				vAddSegment.y = sin( fLastAngle ) * fabs( m_pointForTrench[lastIndex].x - frame->m_firstPoint.x, m_pointForTrench[lastIndex].y - frame->m_firstPoint.y );
				vAddSegment.x += m_pointForTrench[lastIndex].x;
				vAddSegment.y += m_pointForTrench[lastIndex].y;
	
				std::vector<GPoint> points = SplitLineToSegrments( CVec2( m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y ),
					CVec2( vAddSegment.x, vAddSegment.y ), GetTrenchWidth( 0 ) );
				if( points.size() )
				{
					for( int i = 1; i != points.size(); ++i )
					{
							m_pointForTrench.push_back( GPoint( points[i].x, points[i].y ) );				
					}
				}
			}
			else // если же коннеткор не пуcт то надо взять напрвление от последней точки коннектора
			{				
				for( int i = 0; i != connector.m_points.size() ; ++i )
				{		
					m_pointForTrench.push_back( GPoint( connector.m_points[i].x, connector.m_points[i].y ) );
				}
				int nLastConnectorIndex = connector.m_points.size() - 1;
				if( fabs( frame->m_lastPoint.x - connector.m_points[0].x, frame->m_lastPoint.y - connector.m_points[0].y )
					>= fabs( frame->m_lastPoint.x - connector.m_points[nLastConnectorIndex].x, frame->m_lastPoint.y - connector.m_points[nLastConnectorIndex].y ))
				{
					CAngle fLastConnectorAngle;
					if( connector.m_points.size() == 1 )	
					{
						fLastConnectorAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex].x ,m_pointForTrench[lastIndex].y ),
							CVec2(connector.m_points[nLastConnectorIndex].x, connector.m_points[nLastConnectorIndex].y  )
						);
					}
					else
					{
						fLastConnectorAngle = GetLineAngle( CVec2(connector.m_points[nLastConnectorIndex - 1].x, connector.m_points[nLastConnectorIndex - 1].y  )
					,  CVec2(connector.m_points[nLastConnectorIndex].x, connector.m_points[nLastConnectorIndex].y  ));
					}
					CAngle fFirstConnectorAngle = GetLineAngle( CVec2(m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y  )
					,  CVec2(connector.m_points[0].x, connector.m_points[0].y  ));
					if( ( fFirstConnectorAngle - fLastAngle ) < FP_PI2 )
						fLastConnectorAngle += FP_PI / 12.0f ;
					else
						fLastConnectorAngle -= FP_PI / 12.0f ;


					CVec2 vAddSegment;
					vAddSegment.x = cos( fLastConnectorAngle ) * fabs( frame->m_lastPoint.x - connector.m_points[nLastConnectorIndex].x, frame->m_lastPoint.y - connector.m_points[nLastConnectorIndex].y );
					vAddSegment.y = sin( fLastConnectorAngle ) * fabs( frame->m_lastPoint.x - connector.m_points[nLastConnectorIndex].x, frame->m_lastPoint.y - connector.m_points[nLastConnectorIndex].y );
					vAddSegment.x += connector.m_points[nLastConnectorIndex].x;
					vAddSegment.y += connector.m_points[nLastConnectorIndex].y;


					std::vector<GPoint> points = SplitLineToSegrments( CVec2( connector.m_points[nLastConnectorIndex].x, connector.m_points[nLastConnectorIndex].y ),
						CVec2( vAddSegment.x, vAddSegment.y ), GetTrenchWidth( 0 ) );
					if( points.size() )
					{
						for( int i = 1; i != points.size(); ++i )
						{
							m_pointForTrench.push_back( GPoint( points[i].x, points[i].y ) );				
						}
					}
				}
			}
		}
		if ( GetVToolsState( frame ) == roadStateConsts::nTrench && m_pointForTrench.empty() )
		{
			m_pointForTrench.push_back( GPoint( frame->m_firstPoint.x, frame->m_firstPoint.y) );
			frame->RedrawWindow();
			return;
		}
		if ( GetVToolsState( frame ) == roadStateConsts::nTrench && m_pointForTrench.size() == 1 )
		{
			{
				std::vector<GPoint> points = SplitLineToSegrments( CVec2(m_pointForTrench[0].x, m_pointForTrench[0].y ),
					CVec2( frame->m_firstPoint.x, frame->m_firstPoint.y ), GetTrenchWidth( 0 ) );
				if( points.size() )
				{
					for( int i = 1; i != points.size(); ++i )
					{
						m_pointForTrench.push_back( GPoint( points[i].x, points[i].y ) );				
					}
				}
			}
		}
		
}
void 	CRoadDrawState::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame * frame)
{
		if ( !stateParameter.Update( CInputStateParameter::ISE_KEYDOWN, CTPoint<int>( 0, 0 ), frame ) )
		{
			return;
		}
		if ( !stateParameter.LastPointInTerrain() )
		{
			return;
		}

	if ( ( nChar == VK_RETURN ) || ( nChar == VK_SPACE ) )
	{
		if ( ITerrain *pTerrain = GetSingleton<IScene>()->GetTerrain() )
		{
			if ( GetVToolsState( frame ) == roadStateConsts::nBridge )
			{
				SBridgeSpanObject *ptr = 0;
				ptr = frame->PickSpan( m_lastScreenMouse );
				for( std::vector< std::vector< CPtr<SBridgeSpanObject> > >::iterator i = frame->m_Spans.begin(); i != frame->m_Spans.end(); ++i )
				{
					for( std::vector< CPtr<SBridgeSpanObject> >::iterator j = i->begin(); j != i->end(); ++j )
					{
						if ( ( j->GetPtr() == ptr ) && ( (*j)->GetObjectName().find( "WoodenBig_Heavy_" ) != std::string::npos ) )
						{
							for( std::vector< CPtr<SBridgeSpanObject> >::iterator it = i->begin(); it != i->end(); ++it )
							{
								float fHP = (*it)->GetHP();
								if ( fHP > 0 )
								{
									(*it)->SetHP( -( (*it)->GetMaxHP() ) );
									(*it)->SetSpecular( 0xFF0000FF );
								}
								else
								{
									(*it)->SetHP( (*it)->GetMaxHP() );
									(*it)->SetSpecular( 0x0000000 );
								}
							}
							frame->RedrawWindow();
							frame->SetMapModified();
							return;	
						}
					}
				}
			}
			else if ( GetVToolsState( frame ) == roadStateConsts::nTrench )
			{
				if ( !frame->dlg && pTerrain && !frame->isStartCommandPropertyActive ) 
				{
					if ( m_pointForTrench.empty() )
					{
						std::pair<IVisObj*, CVec2> *pObjectsTmp;
						int num;
						GetSingleton<IScene>()->Pick( m_lastScreenMouse, &pObjectsTmp, &num, SGVOGT_UNKNOWN );
						if ( num > 0 )
						{
							for ( int i = 0; i != num; ++i )
							{
								if ( frame->IsExistByVis( (pObjectsTmp)[i].first ) && frame->FindByVis( (pObjectsTmp)[i].first )->pDesc->eGameType == SGVOGT_ENTRENCHMENT )
								{
									if ( frame->dlg == 0 )
									{
										frame->dlg = new CPropertieDialog;
										frame->dlg->Create( CPropertieDialog::IDD, frame );
										frame->SetMapModified();
									}
									static_cast<CObjectPlacerState*>( frame->inputStates.GetInputState( CTemplateEditorFrame::STATE_SIMPLE_OBJECTS ) )->UpdatePropertie( frame->GetEditorObjectItem(frame->FindByVis( (pObjectsTmp)[i].first )), frame  );
								}
							}
						}
					}
				}
			}
		}
	}

	if ( nChar == VK_DELETE  ) 
	{
		std::vector< CTPoint<int> > pointsForUpdate;
		ITerrain *terra = GetSingleton<IScene>()->GetTerrain();

		if ( terra && GetVToolsState( frame ) == roadStateConsts::nTrench )
		{
			if ( m_pointForTrench.empty() )
			{
				if( m_highlightedObjects.size() )
				{
					for( std::vector< interface IVisObj*> ::iterator it = m_highlightedObjects.begin();
					it != m_highlightedObjects.end(); ++ it )
					{
						(*it)->Select( SGVOSS_UNSELECTED );
					}
					m_highlightedObjects.clear();
				}

				std::pair<IVisObj*, CVec2> *pObjects;
				int num;
				GetSingleton<IScene>()->Pick(	m_lastScreenMouse, &pObjects, &num, SGVOGT_UNKNOWN );
				IVisObj *tmpTrench = 0;
				for ( int i = 0; i != num; ++i )
				{
					if ( frame->IsExistByVis( pObjects[i].first ) && frame->FindByVis( pObjects[i].first )->pDesc->eGameType == SGVOGT_ENTRENCHMENT )
					{
						tmpTrench = pObjects[i].first;
						break;
					}
				}
				
				if( tmpTrench )
				{
					int nTrenchIndex = GetTrenchIndex( tmpTrench, frame );
					if( nTrenchIndex != -1 )
					{
						for( int i = 0; i != frame->m_entrenchmentsAI[nTrenchIndex ].size(); ++i )
						{
							for( int i2 = 0; i2 != frame->m_entrenchmentsAI[nTrenchIndex ][i].size(); ++i2 )
							{
								frame->RemoveObject( frame->FindByAI( frame->m_entrenchmentsAI [nTrenchIndex][i][i2] ) );
							}
						}
							
					}
					std::vector< std::vector< std::vector< IRefCount* > > >::iterator it = frame->m_entrenchmentsAI.begin();
					std::advance( it, nTrenchIndex );
					frame->m_entrenchmentsAI.erase( it );
					frame->SetMapModified();
				}
			}

		}
		if ( terra && GetVToolsState( frame ) == roadStateConsts::nBridge )
		{

			SBridgeSpanObject *ptr = 0;
			ptr = frame->PickSpan( m_lastScreenMouse );

			for( std::vector< std::vector< CPtr<SBridgeSpanObject> > >::iterator i = frame->m_Spans.begin(); i != frame->m_Spans.end(); ++i )
			{
				for( std::vector< CPtr<SBridgeSpanObject> >::iterator j = i->begin(); j != i->end(); ++j )
				{
						if( j->GetPtr()  == ptr )		
						{
							for( std::vector< CPtr<SBridgeSpanObject> >::iterator it = i->begin(); it != i->end(); ++it )
							{
								GetSingleton<IAIEditor>()->DeleteObject( (*it)->pAIObj );	
							}
							frame->m_Spans.erase( i );
							frame->RedrawWindow();
							frame->SetMapModified();
							return;	
						}
				}
			}
		}
		frame->RedrawWindow();
	}
}
void CRoadDrawState::OnRButtonUp(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* frame)
{
	if ( stateParameter.Update( CInputStateParameter::ISE_RBUTTONUP, rMousePoint, frame ) )
	{
		if ( GetVToolsState( frame ) == roadStateConsts::nTrench )
		{
			m_pointForTrench.clear();
			frame->RedrawWindow();
		}
	}
}

struct SCVec3NearEqFunctional
{
	bool operator()( const CVec3 &v1, const CVec3 &v2 ) const
	{
		return fabs2( v1 - v2 ) <= 4;
	}
};

void CRoadDrawState::OnLButtonDblClk(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* frame)
{
		if ( !stateParameter.Update( CInputStateParameter::ISE_LBUTTONDBLCLK, rMousePoint, frame ) )
		{
			return;
		}
		if ( !stateParameter.LastPointInTerrain() )
		{
			return;
		}

		CPoint point( rMousePoint.x, rMousePoint.y );
		IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
		RECT r;
		g_frameManager.GetGameWnd()->GetClientRect( &r );
		point.x -= r.left;
		point.y -= r.top;
		CVec3 v;
		CVec2 p ( point.x , point.y );
		GetSingleton<IScene>()->GetPos3( &v, p );
		frame->m_firstPoint.x = frame->m_lastPoint.x = v.x;
		frame->m_firstPoint.y = frame->m_lastPoint.y = v.y;

		if ( GetVToolsState( frame ) == roadStateConsts::nTrench &&  m_pointForTrench.size() > 1 )
		{
			SMapObject *ptrBegin, *ptrEnd;
			{ // пределаем начало и конец окопа		
				int lastIndex = m_pointForTrench.size() - 1;
	
				const SGDBObjectDesc *pDesc = pGDB->GetDesc( "Entrenchment" );
				const SEntrenchmentRPGStats *pRPG = static_cast<const SEntrenchmentRPGStats*>( pGDB->GetRPGStats( pDesc ) );
				int nFrameIndex = pRPG->GetTerminatorIndex();
				
				{
				CAngle angleBegin = GetLineAngle( CVec2( m_pointForTrench[0].x, m_pointForTrench[0].y ),
				CVec2( m_pointForTrench[1].x, m_pointForTrench[1].y ) );
						const SEntrenchmentRPGStats::SSegmentRPGStats &segment = pRPG->GetSegmentStats( nFrameIndex );
			

					float addLength = 0;//GetTrenchWidth( 0 ) / 2.0f;
					angleBegin += FP_PI;

					SMapObjectInfo info;
					info.szName = pDesc->szKey;
					info.fHP = 1.0f;
					info.nDir = ( ( angleBegin ) / ( FP_2PI ) ) * 65535;
					info.nPlayer = frame->m_mapEditorBarPtr->GetObjectWnd()->GetPlayer();
					info.nScriptID = -1;
					info.nFrameIndex = nFrameIndex;
					GPoint pt = ( m_pointForTrench[0]  );
					info.vPos = CVec3( pt.x, pt.y, 0.0f );
			
				
					Vis2AI( &info.vPos );
					ptrBegin =  frame->AddObjectByAI( info );
				}
				{
					CAngle angleEnd = GetLineAngle( CVec2( m_pointForTrench[lastIndex - 1].x, m_pointForTrench[lastIndex - 1].y ),
					CVec2( m_pointForTrench[lastIndex].x, m_pointForTrench[lastIndex].y ) );

					int nType;
					if ( fabs( m_pointForTrench[lastIndex].x - m_pointForTrench[lastIndex - 1].x, m_pointForTrench[lastIndex].y - m_pointForTrench[lastIndex - 1].y ) 
						> GetTrenchWidth( 0 ) * 0.9 )
					{
						nType = 0;		
					}
					else
					{
						nType = 1;
					}
					float addLength = GetTrenchWidth( nType ) / 2.0f;

					SMapObjectInfo info;
					info.szName = pDesc->szKey;
					info.fHP = 1.0f;
					info.nDir = ( ( angleEnd ) / ( FP_2PI ) ) * 65535;
					info.nPlayer = frame->m_mapEditorBarPtr->GetObjectWnd()->GetPlayer();
					info.nScriptID = -1;
					info.nFrameIndex = nFrameIndex;
					GPoint pt = ( m_pointForTrench[lastIndex]  );
					info.vPos = CVec3( pt.x, pt.y, 0.0f );
				
				  Vis2AI( &info.vPos ); 
					ptrEnd =frame->AddObjectByAI( info );

				}
	
			}
			bool switcher = false;
			std::vector< std::vector<IRefCount*> > m_trench;

			bool endIfSection = false;
			std::vector<IRefCount*> tmpSection;
			tmpSection.push_back( ptrBegin->pAIObj );
			for( int i = 0; i != m_pointForTrench.size() - 1; ++i )	
			{
				int nType = -1; // 0 - просто секция  1 - поворот по часовой 2 - против часовой 
				CAngle angle = GetLineAngle( CVec2( m_pointForTrench[i].x, m_pointForTrench[i].y ),
					CVec2( m_pointForTrench[i + 1].x, m_pointForTrench[i + 1].y ) );
				CAngle angleForLastSection = angle;
				CAngle angleForNextSection = angle;
				if( i > 1 )
				{
					angleForLastSection = GetLineAngle( CVec2( m_pointForTrench[i - 1].x, m_pointForTrench[i -1].y ),
						CVec2( m_pointForTrench[i].x, m_pointForTrench[i].y ) );
				}
				if( i < m_pointForTrench.size() -  2 ) // чтобы понять какая секция 
				{
					angleForNextSection = GetLineAngle( CVec2( m_pointForTrench[i + 1].x, m_pointForTrench[i + 1].y ),
						CVec2( m_pointForTrench[i + 2].x, m_pointForTrench[i + 2].y ) );
				}

				if ( fabs( m_pointForTrench[i + 1].x - m_pointForTrench[i].x, m_pointForTrench[i + 1].y - m_pointForTrench[i].y ) 
					> GetTrenchWidth( 0 ) * 0.9 )
				{
					nType = 0;		
				}
				else
				{
					nType = ( angleForLastSection - angle ) > FP_PI ? 1: 2 ;
				}

				const SGDBObjectDesc *pDesc = pGDB->GetDesc( "Entrenchment" );
				const SEntrenchmentRPGStats *pRPG = static_cast<const SEntrenchmentRPGStats*>( pGDB->GetRPGStats( pDesc ) );
				int nFrameIndex;
				if( nType == 0 )
				{
					nFrameIndex= switcher ? pRPG->GetLineIndex() : pRPG->GetFirePlaceIndex() ;
					switcher = !switcher ;
				}
				if( nType == 1 ||  nType == 2 )
					nFrameIndex= pRPG->GetArcIndex();

				if( nType == 2 )
					angle += FP_PI;
 				int nDir = ( angle / FP_2PI ) * 65535;
	
				const SEntrenchmentRPGStats::SSegmentRPGStats &segment = pRPG->GetSegmentStats( nFrameIndex );
		
				SMapObjectInfo info;
				info.szName = pDesc->szKey;
				info.fHP = 1.0f;
				info.nDir = nDir;
				info.nPlayer = frame->m_mapEditorBarPtr->GetObjectWnd()->GetPlayer();
				info.nScriptID = -1;
				info.nFrameIndex = nFrameIndex;
				if( nType == 0 || nType == 1 || nType == 2  )
				{
					GPoint pt = ( m_pointForTrench[i] + m_pointForTrench[i + 1] ) / 2.0f;
					info.vPos = CVec3( pt.x, pt.y, 0.0f );
				}
				Vis2AI( &info.vPos );
				SMapObject *ptr = frame->AddObjectByAI( info );
				if( !endIfSection && nType != 0 )
				{
					endIfSection = true;
				}
				if( endIfSection && nType == 0 )
				{
					m_trench.push_back( tmpSection );
					tmpSection.clear();
					endIfSection = false;
				}			
				tmpSection.push_back( ptr->pAIObj );
			}
			if( tmpSection.size() )
			{
				m_trench.push_back( tmpSection );
			}
			m_trench.back().push_back( ptrEnd->pAIObj ) ; 
			frame->m_entrenchmentsAI.push_back( m_trench );

			m_pointForTrench.clear();
			frame->RedrawWindow();
			frame->SetMapModified();
		}
}

