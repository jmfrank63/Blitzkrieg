#ifndef LINUX
#pragma once
#endif
#ifndef _MAttributeIndex
#define _MAttributeIndex

#if defined __cplusplus




#include <maya/MTypes.h>
#include <maya/MStatus.h>





/**
*/
#ifdef _WIN32
#pragma warning(disable: 4522)
#endif // _WIN32

class OPENMAYA_EXPORT MAttributeIndex {
public:
	enum MIndexType {
		kInteger,
		kFloat
	};

	MAttributeIndex();
	~MAttributeIndex();
	MAttributeIndex( const MAttributeIndex & );
	MAttributeIndex( int );
	MAttributeIndex( double );

public:
	MIndexType		type() const;
	bool			hasRange() const;
	bool			hasValidRange() const;

	bool			hasLowerBound() const;
	bool			hasUpperBound() const;
	MStatus			getLower( int & ) const;
	MStatus			getLower( double & ) const;
	MStatus			getUpper( int & ) const;
	MStatus			getUpper( double & ) const;

	bool			isBounded() const;
	MStatus			getValue( int & ) const;
	MStatus			getValue( double & ) const;

public:
	MStatus			setType( MIndexType );
	MStatus			setValue( int );
	MStatus			setValue( double );
	MStatus			setLower( int );
	MStatus			setLower( double );
	MStatus			setUpper( int );
	MStatus			setUpper( double );
	
public:
	MAttributeIndex & operator=( const MAttributeIndex & );
	bool			operator==( const MAttributeIndex & ) const;
	bool			operator!=( const MAttributeIndex & ) const;

private:
	MAttributeIndex( void* );
	friend class MAttributeSpec;


	void*	data;
	bool	own;
	static const char* className();
};


#ifdef _WIN32
#pragma warning(default: 4522)
#endif // _WIN32

#endif /* __cplusplus */
#endif /* _MAttributeIndex */
