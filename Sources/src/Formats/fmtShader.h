#ifndef __FMTSHADER_H__
#define __FMTSHADER_H__
#pragma ONCE
typedef std::pair<DWORD, DWORD> SShadeValue;
struct SShaderDesc
{
	typedef std::vector<SShadeValue> SValuesList;
	struct SDefsBlock
	{
		SValuesList rses;
		std::vector<SValuesList> tsses;
		int operator&( IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;
			saver.Add( 1, &rses );
			saver.Add( 2, &tsses );
			return 0;
		}
	};
	SDefsBlock blockSet;
	SDefsBlock blockRestore;
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &blockSet );
		saver.Add( 2, &blockRestore );
		return 0;
	}
};
struct STechnique
{
	int nNumTextures;
	int nNumStages;
	int nStencilDepth;
	SShaderDesc shader;
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &nNumTextures );
		saver.Add( 2, &nNumStages );
		saver.Add( 3, &nStencilDepth );
		saver.Add( 4, &shader );
		return 0;
	}
};
struct SShaderFileHeader
{
	enum { SIGNATURE = 0x00444853, VERSION = 1 };
	DWORD dwSignature;										// file signature
	DWORD dwVersion;											// shader version
	SShaderFileHeader() : dwSignature( SIGNATURE ), dwVersion( VERSION ) {  }
};
#endif // __FMTSHADER_H__
