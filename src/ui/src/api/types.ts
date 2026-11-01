/**
 * API types matching C++ workbench_server backend contracts.
 */

export interface HealthResponse {
  status: string;
  core_version: string;
  uptime_ms: number;
  launch_to_ready_ms: number | null;
  hardware?: HardwareInfo;
}

export interface HardwareInfo {
  cpu_brand: string;
  physical_cores: number;
  logical_cores: number;
  total_ram_bytes: number;
  os_version: string;
  compiler_name: string;
  compiler_version: string;
  build_type: string;
  optimization_flags: string;
  is_release_build: boolean;
}

export interface BatchSpecParams {
  seed: number;
  case_count: number;
  n_elements: number;
  k_min: number;
  k_max: number;
  q_min: number;
  q_max: number;
  length_m?: number;
  area_m2?: number;
  T_left_C?: number;
  T_right_C?: number;
  workers?: number;
  simulated_logical_cores?: number;
}

export interface CalibrateResult {
  probe_case_count: number;
  probe_time_ms: number;
  estimated_time_per_case_ms: number;
  suggested_case_count: number;
  recommendation: string;
}

export interface WorkerBenchmarkResult {
  worker_count: number;
  repetition_times_ms: number[];
  min_time_ms: number;
  max_time_ms: number;
  median_time_ms: number;
  speedup: number;
  efficiency: number;
  max_diff_vs_serial: number;
  speedup_assessable: boolean;
  notes: string;
}

export interface BenchmarkReport {
  hardware: HardwareInfo;
  spec: {
    seed: number;
    case_count: number;
    n_elements: number;
    k_min: number;
    k_max: number;
    q_min: number;
    q_max: number;
  };
  launch_to_ready_ms: number | null;
  non_release_build_warning: boolean;
  case_generation_time_ms: number;
  warmup_time_ms: number;
  worker_results: WorkerBenchmarkResult[];
  json_export_path?: string;
  csv_export_path?: string;
  json_data?: string;
  csv_data?: string;
}

export interface Bounds3D {
  min: [number, number, number];
  max: [number, number, number];
}

export interface StlValidationSummary {
  id: string;
  triangle_count: number;
  degenerate_count: number;
  bounds: Bounds3D;
  note: string;
  warning?: string;
}

export interface CsvValidationSummary {
  id: string;
  node_count: number;
  min_temp_C: number;
  max_temp_C: number;
  bounds: Bounds3D;
  note: string;
  warning?: string;
}

export interface ApiValidationError {
  error: string;
  field?: string;
  lineNumber?: number;
}

export interface RodProblemParams {
  length_m?: number;
  area_m2?: number;
  conductivity_W_per_mK?: number;
  heat_generation_W_per_m3?: number;
  T_left_C?: number;
  T_right_C?: number;
  n_elements?: number;
}

export type JobState = 'queued' | 'running' | 'succeeded' | 'failed' | 'cancelled';

export interface ConvergenceRow {
  n_elements: number;
  h_m: number;
  max_nodal_err_C: number;
  max_midpoint_err_C: number;
  observed_order: number | null;
}

export interface SolutionCurve {
  n_elements: number;
  h_m: number;
  x_m: number[];
  T_fe_C: number[];
  T_exact_C: number[];
}

export interface ExactDenseCurve {
  x_m: number[];
  T_exact_C: number[];
}

export interface FemJobResult {
  n_elements: number;
  max_nodal_err_C: number;
  max_midpoint_err_C: number;
  convergence_rows: number;
  convergence?: ConvergenceRow[];
  solutions?: SolutionCurve[];
  exact_curve?: ExactDenseCurve;
  solution_csv?: string;
  convergence_csv?: string;
}

export interface JobStatusResponse<T = any> {
  id: string;
  state: JobState;
  progress: number;
  result?: T;
  error?: string;
}

export interface StlBinaryMesh {
  triangleCount: number;
  positions: Float32Array; // 9 * n_tri
  normals: Float32Array;   // 3 * n_tri
}

export interface CsvBinaryPoints {
  nodeCount: number;
  xyz: Float32Array;           // 3 * n_nodes
  temperatures: Float32Array;  // 1 * n_nodes
  nodeIds: Uint32Array;        // 1 * n_nodes
}

// ── Device & Hardware Types (Phase 6) ──────────────────────────────────────────

export type DeviceState = 'disconnected' | 'connecting' | 'connected' | 'faulted' | 'unknown';

export interface DeviceInfo {
  device_id: string;
  device_type: 'mcu' | 'fpga' | 'jtag_adapter' | string;
  board_name: string;
  capabilities: string[];
  state: DeviceState;
  simulated: boolean;
}

export interface DevicesResponse {
  devices: DeviceInfo[];
  simulated: boolean;
}

export interface DeviceActionResponse {
  device_id: string;
  state?: DeviceState;
  error?: string;
  simulated: boolean;
  inject_on_nth?: number;
  note?: string;
}

export interface DeviceTelemetry {
  device_id: string;
  simulated: boolean;
  // MCU fields
  supply_voltage_V?: number;
  die_temperature_C?: number;
  // FPGA fields
  configuration_done?: boolean;
  pll_locked?: boolean;
  clock_status?: string;
  // JTAG fields
  target_vref_V?: number;
  clock_freq_hz?: number;
  scan_chain_length?: number;
  note?: string;
  error?: string;
}

export interface DeviceEventItem {
  device_id: string;
  description: string;
  new_state: DeviceState;
  simulated: boolean;
  timestamp_ms?: number;
}

export interface DeviceEventsResponse {
  events: DeviceEventItem[];
  simulated: boolean;
}

// ── Backpropagation Types (Phase 7) ──────────────────────────────────────────

export interface BackpropGradientCheckEntry {
  param_name: string;
  analytic_gradient: number;
  numerical_gradient: number;
  absolute_error: number;
  passed: boolean;
}

export interface BackpropGradientCheck {
  passed: boolean;
  failed_param: string;
  entries: BackpropGradientCheckEntry[];
}

export interface BackpropStepRecord {
  step: number;
  loss: number;
  w: number;
  b: number;
  v: number;
  c: number;
}

export interface BackpropResult {
  gradient_check: BackpropGradientCheck;
  history: BackpropStepRecord[];
  csv_data?: string;
  csv_export_path?: string;
}

export interface BackpropJobParams {
  x?: number;
  y?: number;
  w?: number;
  b?: number;
  v?: number;
  c?: number;
  learning_rate?: number;
  subsequent_steps?: number;
  epsilon?: number;
  tolerance?: number;
}



