import React, { useState, useEffect, useRef } from 'react';
import Plotly from 'plotly.js-cartesian-dist-min';
import {
  apiClient,
  WorkbenchApiError,
  extractBatchFieldError,
} from '../../api/client';
import {
  BatchSpecParams,
  BenchmarkReport,
  CalibrateResult,
  HardwareInfo,
  JobStatusResponse,
} from '../../api/types';
import { LearningDemoView } from './LearningDemoView';

interface Props {
  initialHardware?: HardwareInfo;
  initialLaunchToReadyMs?: number | null;
  onError: (err: string | null) => void;
  onJobStateChange: (state: JobStatusResponse['state'] | 'idle', progress?: number) => void;
}

export const PerformanceView: React.FC<Props> = ({
  initialHardware,
  initialLaunchToReadyMs,
  onError,
  onJobStateChange,
}) => {
  // Batch Spec form parameters (sensible defaults)
  const [params, setParams] = useState<BatchSpecParams>({
    seed: 42,
    case_count: 500,
    n_elements: 10,
    k_min: 1.0,
    k_max: 100.0,
    q_min: 1000.0,
    q_max: 50000.0,
    length_m: 1.0,
    area_m2: 0.0001,
    T_left_C: 100.0,
    T_right_C: 20.0,
  });

  const [simulateSingleCpu, setSimulateSingleCpu] = useState<boolean>(false);
  const [subTab, setSubTab] = useState<'benchmark' | 'backprop'>('benchmark');
  const [fieldErrors, setFieldErrors] = useState<Record<string, string>>({});
  const [generalError, setGeneralError] = useState<string | null>(null);

  // Active Job & Benchmark State
  const [activeJobId, setActiveJobId] = useState<string | null>(null);
  const [activeJobKind, setActiveJobKind] = useState<'benchmark' | 'calibrate' | null>(null);
  const [jobState, setJobState] = useState<'idle' | 'queued' | 'running' | 'succeeded' | 'failed' | 'cancelled'>('idle');
  const [progress, setProgress] = useState<number>(0);
  const [cancelling, setCancelling] = useState<boolean>(false);

  // Results
  const [benchmarkResult, setBenchmarkResult] = useState<BenchmarkReport | null>(null);
  const [calibrateResult, setCalibrateResult] = useState<CalibrateResult | null>(null);
  const [hardware, setHardware] = useState<HardwareInfo | undefined>(initialHardware);
  const [launchToReadyMs, setLaunchToReadyMs] = useState<number | null>(
    initialLaunchToReadyMs ?? null
  );

  // Chart DOM reference
  const chartRef = useRef<HTMLDivElement>(null);

  // Sync initial props if updated from parent
  useEffect(() => {
    if (initialHardware && !hardware) setHardware(initialHardware);
    if (initialLaunchToReadyMs !== undefined && launchToReadyMs === null) {
      setLaunchToReadyMs(initialLaunchToReadyMs);
    }
  }, [initialHardware, initialLaunchToReadyMs, hardware, launchToReadyMs]);

  // Handle form change
  const handleChange = (field: keyof BatchSpecParams, valStr: string) => {
    setFieldErrors((prev) => {
      const copy = { ...prev };
      delete copy[field as string];
      return copy;
    });

    const parsed =
      field === 'seed' || field === 'case_count' || field === 'n_elements'
        ? parseInt(valStr, 10)
        : parseFloat(valStr);

    setParams((prev) => ({
      ...prev,
      [field]: isNaN(parsed) ? undefined : parsed,
    }));
  };

  // Submit Calibration
  const handleCalibrate = async () => {
    setFieldErrors({});
    setGeneralError(null);
    onError(null);
    setCalibrateResult(null);

    const submitParams: BatchSpecParams = {
      ...params,
      simulated_logical_cores: simulateSingleCpu ? 1 : undefined,
    };

    try {
      setJobState('queued');
      setActiveJobKind('calibrate');
      setProgress(0);
      onJobStateChange('queued', 0);

      const job = await apiClient.createCalibrateJob(submitParams);
      setActiveJobId(job.id);
    } catch (err: unknown) {
      setJobState('idle');
      setActiveJobKind(null);
      onJobStateChange('idle');
      if (err instanceof WorkbenchApiError) {
        const parsed = extractBatchFieldError(err.message);
        if (parsed.field) {
          setFieldErrors({ [parsed.field]: parsed.message });
        }
        setGeneralError(err.message);
        onError(err.message);
      } else {
        const msg = err instanceof Error ? err.message : String(err);
        setGeneralError(msg);
        onError(msg);
      }
    }
  };

  // Submit Benchmark
  const handleRunBenchmark = async (e: React.FormEvent) => {
    e.preventDefault();
    setFieldErrors({});
    setGeneralError(null);
    onError(null);
    setBenchmarkResult(null);

    const submitParams: BatchSpecParams = {
      ...params,
      simulated_logical_cores: simulateSingleCpu ? 1 : undefined,
    };

    try {
      setJobState('queued');
      setActiveJobKind('benchmark');
      setProgress(0);
      onJobStateChange('queued', 0);

      const job = await apiClient.createBenchmarkJob(submitParams);
      setActiveJobId(job.id);
    } catch (err: unknown) {
      setJobState('idle');
      setActiveJobKind(null);
      onJobStateChange('idle');
      if (err instanceof WorkbenchApiError) {
        const parsed = extractBatchFieldError(err.message);
        if (parsed.field) {
          setFieldErrors({ [parsed.field]: parsed.message });
        }
        setGeneralError(err.message);
        onError(err.message);
      } else {
        const msg = err instanceof Error ? err.message : String(err);
        setGeneralError(msg);
        onError(msg);
      }
    }
  };

  // Cancel Job
  const handleCancel = async () => {
    if (!activeJobId) return;
    setCancelling(true);
    try {
      await apiClient.cancelJob(activeJobId);
      setJobState('cancelled');
      onJobStateChange('cancelled');
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      onError(`Failed to cancel job: ${msg}`);
    } finally {
      setCancelling(false);
    }
  };

  // Poll active job
  useEffect(() => {
    if (!activeJobId || jobState === 'succeeded' || jobState === 'failed' || jobState === 'cancelled') {
      return;
    }

    let isMounted = true;
    const interval = setInterval(async () => {
      try {
        const status = await apiClient.getJob(activeJobId);
        if (!isMounted) return;

        setJobState(status.state);
        setProgress(Math.round((status.progress || 0) * 100));
        onJobStateChange(status.state, status.progress);

        if (status.state === 'succeeded' && status.result) {
          if (activeJobKind === 'benchmark') {
            const report = status.result as BenchmarkReport;
            setBenchmarkResult(report);
            if (report.hardware) setHardware(report.hardware);
            if (report.launch_to_ready_ms !== undefined && report.launch_to_ready_ms !== null) {
              setLaunchToReadyMs(report.launch_to_ready_ms);
            }
          } else if (activeJobKind === 'calibrate') {
            const calib = status.result as CalibrateResult;
            setCalibrateResult(calib);
            if (calib.suggested_case_count > 0) {
              setParams((prev) => ({
                ...prev,
                case_count: calib.suggested_case_count,
              }));
            }
          }
          clearInterval(interval);
        } else if (status.state === 'failed') {
          const err = status.error || 'Job failed';
          const parsed = extractBatchFieldError(err);
          if (parsed.field) {
            setFieldErrors({ [parsed.field]: parsed.message });
          }
          setGeneralError(err);
          onError(err);
          clearInterval(interval);
        } else if (status.state === 'cancelled') {
          clearInterval(interval);
        }
      } catch (err: unknown) {
        if (!isMounted) return;
        const msg = err instanceof Error ? err.message : String(err);
        setGeneralError(msg);
        onError(msg);
      }
    }, 200);

    return () => {
      isMounted = false;
      clearInterval(interval);
    };
  }, [activeJobId, jobState, activeJobKind, onError, onJobStateChange]);

  // Render Speedup & Efficiency Chart with Plotly
  useEffect(() => {
    if (!chartRef.current || !benchmarkResult || !benchmarkResult.worker_results?.length) {
      return;
    }

    const wr = benchmarkResult.worker_results;
    const workers = wr.map((w) => w.worker_count);
    const speedups: number[] = wr.map((w) => (w.speedup_assessable ? w.speedup : NaN));
    const efficiencies: number[] = wr.map((w) => (w.speedup_assessable ? w.efficiency * 100.0 : NaN));
    const idealSpeedups = workers.map((w) => w);

    const traces: Plotly.Data[] = [
      {
        x: workers,
        y: idealSpeedups,
        name: 'Ideal Linear Speedup (p)',
        mode: 'lines',
        type: 'scatter',
        line: { color: '#64748b', width: 2, dash: 'dash' },
        hoverinfo: 'none',
      },
      {
        x: workers,
        y: speedups,
        name: 'Measured Speedup (T1 / Tp)',
        mode: 'lines+markers',
        type: 'scatter',
        line: { color: '#0ea5e9', width: 2.5 },
        marker: { size: 9, color: '#0ea5e9', symbol: 'circle' },
        yaxis: 'y',
      },
      {
        x: workers,
        y: efficiencies,
        name: 'Efficiency %',
        mode: 'lines+markers',
        type: 'scatter',
        line: { color: '#10b981', width: 2 },
        marker: { size: 7, color: '#10b981', symbol: 'diamond' },
        yaxis: 'y2',
      },
    ];

    const layout: Partial<Plotly.Layout> = {
      title: {
        text: `Solver Scaling & Efficiency — ${benchmarkResult.spec.case_count.toLocaleString()} Cases (N=${benchmarkResult.spec.n_elements})`,
        font: { family: 'IBM Plex Sans, sans-serif', size: 14, color: '#f0f2f5' },
      },
      paper_bgcolor: 'rgba(0,0,0,0)',
      plot_bgcolor: '#18191e',
      font: { family: 'IBM Plex Mono, monospace', color: '#9da3b4', size: 11 },
      margin: { l: 55, r: 60, t: 45, b: 45 },
      xaxis: {
        title: { text: 'Worker Count (p)' },
        tickvals: workers,
        tickformat: 'd',
        gridcolor: '#2e313a',
        zerolinecolor: '#3e434f',
      },
      yaxis: {
        title: { text: 'Speedup (T1 / Tp)' },
        gridcolor: '#2e313a',
        zerolinecolor: '#3e434f',
        rangemode: 'tozero',
      },
      yaxis2: {
        title: { text: 'Parallel Efficiency (%)' },
        overlaying: 'y',
        side: 'right',
        range: [0, 115],
        gridcolor: 'rgba(0,0,0,0)',
        zerolinecolor: 'rgba(0,0,0,0)',
      },
      legend: {
        orientation: 'h',
        y: -0.22,
        x: 0,
        font: { size: 10 },
      },
      autosize: true,
    };

    Plotly.newPlot(chartRef.current, traces, layout, {
      responsive: true,
      displayModeBar: false,
    });
  }, [benchmarkResult]);

  // Window resize handler for Plotly
  useEffect(() => {
    const handleResize = () => {
      if (chartRef.current) Plotly.Plots.resize(chartRef.current);
    };
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // Export handlers
  const handleExportCsv = () => {
    if (!activeJobId && !benchmarkResult?.csv_data) return;
    if (activeJobId) {
      window.open(apiClient.getBenchmarkCsvUrl(activeJobId), '_blank');
    } else if (benchmarkResult?.csv_data) {
      const blob = new Blob([benchmarkResult.csv_data], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `benchmark_${params.seed}.csv`;
      a.click();
      URL.revokeObjectURL(url);
    }
  };

  const handleExportJson = () => {
    if (!activeJobId && !benchmarkResult?.json_data) return;
    if (activeJobId) {
      window.open(apiClient.getBenchmarkJsonUrl(activeJobId), '_blank');
    } else if (benchmarkResult?.json_data) {
      const blob = new Blob([benchmarkResult.json_data], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `benchmark_${params.seed}.json`;
      a.click();
      URL.revokeObjectURL(url);
    }
  };

  const isNonRelease = hardware && (!hardware.is_release_build || hardware.build_type !== 'Release');
  const hasSingleCpuNote = benchmarkResult?.worker_results?.some((w) => !w.speedup_assessable);

  return (
    <div className="flex-1 flex flex-col h-full overflow-hidden bg-canvas">
      {/* Sub-tab Navigation */}
      <div className="bg-panel border-b border-border-subtle px-4 py-2 flex items-center gap-2 flex-shrink-0">
        <button
          type="button"
          id="tab-sub-benchmark"
          onClick={() => setSubTab('benchmark')}
          className={`px-3 py-1 text-xs font-mono font-medium rounded transition-all ${
            subTab === 'benchmark'
              ? 'bg-accent text-accent-fg font-semibold shadow-sm'
              : 'text-text-secondary hover:text-text-primary hover:bg-subtle'
          }`}
        >
          Worker Pool Benchmark
        </button>
        <button
          type="button"
          id="tab-sub-backprop"
          onClick={() => setSubTab('backprop')}
          className={`flex items-center gap-1.5 px-3 py-1 text-xs font-mono font-medium rounded transition-all ${
            subTab === 'backprop'
              ? 'bg-accent text-accent-fg font-semibold shadow-sm'
              : 'text-text-secondary hover:text-text-primary hover:bg-subtle'
          }`}
        >
          <span>Learning Demo (Backprop)</span>
          <span className={`text-[10px] px-1 py-0.2 rounded font-mono ${
            subTab === 'backprop'
              ? 'bg-panel/40 text-accent-fg border border-accent/40'
              : 'bg-subtle text-text-secondary border border-border-default'
          }`}>
            Phase 7
          </span>
        </button>
      </div>

      {subTab === 'backprop' ? (
        <LearningDemoView onError={onError} onJobStateChange={onJobStateChange} />
      ) : (
        <div className="flex-1 flex flex-col lg:flex-row h-full overflow-hidden bg-canvas">
          {/* Left Form Controls Sidebar */}
          <aside className="w-full lg:w-80 xl:w-96 bg-panel border-r border-border-subtle flex flex-col p-4 gap-4 overflow-y-auto">
        <div>
          <h2 className="text-xs font-semibold text-text-secondary uppercase tracking-wider font-mono mb-1">
            Batch & Benchmark Spec
          </h2>
          <p className="text-[11px] text-text-muted">
            1D Rod Batch Solver: Deterministic pseudo-random generation &amp; worker pool throughput.
          </p>
        </div>

        {generalError && (
          <div
            role="alert"
            className="bg-status-error-bg border border-status-error/40 text-status-error p-3 rounded-md text-xs font-mono"
          >
            <div className="font-semibold mb-0.5">Validation Error</div>
            <p className="text-[11px] text-text-primary">{generalError}</p>
          </div>
        )}

        <form onSubmit={handleRunBenchmark} className="flex flex-col gap-3 text-xs font-mono">
          {/* Seed */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="seed" className="text-text-secondary">PRNG Seed (uint64):</label>
              {fieldErrors.seed && (
                <span className="text-[10px] text-status-error">{fieldErrors.seed}</span>
              )}
            </div>
            <input
              id="seed"
              type="number"
              min="0"
              step="1"
              value={params.seed ?? ''}
              onChange={(e) => handleChange('seed', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.seed ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Case Count */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="case_count" className="text-text-secondary">Case Count:</label>
              {fieldErrors.case_count && (
                <span className="text-[10px] text-status-error">{fieldErrors.case_count}</span>
              )}
            </div>
            <input
              id="case_count"
              type="number"
              min="1"
              step="1"
              value={params.case_count ?? ''}
              onChange={(e) => handleChange('case_count', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.case_count ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Mesh Size N */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="n_elements" className="text-text-secondary">Elements per Rod (N):</label>
              {fieldErrors.n_elements && (
                <span className="text-[10px] text-status-error">{fieldErrors.n_elements}</span>
              )}
            </div>
            <input
              id="n_elements"
              type="number"
              min="1"
              step="1"
              value={params.n_elements ?? ''}
              onChange={(e) => handleChange('n_elements', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.n_elements ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Conductivity Range k */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <div className="flex justify-between items-center mb-1">
                <label htmlFor="k_min" className="text-text-secondary">k Min:</label>
                {fieldErrors.k_min && (
                  <span className="text-[10px] text-status-error">{fieldErrors.k_min}</span>
                )}
              </div>
              <input
                id="k_min"
                type="number"
                step="any"
                value={params.k_min ?? ''}
                onChange={(e) => handleChange('k_min', e.target.value)}
                className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                  fieldErrors.k_min ? 'border-status-error' : 'border-border-default focus:border-accent'
                }`}
              />
            </div>
            <div>
              <div className="flex justify-between items-center mb-1">
                <label htmlFor="k_max" className="text-text-secondary">k Max:</label>
                {fieldErrors.k_max && (
                  <span className="text-[10px] text-status-error">{fieldErrors.k_max}</span>
                )}
              </div>
              <input
                id="k_max"
                type="number"
                step="any"
                value={params.k_max ?? ''}
                onChange={(e) => handleChange('k_max', e.target.value)}
                className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                  fieldErrors.k_max ? 'border-status-error' : 'border-border-default focus:border-accent'
                }`}
              />
            </div>
          </div>

          {/* Heat Source Range q */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <div className="flex justify-between items-center mb-1">
                <label htmlFor="q_min" className="text-text-secondary">q Min:</label>
                {fieldErrors.q_min && (
                  <span className="text-[10px] text-status-error">{fieldErrors.q_min}</span>
                )}
              </div>
              <input
                id="q_min"
                type="number"
                step="any"
                value={params.q_min ?? ''}
                onChange={(e) => handleChange('q_min', e.target.value)}
                className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                  fieldErrors.q_min ? 'border-status-error' : 'border-border-default focus:border-accent'
                }`}
              />
            </div>
            <div>
              <div className="flex justify-between items-center mb-1">
                <label htmlFor="q_max" className="text-text-secondary">q Max:</label>
                {fieldErrors.q_max && (
                  <span className="text-[10px] text-status-error">{fieldErrors.q_max}</span>
                )}
              </div>
              <input
                id="q_max"
                type="number"
                step="any"
                value={params.q_max ?? ''}
                onChange={(e) => handleChange('q_max', e.target.value)}
                className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                  fieldErrors.q_max ? 'border-status-error' : 'border-border-default focus:border-accent'
                }`}
              />
            </div>
          </div>

          {/* Worker Counts Protocol Notice */}
          <div className="p-2.5 bg-canvas/70 border border-border-subtle rounded text-[11px] text-text-muted">
            <span className="text-text-secondary font-semibold">Worker Scaling Protocol:</span>{' '}
            Evaluates <code className="text-accent">1, 2, 4</code> workers, automatically capped at host
            logical core count ({hardware ? hardware.logical_cores : 'auto'}). 1 warm-up + 5 measured reps per count.
          </div>

          {/* Single-CPU Simulator Toggle for Testing */}
          <div className="flex items-center justify-between p-2 bg-canvas/40 border border-border-subtle rounded">
            <label htmlFor="sim_cpu" className="text-[11px] text-text-secondary cursor-pointer">
              Simulate 1 Logical CPU Path:
            </label>
            <input
              id="sim_cpu"
              type="checkbox"
              checked={simulateSingleCpu}
              onChange={(e) => setSimulateSingleCpu(e.target.checked)}
              className="accent-accent cursor-pointer"
            />
          </div>

          {/* Calibration Banner if Calibrated */}
          {calibrateResult && (
            <div className="p-2.5 bg-accent/10 border border-accent/40 rounded text-[11px] text-text-primary">
              <div className="font-semibold text-accent mb-0.5">Calibration Advisory</div>
              <div>{calibrateResult.recommendation}</div>
              <div className="text-[10px] text-text-muted mt-1 font-mono">
                Probe: {calibrateResult.probe_case_count} cases in {calibrateResult.probe_time_ms.toFixed(2)} ms
                ({calibrateResult.estimated_time_per_case_ms.toFixed(4)} ms/case)
              </div>
            </div>
          )}

          {/* Action Buttons: Calibrate & Run Benchmark */}
          <div className="flex flex-col gap-2 pt-2 border-t border-border-subtle">
            <button
              type="button"
              id="btn-calibrate"
              onClick={handleCalibrate}
              disabled={jobState === 'running' || jobState === 'queued'}
              className="w-full py-1.5 px-3 bg-surface border border-border-default hover:bg-hover hover:border-accent text-text-primary font-medium rounded transition-colors disabled:opacity-50"
            >
              {activeJobKind === 'calibrate' && (jobState === 'running' || jobState === 'queued')
                ? 'Calibrating (~0.5s probe)...'
                : 'Calibrate Workload'}
            </button>

            {jobState === 'running' || jobState === 'queued' ? (
              <div className="flex flex-col gap-2">
                <div className="flex justify-between text-[11px] text-text-secondary font-mono">
                  <span>Running {activeJobKind}...</span>
                  <span>{progress}%</span>
                </div>
                <div className="w-full bg-input h-2 rounded overflow-hidden">
                  <div
                    className="bg-accent h-full transition-all duration-150"
                    style={{ width: `${progress}%` }}
                  />
                </div>
                <button
                  type="button"
                  id="btn-cancel"
                  onClick={handleCancel}
                  disabled={cancelling}
                  className="w-full py-1.5 px-3 bg-status-error/10 border border-status-error hover:bg-status-error hover:text-white text-status-error font-medium rounded transition-colors"
                >
                  {cancelling ? 'Cancelling...' : 'Cancel'}
                </button>
              </div>
            ) : (
              <button
                type="submit"
                id="btn-run-benchmark"
                className="w-full py-2 px-3 bg-accent hover:bg-accent-hover text-accent-fg font-semibold rounded transition-colors shadow-sm"
              >
                Run Benchmark
              </button>
            )}
          </div>
        </form>
      </aside>

      {/* Main Content Area: Hardware, Charts, Results Table & Caveats */}
      <main className="flex-1 flex flex-col overflow-y-auto p-4 gap-4 bg-canvas">
        {/* Top Header Card: Hardware Topology & Launch-to-Ready */}
        <section aria-labelledby="hw-heading" className="bg-panel border border-border-subtle rounded-lg p-4 shadow-sm">
          <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-2 mb-3 pb-2 border-b border-border-subtle">
            <div>
              <h2 id="hw-heading" className="text-sm font-semibold text-text-primary font-sans flex items-center gap-2">
                <span>Hardware &amp; System Configuration</span>
                {hardware?.is_release_build ? (
                  <span className="text-[10px] px-1.5 py-0.5 rounded bg-status-success-bg text-status-success font-mono font-normal">
                    Release Build
                  </span>
                ) : (
                  <span className="text-[10px] px-1.5 py-0.5 rounded bg-status-warning-bg text-status-warning font-mono font-normal">
                    {hardware?.build_type || 'Debug'} Build
                  </span>
                )}
              </h2>
              <p className="text-[11px] text-text-muted">
                System topology queried via Win32 hardware APIs and compile-time reflection.
              </p>
            </div>

            {/* Launch to Ready Metric (Shown Separately) */}
            <div
              id="card-launch-to-ready"
              className="bg-canvas border border-border-default px-3 py-1.5 rounded flex items-center gap-2.5 self-stretch md:self-auto"
            >
              <div className="text-[10px] uppercase tracking-wider text-text-muted font-mono">
                Launch to Ready:
              </div>
              <div className="text-xs font-mono font-bold text-accent">
                {launchToReadyMs !== null ? `${launchToReadyMs} ms` : '—'}
              </div>
            </div>
          </div>

          {/* Visible Warning when non-Release */}
          {isNonRelease && (
            <div
              role="alert"
              className="mb-3 p-3 bg-status-warning-bg border border-status-warning/40 text-status-warning rounded-md text-xs font-mono flex items-start gap-2"
            >
              <span className="text-base leading-none">⚠️</span>
              <div>
                <span className="font-semibold">Non-Release Build Warning:</span> Target was built under{' '}
                <code className="text-text-primary font-bold">{hardware.build_type}</code> mode. Compiler
                optimizations are reduced or absent. Speedup and throughput results reflect debug overhead rather than
                maximum physical performance. Rebuild with <code className="text-text-primary">CMAKE_BUILD_TYPE=Release</code>.
              </div>
            </div>
          )}

          {/* Hardware Specs Grid */}
          {hardware ? (
            <div className="grid grid-cols-2 md:grid-cols-4 gap-3 text-xs font-mono">
              <div className="bg-canvas/50 p-2.5 rounded border border-border-subtle">
                <span className="text-[10px] text-text-muted uppercase block">Processor</span>
                <span className="text-text-primary font-medium truncate block" title={hardware.cpu_brand}>
                  {hardware.cpu_brand || 'x86_64 Processor'}
                </span>
              </div>
              <div className="bg-canvas/50 p-2.5 rounded border border-border-subtle">
                <span className="text-[10px] text-text-muted uppercase block">Cores (Phys / Log)</span>
                <span className="text-text-primary font-medium">
                  {hardware.physical_cores} Physical / {hardware.logical_cores} Logical
                </span>
              </div>
              <div className="bg-canvas/50 p-2.5 rounded border border-border-subtle">
                <span className="text-[10px] text-text-muted uppercase block">System Memory</span>
                <span className="text-text-primary font-medium">
                  {(hardware.total_ram_bytes / 1073741824).toFixed(1)} GB RAM
                </span>
              </div>
              <div className="bg-canvas/50 p-2.5 rounded border border-border-subtle">
                <span className="text-[10px] text-text-muted uppercase block">Compiler &amp; Flags</span>
                <span className="text-text-primary font-medium truncate block" title={`${hardware.compiler_name} ${hardware.compiler_version} (${hardware.optimization_flags})`}>
                  {hardware.compiler_name} ({hardware.optimization_flags || hardware.build_type})
                </span>
              </div>
            </div>
          ) : (
            <div className="text-xs text-text-muted font-mono italic">
              Loading hardware topology from server...
            </div>
          )}
        </section>

        {/* Results Area or Empty State */}
        {!benchmarkResult ? (
          /* Empty State — No Fabricated Numbers */
          <div
            id="empty-benchmark-state"
            className="flex-1 flex flex-col items-center justify-center p-12 bg-panel border border-border-subtle rounded-lg text-center"
          >
            <div className="w-12 h-12 mb-3 rounded-full bg-surface border border-border-default flex items-center justify-center text-text-muted text-xl">
              ⚡
            </div>
            <h3 className="text-sm font-semibold text-text-primary font-sans mb-1">
              No Benchmark Executed Yet
            </h3>
            <p className="text-xs text-text-muted max-w-md font-mono mb-4">
              Configure the batch parameters in the left panel and click &ldquo;Run Benchmark&rdquo; to measure
              multi-threaded solver throughput, scaling efficiency, and numerical agreement.
            </p>
            <div className="text-[11px] text-text-secondary bg-canvas/60 px-3 py-1.5 rounded border border-border-subtle font-mono">
              Tip: Click &ldquo;Calibrate Workload&rdquo; first to discover the optimal case count for &ge; 500 ms measurement.
            </div>
          </div>
        ) : (
          /* Populated Benchmark Results */
          <div className="flex flex-col gap-4">
            {/* Action Bar: Workload Metadata & Exports */}
            <div className="flex flex-wrap items-center justify-between gap-2 p-3 bg-panel border border-border-subtle rounded-lg">
              <div className="flex items-center gap-4 text-xs font-mono text-text-secondary">
                <div>
                  <span className="text-text-muted">Workload:</span>{' '}
                  <span className="text-text-primary font-semibold">
                    {benchmarkResult.spec.case_count.toLocaleString()} cases
                  </span>{' '}
                  (N={benchmarkResult.spec.n_elements})
                </div>
                <div>
                  <span className="text-text-muted">Warmup:</span>{' '}
                  <span className="text-text-primary">{benchmarkResult.warmup_time_ms.toFixed(1)} ms</span>
                </div>
                <div>
                  <span className="text-text-muted">PRNG Seed:</span>{' '}
                  <span className="text-text-primary">{benchmarkResult.spec.seed}</span>
                </div>
              </div>

              <div className="flex items-center gap-2">
                <button
                  type="button"
                  id="btn-export-csv"
                  onClick={handleExportCsv}
                  className="px-3 py-1.5 bg-surface border border-border-default hover:bg-hover text-text-primary text-xs font-mono rounded transition-colors"
                >
                  Export CSV
                </button>
                <button
                  type="button"
                  id="btn-export-json"
                  onClick={handleExportJson}
                  className="px-3 py-1.5 bg-surface border border-border-default hover:bg-hover text-text-primary text-xs font-mono rounded transition-colors"
                >
                  Export JSON
                </button>
              </div>
            </div>

            {/* Speedup and Efficiency Plotly Chart */}
            <div className="bg-panel border border-border-subtle rounded-lg p-3">
              <div ref={chartRef} id="speedup-plot" className="w-full h-72" />
            </div>

            {/* Results Table (All 5 raw reps, median, speedup, efficiency, max |dT|) */}
            <div className="bg-panel border border-border-subtle rounded-lg p-4 shadow-sm overflow-x-auto">
              <h3 className="text-xs font-semibold text-text-secondary uppercase tracking-wider font-mono mb-2">
                Worker Scaling &amp; Repetitions Table
              </h3>
              <table
                id="benchmark-results-table"
                className="w-full text-left border-collapse text-xs font-mono"
              >
                <thead>
                  <tr className="border-b border-border-default text-text-muted text-[11px]">
                    <th className="py-2 px-2.5">Workers (p)</th>
                    <th className="py-2 px-2">Rep 1 (ms)</th>
                    <th className="py-2 px-2">Rep 2 (ms)</th>
                    <th className="py-2 px-2">Rep 3 (ms)</th>
                    <th className="py-2 px-2">Rep 4 (ms)</th>
                    <th className="py-2 px-2">Rep 5 (ms)</th>
                    <th className="py-2 px-2.5 font-bold text-text-primary">Median (ms)</th>
                    <th className="py-2 px-2.5">Speedup</th>
                    <th className="py-2 px-2.5">Efficiency</th>
                    <th className="py-2 px-2.5">Max |ΔT| vs Serial</th>
                    <th className="py-2 px-2.5">Status</th>
                  </tr>
                </thead>
                <tbody>
                  {benchmarkResult.worker_results.map((row) => {
                    const isAgreementPass = row.max_diff_vs_serial <= 1e-8;
                    return (
                      <tr
                        key={row.worker_count}
                        className="border-b border-border-subtle hover:bg-hover/50 transition-colors"
                      >
                        <td className="py-2 px-2.5 font-bold text-text-primary">{row.worker_count}</td>
                        {row.repetition_times_ms.map((t, idx) => (
                          <td key={idx} className="py-2 px-2 text-text-secondary">
                            {t.toFixed(2)}
                          </td>
                        ))}
                        <td className="py-2 px-2.5 font-bold text-accent">{row.median_time_ms.toFixed(2)}</td>
                        <td className="py-2 px-2.5 font-semibold text-text-primary">
                          {row.speedup_assessable ? `${row.speedup.toFixed(2)}x` : 'N/A'}
                        </td>
                        <td className="py-2 px-2.5 text-text-secondary">
                          {row.speedup_assessable ? `${(row.efficiency * 100).toFixed(1)}%` : 'N/A'}
                        </td>
                        <td className="py-2 px-2.5">
                          <span
                            className={
                              isAgreementPass
                                ? 'text-status-success font-semibold'
                                : 'text-status-error font-semibold'
                            }
                          >
                            {row.max_diff_vs_serial.toExponential(2)} °C
                          </span>
                        </td>
                        <td className="py-2 px-2.5">
                          {isAgreementPass ? (
                            <span className="px-1.5 py-0.5 rounded text-[10px] bg-status-success-bg text-status-success font-semibold">
                              PASS
                            </span>
                          ) : (
                            <span className="px-1.5 py-0.5 rounded text-[10px] bg-status-error-bg text-status-error font-semibold">
                              FAIL
                            </span>
                          )}
                          {row.notes && (
                            <span className="text-[10px] text-text-muted block truncate max-w-xs" title={row.notes}>
                              {row.notes}
                            </span>
                          )}
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>

            {/* Honest Notes & Caveats Panel */}
            <div
              id="panel-honest-notes"
              className="bg-panel border border-border-subtle rounded-lg p-4 text-xs font-mono text-text-secondary"
            >
              <h3 className="font-semibold text-text-primary uppercase tracking-wider text-[11px] mb-2 flex items-center gap-1.5">
                <span>🛡️</span>
                <span>Honest Engineering Notes &amp; Scaling Caveats</span>
              </h3>

              {hasSingleCpuNote && (
                <div className="mb-3 p-2.5 bg-accent/10 border border-accent/40 rounded text-text-primary">
                  <div className="font-semibold text-accent mb-0.5">Single-CPU Disclosure:</div>
                  <p className="text-[11px] text-text-muted">
                    This host (or simulation) has 1 logical CPU core. The multi-threaded pipeline was exercised
                    with 2 workers to rigorously verify thread safety and race-free execution, but speedup is marked
                    as physically non-assessable.
                  </p>
                </div>
              )}

              <ul className="list-disc list-inside space-y-1.5 text-[11px] text-text-muted leading-relaxed">
                <li>
                  <strong className="text-text-secondary">Thread Lifecycle Inclusion:</strong> Measurement tightly
                  bounds compute via <code className="text-text-primary">std::chrono::steady_clock</code> but
                  intentionally includes thread spawning and joining latency to capture true end-to-end task throughput.
                </li>
                <li>
                  <strong className="text-text-secondary">Tiny Solves vs. Cache Locality:</strong> Each 1D rod solve
                  is O(N) with N in [10, 80]. The tridiagonal system fits completely within L1 data cache.
                  Speedup is constrained by thread synchronization overhead rather than raw floating-point bandwidth.
                </li>
                <li>
                  <strong className="text-text-secondary">Simultaneous Multithreading (SMT):</strong> Logical sibling cores
                  share execution ALUs and vector units. Hyper-threading scaling typically provides 20%–35% boost rather
                  than 100% linear doubling.
                </li>
                <li>
                  <strong className="text-text-secondary">Memory Bus Saturation:</strong> Concurrent writes from 4+ workers
                  into preallocated solution vectors stress shared memory bus bandwidth on large batch sizes.
                </li>
                <li>
                  <strong className="text-text-secondary">Thermal Throttling &amp; Turbo Boost:</strong> Heavy multi-core
                  utilization triggers package TDP limits, downclocking all-core turbo frequencies compared to peak
                  single-core boost.
                </li>
                <li>
                  <strong className="text-text-secondary">Scheduler Jitter:</strong> Desktop OS context switches add
                  noise to individual repetitions; taking the median of 5 runs filters transient preemption spikes.
                </li>
              </ul>
            </div>
          </div>
        )}
      </main>
    </div>
  )}
</div>
);
};
