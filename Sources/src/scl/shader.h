#ifndef __SHADER_H__
#define __SHADER_H__
#pragma once
enum SYMBOL_CONSTANTS
{
   SYMBOL_EOF                         = 0,  // (EOF)
   SYMBOL_ERROR                       = 1,  // (Error)
   SYMBOL_WHITESPACE                  = 2,  // (Whitespace)
   SYMBOL_COMMENTLINE                 = 3,  // (Comment Line)
   SYMBOL_LPARAN                      = 4,  // '('
   SYMBOL_RPARAN                      = 5,  // ')'
   SYMBOL_COMMA                       = 6,  // ,
   SYMBOL_SEMI                        = 7,  // ;
   SYMBOL_LBRACE                      = 8,  // '{'
   SYMBOL_PIPE                        = 9,  // '|'
   SYMBOL_RBRACE                      = 10,  // '}'
   SYMBOL_EQ                          = 11,  // =
   SYMBOL_ADD                         = 12,  // Add
   SYMBOL_ADDRESS                     = 13,  // address
   SYMBOL_ADDSIGNED                   = 14,  // AddSigned
   SYMBOL_ADDSIGNED2X                 = 15,  // AddSigned2x
   SYMBOL_ADDSMOOTH                   = 16,  // AddSmooth
   SYMBOL_ALPHA                       = 17,  // alpha
   SYMBOL_ALPHATEST                   = 18,  // alphatest
   SYMBOL_ALWAYS                      = 19,  // Always
   SYMBOL_AMBIENT                     = 20,  // ambient
   SYMBOL_ANISOTROPIC                 = 21,  // Anisotropic
   SYMBOL_BLEND                       = 22,  // blend
   SYMBOL_BLENDCURRENTALPHA           = 23,  // BlendCurrentAlpha
   SYMBOL_BLENDDIFFUSEALPHA           = 24,  // BlendDiffuseAlpha
   SYMBOL_BLENDFACTORALPHA            = 25,  // BlendFactorAlpha
   SYMBOL_BLENDTEXTUREALPHA           = 26,  // BlendTextureAlpha
   SYMBOL_BORDER                      = 27,  // Border
   SYMBOL_CAMERASPACENORMAL           = 28,  // CameraSpaceNormal
   SYMBOL_CAMERASPACEPOSITION         = 29,  // CameraSpacePosition
   SYMBOL_CAMERASPACEREFLECTIONVECTOR = 30,  // CameraSpaceReflectionVector
   SYMBOL_CCW                         = 31,  // CCW
   SYMBOL_CDF                         = 32,  // cdf
   SYMBOL_CLAMP                       = 33,  // Clamp
   SYMBOL_CLIPPING                    = 34,  // clipping
   SYMBOL_COLOR                       = 35,  // color
   SYMBOL_COMPLEMENT                  = 36,  // Complement
   SYMBOL_COUNT1                      = 37,  // Count1
   SYMBOL_COUNT2                      = 38,  // Count2
   SYMBOL_COUNT3                      = 39,  // Count3
   SYMBOL_COUNT4                      = 40,  // Count4
   SYMBOL_CSP                         = 41,  // csp
   SYMBOL_CULL                        = 42,  // cull
   SYMBOL_CURR                        = 43,  // curr
   SYMBOL_CW                          = 44,  // CW
   SYMBOL_DEC                         = 45,  // Dec
   SYMBOL_DECIMALLITERAL              = 46,  // DecimalLiteral
   SYMBOL_DECSAT                      = 47,  // DecSat
   SYMBOL_DEPTH                       = 48,  // depth
   SYMBOL_DISABLE                     = 49,  // Disable
   SYMBOL_DO                          = 50,  // do
   SYMBOL_DP3                         = 51,  // DP3
   SYMBOL_DSTALPHA                    = 52,  // DstAlpha
   SYMBOL_DSTCOLOR                    = 53,  // DstColor
   SYMBOL_EQUAL                       = 54,  // Equal
   SYMBOL_FALSE                       = 55,  // false
   SYMBOL_FILTER                      = 56,  // filter
   SYMBOL_FLATCUBIC                   = 57,  // FlatCubic
   SYMBOL_GAUSSIANCUBIC               = 58,  // GaussianCubic
   SYMBOL_GREATER                     = 59,  // Greater
   SYMBOL_GREATEREQUAL                = 60,  // GreaterEqual
   SYMBOL_HEXLITERAL                  = 61,  // HexLiteral
   SYMBOL_INC                         = 62,  // Inc
   SYMBOL_INCSAT                      = 63,  // IncSat
   SYMBOL_INVDSTALPHA                 = 64,  // InvDstAlpha
   SYMBOL_INVDSTCOLOR                 = 65,  // InvDstColor
   SYMBOL_INVERT                      = 66,  // Invert
   SYMBOL_INVSRCALPHA                 = 67,  // InvSrcAlpha
   SYMBOL_INVSRCCOLOR                 = 68,  // InvSrcColor
   SYMBOL_KEEP                        = 69,  // Keep
   SYMBOL_LERP                        = 70,  // Lerp
   SYMBOL_LESS                        = 71,  // Less
   SYMBOL_LESSEQUAL                   = 72,  // LessEqual
   SYMBOL_LIGHTING                    = 73,  // lighting
   SYMBOL_LINEAR                      = 74,  // Linear
   SYMBOL_MAD                         = 75,  // Mad
   SYMBOL_MAX                         = 76,  // Max
   SYMBOL_MIN                         = 77,  // Min
   SYMBOL_MIRROR                      = 78,  // Mirror
   SYMBOL_MIRRORONCE                  = 79,  // MirrorOnce
   SYMBOL_MUL                         = 80,  // Mul
   SYMBOL_MUL2X                       = 81,  // Mul2x
   SYMBOL_MUL4X                       = 82,  // Mul4x
   SYMBOL_NEVER                       = 83,  // Never
   SYMBOL_NONE                        = 84,  // None
   SYMBOL_NOTEQUAL                    = 85,  // NotEqual
   SYMBOL_NOWRITE                     = 86,  // NoWrite
   SYMBOL_ONE                         = 87,  // One
   SYMBOL_POINT                       = 88,  // Point
   SYMBOL_PROJECTEDCOUNT1             = 89,  // ProjectedCount1
   SYMBOL_PROJECTEDCOUNT2             = 90,  // ProjectedCount2
   SYMBOL_PROJECTEDCOUNT3             = 91,  // ProjectedCount3
   SYMBOL_PROJECTEDCOUNT4             = 92,  // ProjectedCount4
   SYMBOL_PROPS                       = 93,  // props
   SYMBOL_REPLACE                     = 94,  // Replace
   SYMBOL_REPLICATE                   = 95,  // Replicate
   SYMBOL_RESTORE                     = 96,  // restore
   SYMBOL_REVSUB                      = 97,  // RevSub
   SYMBOL_SET                         = 98,  // set
   SYMBOL_SPECULAR                    = 99,  // specular
   SYMBOL_SRCALPHA                    = 100,  // SrcAlpha
   SYMBOL_SRCALPHASAT                 = 101,  // SrcAlphaSat
   SYMBOL_SRCCOLOR                    = 102,  // SrcColor
   SYMBOL_STAGEPROPS                  = 103,  // stageprops
   SYMBOL_STENCIL                     = 104,  // stencil
   SYMBOL_SUB                         = 105,  // Sub
   SYMBOL_TECHNIQUE                   = 106,  // technique
   SYMBOL_TEMP                        = 107,  // temp
   SYMBOL_TEX                         = 108,  // tex
   SYMBOL_TEXCOORDS                   = 109,  // texcoords
   SYMBOL_TFACTOR                     = 110,  // tfactor
   SYMBOL_TRUE                        = 111,  // true
   SYMBOL_WRAP                        = 112,  // Wrap
   SYMBOL_ZERO                        = 113,  // Zero
   SYMBOL_ALPHAOPS                    = 114,  // <Alpha Ops>
   SYMBOL_ALPHAREF                    = 115,  // <Alpha Ref>
   SYMBOL_ALPHATESTVALS               = 116,  // <AlphaTest Vals>
   SYMBOL_ARG                         = 117,  // <Arg>
   SYMBOL_BLENDMODES                  = 118,  // <Blend Modes>
   SYMBOL_BLENDOP                     = 119,  // <Blend Op>
   SYMBOL_BOOLVAL                     = 120,  // <Bool Val>
   SYMBOL_CMPFUNC                     = 121,  // <Cmp Func>
   SYMBOL_CMPFUNCCMP                  = 122,  // <Cmp Func Cmp>
   SYMBOL_CMPFUNCNOCMP                = 123,  // <Cmp Func NoCmp>
   SYMBOL_COLOROPS                    = 124,  // <Color Ops>
   SYMBOL_CULLMODE                    = 125,  // <Cull Mode>
   SYMBOL_DECLARATION                 = 126,  // <Declaration>
   SYMBOL_DECLARATIONS                = 127,  // <Declarations>
   SYMBOL_DEFSBLOCK                   = 128,  // <Defs Block>
   SYMBOL_DEPTHFUNC                   = 129,  // <Depth Func>
   SYMBOL_DSTBLEND                    = 130,  // <Dst Blend>
   SYMBOL_EXP                         = 131,  // <Exp>
   SYMBOL_EXP2                        = 132,  // <Exp2>
   SYMBOL_EXP3                        = 133,  // <Exp3>
   SYMBOL_EXPRESSION                  = 134,  // <Expression>
   SYMBOL_FUNCARGS2                   = 135,  // <Func Args 2>
   SYMBOL_FUNCARGS3                   = 136,  // <Func Args 3>
   SYMBOL_INTVAL                      = 137,  // <Int Val>
   SYMBOL_NUMSTAGES                   = 138,  // <Num Stages>
   SYMBOL_NUMTEXTURES                 = 139,  // <Num Textures>
   SYMBOL_PROPALPHATEST               = 140,  // <Prop AlphaTest>
   SYMBOL_PROPAMBIENT                 = 141,  // <Prop Ambient>
   SYMBOL_PROPBLEND                   = 142,  // <Prop Blend>
   SYMBOL_PROPCLIPPING                = 143,  // <Prop Clipping>
   SYMBOL_PROPCULL                    = 144,  // <Prop Cull>
   SYMBOL_PROPDEPTH                   = 145,  // <Prop Depth>
   SYMBOL_PROPLIGHTING                = 146,  // <Prop Lighting>
   SYMBOL_PROPSPECULAR                = 147,  // <Prop Specular>
   SYMBOL_PROPSTENCIL                 = 148,  // <Prop Stencil>
   SYMBOL_PROPTFACTOR                 = 149,  // <Prop TFactor>
   SYMBOL_PROPERTIES                  = 150,  // <Properties>
   SYMBOL_PROPSBLOCK                  = 151,  // <Props Block>
   SYMBOL_PROPSINGLE                  = 152,  // <PropSingle>
   SYMBOL_PROPSLIST                   = 153,  // <PropsList>
   SYMBOL_RESTOREBLOCK                = 154,  // <Restore Block>
   SYMBOL_SETBLOCK                    = 155,  // <Set Block>
   SYMBOL_SIMPLEARG                   = 156,  // <SimpleArg>
   SYMBOL_SRCBLEND                    = 157,  // <Src Blend>
   SYMBOL_STENCILACTION               = 159,  // <Stencil Action>
   SYMBOL_STENCILACTIONS              = 160,  // <Stencil Actions>
   SYMBOL_STENCILARGS                 = 161,  // <Stencil Args>
   SYMBOL_STENCILMASK                 = 162,  // <Stencil Mask>
   SYMBOL_STENCILREFVAL               = 163,  // <Stencil Ref Val>
   SYMBOL_TECHNIQUEDEF                = 165,  // <Technique Def>
   SYMBOL_TEXGEN                      = 166,  // <Tex Gen>
   SYMBOL_TEXFILTERMODE               = 167,  // <TexFilterMode>
   SYMBOL_TEXPROPLIST                 = 168,  // <TexPropList>
   SYMBOL_TEXSINGLEPROP               = 169,  // <TexSingleProp>
   SYMBOL_TEXSTAGEPROPS               = 170,  // <TexStageProps>
   SYMBOL_TEXTUREPROPSBLOCK           = 171,  // <Texture Props Block>
   SYMBOL_TEXWRAPMODE                 = 172,  // <TexWrapMode>
   SYMBOL_TRANSFORMFLAGS              = 173  // <Transform Flags>
};

enum RULE_CONSTANTS
{
   RULE_DECLARATIONS                                                = 0,  // <Declarations> ::= <Declaration> <Declarations>
   RULE_DECLARATIONS2                                               = 1,  // <Declarations> ::= 
   RULE_DECLARATION                                                 = 2,  // <Declaration> ::= <Technique>
   RULE_TECHNIQUE_TECHNIQUE_LPARAN_COMMA_RPARAN_LBRACE_RBRACE       = 3,  // <Technique> ::= technique '(' <Num Textures> , <Num Stages> ')' '{' <Technique Def> '}'
   RULE_TECHNIQUE_TECHNIQUE_LPARAN_COMMA_COMMA_RPARAN_LBRACE_RBRACE = 4,  // <Technique> ::= technique '(' <Num Textures> , <Num Stages> , <Stencil> ')' '{' <Technique Def> '}'
   RULE_NUMTEXTURES_DECIMALLITERAL                                  = 5,  // <Num Textures> ::= DecimalLiteral
   RULE_NUMSTAGES_DECIMALLITERAL                                    = 6,  // <Num Stages> ::= DecimalLiteral
   RULE_STENCIL_DECIMALLITERAL                                      = 7,  // <Stencil> ::= DecimalLiteral
   RULE_TECHNIQUEDEF                                                = 8,  // <Technique Def> ::= <Set Block>
   RULE_TECHNIQUEDEF2                                               = 9,  // <Technique Def> ::= <Set Block> <Restore Block>
   RULE_SETBLOCK_SET_LBRACE_RBRACE                                  = 10,  // <Set Block> ::= set '{' <Defs Block> '}'
   RULE_RESTOREBLOCK_RESTORE_LBRACE_RBRACE                          = 11,  // <Restore Block> ::= restore '{' <Defs Block> '}'
   RULE_DEFSBLOCK                                                   = 12,  // <Defs Block> ::= <Color Ops> <Alpha Ops>
   RULE_DEFSBLOCK2                                                  = 13,  // <Defs Block> ::= <Color Ops> <Alpha Ops> <Properties>
   RULE_COLOROPS_COLOR_EQ_SEMI                                      = 14,  // <Color Ops> ::= color = <Expression> ;
   RULE_COLOROPS                                                    = 15,  // <Color Ops> ::= 
   RULE_ALPHAOPS_ALPHA_EQ_SEMI                                      = 16,  // <Alpha Ops> ::= alpha = <Expression> ;
   RULE_ALPHAOPS                                                    = 17,  // <Alpha Ops> ::= 
   RULE_FUNCARGS2_LPARAN_COMMA_RPARAN                               = 18,  // <Func Args 2> ::= '(' <Arg> , <Arg> ')'
   RULE_FUNCARGS3_LPARAN_COMMA_COMMA_RPARAN                         = 19,  // <Func Args 3> ::= '(' <Arg> , <Arg> , <Arg> ')'
   RULE_EXPRESSION                                                  = 20,  // <Expression> ::= <Exp>
   RULE_EXPRESSION_PIPE                                             = 21,  // <Expression> ::= <Expression> '|' <Exp>
   RULE_EXP                                                         = 22,  // <Exp> ::= <Exp2> <Func Args 2>
   RULE_EXP2                                                        = 23,  // <Exp> ::= <Exp3> <Func Args 3>
   RULE_EXP3                                                        = 24,  // <Exp> ::= <Arg>
   RULE_EXP2_ADD                                                    = 25,  // <Exp2> ::= Add
   RULE_EXP2_ADDSMOOTH                                              = 26,  // <Exp2> ::= AddSmooth
   RULE_EXP2_ADDSIGNED                                              = 27,  // <Exp2> ::= AddSigned
   RULE_EXP2_ADDSIGNED2X                                            = 28,  // <Exp2> ::= AddSigned2x
   RULE_EXP2_SUB                                                    = 29,  // <Exp2> ::= Sub
   RULE_EXP2_MUL                                                    = 30,  // <Exp2> ::= Mul
   RULE_EXP2_MUL2X                                                  = 31,  // <Exp2> ::= Mul2x
   RULE_EXP2_MUL4X                                                  = 32,  // <Exp2> ::= Mul4x
   RULE_EXP2_BLENDCURRENTALPHA                                      = 33,  // <Exp2> ::= BlendCurrentAlpha
   RULE_EXP2_BLENDTEXTUREALPHA                                      = 34,  // <Exp2> ::= BlendTextureAlpha
   RULE_EXP2_BLENDDIFFUSEALPHA                                      = 35,  // <Exp2> ::= BlendDiffuseAlpha
   RULE_EXP2_BLENDFACTORALPHA                                       = 36,  // <Exp2> ::= BlendFactorAlpha
   RULE_EXP2_DP3                                                    = 37,  // <Exp2> ::= DP3
   RULE_EXP3_LERP                                                   = 38,  // <Exp3> ::= Lerp
   RULE_EXP3_MAD                                                    = 39,  // <Exp3> ::= Mad
   RULE_SIMPLEARG_CDF                                               = 40,  // <SimpleArg> ::= cdf
   RULE_SIMPLEARG_CSP                                               = 41,  // <SimpleArg> ::= csp
   RULE_SIMPLEARG_TFACTOR                                           = 42,  // <SimpleArg> ::= tfactor
   RULE_SIMPLEARG_CURR                                              = 43,  // <SimpleArg> ::= curr
   RULE_SIMPLEARG_TEMP                                              = 44,  // <SimpleArg> ::= temp
   RULE_SIMPLEARG_TEX                                               = 45,  // <SimpleArg> ::= tex
   RULE_ARG                                                         = 46,  // <Arg> ::= <SimpleArg>
   RULE_ARG_REPLICATE_LPARAN_RPARAN                                 = 47,  // <Arg> ::= Replicate '(' <SimpleArg> ')'
   RULE_ARG_COMPLEMENT_LPARAN_RPARAN                                = 48,  // <Arg> ::= Complement '(' <SimpleArg> ')'
   RULE_PROPERTIES                                                  = 49,  // <Properties> ::= <Props Block>
   RULE_PROPERTIES2                                                 = 50,  // <Properties> ::= <Texture Props Block>
   RULE_PROPERTIES3                                                 = 51,  // <Properties> ::= <Props Block> <Texture Props Block>
   RULE_PROPERTIES4                                                 = 52,  // <Properties> ::= <Texture Props Block> <Props Block>
   RULE_PROPSBLOCK_PROPS_LBRACE_RBRACE                              = 53,  // <Props Block> ::= props '{' <PropsList> '}'
   RULE_PROPSLIST                                                   = 54,  // <PropsList> ::= <PropSingle>
   RULE_PROPSLIST2                                                  = 55,  // <PropsList> ::= <PropSingle> <PropsList>
   RULE_PROPSINGLE                                                  = 56,  // <PropSingle> ::= <Prop Blend>
   RULE_PROPSINGLE2                                                 = 57,  // <PropSingle> ::= <Prop Depth>
   RULE_PROPSINGLE3                                                 = 58,  // <PropSingle> ::= <Prop Cull>
   RULE_PROPSINGLE4                                                 = 59,  // <PropSingle> ::= <Prop AlphaTest>
   RULE_PROPSINGLE5                                                 = 60,  // <PropSingle> ::= <Prop Specular>
   RULE_PROPSINGLE6                                                 = 61,  // <PropSingle> ::= <Prop Stencil>
   RULE_PROPSINGLE7                                                 = 62,  // <PropSingle> ::= <Prop TFactor>
   RULE_PROPSINGLE8                                                 = 63,  // <PropSingle> ::= <Prop Clipping>
   RULE_PROPSINGLE9                                                 = 64,  // <PropSingle> ::= <Prop Lighting>
   RULE_PROPSINGLE10                                                = 65,  // <PropSingle> ::= <Prop Ambient>
   RULE_CMPFUNC                                                     = 66,  // <Cmp Func> ::= <Cmp Func Cmp>
   RULE_CMPFUNC2                                                    = 67,  // <Cmp Func> ::= <Cmp Func NoCmp>
   RULE_CMPFUNCCMP_LESS                                             = 68,  // <Cmp Func Cmp> ::= Less
   RULE_CMPFUNCCMP_LESSEQUAL                                        = 69,  // <Cmp Func Cmp> ::= LessEqual
   RULE_CMPFUNCCMP_GREATER                                          = 70,  // <Cmp Func Cmp> ::= Greater
   RULE_CMPFUNCCMP_GREATEREQUAL                                     = 71,  // <Cmp Func Cmp> ::= GreaterEqual
   RULE_CMPFUNCCMP_EQUAL                                            = 72,  // <Cmp Func Cmp> ::= Equal
   RULE_CMPFUNCCMP_NOTEQUAL                                         = 73,  // <Cmp Func Cmp> ::= NotEqual
   RULE_CMPFUNCNOCMP_NEVER                                          = 74,  // <Cmp Func NoCmp> ::= Never
   RULE_CMPFUNCNOCMP_ALWAYS                                         = 75,  // <Cmp Func NoCmp> ::= Always
   RULE_PROPAMBIENT_AMBIENT_EQ_SEMI                                 = 76,  // <Prop Ambient> ::= ambient = <Int Val> ;
   RULE_PROPLIGHTING_LIGHTING_EQ_SEMI                               = 77,  // <Prop Lighting> ::= lighting = <Bool Val> ;
   RULE_PROPCLIPPING_CLIPPING_EQ_SEMI                               = 78,  // <Prop Clipping> ::= clipping = <Bool Val> ;
   RULE_PROPTFACTOR_TFACTOR_EQ_SEMI                                 = 79,  // <Prop TFactor> ::= tfactor = <Int Val> ;
   RULE_PROPSTENCIL_STENCIL_EQ_DO_LPARAN_RPARAN_SEMI                = 80,  // <Prop Stencil> ::= stencil = <Stencil Args> do '(' <Stencil Actions> ')' ;
   RULE_PROPSTENCIL_STENCIL_EQ_SEMI                                 = 81,  // <Prop Stencil> ::= stencil = <Stencil Args> ;
   RULE_STENCILARGS_LPARAN_COMMA_RPARAN                             = 82,  // <Stencil Args> ::= <Cmp Func Cmp> '(' <Stencil Ref Val> , <Stencil Mask> ')'
   RULE_STENCILARGS_LPARAN_RPARAN                                   = 83,  // <Stencil Args> ::= <Cmp Func Cmp> '(' <Stencil Ref Val> ')'
   RULE_STENCILARGS                                                 = 84,  // <Stencil Args> ::= <Cmp Func NoCmp>
   RULE_STENCILARGS_NONE                                            = 85,  // <Stencil Args> ::= None
   RULE_STENCILREFVAL_DECIMALLITERAL                                = 86,  // <Stencil Ref Val> ::= DecimalLiteral
   RULE_STENCILREFVAL_HEXLITERAL                                    = 87,  // <Stencil Ref Val> ::= HexLiteral
   RULE_STENCILMASK_DECIMALLITERAL                                  = 88,  // <Stencil Mask> ::= DecimalLiteral
   RULE_STENCILMASK_HEXLITERAL                                      = 89,  // <Stencil Mask> ::= HexLiteral
   RULE_STENCILACTIONS_COMMA_COMMA                                  = 90,  // <Stencil Actions> ::= <Stencil Action> , <Stencil Action> , <Stencil Action>
   RULE_STENCILACTION_KEEP                                          = 91,  // <Stencil Action> ::= Keep
   RULE_STENCILACTION_ZERO                                          = 92,  // <Stencil Action> ::= Zero
   RULE_STENCILACTION_REPLACE                                       = 93,  // <Stencil Action> ::= Replace
   RULE_STENCILACTION_INC                                           = 94,  // <Stencil Action> ::= Inc
   RULE_STENCILACTION_DEC                                           = 95,  // <Stencil Action> ::= Dec
   RULE_STENCILACTION_INCSAT                                        = 96,  // <Stencil Action> ::= IncSat
   RULE_STENCILACTION_DECSAT                                        = 97,  // <Stencil Action> ::= DecSat
   RULE_STENCILACTION_INVERT                                        = 98,  // <Stencil Action> ::= Invert
   RULE_PROPSPECULAR_SPECULAR_EQ_SEMI                               = 99,  // <Prop Specular> ::= specular = <Bool Val> ;
   RULE_PROPALPHATEST_ALPHATEST_EQ_SEMI                             = 100,  // <Prop AlphaTest> ::= alphatest = <AlphaTest Vals> ;
   RULE_ALPHATESTVALS_NONE                                          = 101,  // <AlphaTest Vals> ::= None
   RULE_ALPHATESTVALS_LPARAN_RPARAN                                 = 102,  // <AlphaTest Vals> ::= <Cmp Func Cmp> '(' <Alpha Ref> ')'
   RULE_ALPHATESTVALS                                               = 103,  // <AlphaTest Vals> ::= <Cmp Func NoCmp>
   RULE_ALPHAREF                                                    = 104,  // <Alpha Ref> ::= <Int Val>
   RULE_PROPCULL_CULL_EQ_SEMI                                       = 105,  // <Prop Cull> ::= cull = <Cull Mode> ;
   RULE_CULLMODE_NONE                                               = 106,  // <Cull Mode> ::= None
   RULE_CULLMODE_CW                                                 = 107,  // <Cull Mode> ::= CW
   RULE_CULLMODE_CCW                                                = 108,  // <Cull Mode> ::= CCW
   RULE_PROPDEPTH_DEPTH_EQ_SEMI                                     = 109,  // <Prop Depth> ::= depth = <Depth Func> ;
   RULE_DEPTHFUNC_NONE                                              = 110,  // <Depth Func> ::= None
   RULE_DEPTHFUNC_NOWRITE                                           = 111,  // <Depth Func> ::= NoWrite
   RULE_DEPTHFUNC                                                   = 112,  // <Depth Func> ::= <Cmp Func>
   RULE_PROPBLEND_BLEND_EQ_LPARAN_COMMA_RPARAN_SEMI                 = 113,  // <Prop Blend> ::= blend = <Blend Op> '(' <Src Blend> , <Dst Blend> ')' ;
   RULE_BLENDOP_ADD                                                 = 114,  // <Blend Op> ::= Add
   RULE_BLENDOP_SUB                                                 = 115,  // <Blend Op> ::= Sub
   RULE_BLENDOP_REVSUB                                              = 116,  // <Blend Op> ::= RevSub
   RULE_BLENDOP_MIN                                                 = 117,  // <Blend Op> ::= Min
   RULE_BLENDOP_MAX                                                 = 118,  // <Blend Op> ::= Max
   RULE_SRCBLEND                                                    = 119,  // <Src Blend> ::= <Blend Modes>
   RULE_DSTBLEND                                                    = 120,  // <Dst Blend> ::= <Blend Modes>
   RULE_BLENDMODES_ZERO                                             = 121,  // <Blend Modes> ::= Zero
   RULE_BLENDMODES_ONE                                              = 122,  // <Blend Modes> ::= One
   RULE_BLENDMODES_SRCCOLOR                                         = 123,  // <Blend Modes> ::= SrcColor
   RULE_BLENDMODES_INVSRCCOLOR                                      = 124,  // <Blend Modes> ::= InvSrcColor
   RULE_BLENDMODES_SRCALPHA                                         = 125,  // <Blend Modes> ::= SrcAlpha
   RULE_BLENDMODES_INVSRCALPHA                                      = 126,  // <Blend Modes> ::= InvSrcAlpha
   RULE_BLENDMODES_DSTCOLOR                                         = 127,  // <Blend Modes> ::= DstColor
   RULE_BLENDMODES_INVDSTCOLOR                                      = 128,  // <Blend Modes> ::= InvDstColor
   RULE_BLENDMODES_DSTALPHA                                         = 129,  // <Blend Modes> ::= DstAlpha
   RULE_BLENDMODES_INVDSTALPHA                                      = 130,  // <Blend Modes> ::= InvDstAlpha
   RULE_BLENDMODES_SRCALPHASAT                                      = 131,  // <Blend Modes> ::= SrcAlphaSat
   RULE_TEXTUREPROPSBLOCK                                           = 132,  // <Texture Props Block> ::= <TexStageProps>
   RULE_TEXTUREPROPSBLOCK2                                          = 133,  // <Texture Props Block> ::= <TexStageProps> <Texture Props Block>
   RULE_TEXSTAGEPROPS_STAGEPROPS_LPARAN_RPARAN_LBRACE_RBRACE        = 134,  // <TexStageProps> ::= stageprops '(' <Int Val> ')' '{' <TexPropList> '}'
   RULE_TEXPROPLIST                                                 = 135,  // <TexPropList> ::= <TexSingleProp>
   RULE_TEXPROPLIST2                                                = 136,  // <TexPropList> ::= <TexSingleProp> <TexPropList>
   RULE_TEXSINGLEPROP_ADDRESS_LPARAN_COMMA_COMMA_RPARAN_SEMI        = 137,  // <TexSingleProp> ::= address '(' <TexWrapMode> , <TexWrapMode> , <TexWrapMode> ')' ;
   RULE_TEXSINGLEPROP_ADDRESS_LPARAN_COMMA_RPARAN_SEMI              = 138,  // <TexSingleProp> ::= address '(' <TexWrapMode> , <TexWrapMode> ')' ;
   RULE_TEXSINGLEPROP_FILTER_LPARAN_COMMA_COMMA_RPARAN_SEMI         = 139,  // <TexSingleProp> ::= filter '(' <TexFilterMode> , <TexFilterMode> , <TexFilterMode> ')' ;
   RULE_TEXSINGLEPROP_FILTER_LPARAN_COMMA_RPARAN_SEMI               = 140,  // <TexSingleProp> ::= filter '(' <TexFilterMode> , <TexFilterMode> ')' ;
   RULE_TEXSINGLEPROP_TEXCOORDS_LPARAN_RPARAN_SEMI                  = 141,  // <TexSingleProp> ::= texcoords '(' <Tex Gen> ')' ;
   RULE_TEXSINGLEPROP_TEXCOORDS_LPARAN_COMMA_RPARAN_SEMI            = 142,  // <TexSingleProp> ::= texcoords '(' <Tex Gen> , <Transform Flags> ')' ;
   RULE_TEXWRAPMODE_WRAP                                            = 143,  // <TexWrapMode> ::= Wrap
   RULE_TEXWRAPMODE_MIRROR                                          = 144,  // <TexWrapMode> ::= Mirror
   RULE_TEXWRAPMODE_CLAMP                                           = 145,  // <TexWrapMode> ::= Clamp
   RULE_TEXWRAPMODE_BORDER                                          = 146,  // <TexWrapMode> ::= Border
   RULE_TEXWRAPMODE_MIRRORONCE                                      = 147,  // <TexWrapMode> ::= MirrorOnce
   RULE_TEXFILTERMODE_NONE                                          = 148,  // <TexFilterMode> ::= None
   RULE_TEXFILTERMODE_POINT                                         = 149,  // <TexFilterMode> ::= Point
   RULE_TEXFILTERMODE_LINEAR                                        = 150,  // <TexFilterMode> ::= Linear
   RULE_TEXFILTERMODE_ANISOTROPIC                                   = 151,  // <TexFilterMode> ::= Anisotropic
   RULE_TEXFILTERMODE_FLATCUBIC                                     = 152,  // <TexFilterMode> ::= FlatCubic
   RULE_TEXFILTERMODE_GAUSSIANCUBIC                                 = 153,  // <TexFilterMode> ::= GaussianCubic
   RULE_TEXGEN                                                      = 154,  // <Tex Gen> ::= <Int Val>
   RULE_TEXGEN_CAMERASPACENORMAL                                    = 155,  // <Tex Gen> ::= CameraSpaceNormal
   RULE_TEXGEN_CAMERASPACEPOSITION                                  = 156,  // <Tex Gen> ::= CameraSpacePosition
   RULE_TEXGEN_CAMERASPACEREFLECTIONVECTOR                          = 157,  // <Tex Gen> ::= CameraSpaceReflectionVector
   RULE_TRANSFORMFLAGS_DISABLE                                      = 158,  // <Transform Flags> ::= Disable
   RULE_TRANSFORMFLAGS_COUNT1                                       = 159,  // <Transform Flags> ::= Count1
   RULE_TRANSFORMFLAGS_COUNT2                                       = 160,  // <Transform Flags> ::= Count2
   RULE_TRANSFORMFLAGS_COUNT3                                       = 161,  // <Transform Flags> ::= Count3
   RULE_TRANSFORMFLAGS_COUNT4                                       = 162,  // <Transform Flags> ::= Count4
   RULE_TRANSFORMFLAGS_PROJECTEDCOUNT1                              = 163,  // <Transform Flags> ::= ProjectedCount1
   RULE_TRANSFORMFLAGS_PROJECTEDCOUNT2                              = 164,  // <Transform Flags> ::= ProjectedCount2
   RULE_TRANSFORMFLAGS_PROJECTEDCOUNT3                              = 165,  // <Transform Flags> ::= ProjectedCount3
   RULE_TRANSFORMFLAGS_PROJECTEDCOUNT4                              = 166,  // <Transform Flags> ::= ProjectedCount4
   RULE_INTVAL_HEXLITERAL                                           = 167,  // <Int Val> ::= HexLiteral
   RULE_INTVAL_DECIMALLITERAL                                       = 168,  // <Int Val> ::= DecimalLiteral
   RULE_BOOLVAL_TRUE                                                = 169,  // <Bool Val> ::= true
   RULE_BOOLVAL_FALSE                                               = 170  // <Bool Val> ::= false
};
#endif // __SHADER_H__