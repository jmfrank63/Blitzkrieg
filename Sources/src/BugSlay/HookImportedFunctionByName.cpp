/*----------------------------------------------------------------------
       John Robbins - Microsoft Systems Journal Bugslayer Column
----------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "BugSlayer.h"
// The project internal header file.
#include "Internal.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct tag_HOOKFUNCDESCA
{
  // The name of the function to hook.
  LPCSTR szFunc   ;
  // The procedure to blast in.
  PROC   pProc    ;
} HOOKFUNCDESCA , * LPHOOKFUNCDESCA ;

typedef struct tag_HOOKFUNCDESCW
{
  // The name of the function to hook.
  LPCWSTR szFunc   ;
  // The procedure to blast in.
  PROC    pProc    ;
} HOOKFUNCDESCW , * LPHOOKFUNCDESCW ;

#ifdef UNICODE
#define HOOKFUNCDESC   HOOKFUNCDESCW
#define LPHOOKFUNCDESC LPHOOKFUNCDESCW
#else
#define HOOKFUNCDESC   HOOKFUNCDESCA
#define LPHOOKFUNCDESC LPHOOKFUNCDESCA
#endif  // UNICODE

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*//////////////////////////////////////////////////////////////////////
                         File Specific Defines
//////////////////////////////////////////////////////////////////////*/
#define MakePtr( cast , ptr , AddValue ) (cast)( (DWORD)(ptr) + (DWORD)(AddValue) )

/*//////////////////////////////////////////////////////////////////////
                        File Specific Prototypes
//////////////////////////////////////////////////////////////////////*/
static PIMAGE_IMPORT_DESCRIPTOR GetNamedImportDescriptor( HMODULE hModule, LPCSTR szImportMod );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*//////////////////////////////////////////////////////////////////////
                             Implementation
//////////////////////////////////////////////////////////////////////*/

BOOL STDCALL HookImportedFunctionsByName( HMODULE hModule,
																						LPCSTR          szImportMod,
																						UINT            uiCount,
																						LPHOOKFUNCDESCA paHookArray,
																						PROC *          paOrigFuncs,
																						LPUINT          puiHooked )
{
  // Double check the parameters.
  ASSERT( NULL != szImportMod );
  ASSERT( 0 != uiCount );
  ASSERT( FALSE == IsBadReadPtr(paHookArray, sizeof(HOOKFUNCDESC)*uiCount) );
#ifdef _DEBUG
  if ( NULL != paOrigFuncs )
  {
    ASSERT( FALSE == IsBadWritePtr(paOrigFuncs, sizeof(PROC)*uiCount) );
  }
  if ( NULL != puiHooked )
  {
    ASSERT( FALSE == IsBadWritePtr(puiHooked, sizeof(UINT)) );
  }

  // Check each function name in the hook array.
  {
    for ( UINT i = 0; i < uiCount; i++ )
    {
      ASSERT( NULL != paHookArray[i].szFunc );
      ASSERT( '\0' != *paHookArray[i].szFunc );
      // If the proc is not NULL, then it is checked.
      if ( NULL != paHookArray[i].pProc )
      {
        ASSERT( FALSE == IsBadCodePtr(paHookArray[i].pProc) );
      }
    }
  }
#endif
  // Do the parameter validation for real.
  if ( ( 0 == uiCount ) || ( 0 == szImportMod ) || ( TRUE == IsBadReadPtr(paHookArray, sizeof(HOOKFUNCDESC)*uiCount) ) )
  {
    SetLastErrorEx( ERROR_INVALID_PARAMETER , SLE_ERROR );
    return FALSE;
  }
  if ( ( 0 != paOrigFuncs ) && ( TRUE == IsBadWritePtr(paOrigFuncs, sizeof(PROC)*uiCount) ) )
  {
    SetLastErrorEx( ERROR_INVALID_PARAMETER , SLE_ERROR );
    return FALSE;
  }
  if ( ( 0 != puiHooked ) && ( TRUE == IsBadWritePtr(puiHooked, sizeof(UINT)) ) )
  {
    SetLastErrorEx( ERROR_INVALID_PARAMETER , SLE_ERROR );
    return FALSE;
  }

  // Is this a system DLL, which Windows95 will not let you patch
  //  since it is above the 2GB line?
  if ( ( FALSE == IsNT() ) && ( (DWORD)hModule >= 0x80000000 ) )
  {
    SetLastErrorEx( ERROR_INVALID_HANDLE , SLE_ERROR );
    return FALSE;
  }


  // TODO TODO
  //  Should each item in the hook array be checked in release builds?

  // Set all the values in paOrigFuncs to NULL.
  if ( NULL != paOrigFuncs )
    memset( paOrigFuncs, 0, sizeof(PROC)*uiCount );
  // Set the number of functions hooked to zero.
  if ( NULL != puiHooked )
    *puiHooked = 0 ;

  // Get the specific import descriptor.
  PIMAGE_IMPORT_DESCRIPTOR pImportDesc = GetNamedImportDescriptor( hModule , szImportMod );
  // The requested module was not imported.
  if ( NULL == pImportDesc )
    return FALSE;

  // Get the original thunk information for this DLL.  I cannot use
  //  the thunk information stored in the pImportDesc->FirstThunk
  //  because the that is the array that the loader has already
  //  bashed to fix up all the imports.  This pointer gives us acess
  //  to the function names.
  PIMAGE_THUNK_DATA pOrigThunk = MakePtr( PIMAGE_THUNK_DATA, hModule, pImportDesc->OriginalFirstThunk );
  // Get the array pointed to by the pImportDesc->FirstThunk.  This is
  //  where I will do the actual bash.
  PIMAGE_THUNK_DATA pRealThunk = MakePtr( PIMAGE_THUNK_DATA, hModule, pImportDesc->FirstThunk );

  // Loop through and look for the one that matches the name.
  while ( NULL != pOrigThunk->u1.Function )
  {
    // Only look at those that are imported by name, not ordinal.
    if ( IMAGE_ORDINAL_FLAG != (pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) )
    {
      // Look get the name of this imported function.
      PIMAGE_IMPORT_BY_NAME pByName = MakePtr( PIMAGE_IMPORT_BY_NAME, hModule, pOrigThunk->u1.AddressOfData );

      // If the name starts with NULL, then just skip out now.
      if ( '\0' == pByName->Name[0] )
        continue;

      // Determines if we do the hook.
      BOOL bDoHook = FALSE;

      // TODO TODO
      //  Might want to consider bsearch here.

      // See if the particular function name is in the import
      //  list.  It might be good to consider requiring the
      //  paHookArray to be in sorted order so bsearch could be
      //  used so the lookup will be faster.  However, the size of
      //  uiCount coming into this function should be rather
      //  small but it is called for each function imported by
      //  szImportMod.
			UINT i;
      for ( i=0; i<uiCount; i++ )
      {
        if ( ( paHookArray[i].szFunc[0] == pByName->Name[0] ) && ( 0 == _strcmpi(paHookArray[i].szFunc, (char*)pByName->Name) ) )
        {
          // If the proc is NULL, kick out, otherwise go
          //  ahead and hook it.
          if ( NULL != paHookArray[i].pProc )
            bDoHook = TRUE;
          break;
        }
      }

      if ( TRUE == bDoHook )
      {
        // I found it.  Now I need to change the protection to
        //  writable before I do the blast.  Note that I am now
        //  blasting into the real thunk area!
        MEMORY_BASIC_INFORMATION mbi_thunk;

        VirtualQuery( pRealThunk, &mbi_thunk, sizeof(MEMORY_BASIC_INFORMATION) );

        VERIFY( VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, PAGE_READWRITE, &mbi_thunk.Protect) );

        // Save the original address if requested.
        if ( 0 != paOrigFuncs )
          paOrigFuncs[i] = (PROC)pRealThunk->u1.Function;
        // Do the actual hook.
        pRealThunk->u1.Function = reinterpret_cast<DWORD_PTR>( paHookArray[i].pProc );

        DWORD dwOldProtect;

        // Change the protection back to what it was before I
        //  blasted.
        VERIFY( VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, mbi_thunk.Protect, &dwOldProtect) );

        // Increment the total number hooked.
        if ( 0 != puiHooked )
					*puiHooked += 1;
      }
    }
    // Increment both tables.
    pOrigThunk++;
    pRealThunk++;
  }

  // All OK, JumpMaster!
  SetLastError( ERROR_SUCCESS );
  return TRUE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*----------------------------------------------------------------------
FUNCTION        :   GetNamedImportDescriptor
DISCUSSION      :
    Gets the import descriptor for the requested module.  If the module
is not imported in hModule, NULL is returned.
    This is a potential useful function in the future.
PARAMETERS      :
    hModule      - The module to hook in.
    szImportMod  - The module name to get the import descriptor for.
RETURNS         :
    NULL  - The module was not imported or hModule is invalid.
    !NULL - The import descriptor.
----------------------------------------------------------------------*/
static PIMAGE_IMPORT_DESCRIPTOR GetNamedImportDescriptor( HMODULE hModule, LPCSTR szImportMod )
{
  // Always check parameters.
  ASSERT( NULL != szImportMod );
  ASSERT( NULL != hModule );
  if ( ( NULL == szImportMod ) || ( NULL == hModule ) )
  {
    SetLastErrorEx( ERROR_INVALID_PARAMETER , SLE_ERROR );
    return 0;
  }

  PIMAGE_DOS_HEADER pDOSHeader = (PIMAGE_DOS_HEADER)hModule;

  // Is this the MZ header?
  if ( ( TRUE == IsBadReadPtr(pDOSHeader, sizeof(IMAGE_DOS_HEADER)) ) ||
       ( IMAGE_DOS_SIGNATURE != pDOSHeader->e_magic ) )
  {
    ASSERT ( FALSE );
    SetLastErrorEx( ERROR_INVALID_PARAMETER , SLE_ERROR );
    return 0;
  }

  // Get the PE header.
  PIMAGE_NT_HEADERS pNTHeader = MakePtr( PIMAGE_NT_HEADERS, pDOSHeader, pDOSHeader->e_lfanew );

  // Is this a real PE image?
  if ( ( TRUE == IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) ) ||
       ( IMAGE_NT_SIGNATURE != pNTHeader->Signature ) )
  {
    ASSERT( FALSE );
    SetLastErrorEx( ERROR_INVALID_PARAMETER , SLE_ERROR );
    return 0;
  }

  // If there is no imports section, leave now.
  if ( 0 == pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress )
    return 0;

  // Get the pointer to the imports section.
  PIMAGE_IMPORT_DESCRIPTOR pImportDesc = MakePtr( PIMAGE_IMPORT_DESCRIPTOR, pDOSHeader,
                                                  pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress );

  // Loop through the import module descriptors looking for the
  //  module whose name matches szImportMod.
  while ( 0 != pImportDesc->Name )
  {
    PSTR szCurrMod = MakePtr( PSTR, pDOSHeader, pImportDesc->Name );
    if ( 0 == _stricmp(szCurrMod , szImportMod) )
    {
      // Found it.
      break;
    }
    // Look at the next one.
    pImportDesc++;
  }

  // If the name is NULL, then the module is not imported.
  if ( NULL == pImportDesc->Name )
		return 0;

  // All OK, Jumpmaster!
  return pImportDesc;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
