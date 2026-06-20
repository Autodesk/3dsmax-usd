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
| PhysicalMaterial.anisotropy_angle | `standard_surface.specular_rotation` | bug normalization | `ND_standard_surface_surfaceshader` | Bridge emits `0.25` AND `specular_anisotropy` is static-zero or absent | (this PR) | 2026-06-20 |

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

## Expressions with no MaterialX equivalent

| Source (3ds Max) | Why no equivalent | Behavior in current fork |
| --- | --- | --- |
| (none catalogued yet) | | |

## Change log

* 2026-06-20 — Initial doc. MAX-MAT-001 specular_rotation default
  normalization landed.
