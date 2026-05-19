// DeepCPtrCopy.h: interface for the CDeepCPtrCopy class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DEEPCPTRCOPY_H__C87849A5_AFF1_4D0A_8E89_F1B20F66F719__INCLUDED_)
#define AFX_DEEPCPTRCOPY_H__C87849A5_AFF1_4D0A_8E89_F1B20F66F719__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// final class must implement clonable 
#define IMPLEMENT_CLONABLE(classname) \
	void* classname::Clone() const\
	{ \
		CCloneStart l;\
		classname * p = new classname;\
		*p = (*this);\
		return p;\
	}
// some class don't allow cloning
#define DECLARE_CLONE_PROHIBITED\
	public:\
	virtual void* STDCALL Clone() const\
	{\
		NI_ASSERT_T( false, "Not implemented" );\
		return 0;\
	}\
	private:\
	
/////////////////////////////////////////////////////////////////////////////
// for classes
#define DECLARE_CLONABLE_CLASS \
	public:\
	virtual void* STDCALL Clone() const;
// for interfaces
#define DECLARE_CLONABLE_INTERFACE \
	public:\
	virtual void* STDCALL Clone() const = 0;
/////////////////////////////////////////////////////////////////////////////
// cloning parameter
class CCloning
{
	static bool bCloning;
	CCloning();
public:
	static void SetClone( const bool _bCloning ) { bCloning = _bCloning; }
	static bool IsClone(){ return bCloning; }
};
/////////////////////////////////////////////////////////////////////////////
// create object of this type on start cloning
class CCloneStart
{
	const bool bOldClone;
public:
	CCloneStart() : bOldClone( CCloning::IsClone() ) { CCloning::SetClone( true ); }
	~CCloneStart() { CCloning::SetClone( bOldClone ); }
};
/////////////////////////////////////////////////////////////////////////////
// performs deep copy if IsClone()
template <class TPtr>
class CDCPtr : public CObj<TPtr>
{
	void Clone( const CDCPtr &p )
	{
		if ( CCloning::IsClone() )
		{
			if ( p != 0 )
				CObj<TPtr>::operator=( (TPtr*)p->Clone() );
			else
				CObj<TPtr>::operator=( 0 );
		}
		else
			CObj<TPtr>::operator=( p );
	
	}
public:
	CDCPtr( TPtr *p )
	{
		CObj<TPtr>::operator=( p );
	}
	CDCPtr() { }
	
	CDCPtr( const CDCPtr & p ) { Clone( p ); }
	CDCPtr &operator=( const CDCPtr &p ) { Clone( p ); return *this; }
};
/////////////////////////////////////////////////////////////////////////////
// don't copy et all if IsClone
template <class TPtr>
class CNCPtr : public CObj<TPtr>
{
	void Clone( const CNCPtr &p )
	{
		if ( CCloning::IsClone() )
		{
			// not copy
		}
		else
			CObj<TPtr>::operator=( p );
	}
public: 
	CNCPtr() { }
	CNCPtr( TPtr *p )
	{
		CObj<TPtr>::operator=( p );
	}
	CNCPtr( const CNCPtr &p ) { Clone( p ); }
	CNCPtr &operator=( const CNCPtr &p ) { Clone( p ); return *this; }
};
/////////////////////////////////////////////////////////////////////////////
#endif // !defined(AFX_DEEPCPTRCOPY_H__C87849A5_AFF1_4D0A_8E89_F1B20F66F719__INCLUDED_)
