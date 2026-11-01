import React from 'react';
import { JobState } from '../../api/types';

export interface BackendStatus {
  connected: boolean;
  checking: boolean;
  uptimeMs?: number;
  coreVersion?: string;
  error?: string;
}

export interface ActiveJobStatus {
  id?: string;
  kind?: string;
  state: JobState | 'idle';
  progress?: number;
}

interface Props {
  backend: BackendStatus;
  job: ActiveJobStatus;
  lastError: string | null;
  onClearError: () => void;
}

export const StatusBar: React.FC<Props> = ({
  backend,
  job,
  lastError,
  onClearError,
}) => {
  return (
    <footer
      aria-live="polite"
      role="status"
      className="h-8 bg-panel border-t border-border-subtle flex items-center justify-between px-3 text-[11px] font-mono text-text-secondary select-none z-20"
    >
      {/* Left: Backend Connection & Server State */}
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-1.5" title={backend.error || 'Backend status'}>
          <span
            className={`w-2 h-2 rounded-full ${
              backend.checking
                ? 'bg-status-warning animate-pulse'
                : backend.connected
                ? 'bg-status-success'
                : 'bg-status-error'
            }`}
          />
          <span className="text-text-primary">
            {backend.checking
              ? 'Checking server…'
              : backend.connected
              ? '127.0.0.1 (Connected)'
              : 'Backend Disconnected'}
          </span>
        </div>

        {backend.connected && backend.uptimeMs !== undefined && (
          <div className="hidden sm:flex items-center gap-1 text-text-muted tabular-nums">
            <span>Uptime:</span>
            <span>{(backend.uptimeMs / 1000).toFixed(1)}s</span>
          </div>
        )}
      </div>

      {/* Center: Error notification if present */}
      {lastError && (
        <div className="flex items-center gap-2 max-w-md bg-status-error/15 text-status-error px-2 py-0.5 rounded border border-status-error/30 text-[11px] truncate">
          <svg className="w-3.5 h-3.5 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
          </svg>
          <span className="truncate" title={lastError}>
            {lastError}
          </span>
          <button
            onClick={onClearError}
            aria-label="Dismiss error"
            className="text-status-error hover:text-white font-bold ml-1 px-1 rounded hover:bg-status-error/30"
          >
            ×
          </button>
        </div>
      )}

      {/* Right: Job Status */}
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-1.5">
          <span className="text-text-muted">Job:</span>
          {job.state === 'idle' && (
            <span className="text-text-muted">Idle</span>
          )}
          {job.state === 'running' && (
            <span className="text-status-info font-semibold flex items-center gap-1">
              <span className="inline-block w-1.5 h-1.5 rounded-full bg-status-info animate-ping" />
              Running {job.progress !== undefined ? `${Math.round(job.progress * 100)}%` : ''}
            </span>
          )}
          {job.state === 'queued' && (
            <span className="text-status-warning font-semibold">Queued</span>
          )}
          {job.state === 'succeeded' && (
            <span className="text-status-success font-semibold">Succeeded</span>
          )}
          {job.state === 'failed' && (
            <span className="text-status-error font-semibold">Failed</span>
          )}
          {job.state === 'cancelled' && (
            <span className="text-text-muted font-semibold">Cancelled</span>
          )}
        </div>
      </div>
    </footer>
  );
};

