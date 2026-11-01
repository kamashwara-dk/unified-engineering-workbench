declare module 'plotly.js-cartesian-dist-min' {
  namespace Plotly {
    export interface Data {
      x?: (number | string)[];
      y?: (number | string)[];
      name?: string;
      mode?: string;
      type?: string;
      line?: {
        color?: string;
        width?: number;
        dash?: string;
      };
      marker?: {
        size?: number;
        symbol?: string;
        color?: string;
      };
      [key: string]: any;
    }

    export interface Layout {
      title?: string | { text: string; font?: { family?: string; size?: number; color?: string } };
      paper_bgcolor?: string;
      plot_bgcolor?: string;
      font?: { family?: string; size?: number; color?: string };
      margin?: { l?: number; r?: number; t?: number; b?: number };
      xaxis?: {
        title?: string | { text: string };
        type?: string;
        gridcolor?: string;
        zerolinecolor?: string;
        autorange?: string | boolean;
        [key: string]: any;
      };
      yaxis?: {
        title?: string | { text: string };
        type?: string;
        gridcolor?: string;
        zerolinecolor?: string;
        [key: string]: any;
      };
      legend?: {
        orientation?: string;
        y?: number;
        x?: number;
        font?: { size?: number };
      };
      autosize?: boolean;
      [key: string]: any;
    }

    export interface Config {
      responsive?: boolean;
      displayModeBar?: boolean;
      [key: string]: any;
    }

    export interface DownloadImageOptions {
      format: 'png' | 'svg' | 'jpeg' | 'webp';
      width?: number;
      height?: number;
      filename?: string;
    }

    export function newPlot(
      root: HTMLElement | string,
      data: Data[],
      layout?: Partial<Layout>,
      config?: Partial<Config>
    ): Promise<any>;

    export function downloadImage(
      root: HTMLElement | string,
      options: DownloadImageOptions
    ): Promise<string>;

    export namespace Plots {
      export function resize(root: HTMLElement | string): void;
    }
  }

  const Plotly: typeof Plotly;
  export default Plotly;
}

