/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        canvas: 'var(--bg-canvas)',
        panel: 'var(--bg-panel)',
        card: 'var(--bg-card)',
        subtle: 'var(--bg-subtle)',
        input: 'var(--bg-input)',
        border: {
          subtle: 'var(--border-subtle)',
          default: 'var(--border-default)',
          strong: 'var(--border-strong)',
        },
        text: {
          primary: 'var(--text-primary)',
          secondary: 'var(--text-secondary)',
          muted: 'var(--text-muted)',
        },
        accent: {
          subtle: 'var(--accent-subtle)',
          DEFAULT: 'var(--accent-default)',
          hover: 'var(--accent-hover)',
          active: 'var(--accent-active)',
          fg: 'var(--accent-fg)',
        },
        status: {
          success: 'var(--color-success)',
          warning: 'var(--color-warning)',
          error: 'var(--color-error)',
          info: 'var(--color-info)',
        }
      },
      fontFamily: {
        sans: ['"IBM Plex Sans"', 'system-ui', '-apple-system', 'sans-serif'],
        mono: ['"IBM Plex Mono"', 'ui-monospace', 'Menlo', 'monospace'],
      },
      borderRadius: {
        xs: 'var(--radius-xs)',
        sm: 'var(--radius-sm)',
        md: 'var(--radius-md)',
        lg: 'var(--radius-lg)',
      },
      boxShadow: {
        panel: 'var(--shadow-sm)',
        dropdown: 'var(--shadow-md)',
        modal: 'var(--shadow-lg)',
      }
    },
  },
  plugins: [],
};

