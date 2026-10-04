#include "factory.h"

namespace NResourceModel
{

CTreeItemFactory &CTreeItemFactory::Instance()
{
	// Meyers singleton; order of registration is the order of static-initialiser
	// execution in the subeditor translation units. T02 adds those; this
	// instance is deliberately empty until then so the scaffold test can prove
	// the IsRegistered() == false path lands in a FutureBlob.
	static CTreeItemFactory instance;
	return instance;
}

}
