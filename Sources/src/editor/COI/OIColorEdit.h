#ifndef __OICOLOREDIT_H__
#define __OICOLOREDIT_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OIBrowEdit.h"

class COIColorEdit : public COIBrowseEdit
{
public:
	virtual void OnBrowse();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	
  DECLARE_MESSAGE_MAP()
		
};

#endif // __OICOLOREDIT_H__
