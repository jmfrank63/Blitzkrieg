/*
 *   Copyright (c) 1997-8  S3 Inc.  All Rights Reserved.
 *
 *   Module Name:  s3tc.h
 *
 *   Purpose:  Constant, structure, and prototype definitions for S3TC
 *			   interface to DX surface
 *
 *   Author:  Dan Hung, Martin Hoffesommer
 *
 *   Revision History:
 *	  version Beta 1.00.00-98-03-26
 */


#ifndef _S3_INTRF_H_
#define _S3_INTRF_H_

#include <ddraw.h>

#define S3TC_ENCODE_RGB_FULL    		   0x0
#define S3TC_ENCODE_RGB_COLOR_KEY		0x1
#define S3TC_ENCODE_RGB_ALPHA_COMPARE	0x2
#define _S3TC_ENCODE_RGB_MASK			   0xff

#define S3TC_ENCODE_ALPHA_NONE			0x000
#define S3TC_ENCODE_ALPHA_EXPLICIT		0x100
#define S3TC_ENCODE_ALPHA_INTERPOLATED	0x200
#define _S3TC_ENCODE_ALPHA_MASK			0xff00

#define S3TC_ENCODE_ALPHA_NEED0			0x10000
#define S3TC_ENCODE_ALPHA_NEED1			0x20000


void S3TCsetAlphaReference(int nRef);

unsigned S3TCgetEncodeSize(DDSURFACEDESC *lpDesc,	 // [in]
						   unsigned dwEncodeType 	       // [in]
						   );

void S3TCencode(DDSURFACEDESC *lpSrc,		// [in]
				PALETTEENTRY *lpPal,		      // [in], may be NULL
				DDSURFACEDESC *lpDest,		   // [out]
				void *lpDestBuf,			      // [in]
				unsigned dwEncodeType,  	   // [in]
				float *weight			       	// [in]
				);

unsigned S3TCgetDecodeSize(DDSURFACEDESC *lpDesc);

void S3TCdecode(DDSURFACEDESC *lpSrc,		// [in]
				DDSURFACEDESC *lpDest,		   // [out]
				void *lpDestBuf				   // [in]
				);

#endif
