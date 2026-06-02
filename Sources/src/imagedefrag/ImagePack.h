#if !defined(__ImagePack__)
#define __ImagePack__

struct SImagePack
{
  static const int LARGE_SIDE;
  static const int SMALL_SIDE;
  static const int GRANULARITY;

  struct SPackedImage
  {
    struct SPackedImageNode
    {
      CTPoint<int> original;              //original (defragmented) coord in original image 
      CTRect<float> packed;               //packed (fragmented) coord in packed image
      bool isLarge;                       //is fragment large (IF_LARGE) or small (IF_SMALL)

      SPackedImageNode() : original( 0, 0 ), packed( 0.0f, 0.0f, 0.0f, 0.0f ), isLarge( true ) {  }
      int operator&( IStructureSaver &ss )
      {
        CSaverAccessor saver = &ss;
        saver.Add( 1,  &original );
        saver.Add( 2,  &packed );
        saver.Add( 3,  &isLarge );
	      return 0;
      }
    };
  
    CTPoint<int> originalLeftTop;
    std::vector<SPackedImageNode> vPackedImageNodes;

    SPackedImage() : originalLeftTop( 0, 0 ) {  }
    int operator&( IStructureSaver &ss )
    {
      CSaverAccessor saver = &ss;
	    saver.Add( 1, &originalLeftTop );
	    saver.Add( 2, &vPackedImageNodes );
	    return 0;
    }
  };

  std::vector<SPackedImage> vPackedImages;
  int operator&( IStructureSaver &ss )
  {
    CSaverAccessor saver = &ss;
	  saver.Add( 1, &vPackedImages );
	  return 0;
  }
    
  IImage* CreateImagePack( IImage **pImages, CTPoint<int> *pImageLeftTops, int nImageCount ,DWORD dwMinAlpha = 1 );
  IImage* UnpackImage( IImage *pPackedImage, int imageIndex ) const;

private:
  void GetSquaresCount( int &nLargeSquaresCount, int &nSmallSquaresCount ) const;
  bool GetMinimalDimensions( CTPoint<int> &rDim ) const;
  bool FindEdges( IImage *pImage, CTRect<int> &rEdges , DWORD minAlpha ) const;
};
#endif // !defined(__ImagePack__)
