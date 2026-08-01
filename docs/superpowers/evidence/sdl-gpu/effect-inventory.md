# Legacy shading-effect inventory

Source basis: `Sources/src/GFX/GraphicsEngine.cpp` (`SetShadingEffect`) and
literal call sites under `Sources/src`. The switch contains the built-in IDs
1–18, 100–104, 110–113, 200, and 300–304. The depth-complexity path invokes
the dynamic range 310–329 (`310 + i`, `i = 0..19`). IDs 20–23 are shader-map
entries used by the legacy pipeline and are included in the catalog.

| IDs | Family | Static call sites | Requirements observed |
| --- | --- | ---: | --- |
| 1, 3, 8, 13 | alpha-test / UI | 44 for 3; 5 for 8; 1 for 13 | textured, one sampler, alpha test + blend |
| 2 | lit | 17 | textured, one sampler, opaque |
| 4, 5 | lightmap | 0 | textured, two samplers, blend policy |
| 6, 7 | stencil | 0 | state-only stencil transitions |
| 9, 14, 15 | alpha-blend | 1 each | textured or diffuse-only, blend policy |
| 10, 11, 12, 16 | particle | 0, 2, 1, 1 | additive/modulate variants; 10/12 disable depth writes |
| 17, 18, 19, 20 | special | 0, 1, 0, 1 | video/special texture state |
| 21, 22, 23 | UI/special | 1 each | minimap texture variants |
| 100–104 | water | 2, 1, 0, 1, 1 | terrain/noise and cross-texture variants |
| 110–113 | shadow | 2, 1, 2, 2 | stencil/depth setup and sprite/mesh shadow |
| 200 | alpha-blend | 0 | additive sprite, depth disabled |
| 300–304 | stencil/special | 1 each | stencil/depth complexity setup |
| 310–329 | special | dynamic range | depth-complexity color overlays |

The catalog deliberately records zero static call sites for IDs that are
reachable through the legacy shader map or dynamic paths; it does not mark
them unused. Vertex masks remain zero until the legacy FVF-to-layout mapping
is completed in the following effect-family packets.
