import React, { useState, useEffect, useRef } from 'react';
import Plotly from 'plotly.js-cartesian-dist-min';
import {
  apiClient,
  WorkbenchApiError,
} from '../../api/client';
import {
  RodProblemParams,
  JobStatusResponse,
  FemJobResult,
} from '../../api/types';

interface Props {
  onError: (err: string | null) => void;
  onJobStateChange: (state: JobStatusResponse['state'] | 'idle', progress?: number) => void;
}

export const ThermalView: React.FC<Props> = ({ onError, onJobStateChange }) => {
  // Parameter form with assignment values as defaults:
  // L=1.0, A=1e-4, k=10.0, q=1e4, T0=100.0, TL=20.0, N=10
  const [params, setParams] = useState<RodProblemParams>({
    length_m: 1.0,
    area_m2: 0.0001,
    conductivity_W_per_mK: 10.0,
    heat_generation_W_per_m3: 10000.0,
    T_left_C: 100.0,
    T_right_C: 20.0,
    n_elements: 10,
  });

  const [fieldErrors, setFieldErrors] = useState<Record<string, string>>({});
  const [generalError, setGeneralError] = useState<string | null>(null);

  // Job management
  const [activeJobId, setActiveJobId] = useState<string | null>(null);
  const [jobState, setJobState] = useState<'idle' | 'queued' | 'running' | 'succeeded' | 'failed' | 'cancelled'>('idle');
  const [progress, setProgress] = useState<number>(0);
  const [jobResult, setJobResult] = useState<FemJobResult | null>(null);
  const [cancelling, setCancelling] = useState<boolean>(false);

  // Plot DOM refs
  const tempPlotRef = useRef<HTMLDivElement>(null);
  const convPlotRef = useRef<HTMLDivElement>(null);

  // Handle input change
  const handleChange = (field: keyof RodProblemParams, valStr: string) => {
    setFieldErrors((prev) => {
      const copy = { ...prev };
      delete copy[field];
      return copy;
    });

    const parsed = field === 'n_elements' ? parseInt(valStr, 10) : parseFloat(valStr);
    setParams((prev) => ({
      ...prev,
      [field]: isNaN(parsed) ? undefined : parsed,
    }));
  };

  // Submit Job
  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setFieldErrors({});
    setGeneralError(null);
    onError(null);
    setJobResult(null);

    try {
      setJobState('queued');
      setProgress(0);
      onJobStateChange('queued', 0);

      const job = await apiClient.createFemJob(params);
      setActiveJobId(job.id);
    } catch (err: unknown) {
      setJobState('idle');
      onJobStateChange('idle');
      if (err instanceof WorkbenchApiError) {
        if (err.field) {
          setFieldErrors({ [err.field]: err.message });
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
        setProgress(status.progress);
        onJobStateChange(status.state, status.progress);

        if (status.state === 'succeeded') {
          if (status.result) {
            setJobResult(status.result);
          }
          clearInterval(interval);
        } else if (status.state === 'failed' || status.state === 'cancelled') {
          if (status.error) {
            setGeneralError(status.error);
            onError(status.error);
          }
          clearInterval(interval);
        }
      } catch (err: unknown) {
        const msg = err instanceof Error ? err.message : String(err);
        console.error('Job polling error:', msg);
      }
    }, 250);

    return () => {
      isMounted = false;
      clearInterval(interval);
    };
  }, [activeJobId, jobState, onError, onJobStateChange]);

  // Render Plots when jobResult is updated
  useEffect(() => {
    if (!jobResult) return;

    // 1. Temperature vs x plot
    if (tempPlotRef.current && jobResult.solutions) {
      const traces: Plotly.Data[] = [];

      // Add solution traces for each N
      const colors = ['#f59e0b', '#10b981', '#06b6d4', '#3b82f6'];
      jobResult.solutions.forEach((sol, i) => {
        traces.push({
          x: sol.x_m,
          y: sol.T_fe_C,
          name: `FEA N=${sol.n_elements}`,
          mode: 'lines+markers',
          type: 'scatter',
          line: { color: colors[i % colors.length], width: 2 },
          marker: { size: 6 },
        });
      });

      // Add exact dense curve
      if (jobResult.exact_curve) {
        traces.push({
          x: jobResult.exact_curve.x_m,
          y: jobResult.exact_curve.T_exact_C,
          name: 'Exact Analytical',
          mode: 'lines',
          type: 'scatter',
          line: { color: '#f43f5e', width: 2, dash: 'dot' },
        });
      }

      const layout: Partial<Plotly.Layout> = {
        title: {
          text: '1D Thermal FEA: Temperature vs Position',
          font: { family: 'IBM Plex Sans, sans-serif', size: 14, color: '#f0f2f5' },
        },
        paper_bgcolor: 'rgba(0,0,0,0)',
        plot_bgcolor: '#18191e',
        font: { family: 'IBM Plex Mono, monospace', color: '#9da3b4', size: 11 },
        margin: { l: 55, r: 25, t: 45, b: 45 },
        xaxis: {
          title: { text: 'Position x (m)' },
          gridcolor: '#2e313a',
          zerolinecolor: '#3e434f',
        },
        yaxis: {
          title: { text: 'Temperature T (°C)' },
          gridcolor: '#2e313a',
          zerolinecolor: '#3e434f',
        },
        legend: {
          orientation: 'h',
          y: -0.2,
          x: 0,
          font: { size: 10 },
        },
        autosize: true,
      };

      Plotly.newPlot(tempPlotRef.current, traces, layout, {
        responsive: true,
        displayModeBar: false,
      });
    }

    // 2. Convergence plot (log-log)
    if (convPlotRef.current && jobResult.convergence) {
      const h_vals = jobResult.convergence.map((r) => r.h_m);
      const mid_errs = jobResult.convergence.map((r) => r.max_midpoint_err_C);
      const nod_errs = jobResult.convergence.map((r) => Math.max(r.max_nodal_err_C, 1e-15));

      const observedOrderAvg = jobResult.convergence
        .filter((r) => r.observed_order !== null && !isNaN(r.observed_order!))
        .map((r) => r.observed_order!)
        .pop() || 2.0;

      const traces: Plotly.Data[] = [
        {
          x: h_vals,
          y: mid_errs,
          name: 'Midpoint Error',
          mode: 'lines+markers',
          type: 'scatter',
          line: { color: '#f59e0b', width: 2.5 },
          marker: { size: 8, symbol: 'circle' },
        },
        {
          x: h_vals,
          y: nod_errs,
          name: 'Nodal Error',
          mode: 'lines+markers',
          type: 'scatter',
          line: { color: '#38bdf8', width: 2, dash: 'dash' },
          marker: { size: 7, symbol: 'diamond' },
        },
      ];

      const layout: Partial<Plotly.Layout> = {
        title: {
          text: `Mesh Convergence (Log-Log) — Order: ~${observedOrderAvg.toFixed(2)}`,
          font: { family: 'IBM Plex Sans, sans-serif', size: 14, color: '#f0f2f5' },
        },
        paper_bgcolor: 'rgba(0,0,0,0)',
        plot_bgcolor: '#18191e',
        font: { family: 'IBM Plex Mono, monospace', color: '#9da3b4', size: 11 },
        margin: { l: 65, r: 25, t: 45, b: 45 },
        xaxis: {
          title: { text: 'Element size h (m)' },
          type: 'log',
          gridcolor: '#2e313a',
          zerolinecolor: '#3e434f',
          autorange: 'reversed', // smaller h to the right
        },
        yaxis: {
          title: { text: 'Max Error (°C)' },
          type: 'log',
          gridcolor: '#2e313a',
          zerolinecolor: '#3e434f',
        },
        legend: {
          orientation: 'h',
          y: -0.2,
          x: 0,
          font: { size: 10 },
        },
        autosize: true,
      };

      Plotly.newPlot(convPlotRef.current, traces, layout, {
        responsive: true,
        displayModeBar: false,
      });
    }
  }, [jobResult]);

  // Resize handler for Plotly charts
  useEffect(() => {
    const handleResize = () => {
      if (tempPlotRef.current) Plotly.Plots.resize(tempPlotRef.current);
      if (convPlotRef.current) Plotly.Plots.resize(convPlotRef.current);
    };
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // Export handlers
  const handleDownloadCsv = (kind: 'solution' | 'convergence') => {
    if (!activeJobId || !jobResult) return;
    const url = kind === 'solution'
      ? apiClient.getSolutionCsvUrl(activeJobId)
      : apiClient.getConvergenceCsvUrl(activeJobId);

    const a = document.createElement('a');
    a.href = url;
    a.download = `${activeJobId}_${kind}.csv`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  };

  const handleExportPng = async (targetRef: React.RefObject<HTMLDivElement>, filename: string) => {
    if (!targetRef.current) return;
    try {
      await Plotly.downloadImage(targetRef.current, {
        format: 'png',
        filename,
        height: 600,
        width: 800,
      });
    } catch (err) {
      console.error('Failed to export plot PNG:', err);
    }
  };

  return (
    <div className="flex-1 flex flex-col lg:flex-row h-full overflow-hidden bg-canvas">
      {/* Left Form Controls Sidebar */}
      <aside className="w-full lg:w-80 xl:w-96 bg-panel border-r border-border-subtle flex flex-col p-4 gap-4 overflow-y-auto">
        <div>
          <h2 className="text-xs font-semibold text-text-secondary uppercase tracking-wider font-mono mb-1">
            Thermal FEA Parameters
          </h2>
          <p className="text-[11px] text-text-muted">
            1D Rod: <code className="text-text-secondary">-d/dx(kA dT/dx) = qA</code>
          </p>
        </div>

        {generalError && (
          <div className="bg-status-error-bg border border-status-error/40 text-status-error p-3 rounded-md text-xs font-mono">
            <div className="font-semibold mb-0.5">Validation Error</div>
            <p className="text-[11px] text-text-primary">{generalError}</p>
          </div>
        )}

        <form onSubmit={handleSubmit} className="flex flex-col gap-3 text-xs font-mono">
          {/* Length */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="length_m" className="text-text-secondary">Length L (m):</label>
              {fieldErrors.length_m && (
                <span className="text-[10px] text-status-error">{fieldErrors.length_m}</span>
              )}
            </div>
            <input
              id="length_m"
              type="number"
              step="any"
              value={params.length_m ?? ''}
              onChange={(e) => handleChange('length_m', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.length_m ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Area */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="area_m2" className="text-text-secondary">Area A (m²):</label>
              {fieldErrors.area_m2 && (
                <span className="text-[10px] text-status-error">{fieldErrors.area_m2}</span>
              )}
            </div>
            <input
              id="area_m2"
              type="number"
              step="any"
              value={params.area_m2 ?? ''}
              onChange={(e) => handleChange('area_m2', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.area_m2 ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Conductivity k */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="conductivity" className="text-text-secondary">Conductivity k (W/(m·K)):</label>
              {fieldErrors.conductivity_W_per_mK && (
                <span className="text-[10px] text-status-error">{fieldErrors.conductivity_W_per_mK}</span>
              )}
            </div>
            <input
              id="conductivity"
              type="number"
              step="any"
              value={params.conductivity_W_per_mK ?? ''}
              onChange={(e) => handleChange('conductivity_W_per_mK', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.conductivity_W_per_mK ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Heat Generation q */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="heat_gen" className="text-text-secondary">Heat Gen q (W/m³):</label>
              {fieldErrors.heat_generation_W_per_m3 && (
                <span className="text-[10px] text-status-error">{fieldErrors.heat_generation_W_per_m3}</span>
              )}
            </div>
            <input
              id="heat_gen"
              type="number"
              step="any"
              value={params.heat_generation_W_per_m3 ?? ''}
              onChange={(e) => handleChange('heat_generation_W_per_m3', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.heat_generation_W_per_m3 ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Dirichlet BCs: T(0) and T(L) */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <div className="flex justify-between items-center mb-1">
                <label htmlFor="t_left" className="text-text-secondary">T(0) (°C):</label>
                {fieldErrors.T_left_C && (
                  <span className="text-[10px] text-status-error">*</span>
                )}
              </div>
              <input
                id="t_left"
                type="number"
                step="any"
                value={params.T_left_C ?? ''}
                onChange={(e) => handleChange('T_left_C', e.target.value)}
                className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                  fieldErrors.T_left_C ? 'border-status-error' : 'border-border-default focus:border-accent'
                }`}
              />
            </div>
            <div>
              <div className="flex justify-between items-center mb-1">
                <label htmlFor="t_right" className="text-text-secondary">T(L) (°C):</label>
                {fieldErrors.T_right_C && (
                  <span className="text-[10px] text-status-error">*</span>
                )}
              </div>
              <input
                id="t_right"
                type="number"
                step="any"
                value={params.T_right_C ?? ''}
                onChange={(e) => handleChange('T_right_C', e.target.value)}
                className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                  fieldErrors.T_right_C ? 'border-status-error' : 'border-border-default focus:border-accent'
                }`}
              />
            </div>
          </div>

          {/* Elements N */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <label htmlFor="n_elements" className="text-text-secondary">Elements N:</label>
              {fieldErrors.n_elements && (
                <span className="text-[10px] text-status-error">{fieldErrors.n_elements}</span>
              )}
            </div>
            <input
              id="n_elements"
              type="number"
              step="1"
              value={params.n_elements ?? ''}
              onChange={(e) => handleChange('n_elements', e.target.value)}
              className={`w-full px-2.5 py-1.5 bg-input border rounded text-text-primary ${
                fieldErrors.n_elements ? 'border-status-error' : 'border-border-default focus:border-accent'
              }`}
            />
          </div>

          {/* Progress Bar & Actions */}
          {(jobState === 'running' || jobState === 'queued') && (
            <div className="mt-2 flex flex-col gap-1.5">
              <div className="flex justify-between text-[11px] text-text-secondary">
                <span>{jobState === 'queued' ? 'Queued in C++ worker…' : 'Solving thermal FEA…'}</span>
                <span className="tabular-nums font-semibold">{Math.round(progress * 100)}%</span>
              </div>
              <div className="w-full h-2 bg-canvas rounded-full overflow-hidden border border-border-subtle">
                <div
                  className="h-full bg-accent transition-all duration-200"
                  style={{ width: `${Math.round(progress * 100)}%` }}
                />
              </div>
            </div>
          )}

          <div className="flex gap-2 mt-2">
            <button
              type="submit"
              disabled={jobState === 'running' || jobState === 'queued'}
              className="flex-1 py-2 px-3 bg-accent hover:bg-accent-hover disabled:opacity-50 text-accent-fg font-semibold text-xs rounded transition-colors"
            >
              Run Analysis
            </button>
            {(jobState === 'running' || jobState === 'queued') && (
              <button
                type="button"
                onClick={handleCancel}
                disabled={cancelling}
                className="py-2 px-3 bg-status-error/20 hover:bg-status-error/30 text-status-error border border-status-error/40 font-medium text-xs rounded transition-colors"
              >
                {cancelling ? 'Cancelling…' : 'Cancel'}
              </button>
            )}
          </div>
        </form>

        {/* Results summary & CSV exports */}
        {jobResult && (
          <div className="mt-auto border-t border-border-subtle pt-3 flex flex-col gap-2 font-mono text-xs">
            <div className="font-semibold text-text-primary text-xs">Data Exports:</div>
            <div className="flex flex-col gap-1.5">
              <button
                onClick={() => handleDownloadCsv('solution')}
                className="py-1 px-2.5 bg-subtle hover:bg-border-subtle text-text-primary text-[11px] rounded border border-border-default transition-colors text-left flex items-center justify-between"
              >
                <span>Solution CSV</span>
                <svg className="w-3.5 h-3.5 text-text-muted" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
                </svg>
              </button>
              <button
                onClick={() => handleDownloadCsv('convergence')}
                className="py-1 px-2.5 bg-subtle hover:bg-border-subtle text-text-primary text-[11px] rounded border border-border-default transition-colors text-left flex items-center justify-between"
              >
                <span>Convergence CSV</span>
                <svg className="w-3.5 h-3.5 text-text-muted" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
                </svg>
              </button>
            </div>
          </div>
        )}
      </aside>

      {/* Main Charts & Results Table Area */}
      <main className="flex-1 flex flex-col h-full overflow-y-auto p-4 gap-4">
        {jobResult ? (
          <>
            {/* Top Row: Two Plotly Charts */}
            <div className="grid grid-cols-1 xl:grid-cols-2 gap-4">
              {/* Plot 1: T vs x */}
              <div className="bg-panel border border-border-subtle rounded-lg p-3 flex flex-col shadow-panel">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-xs font-mono font-semibold text-text-primary">
                    Temperature Profile (T vs x)
                  </span>
                  <button
                    onClick={() => handleExportPng(tempPlotRef, 'temperature_profile.png')}
                    className="text-[11px] font-mono text-text-secondary hover:text-text-primary px-2 py-0.5 bg-subtle hover:bg-border-subtle rounded border border-border-subtle transition-colors flex items-center gap-1"
                  >
                    <span>PNG</span>
                    <svg className="w-3 h-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
                    </svg>
                  </button>
                </div>
                <div ref={tempPlotRef} className="w-full h-72 min-h-[280px]" />
              </div>

              {/* Plot 2: Convergence (log-log) */}
              <div className="bg-panel border border-border-subtle rounded-lg p-3 flex flex-col shadow-panel">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-xs font-mono font-semibold text-text-primary">
                    Mesh Convergence (log-log)
                  </span>
                  <button
                    onClick={() => handleExportPng(convPlotRef, 'mesh_convergence.png')}
                    className="text-[11px] font-mono text-text-secondary hover:text-text-primary px-2 py-0.5 bg-subtle hover:bg-border-subtle rounded border border-border-subtle transition-colors flex items-center gap-1"
                  >
                    <span>PNG</span>
                    <svg className="w-3 h-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
                    </svg>
                  </button>
                </div>
                <div ref={convPlotRef} className="w-full h-72 min-h-[280px]" />
              </div>
            </div>

            {/* Bottom Row: Numerical Results Table */}
            {jobResult.convergence && (
              <div className="bg-panel border border-border-subtle rounded-lg p-3 flex flex-col shadow-panel">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-xs font-mono font-semibold text-text-primary">
                    Convergence Verification Table
                  </span>
                  <span className="text-[10px] font-mono text-text-muted">
                    C++ 1D Thomas Solver Verification
                  </span>
                </div>
                <div className="overflow-x-auto">
                  <table className="w-full text-left font-mono text-xs tabular-nums border-collapse">
                    <thead>
                      <tr className="border-b border-border-default text-text-secondary text-[11px]">
                        <th className="py-2 px-3">Elements N</th>
                        <th className="py-2 px-3">Mesh Size h (m)</th>
                        <th className="py-2 px-3">Max Midpoint Err (°C)</th>
                        <th className="py-2 px-3">Max Nodal Err (°C)</th>
                        <th className="py-2 px-3">Observed Order (p)</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-border-subtle">
                      {jobResult.convergence.map((row) => (
                        <tr key={row.n_elements} className="hover:bg-subtle/50">
                          <td className="py-2 px-3 font-semibold text-text-primary">{row.n_elements}</td>
                          <td className="py-2 px-3 text-text-secondary">{row.h_m.toFixed(4)}</td>
                          <td className="py-2 px-3 text-status-warning font-semibold">
                            {row.max_midpoint_err_C.toExponential(4)}
                          </td>
                          <td className="py-2 px-3 text-status-info">
                            {row.max_nodal_err_C.toExponential(4)}
                          </td>
                          <td className="py-2 px-3 text-text-primary">
                            {row.observed_order !== null && !isNaN(row.observed_order)
                              ? row.observed_order.toFixed(3)
                              : '— (base)'}
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              </div>
            )}
          </>
        ) : (
          <div className="flex-1 flex flex-col items-center justify-center border-2 border-dashed border-border-subtle rounded-lg p-8 text-center bg-card/30">
            <svg className="w-12 h-12 text-text-muted mb-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 19v-6a2 2 0 00-2-2H5a2 2 0 00-2 2v6a2 2 0 002 2h2a2 2 0 002-2zm0 0V9a2 2 0 012-2h2a2 2 0 012 2v10m-6 0a2 2 0 002 2h2a2 2 0 002-2m0 0V5a2 2 0 012-2h2a2 2 0 012 2v14a2 2 0 01-2 2h-2a2 2 0 01-2-2z" />
            </svg>
            <h3 className="text-sm font-semibold font-mono text-text-primary mb-1">
              Ready for Thermal Analysis
            </h3>
            <p className="text-xs text-text-secondary max-w-sm">
              Adjust parameters in the left panel and click <strong>Run Analysis</strong> to execute the 1D FEA solver and convergence study in the C++ backend.
            </p>
          </div>
        )}
      </main>
    </div>
  );
};

