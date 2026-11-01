# API Reference

All routes are loopback-only (127.0.0.1). Every /api/* request requires the
session cookie wb_session=<token> (set by the token-exchange flow) unless the
server was started with --dev. The Host header must equal 127.0.0.1:<port>.

## Health

GET /api/health
Response: { status, core_version, uptime_ms, launch_to_ready_ms }

## File upload and validation

POST /api/files/stl
  Body: raw ASCII STL bytes (Content-Type: application/octet-stream)
  Success 201: { id, triangle_count, degenerate_count, bounds, note, warning? }
  Error   422: { error, field }

POST /api/files/csv
  Body: raw CSV bytes. Header must be: node_id,x_m,y_m,z_m,temperature_C
  Success 201: { id, node_count, min_temp_C, max_temp_C, bounds, note, warning? }
  Error   422: { error, field }

## Binary data download

GET /api/files/{id}/stl.bin
  Returns little-endian binary payload:
    [n_tri : uint32]
    [v0x v0y v0z  v1x v1y v1z  v2x v2y v2z : float32 * 9 * n_tri]  (positions)
    [nx  ny  nz : float32 * 3 * n_tri]                               (recomputed face normals)
  Total: 4 + 48 * n_tri bytes

GET /api/files/{id}/points.bin
  Returns little-endian binary payload:
    [n_nodes : uint32]
    [x y z : float32 * 3 * n_nodes]   (coordinates in metres)
    [temp   : float32 * n_nodes]       (temperature in Celsius)
    [id     : uint32  * n_nodes]       (node IDs)
  Total: 4 + 20 * n_nodes bytes

## FEM jobs

POST /api/jobs
  Body: { kind: "fem", params?: { length_m, area_m2, conductivity_W_per_mK,
          heat_generation_W_per_m3, T_left_C, T_right_C, n_elements } }
  Defaults: assignment values. All params optional.
  Success 202: { id, state: "queued" }
  Error   422: { error }  (field-level validation message)

GET /api/jobs/{id}
  Returns: { id, state, progress, result?, error? }
  States: queued | running | succeeded | failed | cancelled

DELETE /api/jobs/{id}
  Cooperative cancel. Returns: { cancelled: true }

GET /api/jobs/{id}/export/solution.csv
GET /api/jobs/{id}/export/convergence.csv
  Only available when state == succeeded.

## Size caps

| Resource      | Cap              |
|---------------|------------------|
| Upload size   | 64 MiB           |
| STL triangles | 2,000,000        |
| CSV nodes     | 1,000,000        |
| Stored files  | 20 (LRU evict)   |
| Concurrent jobs | 2 (configurable) |