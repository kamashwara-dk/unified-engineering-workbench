# data/ -- Sample data provenance

All files in this directory are **synthetic** and computer-generated. None are
exported from ANSYS or any other commercial CAD/FEA tool.

## Files

### synthetic_lbracket.stl

- **Creator:** tools/gen_samples.cpp (workbench project, Phase 3)
- **Format:** ASCII STL
- **Content:** An L-bracket shape (two rectangular prisms) with 32 triangles
- **Units:** Metres (STL is unitless; metres assumed by convention here)
- **How generated:** Procedural quad-to-triangle expansion of box faces

### synthetic_temperature.csv

- **Creator:** tools/gen_samples.cpp (workbench project, Phase 3)
- **Format:** CSV with header node_id,x_m,y_m,z_m,temperature_C
- **Content:** SYNTHETIC temperature field (Gaussian blob) over a 20x30x5 grid (3000 nodes)
- **Units:** Coordinates in metres, temperature in Celsius
- **Schema:** node_id (non-negative integer), x_m (float), y_m (float), z_m (float), temperature_C (float)
- **NOTE:** This CSV is SYNTHETIC and NOT an ANSYS export. Values are computed
  as T(x,y,z) = 100 * exp(-((x-0.5)^2 + (y-1.5)^2 + z^2))

## Integrity

After generation, both files were validated with parse_stl()/parse_csv() and
reported no errors. Run gen_samples to regenerate them.