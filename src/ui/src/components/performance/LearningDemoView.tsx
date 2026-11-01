import React, { useState, useEffect, useRef } from 'react';
import Plotly from 'plotly.js-cartesian-dist-min';
import { apiClient, WorkbenchApiError } from '../../api/client';
import {
  BackpropJobParams,
  BackpropResult,
  JobStatusResponse,
} from '../../api/types';

interface Props {
  onError: (err: string | null) => void;
  onJobStateChange: (state: JobStatusResponse['state'] | 'idle', progress?: number) => void;
}

export const LearningDemoView: React.FC<Props> = ({
  onError,
  onJobStateChange,
}) => {
  const [params, setParams] = useState<BackpropJobParams>({
    x: 1.0,
    y: 2.0,
    w: 0.5,
    b: 0.0,
    v: 1.0,
    c: 0.0,
    learning_rate: 0.1,
    subsequent_steps: 100,
    epsilon: 1e-6,
    tolerance: 1e-5,
  });

  const [activeJobId, setActiveJobId] = useState<string | null>(null);
  const [jobState, setJobState] = useState<'idle' | 'queued' | 'running' | 'succeeded' | 'failed' | 'cancelled'>('idle');
  const [progress, setProgress] = useState<number>(0);
  const [cancelling, setCancelling] = useState<boolean>(false);
  const [generalError, setGeneralError] = useState<string | null>(null);
  const [result, setResult] = useState<BackpropResult | null>(null);
  const [useLogScale, setUseLogScale] = useState<boolean>(true);

  const chartRef = useRef<HTMLDivElement>(null);

  const handleParamChange = (field: keyof BackpropJobParams, valStr: string) => {
    const parsed = field === 'subsequent_steps' ? parseInt(valStr, 10) : parseFloat(valStr);
    setParams((prev) => ({
      ...prev,
      [field]: isNaN(parsed) ? undefined : parsed,
    }));
  };

  const handleResetDefaults = () => {
    setParams({
      x: 1.0,
      y: 2.0,
      w: 0.5,
      b: 0.0,
      v: 1.0,
      c: 0.0,
      learning_rate: 0.1,
      subsequent_steps: 100,
      epsilon: 1e-6,
      tolerance: 1e-5,
    });
  };

  const handleRun = async (e: React.FormEvent) => {
    e.preventDefault();
    setGeneralError(null);
    onError(null);
    setJobState('queued');
    setProgress(0);
    onJobStateChange('queued', 0);

    try {
      const job = await apiClient.createBackpropJob(params);
      setActiveJobId(job.id);
    } catch (err: unknown) {
      setJobState('idle');
      onJobStateChange('idle');
      const msg = err instanceof WorkbenchApiError ? err.message : (err instanceof Error ? err.message : String(err));
      setGeneralError(msg);
      onError(msg);
    }
  };

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

  // Poll active backprop job
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
          setResult(status.result as BackpropResult);
          clearInterval(interval);
        } else if (status.state === 'failed') {
          const err = status.error || 'Backprop job failed';
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
  }, [activeJobId, jobState, onError, onJobStateChange]);

  // Render Loss curve chart with Plotly
  useEffect(() => {
    if (!chartRef.current || !result || !result.history?.length) {
      return;
    }

    const steps = result.history.map((h) => h.step);
    const losses = result.history.map((h) => h.loss);

    const trace: Plotly.Data = {
      x: steps,
      y: losses,
      name: 'Loss J = 0.5(y_hat - y)^2',
      mode: 'lines+markers',
      type: 'scatter',
      line: { color: '#0ea5e9', width: 2 },
      marker: {
        size: steps.map((s) => (s <= 1 ? 7 : 3)) as unknown as number,
        color: steps.map((s) => (s === 0 ? '#f59e0b' : s === 1 ? '#10b981' : '#0ea5e9')) as unknown as string,
      },
      hovertemplate: 'Step %{x}<br>Loss J: %{y:.8f}<extra></extra>',
    };

    const layout: Partial<Plotly.Layout> = {
      title: {
        text: `Gradient Descent Loss Trajectory (${result.history.length - 1} Steps, lr=${params.learning_rate})`,
        font: { family: 'IBM Plex Sans, sans-serif', size: 14, color: '#f0f2f5' },
      },
      paper_bgcolor: 'rgba(0,0,0,0)',
      plot_bgcolor: '#18191e',
      font: { family: 'IBM Plex Mono, monospace', color: '#9da3b4', size: 11 },
      margin: { l: 65, r: 40, t: 45, b: 45 },
      xaxis: {
        title: { text: 'Optimization Step' },
        gridcolor: '#2e313a',
        zerolinecolor: '#3e434f',
        tickformat: 'd',
      },
      yaxis: {
        title: { text: useLogScale ? 'Loss J (log scale)' : 'Loss J (linear scale)' },
        type: useLogScale ? 'log' : 'linear',
        gridcolor: '#2e313a',
        zerolinecolor: '#3e434f',
      },
      autosize: true,
    };

    Plotly.newPlot(chartRef.current, [trace], layout, {
      responsive: true,
      displayModeBar: false,
    });
  }, [result, useLogScale, params.learning_rate]);

  // Window resize handler for Plotly
  useEffect(() => {
    const handleResize = () => {
      if (chartRef.current) {
        Plotly.Plots.resize(chartRef.current);
      }
    };
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  const handleExportCsv = () => {
    if (!activeJobId && !result?.csv_data) return;
    if (activeJobId) {
      window.open(apiClient.getBackpropCsvUrl(activeJobId), '_blank');
    } else if (result?.csv_data) {
      const blob = new Blob([result.csv_data], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'backprop.csv';
      a.click();
      URL.revokeObjectURL(url);
    }
  };

  const handleExportPng = () => {
    if (!chartRef.current) return;
    Plotly.downloadImage(chartRef.current, {
      format: 'png',
      width: 1000,
      height: 500,
      filename: 'backprop_loss_curve',
    });
  };

  const initialRecord = result?.history?.[0];
  const step1Record = result?.history?.[1];
  const finalRecord = result?.history?.[result.history.length - 1];

  return (
    <div className="flex-1 flex flex-col lg:flex-row h-full overflow-hidden bg-canvas">
      {/* Sidebar Controls */}
      <aside className="w-full lg:w-80 xl:w-96 bg-panel border-r border-border-subtle flex flex-col p-4 gap-4 overflow-y-auto">
        <div>
          <h2 className="text-xs font-semibold text-text-secondary uppercase tracking-wider font-mono mb-1">
            Learning Demo Config
          </h2>
          <p className="text-[11px] text-text-muted">
            1-Neuron tanh regression backpropagation with central-difference check and simultaneous parameter updates.
          </p>
        </div>

        {generalError && (
          <div
            role="alert"
            className="bg-status-error-bg border border-status-error/40 text-status-error p-3 rounded-md text-xs font-mono"
          >
            <div className="font-semibold mb-0.5">Execution Error</div>
            <p className="text-[11px] text-text-primary">{generalError}</p>
          </div>
        )}

        <form onSubmit={handleRun} className="flex flex-col gap-3 text-xs font-mono">
          {/* Inputs & Target */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <label htmlFor="bp_x" className="text-text-secondary mb-1 block">Input x:</label>
              <input
                id="bp_x"
                type="number"
                step="any"
                value={params.x ?? ''}
                onChange={(e) => handleParamChange('x', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
            <div>
              <label htmlFor="bp_y" className="text-text-secondary mb-1 block">Target y:</label>
              <input
                id="bp_y"
                type="number"
                step="any"
                value={params.y ?? ''}
                onChange={(e) => handleParamChange('y', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
          </div>

          {/* Initial Weights: w & b */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <label htmlFor="bp_w" className="text-text-secondary mb-1 block">Initial w:</label>
              <input
                id="bp_w"
                type="number"
                step="any"
                value={params.w ?? ''}
                onChange={(e) => handleParamChange('w', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
            <div>
              <label htmlFor="bp_b" className="text-text-secondary mb-1 block">Initial b:</label>
              <input
                id="bp_b"
                type="number"
                step="any"
                value={params.b ?? ''}
                onChange={(e) => handleParamChange('b', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
          </div>

          {/* Initial Output Weights: v & c */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <label htmlFor="bp_v" className="text-text-secondary mb-1 block">Initial v:</label>
              <input
                id="bp_v"
                type="number"
                step="any"
                value={params.v ?? ''}
                onChange={(e) => handleParamChange('v', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
            <div>
              <label htmlFor="bp_c" className="text-text-secondary mb-1 block">Initial c:</label>
              <input
                id="bp_c"
                type="number"
                step="any"
                value={params.c ?? ''}
                onChange={(e) => handleParamChange('c', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
          </div>

          {/* Learning Rate & Subsequent Steps */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <label htmlFor="bp_lr" className="text-text-secondary mb-1 block">Learning Rate (&eta;):</label>
              <input
                id="bp_lr"
                type="number"
                step="any"
                value={params.learning_rate ?? ''}
                onChange={(e) => handleParamChange('learning_rate', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
            <div>
              <label htmlFor="bp_steps" className="text-text-secondary mb-1 block">Further Steps:</label>
              <input
                id="bp_steps"
                type="number"
                step="1"
                min="1"
                max="5000"
                value={params.subsequent_steps ?? ''}
                onChange={(e) => handleParamChange('subsequent_steps', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
          </div>

          {/* Epsilon & Tolerance */}
          <div className="grid grid-cols-2 gap-2">
            <div>
              <label htmlFor="bp_eps" className="text-text-secondary mb-1 block">&epsilon; (Num Diff):</label>
              <input
                id="bp_eps"
                type="number"
                step="any"
                value={params.epsilon ?? ''}
                onChange={(e) => handleParamChange('epsilon', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
            <div>
              <label htmlFor="bp_tol" className="text-text-secondary mb-1 block">Tol (Grad Check):</label>
              <input
                id="bp_tol"
                type="number"
                step="any"
                value={params.tolerance ?? ''}
                onChange={(e) => handleParamChange('tolerance', e.target.value)}
                className="w-full px-2.5 py-1.5 bg-input border border-border-default focus:border-accent rounded text-text-primary"
              />
            </div>
          </div>

          <div className="pt-2 flex flex-col gap-2">
            <button
              type="button"
              onClick={handleResetDefaults}
              className="py-1 px-3 text-[11px] text-text-muted hover:text-text-primary border border-border-subtle rounded hover:bg-panel-hover transition-colors"
            >
              Reset to Phase 7 Defaults
            </button>

            {jobState === 'queued' || jobState === 'running' ? (
              <div className="flex flex-col gap-2">
                <div className="flex justify-between text-[11px] text-text-secondary">
                  <span>{jobState === 'queued' ? 'Queued...' : 'Computing backprop...'}</span>
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
                  id="btn-cancel-backprop"
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
                id="btn-run-backprop"
                className="w-full py-2 px-3 bg-accent hover:bg-accent-hover text-accent-fg font-semibold rounded transition-colors shadow-sm"
              >
                Run Gradient Check &amp; Train
              </button>
            )}
          </div>
        </form>

        {/* Model Reference Card */}
        <div className="mt-auto p-3 bg-canvas/40 border border-border-subtle rounded text-[11px] font-mono text-text-muted space-y-1">
          <div className="font-semibold text-text-secondary text-[12px]">Model Equations</div>
          <div>z = w &middot; x + b</div>
          <div>h = tanh(z)</div>
          <div>&ycirc; = v &middot; h + c</div>
          <div>J = 0.5 &middot; (&ycirc; - y)&sup2;</div>
          <div className="text-[10px] text-text-muted pt-1 border-t border-border-subtle">
            Step 1 is the initial simultaneous update; subsequent steps continue gradient descent.
          </div>
        </div>
      </aside>

      {/* Main View Area */}
      <main className="flex-1 flex flex-col overflow-y-auto p-4 gap-4 bg-canvas">
        {/* Gradient Check Card */}
        <section className="bg-panel border border-border-subtle rounded-lg p-4 shadow-sm">
          <div className="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-2 mb-3 pb-2 border-b border-border-subtle">
            <div>
              <h2 className="text-sm font-semibold text-text-primary flex items-center gap-2">
                <span>Gradient Check Table</span>
                {result?.gradient_check?.passed ? (
                  <span className="text-[10px] px-1.5 py-0.5 rounded bg-status-success-bg text-status-success font-mono">
                    ALL CHECKS PASSED (&le; 1e-5)
                  </span>
                ) : result?.gradient_check?.passed === false ? (
                  <span className="text-[10px] px-1.5 py-0.5 rounded bg-status-error-bg text-status-error font-mono">
                    CHECK FAILED
                  </span>
                ) : (
                  <span className="text-[10px] px-1.5 py-0.5 rounded bg-input text-text-muted font-mono">
                    Pending Run
                  </span>
                )}
              </h2>
              <p className="text-[11px] text-text-muted">
                Central-difference comparison: (J(&theta; + &epsilon;) - J(&theta; - &epsilon;)) / 2&epsilon; with &epsilon; = 1e-6.
              </p>
            </div>
            {result && (
              <div className="flex items-center gap-2">
                <button
                  type="button"
                  id="btn-export-bp-csv"
                  onClick={handleExportCsv}
                  className="px-2.5 py-1 text-xs font-mono bg-panel hover:bg-panel-hover border border-border-default rounded text-text-secondary hover:text-text-primary transition-colors"
                >
                  Export CSV
                </button>
                <button
                  type="button"
                  id="btn-export-bp-png"
                  onClick={handleExportPng}
                  className="px-2.5 py-1 text-xs font-mono bg-panel hover:bg-panel-hover border border-border-default rounded text-text-secondary hover:text-text-primary transition-colors"
                >
                  Export PNG
                </button>
              </div>
            )}
          </div>

          {result?.gradient_check ? (
            <div className="overflow-x-auto">
              <table className="w-full text-left border-collapse font-mono text-xs">
                <thead>
                  <tr className="border-b border-border-subtle text-text-muted text-[11px]">
                    <th className="py-2 px-3">Parameter</th>
                    <th className="py-2 px-3">Analytic &part;J/&part;&theta;</th>
                    <th className="py-2 px-3">Numerical (Central Diff)</th>
                    <th className="py-2 px-3">Absolute Error</th>
                    <th className="py-2 px-3">Tolerance</th>
                    <th className="py-2 px-3 text-right">Status</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-border-subtle">
                  {result.gradient_check.entries.map((entry) => (
                    <tr key={entry.param_name} className="hover:bg-panel-hover/50">
                      <td className="py-2 px-3 font-semibold text-text-primary">
                        {entry.param_name}
                      </td>
                      <td className="py-2 px-3 text-text-secondary">
                        {entry.analytic_gradient.toFixed(10)}
                      </td>
                      <td className="py-2 px-3 text-text-secondary">
                        {entry.numerical_gradient.toFixed(10)}
                      </td>
                      <td className="py-2 px-3 text-text-secondary font-mono">
                        {entry.absolute_error.toExponential(3)}
                      </td>
                      <td className="py-2 px-3 text-text-muted font-mono">
                        &le; {(params.tolerance ?? 1e-5).toExponential(0)}
                      </td>
                      <td className="py-2 px-3 text-right">
                        {entry.passed ? (
                          <span className="inline-block px-1.5 py-0.5 rounded text-[10px] bg-status-success-bg text-status-success">
                            PASS
                          </span>
                        ) : (
                          <span className="inline-block px-1.5 py-0.5 rounded text-[10px] bg-status-error-bg text-status-error">
                            FAIL
                          </span>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          ) : (
            <div className="py-6 text-center text-xs font-mono text-text-muted">
              Click &quot;Run Gradient Check &amp; Train&quot; to execute the C++ analytical and numerical gradient validation.
            </div>
          )}

          <div className="mt-3 p-2.5 rounded bg-canvas/30 border border-border-subtle text-[11px] text-text-muted">
            <span className="font-semibold text-text-secondary">Provisional Notice:</span> The analytical gradient formulas implemented in the C++ engine were provisionally supplied by the AI assistant per user instructions. The candidate&apos;s handwritten derivation is conducted independently and will be compared against this implementation afterwards.
          </div>
        </section>

        {/* Loss Trajectory Plot Card */}
        <section className="bg-panel border border-border-subtle rounded-lg p-4 shadow-sm flex flex-col">
          <div className="flex justify-between items-center mb-2 pb-2 border-b border-border-subtle">
            <div>
              <h2 className="text-sm font-semibold text-text-primary">
                Loss Curve &amp; Optimization Trajectory
              </h2>
              <p className="text-[11px] text-text-muted">
                Evolution of Mean Squared Loss J = 0.5(&ycirc; - y)&sup2; across simultaneous updates.
              </p>
            </div>
            {result && (
              <div className="flex items-center gap-2">
                <button
                  type="button"
                  onClick={() => setUseLogScale(!useLogScale)}
                  className="px-2 py-1 text-[11px] font-mono bg-panel hover:bg-panel-hover border border-border-default rounded text-text-secondary hover:text-text-primary transition-colors"
                >
                  Scale: {useLogScale ? 'Logarithmic' : 'Linear'}
                </button>
              </div>
            )}
          </div>

          {result ? (
            <div className="w-full h-80 relative" ref={chartRef} />
          ) : (
            <div className="h-64 flex flex-col items-center justify-center text-center p-6 border border-dashed border-border-subtle rounded bg-canvas/30 text-text-muted">
              <span className="text-xs font-mono mb-1">No backpropagation run active</span>
              <p className="text-[11px] max-w-sm">
                Run the demo to view the 101-step convergence plot, comparing the initial loss, the handwritten update (step 1), and subsequent gradient descent iterations.
              </p>
            </div>
          )}
        </section>

        {/* Trajectory Key Points */}
        {result && initialRecord && step1Record && finalRecord && (
          <section className="grid grid-cols-1 md:grid-cols-3 gap-3">
            <div className="bg-panel border border-border-subtle rounded p-3 font-mono text-xs">
              <div className="text-text-muted text-[10px] uppercase tracking-wider mb-1">Step 0 (Initial)</div>
              <div className="text-text-primary text-sm font-semibold mb-1">
                J = {initialRecord.loss.toFixed(7)}
              </div>
              <div className="text-[11px] text-text-secondary space-y-0.5">
                <div>w: {initialRecord.w.toFixed(4)}</div>
                <div>b: {initialRecord.b.toFixed(4)}</div>
                <div>v: {initialRecord.v.toFixed(4)}</div>
                <div>c: {initialRecord.c.toFixed(4)}</div>
              </div>
            </div>

            <div className="bg-panel border border-border-subtle rounded p-3 font-mono text-xs">
              <div className="text-text-muted text-[10px] uppercase tracking-wider mb-1">Step 1 (First Update)</div>
              <div className="text-status-success text-sm font-semibold mb-1">
                J = {step1Record.loss.toFixed(7)}
              </div>
              <div className="text-[11px] text-text-secondary space-y-0.5">
                <div>w: {step1Record.w.toFixed(8)}</div>
                <div>b: {step1Record.b.toFixed(8)}</div>
                <div>v: {step1Record.v.toFixed(8)}</div>
                <div>c: {step1Record.c.toFixed(8)}</div>
              </div>
            </div>

            <div className="bg-panel border border-border-subtle rounded p-3 font-mono text-xs">
              <div className="text-text-muted text-[10px] uppercase tracking-wider mb-1">Step {finalRecord.step} (Final)</div>
              <div className="text-accent text-sm font-semibold mb-1">
                J = {finalRecord.loss.toExponential(4)}
              </div>
              <div className="text-[11px] text-text-secondary space-y-0.5">
                <div>w: {finalRecord.w.toFixed(8)}</div>
                <div>b: {finalRecord.b.toFixed(8)}</div>
                <div>v: {finalRecord.v.toFixed(8)}</div>
                <div>c: {finalRecord.c.toFixed(8)}</div>
              </div>
            </div>
          </section>
        )}
      </main>
    </div>
  );
};
