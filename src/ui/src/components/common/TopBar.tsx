import React, { useRef } from 'react';

export type ViewTab = 'geometry' | 'thermal' | 'performance' | 'hardware';

interface Props {
  currentTab: ViewTab;
  onSelectTab: (tab: ViewTab) => void;
  isDark: boolean;
  onToggleTheme: () => void;
  coreVersion?: string;
}

interface TabDef {
  id: ViewTab;
  label: string;
  badge?: string;
}

const TABS: TabDef[] = [
  { id: 'geometry', label: 'Geometry' },
  { id: 'thermal', label: 'Thermal Analysis' },
  { id: 'performance', label: 'Performance', badge: 'Phase 5' },
  { id: 'hardware', label: 'Hardware', badge: 'Phase 6' },
];

export const TopBar: React.FC<Props> = ({
  currentTab,
  onSelectTab,
  isDark,
  onToggleTheme,
  coreVersion,
}) => {
  const tabRefs = useRef<(HTMLButtonElement | null)[]>([]);

  const handleKeyDown = (e: React.KeyboardEvent, index: number) => {
    let targetIndex = -1;
    if (e.key === 'ArrowRight') {
      targetIndex = (index + 1) % TABS.length;
    } else if (e.key === 'ArrowLeft') {
      targetIndex = (index - 1 + TABS.length) % TABS.length;
    } else if (e.key === 'Home') {
      targetIndex = 0;
    } else if (e.key === 'End') {
      targetIndex = TABS.length - 1;
    }

    if (targetIndex >= 0) {
      e.preventDefault();
      onSelectTab(TABS[targetIndex].id);
      tabRefs.current[targetIndex]?.focus();
    }
  };

  return (
    <header className="h-14 bg-panel border-b border-border-subtle flex items-center justify-between px-4 select-none z-20">
      {/* Brand & Instrument Title */}
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-2">
          <div className="w-6 h-6 rounded bg-accent flex items-center justify-center font-mono font-bold text-xs text-white">
            W
          </div>
          <span className="font-semibold text-sm tracking-wide text-text-primary">
            Unified Engineering Workbench
          </span>
        </div>
        {coreVersion && (
          <span className="text-[11px] font-mono px-1.5 py-0.5 rounded bg-subtle text-text-secondary border border-border-subtle tabular-nums">
            v{coreVersion}
          </span>
        )}
      </div>

      {/* Navigation Tabs (role="tablist" with keyboard arrows) */}
      <nav
        role="tablist"
        aria-label="Workbench primary views"
        className="flex items-center bg-canvas p-1 rounded-md border border-border-subtle"
      >
        {TABS.map((tab, idx) => {
          const isSelected = currentTab === tab.id;
          return (
            <button
              key={tab.id}
              ref={(el) => (tabRefs.current[idx] = el)}
              role="tab"
              id={`tab-${tab.id}`}
              aria-selected={isSelected}
              aria-controls={`panel-${tab.id}`}
              tabIndex={isSelected ? 0 : -1}
              onClick={() => onSelectTab(tab.id)}
              onKeyDown={(e) => handleKeyDown(e, idx)}
              className={`flex items-center gap-2 px-3 py-1.5 rounded text-xs font-medium transition-all ${
                isSelected
                  ? 'bg-panel text-accent font-semibold shadow-panel border border-border-default'
                  : 'text-text-secondary hover:text-text-primary hover:bg-subtle'
              }`}
            >
              <span>{tab.label}</span>
              {tab.badge && (
                <span className="text-[10px] px-1 py-0.2 rounded bg-subtle text-text-secondary border border-border-default font-mono">
                  {tab.badge}
                </span>
              )}
            </button>
          );
        })}
      </nav>

      {/* Right Utility Controls */}
      <div className="flex items-center gap-3">
        <button
          onClick={onToggleTheme}
          title={`Switch to ${isDark ? 'Light' : 'Dark'} Theme`}
          aria-label={`Switch to ${isDark ? 'Light' : 'Dark'} Theme`}
          className="p-1.5 rounded text-text-secondary hover:text-text-primary hover:bg-subtle border border-border-subtle text-xs flex items-center gap-1.5"
        >
          {isDark ? (
            <>
              <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z" />
              </svg>
              <span className="hidden sm:inline">Light</span>
            </>
          ) : (
            <>
              <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M20.354 15.354A9 9 0 018.646 3.646 9.003 9.003 0 0012 21a9.003 9.003 0 008.354-5.646z" />
              </svg>
              <span className="hidden sm:inline">Dark</span>
            </>
          )}
        </button>
      </div>
    </header>
  );
};

