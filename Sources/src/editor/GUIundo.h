#ifndef __GUI_UNDO_H__
#define __GUI_UNDO_H__

#include "..\ui\ui.h"

interface IGUIUndo : public IRefCount
{
public:
	virtual void Undo() = 0;
};

class CSaveAllUndo : public IGUIUndo
{
OBJECT_MINIMAL_METHODS( CSaveAllUndo );

private:
	CPtr<IDataStorage> pStorage;
	CPtr<IUIElement> pSavedElement;

public:
	CSaveAllUndo( IUIElement *pElement );
	~CSaveAllUndo() {}

	virtual void Undo();
};


#endif		//__GUI_UNDO_H__
