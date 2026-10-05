#pragma once
// The Effect exporter: CEffectFrame::SaveRPGStats / ExportFrameData
// (Sources/src/editor/EffectFrm.cpp:170-250). ExportEffect is declared in
// stats_export.h with the other exporters; this header only names the
// importer's absence. MFC's effect editor has no GetRPGStats or LoadRPGStats,
// so an effect cannot be read back into a project (D026) and the bridge
// refuses importing kind 12.

namespace NResourceModel
{

// The refusal importing a .eff gets, one sentence naming the MFC reason.
inline const char *EffectImportRefusal()
{
	return "importing .eff is refused: MFC's effect editor has no reverse path (EffectFrm.cpp has no GetRPGStats/LoadRPGStats)";
}

}
