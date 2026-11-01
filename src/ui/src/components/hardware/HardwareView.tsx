import React, { useState, useEffect, useCallback, useRef } from 'react';
import {
  apiClient,
  WorkbenchApiError,
} from '../../api/client';
import {
  DeviceInfo,
  DeviceState,
  DeviceTelemetry,
  DeviceEventItem,
} from '../../api/types';

interface Props {
  onError: (err: string | null) => void;
}

export const HardwareView: React.FC<Props> = ({ onError }) => {
  const [devices, setDevices] = useState<DeviceInfo[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [telemetry, setTelemetry] = useState<Record<string, DeviceTelemetry>>({});
  const [deviceErrors, setDeviceErrors] = useState<Record<string, string>>({});
  const [actionFeedback, setActionFeedback] = useState<Record<string, string>>({});
  const [events, setEvents] = useState<DeviceEventItem[]>([]);
  const [timeoutArm, setTimeoutArm] = useState<Record<string, boolean>>({});
  const isPollingRef = useRef<boolean>(false);

  // Poll events from API
  const pollEvents = useCallback(async () => {
    try {
      const resp = await apiClient.getDeviceEvents();
      if (resp.events && resp.events.length > 0) {
        setEvents((prev) => {
          // Avoid duplicate events if any
          return [...prev, ...resp.events];
        });
      }
    } catch (err) {
      console.error('Failed to poll device events:', err);
    }
  }, []);

  // Fetch initial devices list
  const fetchDevices = useCallback(async () => {
    try {
      setLoading(true);
      const resp = await apiClient.getDevices();
      setDevices(resp.devices || []);
      await pollEvents();
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      onError(msg);
    } finally {
      setLoading(false);
    }
  }, [onError, pollEvents]);

  useEffect(() => {
    fetchDevices();
  }, [fetchDevices]);

  // Periodic poll for events every 2 seconds
  useEffect(() => {
    const timer = setInterval(() => {
      if (!isPollingRef.current) {
        pollEvents();
      }
    }, 2000);
    return () => clearInterval(timer);
  }, [pollEvents]);

  // Connect Device
  const handleConnect = async (deviceId: string) => {
    setDeviceErrors((prev) => ({ ...prev, [deviceId]: '' }));
    setActionFeedback((prev) => ({ ...prev, [deviceId]: '' }));
    try {
      const resp = await apiClient.connectDevice(deviceId);
      setDevices((prev) =>
        prev.map((d) =>
          d.device_id === deviceId ? { ...d, state: resp.state || 'connected' } : d
        )
      );
      setActionFeedback((prev) => ({
        ...prev,
        [deviceId]: `Connected successfully (SIMULATED)`,
      }));
      await pollEvents();
    } catch (err: unknown) {
      let msg = err instanceof Error ? err.message : String(err);
      if (err instanceof WorkbenchApiError && err.status === 409) {
        msg = `${deviceId}: already connected — please disconnect first if you wish to reset.`;
      }
      setDeviceErrors((prev) => ({ ...prev, [deviceId]: msg }));
      onError(msg);
      await pollEvents();
    }
  };

  // Disconnect Device
  const handleDisconnect = async (deviceId: string) => {
    setDeviceErrors((prev) => ({ ...prev, [deviceId]: '' }));
    setActionFeedback((prev) => ({ ...prev, [deviceId]: '' }));
    try {
      const resp = await apiClient.disconnectDevice(deviceId);
      setDevices((prev) =>
        prev.map((d) =>
          d.device_id === deviceId ? { ...d, state: resp.state || 'disconnected' } : d
        )
      );
      setActionFeedback((prev) => ({
        ...prev,
        [deviceId]: `Disconnected (SIMULATED)`,
      }));
      await pollEvents();
    } catch (err: unknown) {
      let msg = err instanceof Error ? err.message : String(err);
      if (err instanceof WorkbenchApiError && err.status === 409) {
        msg = `${deviceId}: cannot disconnect — device is not currently connected.`;
      }
      setDeviceErrors((prev) => ({ ...prev, [deviceId]: msg }));
      onError(msg);
      await pollEvents();
    }
  };

  // Read Telemetry
  const handleReadTelemetry = async (deviceId: string) => {
    setDeviceErrors((prev) => ({ ...prev, [deviceId]: '' }));
    setActionFeedback((prev) => ({ ...prev, [deviceId]: '' }));
    try {
      const telem = await apiClient.readDeviceTelemetry(deviceId);
      setTelemetry((prev) => ({ ...prev, [deviceId]: telem }));
      setActionFeedback((prev) => ({
        ...prev,
        [deviceId]: `Telemetry updated (SIMULATED)`,
      }));
      // In case state transitioned
      setDevices((prev) =>
        prev.map((d) => (d.device_id === deviceId ? { ...d, state: 'connected' } : d))
      );
      setTimeoutArm((prev) => ({ ...prev, [deviceId]: false }));
      await pollEvents();
    } catch (err: unknown) {
      let msg = err instanceof Error ? err.message : String(err);
      if (err instanceof WorkbenchApiError && err.status === 409) {
        if (msg.includes('state disconnected')) {
          msg = `${deviceId}: cannot read telemetry while disconnected — please click 'Connect' first.`;
        }
      }
      setDeviceErrors((prev) => ({ ...prev, [deviceId]: msg }));
      onError(msg);
      await pollEvents();
    }
  };

  // Simulate Timeout (inject fault on next read)
  const handleInjectTimeout = async (deviceId: string) => {
    setDeviceErrors((prev) => ({ ...prev, [deviceId]: '' }));
    setActionFeedback((prev) => ({ ...prev, [deviceId]: '' }));
    try {
      await apiClient.injectDeviceTimeout(deviceId, 1);
      setTimeoutArm((prev) => ({ ...prev, [deviceId]: true }));
      setActionFeedback((prev) => ({
        ...prev,
        [deviceId]: `Simulated timeout armed! The next telemetry read will fail and trigger retry recovery (SIMULATED).`,
      }));
      await pollEvents();
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      setDeviceErrors((prev) => ({ ...prev, [deviceId]: msg }));
      onError(msg);
    }
  };

  // Format state badge color
  const renderStateBadge = (state: DeviceState) => {
    switch (state) {
      case 'connected':
        return (
          <span className="px-2 py-0.5 rounded text-[10px] font-mono font-bold bg-status-success-bg text-status-success border border-status-success/40">
            CONNECTED
          </span>
        );
      case 'connecting':
        return (
          <span className="px-2 py-0.5 rounded text-[10px] font-mono font-bold bg-status-warning-bg text-status-warning border border-status-warning/40 animate-pulse">
            CONNECTING...
          </span>
        );
      case 'faulted':
        return (
          <span className="px-2 py-0.5 rounded text-[10px] font-mono font-bold bg-status-error-bg text-status-error border border-status-error/40 animate-pulse">
            FAULTED
          </span>
        );
      case 'disconnected':
      default:
        return (
          <span className="px-2 py-0.5 rounded text-[10px] font-mono font-bold bg-surface text-text-muted border border-border-default">
            DISCONNECTED
          </span>
        );
    }
  };

  return (
    <div className="flex-1 flex flex-col h-full overflow-y-auto bg-canvas p-4 gap-4">
      {/* ── Persistent Unmissable SIMULATED Header Banner ───────────────── */}
      <div
        id="simulated-view-banner"
        role="alert"
        className="bg-status-warning-bg/40 border border-status-warning text-status-warning px-4 py-3 rounded-lg flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 text-xs font-mono shadow-sm"
      >
        <div className="flex items-center gap-2.5">
          <span className="text-lg">⚠️</span>
          <div>
            <div className="font-bold tracking-wider uppercase text-status-warning">
              SIMULATED HARDWARE ENVIRONMENT
            </div>
            <div className="text-[11px] text-text-secondary mt-0.5">
              All 3 devices, electrical voltages, die temperatures, PLL registers, and timeout-retry policies are
              software mocks. No physical hardware is accessed or modified.
            </div>
          </div>
        </div>
        <span className="px-2.5 py-1 bg-status-warning/20 border border-status-warning rounded font-bold uppercase text-[10px] tracking-widest text-status-warning self-start sm:self-auto">
          SIMULATED
        </span>
      </div>

      {/* ── Three Device Cards Grid ────────────────────────────────────── */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
        {loading && devices.length === 0 ? (
          <div className="col-span-3 text-center py-12 text-xs font-mono text-text-muted">
            Querying simulated device adapters from backend...
          </div>
        ) : (
          devices.map((device) => {
            const telem = telemetry[device.device_id];
            const isConnected = device.state === 'connected';
            const isFaulted = device.state === 'faulted';
            const isArmed = timeoutArm[device.device_id];

            return (
              <div
                key={device.device_id}
                id={`card-${device.device_id}`}
                className="bg-panel border border-border-subtle rounded-lg p-4 flex flex-col justify-between gap-3 shadow-sm hover:border-border-default transition-colors relative"
              >
                {/* Card Top: Board Name, Type, State Badge & SIMULATED Label */}
                <div>
                  <div className="flex items-start justify-between gap-2 mb-1.5">
                    <div>
                      <div className="text-[10px] uppercase font-mono tracking-wider text-text-muted">
                        Device Mock ({device.device_type.toUpperCase()})
                      </div>
                      <h3 className="text-sm font-semibold text-text-primary font-sans flex items-center gap-1.5">
                        <span>{device.board_name}</span>
                      </h3>
                    </div>
                    <div className="flex flex-col items-end gap-1">
                      {renderStateBadge(device.state)}
                      <span className="px-1.5 py-0.5 rounded text-[9px] font-mono font-bold bg-accent/15 text-accent border border-accent/30 tracking-wider">
                        SIMULATED
                      </span>
                    </div>
                  </div>

                  {/* Capabilities List */}
                  <div className="flex items-center gap-1.5 my-2">
                    <span className="text-[10px] text-text-muted font-mono">Capabilities:</span>
                    <div className="flex flex-wrap gap-1">
                      {device.capabilities.map((c) => (
                        <span
                          key={c}
                          className="px-1.5 py-0.5 rounded bg-surface border border-border-subtle text-[10px] font-mono text-text-secondary"
                        >
                          {c}
                        </span>
                      ))}
                    </div>
                  </div>

                  {/* Action Feedback Notice */}
                  {actionFeedback[device.device_id] && (
                    <div className="my-2 p-2 rounded bg-canvas border border-border-subtle text-[11px] font-mono text-text-secondary">
                      {actionFeedback[device.device_id]}
                    </div>
                  )}

                  {/* Actionable Error Box if Action Failed */}
                  {deviceErrors[device.device_id] && (
                    <div
                      role="alert"
                      className="my-2 p-2.5 rounded bg-status-error-bg border border-status-error/40 text-status-error text-xs font-mono"
                    >
                      <div className="font-semibold mb-0.5">Actionable Fault Notice:</div>
                      <div className="text-[11px] text-text-primary">{deviceErrors[device.device_id]}</div>
                    </div>
                  )}

                  {/* Telemetry Readout Box */}
                  <div className="mt-2 p-3 bg-canvas/70 rounded border border-border-subtle">
                    <div className="flex justify-between items-center mb-1.5 pb-1 border-b border-border-subtle text-[10px] font-mono text-text-muted">
                      <span>Telemetry Readout</span>
                      <span className="text-accent font-semibold">SIMULATED</span>
                    </div>

                    {!telem ? (
                      <div className="text-[11px] font-mono text-text-muted italic py-1">
                        No telemetry acquired yet. Click &ldquo;Read Telemetry&rdquo; to sample.
                      </div>
                    ) : (
                      <div className="space-y-1 text-xs font-mono">
                        {device.device_id === 'mcu' && (
                          <>
                            <div className="flex justify-between">
                              <span className="text-text-muted">Supply Voltage:</span>
                              <span className="text-text-primary font-bold">
                                {telem.supply_voltage_V !== undefined
                                  ? `${telem.supply_voltage_V.toFixed(2)} V`
                                  : '—'}
                              </span>
                            </div>
                            <div className="flex justify-between">
                              <span className="text-text-muted">Die Temperature:</span>
                              <span className="text-text-primary font-bold">
                                {telem.die_temperature_C !== undefined
                                  ? `${telem.die_temperature_C.toFixed(1)} °C`
                                  : '—'}
                              </span>
                            </div>
                          </>
                        )}

                        {device.device_id === 'fpga' && (
                          <>
                            <div className="flex justify-between">
                              <span className="text-text-muted">Configuration:</span>
                              <span
                                className={`font-bold ${
                                  telem.configuration_done ? 'text-status-success' : 'text-text-muted'
                                }`}
                              >
                                {telem.configuration_done ? 'DONE (True)' : 'Pending'}
                              </span>
                            </div>
                            <div className="flex justify-between">
                              <span className="text-text-muted">Clock PLL:</span>
                              <span
                                className={`font-bold ${
                                  telem.pll_locked ? 'text-status-success' : 'text-status-warning'
                                }`}
                              >
                                {telem.pll_locked ? 'Locked (True)' : 'Unlocked'}
                              </span>
                            </div>
                            <div className="flex justify-between">
                              <span className="text-text-muted">Status:</span>
                              <span className="text-text-secondary text-[11px]">
                                {telem.clock_status || '—'}
                              </span>
                            </div>
                          </>
                        )}

                        {device.device_id === 'jtag' && (
                          <>
                            <div className="flex justify-between">
                              <span className="text-text-muted">Target VREF:</span>
                              <span className="text-text-primary font-bold">
                                {telem.target_vref_V !== undefined ? `${telem.target_vref_V.toFixed(2)} V` : '—'}
                              </span>
                            </div>
                            <div className="flex justify-between">
                              <span className="text-text-muted">TCK Clock:</span>
                              <span className="text-text-primary font-bold">
                                {telem.clock_freq_hz !== undefined
                                  ? `${(telem.clock_freq_hz / 1000000).toFixed(1)} MHz`
                                  : '—'}
                              </span>
                            </div>
                            <div className="flex justify-between">
                              <span className="text-text-muted">Scan Chain:</span>
                              <span className="text-text-primary font-bold">
                                {telem.scan_chain_length !== undefined ? `${telem.scan_chain_length} TAP` : '—'}
                              </span>
                            </div>
                          </>
                        )}
                      </div>
                    )}
                  </div>
                </div>

                {/* Card Bottom: Action Controls */}
                <div className="flex flex-col gap-2 pt-2 border-t border-border-subtle">
                  <div className="grid grid-cols-2 gap-2">
                    <button
                      type="button"
                      id={`btn-connect-${device.device_id}`}
                      onClick={() => handleConnect(device.device_id)}
                      disabled={isConnected}
                      className="py-1.5 px-2 bg-surface border border-border-default hover:bg-hover hover:border-accent text-text-primary text-xs font-mono font-medium rounded transition-colors disabled:opacity-40"
                    >
                      Connect
                    </button>
                    <button
                      type="button"
                      id={`btn-disconnect-${device.device_id}`}
                      onClick={() => handleDisconnect(device.device_id)}
                      disabled={!isConnected && !isFaulted}
                      className="py-1.5 px-2 bg-surface border border-border-default hover:bg-hover hover:border-status-error text-text-primary text-xs font-mono font-medium rounded transition-colors disabled:opacity-40"
                    >
                      Disconnect
                    </button>
                  </div>

                  <div className="grid grid-cols-2 gap-2">
                    <button
                      type="button"
                      id={`btn-telemetry-${device.device_id}`}
                      onClick={() => handleReadTelemetry(device.device_id)}
                      className="py-1.5 px-2 bg-subtle border border-border-default hover:border-accent hover:text-accent text-text-primary text-xs font-mono font-medium rounded transition-colors"
                    >
                      Read Telemetry
                    </button>
                    <button
                      type="button"
                      id={`btn-timeout-${device.device_id}`}
                      onClick={() => handleInjectTimeout(device.device_id)}
                      className={`py-1.5 px-2 border text-xs font-mono font-medium rounded transition-colors ${
                        isArmed
                          ? 'bg-status-warning text-black border-status-warning'
                          : 'bg-status-warning/10 border-status-warning/40 hover:bg-status-warning/20 text-status-warning'
                      }`}
                    >
                      {isArmed ? 'Timeout Armed!' : 'Simulate Timeout'}
                    </button>
                  </div>
                </div>
              </div>
            );
          })
        )}
      </div>

      {/* ── Event Timeline: Timeout, Faulted, Retry, Recovered ─────────── */}
      <section aria-labelledby="timeline-heading" className="bg-panel border border-border-subtle rounded-lg p-4 shadow-sm flex flex-col gap-3">
        <div className="flex items-center justify-between pb-2 border-b border-border-subtle">
          <div className="flex items-center gap-2">
            <h2 id="timeline-heading" className="text-sm font-semibold text-text-primary font-sans">
              Device Recovery &amp; Fault Timeline
            </h2>
            <span className="px-1.5 py-0.5 rounded text-[10px] font-mono bg-surface border border-border-default text-text-muted">
              {events.length} events
            </span>
            <span className="px-1.5 py-0.5 rounded text-[9px] font-mono font-bold bg-accent/15 text-accent border border-accent/30 tracking-wider">
              SIMULATED LOG
            </span>
          </div>

          <button
            type="button"
            onClick={() => setEvents([])}
            className="text-xs font-mono text-text-muted hover:text-text-primary transition-colors"
          >
            Clear Local Log
          </button>
        </div>

        {events.length === 0 ? (
          <div className="py-8 text-center text-xs font-mono text-text-muted">
            No events recorded yet. Connect devices or simulate a timeout to observe recovery events.
          </div>
        ) : (
          <div className="max-h-72 overflow-y-auto space-y-2 pr-1 font-mono text-xs">
            {events.map((ev, index) => {
              const isTimeout = ev.description.toLowerCase().includes('timeout');
              const isRetry = ev.description.toLowerCase().includes('retry');
              const isRecovered = ev.description.toLowerCase().includes('recovered');
              const isConnect = ev.description.toLowerCase().includes('connected');

              let badgeColor = 'bg-surface text-text-secondary border-border-default';
              if (isTimeout) badgeColor = 'bg-status-error-bg text-status-error border-status-error/40';
              else if (isRetry) badgeColor = 'bg-status-warning-bg text-status-warning border-status-warning/40';
              else if (isRecovered || isConnect) badgeColor = 'bg-status-success-bg text-status-success border-status-success/40';

              const timeStr = ev.timestamp_ms
                ? new Date(ev.timestamp_ms).toLocaleTimeString() + `.${(ev.timestamp_ms % 1000).toString().padStart(3, '0')}`
                : 'API Log';

              return (
                <div
                  key={index}
                  className="flex items-start justify-between gap-3 p-2 bg-canvas/60 rounded border border-border-subtle"
                >
                  <div className="flex items-start gap-2.5">
                    <span className="px-1.5 py-0.5 rounded uppercase text-[10px] font-bold bg-panel border border-border-default text-text-primary">
                      {ev.device_id.toUpperCase()}
                    </span>
                    <span className={`px-1.5 py-0.5 rounded text-[10px] font-bold border ${badgeColor}`}>
                      {ev.new_state.toUpperCase()}
                    </span>
                    <span className="text-text-secondary text-[11px] leading-tight">
                      {ev.description}
                    </span>
                  </div>

                  <div className="flex items-center gap-2 self-start shrink-0 text-[10px] text-text-muted">
                    <span className="px-1 rounded bg-accent/10 text-accent font-semibold">SIMULATED</span>
                    <span>{timeStr}</span>
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </section>
    </div>
  );
};
