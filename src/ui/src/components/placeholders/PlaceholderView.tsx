import React from 'react';

interface Props {
  title: string;
  phase: string;
  description: string;
}

export const PlaceholderView: React.FC<Props> = ({ title, phase, description }) => {
  return (
    <div className="flex-1 flex flex-col items-center justify-center p-8 bg-canvas text-center">
      <div className="max-w-md w-full bg-panel border border-border-default rounded-lg p-6 shadow-panel flex flex-col items-center">
        <div className="w-10 h-10 rounded-full bg-subtle flex items-center justify-center text-accent mb-3 font-mono font-bold text-sm border border-border-subtle">
          {phase}
        </div>
        <h2 className="text-base font-semibold font-mono text-text-primary mb-1">
          {title}
        </h2>
        <span className="text-xs font-mono text-status-warning bg-status-warning-bg px-2 py-0.5 rounded border border-status-warning/30 mb-3">
          Arrives in {phase}
        </span>
        <p className="text-xs text-text-secondary leading-relaxed">
          {description}
        </p>
      </div>
    </div>
  );
};

