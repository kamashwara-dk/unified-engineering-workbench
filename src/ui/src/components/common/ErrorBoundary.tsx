import { Component, ErrorInfo, ReactNode } from 'react';

interface Props {
  children: ReactNode;
}

interface State {
  hasError: boolean;
  error: Error | null;
  errorInfo: ErrorInfo | null;
}

export class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = {
      hasError: false,
      error: null,
      errorInfo: null,
    };
  }

  static getDerivedStateFromError(error: Error): State {
    return {
      hasError: true,
      error,
      errorInfo: null,
    };
  }

  componentDidCatch(error: Error, errorInfo: ErrorInfo) {
    console.error('ErrorBoundary caught an unhandled render error:', error, errorInfo);
    this.setState({ errorInfo });
  }

  handleReload = () => {
    window.location.reload();
  };

  render() {
    if (this.state.hasError) {
      return (
        <div className="flex flex-col items-center justify-center min-h-screen bg-canvas p-6 text-text-primary">
          <div className="max-w-xl w-full bg-card border border-status-error/40 rounded-lg p-6 shadow-modal">
            <div className="flex items-center gap-3 text-status-error mb-4">
              <svg className="w-6 h-6 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
              </svg>
              <h1 className="text-lg font-semibold font-mono">Application Error</h1>
            </div>

            <p className="text-text-secondary text-sm mb-4">
              An unexpected render fault occurred in the engineering workbench interface.
            </p>

            {this.state.error && (
              <div className="bg-canvas border border-border-default rounded p-3 mb-4 font-mono text-xs text-status-error overflow-auto max-h-48 whitespace-pre-wrap">
                {this.state.error.toString()}
                {this.state.errorInfo?.componentStack && (
                  <div className="mt-2 text-text-muted">
                    {this.state.errorInfo.componentStack}
                  </div>
                )}
              </div>
            )}

            <button
              onClick={this.handleReload}
              className="px-4 py-2 bg-accent hover:bg-accent-hover text-white text-sm font-medium rounded transition-colors"
            >
              Reload Workbench
            </button>
          </div>
        </div>
      );
    }

    return this.props.children;
  }
}
