import React, { useState, useRef, useId } from 'react';
import {
  apiClient,
  WorkbenchApiError,
} from '../../api/client';
import {
  StlValidationSummary,
  CsvValidationSummary,
  StlBinaryMesh,
  CsvBinaryPoints,
} from '../../api/types';
import { ThreeCanvas, InspectedNode } from './ThreeCanvas';
import { generateLegendTicks } from '../../core/colormap';

interface Props {
  onError: (err: string | null) => void;
}

export const GeometryView: React.FC<Props> = ({ onError }) => {
  const [fileMode, setFileMode] = useState<'stl' | 'csv' | null>(null);
  const [loading, setLoading] = useState<boolean>(false);
  const [fileName, setFileName] = useState<string | null>(null);
  const [fileSize, setFileSize] = useState<number | null>(null);

  // Validation summaries from C++ backend
  const [stlSummary, setStlSummary] = useState<StlValidationSummary | null>(null);
  const [csvSummary, setCsvSummary] = useState<CsvValidationSummary | null>(null);
  const [validationError, setValidationError] = useState<string | null>(null);
  const [errorLineNumber, setErrorLineNumber] = useState<number | undefined>(undefined);

  // Binary data for 3D rendering
  const [stlBinary, setStlBinary] = useState<StlBinaryMesh | null>(null);
  const [pointsBinary, setPointsBinary] = useState<CsvBinaryPoints | null>(null);

  // CSV Viewer settings
  const [pointSize, setPointSize] = useState<number>(0.04);
  const [selectedNode, setSelectedNode] = useState<InspectedNode | null>(null);
  const [gotoNodeInput, setGotoNodeInput] = useState<string>('');
  const [gotoError, setGotoError] = useState<string | null>(null);

  // Triggers for Canvas
  const [fitTrigger, setFitTrigger] = useState<number>(0);
  const [resetTrigger, setResetTrigger] = useState<number>(0);

  const fileInputRef = useRef<HTMLInputElement>(null);
  const fileUploadId = useId();
  const pointSizeId = useId();
  const gotoNodeId = useId();

  // Handle file upload and validation through C++ API
  const processFile = async (file: File) => {
    setLoading(true);
    setValidationError(null);
    setErrorLineNumber(undefined);
    onError(null);
    setFileName(file.name);
    setFileSize(file.size);
    setSelectedNode(null);
    setGotoError(null);

    const nameLower = file.name.toLowerCase();
    const isStl = nameLower.endsWith('.stl');
    const isCsv = nameLower.endsWith('.csv');

    if (!isStl && !isCsv) {
      const err = 'Unsupported file format: only ASCII .stl and ANSYS nodal .csv are supported.';
      setValidationError(err);
      onError(err);
      setLoading(false);
      return;
    }

    try {
      const content = await file.text();

      if (isStl) {
        setFileMode('stl');
        setCsvSummary(null);
        setPointsBinary(null);

        // Upload and validate in C++
        const summary = await apiClient.uploadStl(content);
        setStlSummary(summary);

        // Download binary mesh
        const bin = await apiClient.getStlBinary(summary.id);
        setStlBinary(bin);
      } else {
        setFileMode('csv');
        setStlSummary(null);
        setStlBinary(null);

        // Upload and validate in C++
        const summary = await apiClient.uploadCsv(content);
        setCsvSummary(summary);

        // Download binary points
        const bin = await apiClient.getPointsBinary(summary.id);
        setPointsBinary(bin);
      }
    } catch (err: unknown) {
      if (err instanceof WorkbenchApiError) {
        const lineNotice = err.lineNumber ? ` (Line ${err.lineNumber})` : '';
        const fullMsg = `${err.message}${lineNotice}`;
        setValidationError(fullMsg);
        setErrorLineNumber(err.lineNumber);
        onError(fullMsg);
      } else {
        const msg = err instanceof Error ? err.message : String(err);
        setValidationError(msg);
        onError(msg);
      }
      setStlSummary(null);
      setCsvSummary(null);
      setStlBinary(null);
      setPointsBinary(null);
    } finally {
      setLoading(false);
    }
  };

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    if (e.dataTransfer.files && e.dataTransfer.files[0]) {
      processFile(e.dataTransfer.files[0]);
    }
  };

  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault();
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    if (e.target.files && e.target.files[0]) {
      processFile(e.target.files[0]);
    }
  };

  // Keyboard-accessible "Go to node ID" handler
  const handleGotoNode = (e: React.FormEvent) => {
    e.preventDefault();
    setGotoError(null);
    if (!pointsBinary) return;

    const id = parseInt(gotoNodeInput.trim(), 10);
    if (isNaN(id)) {
      setGotoError('Please enter a valid numeric node ID');
      return;
    }

    // Lookup node by ID in Uint32Array
    const ids = pointsBinary.nodeIds;
    let foundIndex = -1;
    for (let i = 0; i < ids.length; i++) {
      if (ids[i] === id) {
        foundIndex = i;
        break;
      }
    }

    if (foundIndex === -1) {
      setGotoError(`Node ID #${id} not found in this point cloud`);
      return;
    }

    const inspected: InspectedNode = {
      nodeId: ids[foundIndex],
      x: pointsBinary.xyz[foundIndex * 3],
      y: pointsBinary.xyz[foundIndex * 3 + 1],
      z: pointsBinary.xyz[foundIndex * 3 + 2],
      temperature: pointsBinary.temperatures[foundIndex],
      index: foundIndex,
    };
    setSelectedNode(inspected);
  };

  const legendTicks = csvSummary
    ? generateLegendTicks(csvSummary.min_temp_C, csvSummary.max_temp_C, 5)
    : [];

  return (
    <div className="flex-1 flex flex-col lg:flex-row h-full overflow-hidden bg-canvas">
      {/* Left Control & Inspector Sidebar */}
      <aside className="w-full lg:w-80 xl:w-96 bg-panel border-r border-border-subtle flex flex-col p-4 gap-4 overflow-y-auto">
        {/* File Dropzone & Selection */}
        <div className="flex flex-col gap-2">
          <label htmlFor={fileUploadId} className="text-xs font-semibold text-text-secondary uppercase tracking-wider font-mono">
            Geometry Importer
          </label>
          <div
            onDrop={handleDrop}
            onDragOver={handleDragOver}
            className="border-2 border-dashed border-border-default hover:border-accent rounded-lg p-4 flex flex-col items-center justify-center text-center bg-card/60 transition-colors"
          >
            <input
              id={fileUploadId}
              type="file"
              ref={fileInputRef}
              onChange={handleFileChange}
              accept=".stl,.csv"
              className="hidden"
            />
            <svg className="w-8 h-8 text-text-muted mb-2" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
            </svg>
            <p className="text-xs text-text-secondary mb-2">
              Drag & drop <strong className="text-text-primary">.stl</strong> or <strong className="text-text-primary">.csv</strong> file
            </p>
            <button
              type="button"
              onClick={() => fileInputRef.current?.click()}
              disabled={loading}
              className="px-3 py-1.5 bg-subtle hover:bg-border-subtle text-text-primary text-xs font-medium rounded border border-border-default transition-colors flex items-center gap-1.5"
            >
              {loading ? (
                <>
                  <span className="w-3 h-3 rounded-full border-2 border-accent border-t-transparent animate-spin" />
                  Validating in C++…
                </>
              ) : (
                'Select File'
              )}
            </button>
            <span className="text-[10px] text-text-muted mt-2">
              ASCII STL & ANSYS nodal CSV formats
            </span>
          </div>
        </div>

        {/* Validation Error Message */}
        {validationError && (
          <div className="bg-status-error-bg border border-status-error/40 text-status-error p-3 rounded-md text-xs font-mono">
            <div className="flex items-center gap-1.5 font-semibold mb-1">
              <svg className="w-4 h-4 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
              </svg>
              <span>Validation Rejected</span>
            </div>
            <p className="text-text-primary text-[11px] leading-relaxed">
              {validationError}
            </p>
            {errorLineNumber && (
              <div className="mt-1.5 text-[10px] px-1.5 py-0.5 bg-status-error/20 rounded inline-block text-status-error font-bold">
                Fault detected on line {errorLineNumber}
              </div>
            )}
          </div>
        )}

        {/* C++ Validation Summary: STL */}
        {fileMode === 'stl' && stlSummary && (
          <div className="bg-card border border-border-subtle rounded-md p-3 text-xs flex flex-col gap-2.5">
            <div className="flex items-center justify-between border-b border-border-subtle pb-2">
              <span className="font-semibold text-text-primary font-mono text-xs">STL Mesh Validation</span>
              <span className="px-1.5 py-0.5 rounded bg-status-success-bg text-status-success font-mono text-[10px] font-bold">
                Valid C++
              </span>
            </div>

            <div className="grid grid-cols-2 gap-2 text-[11px] font-mono tabular-nums">
              <div>
                <span className="text-text-muted block text-[10px]">Triangles:</span>
                <span className="text-text-primary font-semibold">{stlSummary.triangle_count.toLocaleString()}</span>
              </div>
              <div>
                <span className="text-text-muted block text-[10px]">Degenerate:</span>
                <span className={stlSummary.degenerate_count > 0 ? 'text-status-warning font-semibold' : 'text-text-primary'}>
                  {stlSummary.degenerate_count}
                </span>
              </div>
              {fileSize && (
                <div>
                  <span className="text-text-muted block text-[10px]">File Size:</span>
                  <span className="text-text-secondary">{(fileSize / 1024).toFixed(1)} KB</span>
                </div>
              )}
            </div>

            {/* Bounding Box */}
            <div className="text-[11px] font-mono bg-canvas p-2 rounded border border-border-subtle tabular-nums">
              <span className="text-[10px] text-text-muted uppercase tracking-wider block mb-1">Bounds [X, Y, Z]</span>
              <div className="text-text-secondary">
                Min: [{stlSummary.bounds.min.map((v) => v.toFixed(3)).join(', ')}]
              </div>
              <div className="text-text-secondary">
                Max: [{stlSummary.bounds.max.map((v) => v.toFixed(3)).join(', ')}]
              </div>
            </div>

            {/* Prominent Unitless Note */}
            <div className="p-2 bg-subtle rounded border border-accent/30 text-[11px] text-text-secondary font-mono">
              <strong className="text-accent block text-[10px] uppercase tracking-wider">Unit Note:</strong>
              {stlSummary.note}
            </div>

            {stlSummary.warning && (
              <div className="text-status-warning text-[11px] font-mono bg-status-warning-bg p-2 rounded border border-status-warning/30">
                Warning: {stlSummary.warning}
              </div>
            )}
          </div>
        )}

        {/* C++ Validation Summary: CSV */}
        {fileMode === 'csv' && csvSummary && (
          <div className="bg-card border border-border-subtle rounded-md p-3 text-xs flex flex-col gap-2.5">
            <div className="flex items-center justify-between border-b border-border-subtle pb-2">
              <span className="font-semibold text-text-primary font-mono text-xs">Point Cloud Validation</span>
              <span className="px-1.5 py-0.5 rounded bg-status-success-bg text-status-success font-mono text-[10px] font-bold">
                Valid C++
              </span>
            </div>

            <div className="grid grid-cols-2 gap-2 text-[11px] font-mono tabular-nums">
              <div>
                <span className="text-text-muted block text-[10px]">Nodes:</span>
                <span className="text-text-primary font-semibold">{csvSummary.node_count.toLocaleString()}</span>
              </div>
              <div>
                <span className="text-text-muted block text-[10px]">Temperature:</span>
                <span className="text-status-info font-semibold">
                  {csvSummary.min_temp_C.toFixed(1)}° to {csvSummary.max_temp_C.toFixed(1)}°C
                </span>
              </div>
            </div>

            {/* Bounds */}
            <div className="text-[11px] font-mono bg-canvas p-2 rounded border border-border-subtle tabular-nums">
              <span className="text-[10px] text-text-muted uppercase tracking-wider block mb-1">Spatial Bounds (m)</span>
              <div className="text-text-secondary">
                Min: [{csvSummary.bounds.min.map((v) => v.toFixed(3)).join(', ')}]
              </div>
              <div className="text-text-secondary">
                Max: [{csvSummary.bounds.max.map((v) => v.toFixed(3)).join(', ')}]
              </div>
            </div>

            {/* Point Size Control */}
            <div className="flex flex-col gap-1 mt-1">
              <div className="flex justify-between items-center text-[11px] font-mono">
                <label htmlFor={pointSizeId} className="text-text-secondary">Point Size:</label>
                <span className="text-text-primary tabular-nums">{pointSize.toFixed(3)}</span>
              </div>
              <input
                id={pointSizeId}
                type="range"
                min="0.005"
                max="0.2"
                step="0.005"
                value={pointSize}
                onChange={(e) => setPointSize(parseFloat(e.target.value))}
                className="w-full accent-accent cursor-pointer"
              />
            </div>

            {/* Inferno Color Legend with Ticks */}
            <div className="mt-2 flex flex-col gap-1">
              <div className="flex justify-between text-[10px] font-mono text-text-secondary">
                <span>Temperature Colormap (Inferno)</span>
                <span className="font-semibold text-text-primary">°C</span>
              </div>
              {/* Gradient Bar */}
              <div
                className="h-3.5 w-full rounded border border-border-subtle"
                style={{
                  background:
                    'linear-gradient(to right, rgb(0,0,3), rgb(66,25,104), rgb(147,38,103), rgb(221,81,58), rgb(252,165,10), rgb(252,255,164))',
                }}
              />
              {/* Ticks */}
              <div className="relative h-6 w-full text-[10px] font-mono text-text-muted select-none tabular-nums">
                {legendTicks.map((tick, i) => (
                  <span
                    key={i}
                    className="absolute -translate-x-1/2 whitespace-nowrap text-center"
                    style={{ left: `${tick.percent}%` }}
                  >
                    {tick.label}
                  </span>
                ))}
              </div>
            </div>

            {/* Keyboard-accessible "Go to node ID" */}
            <form onSubmit={handleGotoNode} className="mt-2 flex flex-col gap-1.5 border-t border-border-subtle pt-2">
              <label htmlFor={gotoNodeId} className="text-[11px] font-mono text-text-secondary">
                Go to Node ID:
              </label>
              <div className="flex gap-2">
                <input
                  id={gotoNodeId}
                  type="number"
                  placeholder="e.g. 42"
                  value={gotoNodeInput}
                  onChange={(e) => setGotoNodeInput(e.target.value)}
                  className="w-full px-2 py-1 bg-input border border-border-default rounded text-xs font-mono text-text-primary focus:border-accent"
                />
                <button
                  type="submit"
                  className="px-3 py-1 bg-accent hover:bg-accent-hover text-accent-fg text-xs font-mono font-semibold rounded transition-colors"
                >
                  Go
                </button>
              </div>
              {gotoError && (
                <span className="text-[10px] text-status-error font-mono">{gotoError}</span>
              )}
            </form>

            {/* Selected / Inspected Node Details Card */}
            {selectedNode && (
              <div className="bg-subtle p-2.5 rounded border border-accent/40 font-mono text-xs tabular-nums">
                <div className="flex justify-between items-center text-accent font-semibold mb-1">
                  <span>Selected Node #{selectedNode.nodeId}</span>
                  <span className="text-status-info">{selectedNode.temperature.toFixed(2)} °C</span>
                </div>
                <div className="text-[11px] text-text-secondary grid grid-cols-3 gap-1">
                  <span>X: {selectedNode.x.toFixed(4)}m</span>
                  <span>Y: {selectedNode.y.toFixed(4)}m</span>
                  <span>Z: {selectedNode.z.toFixed(4)}m</span>
                </div>
              </div>
            )}
          </div>
        )}

        {/* Viewport Action Buttons */}
        <div className="mt-auto pt-4 border-t border-border-subtle flex gap-2">
          <button
            onClick={() => setFitTrigger((n) => n + 1)}
            disabled={!stlBinary && !pointsBinary}
            className="flex-1 py-1.5 px-3 bg-subtle hover:bg-border-subtle disabled:opacity-50 text-text-primary text-xs font-mono rounded border border-border-default transition-colors flex items-center justify-center gap-1"
          >
            <span>Fit View</span>
            <kbd className="text-[10px] bg-canvas px-1 rounded border border-border-subtle">F</kbd>
          </button>
          <button
            onClick={() => setResetTrigger((n) => n + 1)}
            disabled={!stlBinary && !pointsBinary}
            className="py-1.5 px-3 bg-subtle hover:bg-border-subtle disabled:opacity-50 text-text-secondary hover:text-text-primary text-xs font-mono rounded border border-border-default transition-colors"
          >
            Reset
          </button>
        </div>
      </aside>

      {/* Right 3D Viewport Area */}
      <main className="flex-1 relative flex flex-col h-full overflow-hidden">
        {fileName && (
          <div className="absolute top-3 left-3 z-10 bg-panel/85 backdrop-blur border border-border-subtle px-2.5 py-1 rounded text-xs font-mono text-text-primary flex items-center gap-2 shadow-panel">
            <span className="w-2 h-2 rounded-full bg-accent" />
            <span className="font-semibold">{fileName}</span>
            <span className="text-text-muted text-[10px]">
              {fileMode === 'stl' ? 'Mesh (STL)' : 'Point Cloud (CSV)'}
            </span>
          </div>
        )}

        <ThreeCanvas
          mode={fileMode}
          stlData={stlBinary}
          pointsData={pointsBinary}
          minTemp={csvSummary?.min_temp_C}
          maxTemp={csvSummary?.max_temp_C}
          pointSize={pointSize}
          selectedNode={selectedNode}
          onSelectNode={setSelectedNode}
          fitTrigger={fitTrigger}
          resetTrigger={resetTrigger}
        />
      </main>
    </div>
  );
};

