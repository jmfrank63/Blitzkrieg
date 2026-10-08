#include "future_blob.h"

// Header-only today; the translation unit exists so static_cast<FutureBlob *>
// and dynamic_cast can resolve past the vtable's single translation unit, and
// so later T02/T03 code can add helpers (eg. merge a known subtree into a blob
// when a user re-saves a mixed project) without touching every includer.

namespace NResourceModel
{

}
