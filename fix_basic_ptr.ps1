# Fix CPtr comparison operators in Basic.h
$file = "Misc\Basic.h"
$content = Get-Content $file -Raw

# Find the line number where the macro starts
$lines = Get-Content $file
$startLine = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '#define BASIC_PTR_DECLARE') {
        $startLine = $i
        break
    }
}

if ($startLine -eq -1) {
    Write-Host "Could not find BASIC_PTR_DECLARE macro" -ForegroundColor Red
    exit
}

# Build the new macro with explicit pointer operators
$newMacro = @"
#define BASIC_PTR_DECLARE( TPtrName, TRefFunc )																				\
template <class TUserObj>																															\
class TPtrName: public CPtrBase<TUserObj, TRefFunc>																		\
{																																											\
    typedef CPtrBase<TUserObj, TRefFunc> TBase;																					\
public:																																								\
    TPtrName() {}																																				\
    TPtrName( TUserObj *_ptr ): TBase( _ptr ) {  }																			\
    TPtrName( const TPtrName &a ): TBase( a ) {  }																			\
    TPtrName( int _ptr ) { (void)_ptr; Set( 0 ); }	\
    TPtrName& operator=( TUserObj *_ptr ) { Set( _ptr ); return *this; }								\
    TPtrName& operator=( const TPtrName &a ) { Set( a.GetPtr() ); return *this; }				\
    bool operator==( const TPtrName &a ) const { return GetPtr() == a.GetPtr(); }				\
    bool operator==( TUserObj *a ) const { return GetPtr() == a; }	\
    bool operator==( int a ) const { (void)a; return GetPtr() == 0; }	\
    bool operator!=( const TPtrName &a ) const { return GetPtr() != a.GetPtr(); }				\
    bool operator!=( TUserObj *a ) const { return GetPtr() != a; }	\
    bool operator!=( int a ) const { (void)a; return GetPtr() != 0; }	\
    bool operator< ( const TPtrName &a ) const { return GetPtr() < a.GetPtr(); }				\
    bool operator> ( const TPtrName &a ) const { return GetPtr() > a.GetPtr(); }				\
    bool operator<=( const TPtrName &a ) const { return GetPtr() <= a.GetPtr(); }			\
    bool operator>=( const TPtrName &a ) const { return GetPtr() >= a.GetPtr(); }			\
};
"@

# Read lines and replace the macro (lines startLine to startLine+20)
$newLines = @()
$newLines += $lines[0..($startLine-1)]
$newLines += $newMacro
# Skip the old macro lines (about 21 lines)
$newLines += $lines[($startLine+21)..($lines.Count-1)]

$newLines | Set-Content $file -Encoding UTF8
Write-Host "✓ Fixed CPtr operators in Basic.h" -ForegroundColor Green
