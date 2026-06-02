#if !defined(__Bresenham__Types__)
#define __Bresenham__Types__

template <class TFunctional>
void BresenhamEllipse( int nCenterX, int nCenterY, int nRadius, TFunctional &func )
{
	int x = 0, y = nRadius;
	int d = 3 - ( 2 * y );
	do 
	{
		if ( d < 0 )
		{
			d += ( 4 * x ) + 6;
		}
		else
		{
			d += ( 4 * ( x - y ) ) + 10;
			--y;
		}
		++x;
		func( nCenterX - x, nCenterY + ( y / 2 ) );
		func( nCenterX + x, nCenterY + ( y / 2 ) );
		func( nCenterX - x, nCenterY - ( y / 2 ) );
		func( nCenterX + x, nCenterY - ( y / 2 ) );
		func( nCenterX - y, nCenterY + ( x / 2 ) );
		func( nCenterX + y, nCenterY + ( x / 2 ) );
		func( nCenterX - y, nCenterY - ( x / 2 ) );
		func( nCenterX + y, nCenterY - ( x / 2 ) );
	}	while ( x <= y );
	func( nCenterX - nRadius, nCenterY );
	func( nCenterX + nRadius, nCenterY );
	func( nCenterX, nCenterY - ( nRadius / 2 ) );
	func( nCenterX, nCenterY + ( nRadius / 2 ) );
}

template <class TFunctional>
void BresenhamFilledCircle( int nCenterX, int nCenterY, int nRadius, TFunctional &func )
{
	int x = 0, y = nRadius;
	int d = 3 - ( 2 * y );
	do 
	{
		if ( d < 0 )
			d += ( 4 * x ) + 6;
		else
		{
			d += ( 4 * ( x - y ) ) + 10;
			--y;
		}
		++x;
		for ( int index = ( nCenterX - x ); index <= ( nCenterX + x ); ++index )
		{
			func( index, nCenterY + y );
		}
		for ( int index = ( nCenterX - x ); index <= ( nCenterX + x ); ++index )
		{
			func( index, nCenterY - y );
		}
		for ( int index = ( nCenterX - y ); index <= ( nCenterX + y ); ++index )
		{
			func( index, nCenterY + x );
		}
		for ( int index = ( nCenterX - y ); index <= ( nCenterX + y ); ++index )
		{
			func( index, nCenterY - x );
		}
	}	while ( x <= y );

	for ( int index = ( nCenterX - nRadius ); index <= ( nCenterX + nRadius ); ++index )
	{
		func( index, nCenterY );
	}
	func( nCenterX, nCenterY - nRadius );
	func( nCenterX, nCenterY + nRadius );
}
#endif // #if !defined(__Bresenham__Types__)
