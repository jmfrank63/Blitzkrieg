#ifndef __PROGRESSHOOK_H__
#define __PROGRESSHOOK_H__
interface IProgressHook : public IRefCount
{
	virtual void STDCALL SetNumSteps( const int nRange, const float fPercentage = 1.0f ) = 0;
	virtual void STDCALL Step() = 0;
	virtual void STDCALL Recover() = 0;
	virtual void STDCALL SetCurrPos( const int nPos ) = 0;
	virtual int STDCALL GetCurrPos() const = 0;
	virtual void Stop() = 0;
};
interface IMovieProgressHook : public IProgressHook
{
	enum EProgressType
	{
		PT_MAPGEN = 1,
		PT_LOAD = 2,
		PT_NEWMISSION = 3,
		PT_MINIMAP = 4,
		PT_TOTAL_ENCYCLOPEDIA_LOAD = 5,
		PT_CONNECTING_TO_SERVER = 6,
	};
	virtual void Init( EProgressType eType ) = 0;
	virtual void Init( const std::string &szMovieName ) = 0;
};
#endif // __PROGRESSHOOK_H__