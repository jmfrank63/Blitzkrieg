#ifndef __STREAMINGSOUND_H__
#define __STREAMINGSOUND_H__
class CPlayList : public IPlayList
{
	OBJECT_NORMAL_METHODS( CPlayList );
	DECLARE_SERIALIZE;
	std::vector<std::string> szMelodies;	// all melodies in the playlist
	std::vector<std::string> szRandomized;//
	int nCurrentStream;										// current melody index
	int nSequenceOrder;										// sequencing type
	void CreateRandomList();
public:
	CPlayList() : nCurrentStream( -1 ), nSequenceOrder( ORDER_CYCLE ) {  }
	virtual void STDCALL Clear() { szMelodies.clear(); nCurrentStream = -1; }
	virtual void STDCALL SetSequenceOrder( int nOrder ) { nSequenceOrder = nOrder; }
	virtual void STDCALL AddMelody( const char *pszFileName ) { szMelodies.push_back( pszFileName ); }
	virtual const char* STDCALL GetNextMelody();	
};
#endif // __STREAMINGSOUND_H__
