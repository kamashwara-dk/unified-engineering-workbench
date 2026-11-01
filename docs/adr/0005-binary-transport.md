# ADR 0005: Binary typed-array transport for meshes and point clouds

**Status:** Accepted
**Date:** 2026-11-01

## Context
STL meshes and nodal point clouds can be large (millions of triangles/nodes).
JSON encoding of floating-point arrays is ~3x larger than binary and slower to
parse in JavaScript.

## Decision
Serve geometry data as raw little-endian binary payloads over dedicated endpoints
(/api/files/{id}/stl.bin and /api/files/{id}/points.bin). The layout is documented
in docs/API.md. The UI reads these using TypedArray views (Float32Array, Uint32Array)
directly, which Three.js and Plotly accept natively.

A small JSON summary (triangle count, bounding box, etc.) is returned by the upload
endpoint so the UI knows what it's dealing with before downloading binary data.

## Consequences
- **Positive:** ~3x smaller payloads; no JSON parse overhead for geometry.
- **Positive:** TypedArray views are zero-copy in modern browsers.
- **Negative:** Binary layout is opaque; documentation in API.md is the contract.
- **Negative:** Debugging requires a hex dump tool.