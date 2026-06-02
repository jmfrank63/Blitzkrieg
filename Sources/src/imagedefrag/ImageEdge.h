#if !defined(__ImageEdge__)
#define __ImageEdge__
struct SImageEdge
{
  CTPoint<int> originalLeftTop;
  bool isHorizontal;
  CVarArray2D<short, char> edges;

  SImageEdge() : isHorizontal ( true ) { }
  int operator&( IStructureSaver &ss )
  {
    CSaverAccessor saver = &ss;
	  saver.Add( 1, &originalLeftTop );
	  saver.Add( 2, &isHorizontal );
	  saver.Add( 3, &edges );
    return 0;
  }

  bool In( const CTPoint<int> &rPoint );
  
  bool CreateImageEdge( IImage *pImage,
                        const CTPoint<int> &rOriginalLeftTop,
                        DWORD alpha );

#ifdef _DEBUG
  bool MarkEdge( IImage *pImage );
  bool MarkInEdge( IImage *pImage );
  bool MarkAlpha( IImage *pImage , DWORD dwMinAlpha, DWORD dwMaxAlpha );
#endif //#ifdef _DEBUG
};
#endif // !defined(__ImageEdge__)
