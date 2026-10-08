# Projects made by the MFC ResourceEditor

One empty project per editor, made on win-home on 2026-10-06 with the shipped
`D:\GOG\Blitzkrieg\reseditor.exe` (Editors menu, File > New Project, saved unchanged).
`goldentest.cgc` is a second campaign. They are byte-exact (`.gitattributes` marks them
`-text`): this is the form MFC writes, with `<own_data>` (export path, frame data) and the
cached `<RPG>` stats block before `History`. The port-written fixtures lacked both, and MFC
reads them unguarded when it opens a project; that is why the bridge, mission, chapter and
campaign fixtures crashed the shipped editor (0xC0000005).

`missiontest.mip` keeps MFC's uninitialised `MapImageRect` (`-1.#QNAN` and denormals):
that is what MFC writes for a new mission.
