import React, { useState, useEffect, useCallback } from 'react';
import { TopBar, ViewTab } from './components/common/TopBar';
import { StatusBar, BackendStatus, ActiveJobStatus } from './components/common/StatusBar';
import { ViewTransition } from './components/common/ViewTransition';
import { GeometryView } from './components/geometry/GeometryView';
import { ThermalView } from './components/thermal/ThermalView';
import { PerformanceView } from './components/performance/PerformanceView';
import { HardwareView } from './components/hardware/HardwareView';
import { apiClient } from './api/client';
import { HardwareInfo } from './api/types';

export const App: React.FC = () => {
  const [currentTab, setCurrentTab] = useState<ViewTab>('geometry');
  const [isDark, setIsDark] = useState<boolean>(true);
  const [lastError, setLastError] = useState<string | null>(null);
  const [hardware, setHardware] = useState<HardwareInfo | undefined>(undefined);
  const [launchToReadyMs, setLaunchToReadyMs] = useState<number | null>(null);

  const [backend, setBackend] = useState<BackendStatus>({
    connected: false,
    checking: true,
  });

  const [job, setJob] = useState<ActiveJobStatus>({
    state: 'idle',
  });

  // Check backend health
  const checkHealth = useCallback(async () => {
    try {
      const h = await apiClient.getHealth();
      setBackend({
        connected: true,
        checking: false,
        uptimeMs: h.uptime_ms,
        coreVersion: h.core_version,
      });
      if (h.hardware) setHardware(h.hardware);
      if (h.launch_to_ready_ms !== undefined && h.launch_to_ready_ms !== null) {
        setLaunchToReadyMs(h.launch_to_ready_ms);
      }
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      setBackend({
        connected: false,
        checking: false,
        error: msg,
      });
    }
  }, []);

  useEffect(() => {
    checkHealth();
    const timer = setInterval(checkHealth, 5000);
    return () => clearInterval(timer);
  }, [checkHealth]);

  const handleToggleTheme = () => {
    setIsDark((prev) => {
      const next = !prev;
      if (next) {
        document.documentElement.classList.remove('light');
      } else {
        document.documentElement.classList.add('light');
      }
      return next;
    });
  };

  const handleJobStateChange = (state: ActiveJobStatus['state'], progress?: number) => {
    setJob({ state, progress });
  };

  return (
    <div className={`flex flex-col h-screen w-screen overflow-hidden ${isDark ? '' : 'light'}`}>
      {/* Top Header with Tab Navigation */}
      <TopBar
        currentTab={currentTab}
        onSelectTab={setCurrentTab}
        isDark={isDark}
        onToggleTheme={handleToggleTheme}
        coreVersion={backend.coreVersion}
      />

      {/* Main View Area with Framer Motion View Transitions */}
      <div className="flex-1 relative flex flex-col overflow-hidden">
        <ViewTransition viewKey={currentTab}>
          <div
            role="tabpanel"
            id={`panel-${currentTab}`}
            aria-labelledby={`tab-${currentTab}`}
            className="w-full h-full flex flex-col flex-1 overflow-hidden"
          >
            {currentTab === 'geometry' && (
              <GeometryView onError={setLastError} />
            )}

            {currentTab === 'thermal' && (
              <ThermalView
                onError={setLastError}
                onJobStateChange={handleJobStateChange}
              />
            )}

            {currentTab === 'performance' && (
              <PerformanceView
                initialHardware={hardware}
                initialLaunchToReadyMs={launchToReadyMs}
                onError={setLastError}
                onJobStateChange={handleJobStateChange}
              />
            )}

            {currentTab === 'hardware' && (
              <HardwareView onError={setLastError} />
            )}
          </div>
        </ViewTransition>
      </div>

      {/* Persistent Status Bar */}
      <StatusBar
        backend={backend}
        job={job}
        lastError={lastError}
        onClearError={() => setLastError(null)}
      />
    </div>
  );
};

