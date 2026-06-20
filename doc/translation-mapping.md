# 3ds Max → MaterialX / USD translation mapping

This document tracks how 3ds Max PhysicalMaterial / OpenPBR / MaterialXMaterial
properties translate to MaterialX `standard_surface` (and related) shader
inputs as they are written through the Miris fork's USD exporter
(`src/translators/MtlxShaderWriter.cpp`).

Generated and maintained by the Improvement Agent. Each entry corresponds to
a PR that:

1. Updates the relevant row of the **Status** table below.
2. Lands a C++ change in the writer (almost always
   `MtlxShaderWriter.cpp` for surface-shader mappings).
3. Lands a validator script (Python, runnable under `hython`) that proves the
   logic against the captured corpus in `samples/complex_export.usda` (or a
   synthetic fixture) without requiring a Windows build.
4. Where the bite is a *workaround for an upstream 3ds Max bug*, it
   documents which 3ds Max version(s) the workaround targets and what
   condition retires it.

## Translation model — what the writer can actually change

`MtlxShaderWriter.cpp` is mostly a **passthrough**: it asks 3ds Max for a
MaterialX XML string via the MaxScript bridge `MtlxIOUtil.ExportMtlxString`,
parses it, and walks the resulting `MaterialX::Document` to create USD
shader prims. The MaxScript bridge lives inside 3ds Max itself and cannot be
modified from this plugin.

The C++ writer therefore has two intervention points:

1. **Pre-passthrough** (impossible — the MaxScript bridge has already
   serialized whatever it serialized).
2. **Post-parse, pre-USD-author** — after `readFromXmlString`, the
   in-memory MaterialX document can be normalized before any USD prims are
   created. This is where workarounds for buggy 3ds Max defaults live.

Status legend:

* **direct pairing** — a 1:1 mapping between a 3ds Max source property and a
  MaterialX node/input that the upstream bridge already emits correctly. No
  C++ intervention required.
* **approximating workaround** — the writer mutates the MaterialX document
  to compose a `standard_surface`-compatible result that approximates a 3ds
  Max feature the bridge does not faithfully translate.
* **bug normalization** — the writer strips or rewrites a value that the
  upstream bridge emits incorrectly (hardcoded default, off-by-one, wrong
  unit, etc.) so the resulting USD matches what a correctly-implemented
  exporter should produce.
* **no equivalent** — the source property has no MaterialX representation;
  the writer emits a `TF_WARN` so the divergence is observable.

## Status

| Source (3ds Max) | MaterialX target | Kind | Affects MaterialX nodedef | Workaround triggers when | PR | Date |
| --- | --- | --- | --- | --- | --- | --- |
| PhysicalMaterial.anisotropy_angle | `standard_surface.specular_rotation` | bug normalization | `ND_standard_surface_surfaceshader` | Bridge emits `0.25` AND `specular_anisotropy` is static-zero or absent | MAX-MAT-001 | 2026-06-20 |
| PhysicalMaterial.emission (none authored) | `standard_surface.emission` + `standard_surface.emission_color` | bug normalization | `ND_standard_surface_surfaceshader` | Bridge emits `emission = 1.0` AND `emission_color = (0, 0, 0)`, both static | MAX-MAT-002 | 2026-06-20 |
| Node.wireColor / Node.material.diffuse | `UsdGeomMesh.primvars:displayColor` | bug normalization | (mesh primvar; not a shader nodedef) | `MeshConverter` is about to author wireColor into `primvars:displayColor` AND `node->GetMtl() != nullptr` | (this PR) | 2026-06-20 |

## Notes per expression

### PhysicalMaterial.anisotropy_angle → standard_surface.specular_rotation  (MAX-MAT-001)

**Symptom.** The 3ds Max-shipped `MtlxIOUtil.ExportMtlxString` MaxScript
bridge always emits `<input name="specular_rotation" type="float"
value="0.25" />` on every `ND_standard_surface_surfaceshader`, regardless of
whether the source PhysicalMaterial has any anisotropy authored. Six out of
six materials in the diagnostic corpus exhibit this. The MaterialX
`standard_surface` nodedef defaults `specular_rotation` to **0.0**.

**Why it matters.** `specular_rotation` is multiplied by
`specular_anisotropy` inside the standard_surface BSDF, so the buggy value
has no observable lighting effect in the (universal) `anisotropy == 0`
case. It *does* matter for any downstream layer that turns anisotropy on,
or for tools that read MaterialX values directly for content authoring or
roundtripping. The wrong value silently introduces a 0.25-turns (90°)
highlight rotation the source DCC never asked for.

**Fix.** Add a post-parse normalization pass in `MtlxShaderWriter::Write()`
that walks the parsed `MaterialX::Document`, looks for `standard_surface`
nodes carrying `specular_rotation = 0.25` AND no anisotropy (absent or
static 0), and removes the `specular_rotation` input. After normalization
the input falls back to the nodedef default (`0.0`).

**Bounds (where the fix conservatively does nothing):**

* `specular_rotation` is connected to a node or nodegraph — could carry an
  intentional procedural rotation; keep it.
* `specular_rotation` has any static value other than `0.25` — the user
  authored it explicitly; keep it.
* `specular_anisotropy` is connected — runtime value unknown; assume the
  rotation may be intentional and keep it.
* `specular_anisotropy` is statically non-zero — anisotropy is on, rotation
  matters; keep it.

**Validator.**
`/Users/d.smith/MirisProjects/Agent Builder/agent/arch-builds/191c9984-d6c5-468c-a0ad-a95092dbe6d3/normalize_specular_rotation.py`
mirrors the C++ logic at the USD layer (the C++ runs at the MaterialX-doc
layer earlier in the pipeline). Running it on the captured
`complex_export.usda` strips exactly the six known-bogus values and leaves
every other attribute identical. Karma renders of the prefix and postfix
USDs are byte-identical (same SHA-256), confirming the fix is a visual
no-op for the common case — exactly what the BSDF math predicts.

**Retirement condition.** If/when Autodesk fixes `MtlxIOUtil` in a future
3ds Max release so that the bridge stops emitting the spurious 0.25, the
normalization pass becomes inert (no node matches the trigger condition).
The pass can stay in place as a belt-and-suspenders guard for older Max
installs.

### PhysicalMaterial.emission (none authored) → standard_surface.emission + .emission_color  (MAX-MAT-002)

**Symptom.** The 3ds Max-shipped `MtlxIOUtil.ExportMtlxString` MaxScript
bridge always emits the pair
```
<input name="emission"       type="float"  value="1.0" />
<input name="emission_color" type="color3" value="0, 0, 0" />
```
on every `ND_standard_surface_surfaceshader`, regardless of whether the
source PhysicalMaterial authored any emission. Six out of six materials in
the diagnostic corpus exhibit this. The MaterialX `standard_surface`
nodedef defaults are `emission = 0.0` and `emission_color = (1, 1, 1)`.

**Why it matters.** The buggy pair multiplies to `1.0 * (0,0,0) = (0,0,0)`,
so the BSDF emits no light and the bug is invisible at render time. It
*does* matter for any downstream tool that reads the exported MaterialX
and constructs further overrides:

* A layer that bumps `emission_color` to a non-black value (e.g. to author
  a glowing variant of an existing material) would unexpectedly turn on
  full-strength emission, because the inherited `emission = 1.0` is still
  in force.
* Round-trip importers that read `emission_color = (0, 0, 0)` may treat
  the surface as having explicit black emission rather than “no emission
  authored” — semantically different states that diverge under
  later edits.
* The values do not match the source DCC: the PhysicalMaterial has no
  emission knob set to 1.0, and certainly didn't ask for an emission
  *color* of pure black. The exported document misrepresents what the
  artist authored.

**Fix.** Add a second post-parse normalization pass in
`MtlxShaderWriter::Write()` (run immediately after MAX-MAT-001's pass)
that walks the parsed `MaterialX::Document` and, for each
`standard_surface` node, removes both the `emission` and `emission_color`
inputs when *all* of:

* `emission` is statically `1.0` (no connection, tolerant of float noise);
* `emission_color` is statically `(0, 0, 0)` (no connection, tolerant of
  float noise); and
* both inputs are present.

After normalization both inputs fall back to nodedef defaults
(`0.0` and `(1, 1, 1)`), which evaluate to the *same* zero emission with
the correct meaning.

**Bounds (where the fix conservatively does nothing):**

* Either `emission` or `emission_color` is connected to a node or
  nodegraph — could carry intentional procedural emission; keep both.
* `emission` is static but not `1.0` — user authored it explicitly; keep.
* `emission_color` is static but not exactly `(0, 0, 0)` — user authored
  an explicit emission tint; keep.
* Only one of the two inputs is present — the bug pattern is the pair, so
  treating either half in isolation could destroy a real authored value.

**Validator.**
`/Users/d.smith/MirisProjects/Agent Builder/agent/arch-builds/f72c97e6-301f-400a-9e77-c768979e8fa5/normalize_emission_default.py`
mirrors the C++ logic at the USD layer (the C++ runs at the MaterialX-doc
layer earlier in the pipeline). Running it on the MAX-MAT-001-clean
`complex_export_postfix.usda` strips exactly six pairs of
`emission`/`emission_color` inputs and leaves every other attribute
identical. Karma renders pre- and post-strip are byte-identical (same
SHA-256), confirming the fix is a visual no-op — exactly what the BSDF
math predicts since `1 * black == 0 * white == 0`.

**Retirement condition.** Same as MAX-MAT-001: when Autodesk fixes
`MtlxIOUtil` to stop emitting the spurious emission pair, the pass
becomes inert. Safe to keep as a guard for older 3ds Max installs.

### Node.wireColor / Node.material.diffuse → UsdGeomMesh.primvars:displayColor  (MAX-MAT-003)

**Symptom.** `MaxUsd::MeshConverter::ConvertToUSDMesh()` unconditionally
authors `primvars:displayColor` from the 3ds Max node's *wireframe color*
whenever no explicit displayColor primvar has already been written (e.g.
through the vertex-color → displayColor channel mapping). The source is
`src/MaxUsd/MeshConversion/MeshConverter.cpp` (the `if
(!usdMesh.GetDisplayColorAttr().IsAuthored()) { ... node->GetWireColor()
... }` block, post-MAX-MAT-002 around line 228).

In 3ds Max the *wireframe color* is a viewport organizational tag: a hue
assigned to a scene-graph node so the user can tell nodes apart in the
viewport. It is unrelated to the node's material. The diagnostic corpus
shows 6/6 meshes carrying a `primvars:displayColor` that disagrees with
their bound material. For example: the Teapot is bound to the Gold
material (`base_color = (0.92, 0.71, 0.24)`) but its
`primvars:displayColor` is `(0.85, 0.89, 0.68)` — a pale green-cream,
nothing like gold.

**Why it matters.** USD defines `primvars:displayColor` as the surface
color a consumer should use as a *fallback* when the bound
`UsdShadeMaterial` cannot be evaluated. PBR-capable USD renderers (Hydra
Storm, Karma, RenderMan, etc.) don't read displayColor when a material is
resolvable, so this bug is invisible there. It surfaces in *fallback*
contexts:

* Minimal Hydra delegates and scene-graph viewers that don't implement
  MaterialX.
* ARKit Quick Look paths and other thumbnail generators that consume
  USD/USDZ without a full PBR pipeline.
* The `usdview` "displayColor" overlay used to QA primvars.
* USDZ packagers that fall back to displayColor when bound shaders can't
  be inlined for distribution.

In all of those, the rendered surface color is the wireframe-derived hue
rather than the material's color — a Gold mesh that looks green-cream, a
Red Plastic mesh that looks muddy pink, a Blue Ceramic mesh that looks
olive, and so on.

**Fix.** Change the writer's single fallback line to derive the
displayColor from the bound material when one is present:

```cpp
if (!usdMesh.GetDisplayColorAttr().IsAuthored()) {
    Color displayColorSrc;
    if (Mtl* boundMtl = node->GetMtl()) {
        displayColorSrc = boundMtl->GetDiffuse();
    } else {
        displayColorSrc = Color(node->GetWireColor());
    }
    pxr::VtVec3fArray usdDisplayColor = {
        pxr::GfVec3f(displayColorSrc.r, displayColorSrc.g, displayColorSrc.b) };
    usdMesh.CreateDisplayColorAttr().Set(usdDisplayColor);
}
```

`Mtl::GetDiffuse(int mtlNum = 0, BOOL backFace = FALSE)` is the universal
Max SDK accessor for a material's "main" diffuse color. The
`LastResortUSDPreviewSurfaceWriter` already uses it to author the bound
material's `inputs:diffuseColor`, so the displayColor will be identical
to the diffuseColor the same material exports to USD. For a `MultiMtl`,
`GetDiffuse(0)` returns the first sub-material's diffuse, which is still
materially closer to the artist's intent than the viewport wireframe
color.

**Bounds (where the fix conservatively does nothing):**

* `usdMesh.GetDisplayColorAttr().IsAuthored()` is already true — the
  vertex-color → displayColor channel mapping (the
  `SetChannelPrimvarMapping 0 "displayColor"` opt-in) ran first and wrote
  the artist-authored value. We never overwrite it.
* `node->GetMtl() == nullptr` — no material bound. The wireframe color is
  the best representational color we have, so the previous behavior is
  preserved.

**Validator.**
`/Users/d.smith/MirisProjects/Agent Builder/agent/arch-builds/44966a53-f273-422c-9ce9-c1f2d7e477c1/normalize_display_color.py`
mirrors the C++ logic at the USD layer. Running it on the MAX-MAT-002-
clean corpus rewrites exactly the six mesh `primvars:displayColor`
values to match the bound material's `inputs:diffuseColor`, and leaves
every other attribute identical. PBR-path Karma renders pre- and post-
fix are byte-identical (same SHA-256) — the bound shaders are unchanged
and PBR renderers don't read displayColor. The displayColor-fallback
Karma renders (`material:binding` stripped from both copies) ARE
visually different: the wireframe-derived palette in the prefix is
replaced by the material-derived palette in the postfix, demonstrating
the observable behavior change for fallback consumers.

**Retirement condition.** Unlike MAX-MAT-001/002 this is not a workaround
for an external 3ds Max bug — the code being fixed is the fork's own
`MeshConverter::ConvertToUSDMesh`. The fix is permanent.

## Expressions with no MaterialX equivalent

| Source (3ds Max) | Why no equivalent | Behavior in current fork |
| --- | --- | --- |
| (none catalogued yet) | | |

## Change log

* 2026-06-20 — Initial doc. MAX-MAT-001 specular_rotation default
  normalization landed.
* 2026-06-20 — MAX-MAT-002 emission/emission_color default-pair
  normalization landed (strip `emission = 1.0` paired with
  `emission_color = (0, 0, 0)`).
* 2026-06-20 — MAX-MAT-003 mesh displayColor leak fixed (derive
  `primvars:displayColor` from the bound material's `GetDiffuse()`
  instead of the node's viewport wireframe color, falling back to the
  wireframe color only when no material is bound).
