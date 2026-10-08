#pragma once
// Mirror of the MFC SProp (Sources/src/editor/TreeItem.h:10) without
// CVariant's windows types and CVectorOfStrings' using-namespace coupling.
// Field names match the original so the next tasks (every item class port) can
// one-to-one translate the InitDefaultValues bodies without reading twice.
//
// A note on szDisplayName: the MFC file stored a windows-1251 Russian label.
// The port leaves these bytes untouched; the comparator does not decode them,
// and the XML round-trip never looks inside a value's string.

#include <string>
#include <vector>

#include "domen_id.h"
#include "variant.h"
#include "xml.h"

namespace NResourceModel
{

struct SProp
{
	int nId = 0;                              // UI property identifier (ObjectInspector)
	std::string szDefaultName;                // internal name, used for serialisation keys
	std::string szDisplayName;                // Russian label shown in the UI (opaque bytes)
	DomenID nDomenType = DT_ERROR;            // which widget the UI picks for this value
	CVariant value;                           // the authored value
	std::vector<std::string> szStrings;       // combo-box entries, only used when nDomenType == DT_COMBO

	// The <value> element the value was read from (mfc_value.h). Saving an
	// unedited value writes it back unchanged, stale slots included; a new
	// prop has none and gets the slots MFC's CVariant constructors set.
	NResourceXml::Node mfcValue;
	bool bHasMfcValue = false;
};

using CPropVector = std::vector<SProp>;

}
