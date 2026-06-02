#if !defined(__AUTO_RUN_DATA_FORMAT_BITMAP__)
#define __AUTO_RUN_DATA_FORMAT_BITMAP__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CARBitmap
{
	CPoint size;
	CBitmap bitmap;

public:
	CARBitmap() {}
	CARBitmap( const CARBitmap &rARBitmap ) {}

	bool Load( const std::vector<BYTE> &rData, CDC *pDC );
	const CBitmap& Get() const { return bitmap; }
	const CPoint& GetSize() const { return size; }
};
#endif // !defined(__AUTO_RUN_DATA_FORMAT_BITMAP__)
