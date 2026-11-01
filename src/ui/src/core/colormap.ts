/**
 * Perceptually uniform, colour-blind-safe colormap (Inferno-style LUT)
 * and legend tick mathematics for engineering thermal visualizations.
 */

// Sampled key stops from standard Matplotlib/SciPy Inferno colormap
// [r, g, b] in 0..1
const INFERNO_STOPS: [number, number, number][] = [
  [0.001462, 0.000466, 0.013866], // 0.00 deep purple / black
  [0.087411, 0.044556, 0.224824], // 0.10
  [0.258234, 0.038571, 0.406485], // 0.20 dark violet
  [0.428452, 0.098479, 0.457855], // 0.30
  [0.578304, 0.148039, 0.404411], // 0.40 magenta / crimson
  [0.716387, 0.214982, 0.307393], // 0.50
  [0.842994, 0.310243, 0.177053], // 0.60 fiery red-orange
  [0.935105, 0.439062, 0.057398], // 0.70 orange
  [0.978844, 0.613926, 0.052608], // 0.80 bright amber
  [0.963646, 0.802897, 0.264426], // 0.90 golden yellow
  [0.988362, 0.998364, 0.644924], // 1.00 pale light yellow
];

/**
 * Returns [r, g, b] in [0, 1] for normalized temperature t in [0, 1].
 */
export function infernoRgb(t: number): [number, number, number] {
  if (isNaN(t) || t <= 0) return [...INFERNO_STOPS[0]];
  if (t >= 1) return [...INFERNO_STOPS[INFERNO_STOPS.length - 1]];

  const numSegments = INFERNO_STOPS.length - 1;
  const scaled = t * numSegments;
  const idx = Math.floor(scaled);
  const frac = scaled - idx;

  const c0 = INFERNO_STOPS[idx];
  const c1 = INFERNO_STOPS[Math.min(idx + 1, numSegments)];

  return [
    c0[0] + frac * (c1[0] - c0[0]),
    c0[1] + frac * (c1[1] - c0[1]),
    c0[2] + frac * (c1[2] - c0[2]),
  ];
}

/**
 * Transforms an array of temperatures into a flat Float32Array of RGB triplets [r0, g0, b0, r1, g1, b1, ...]
 * for Three.js BufferGeometry point colors.
 */
export function buildPointsColors(
  temperatures: Float32Array,
  minTemp: number,
  maxTemp: number
): Float32Array {
  const count = temperatures.length;
  const colors = new Float32Array(count * 3);
  const range = maxTemp - minTemp;
  const invRange = range > 1e-12 ? 1.0 / range : 0.0;

  for (let i = 0; i < count; i++) {
    const temp = temperatures[i];
    const t = range > 1e-12 ? Math.max(0, Math.min(1, (temp - minTemp) * invRange)) : 0.5;
    const [r, g, b] = infernoRgb(t);
    const offset = i * 3;
    colors[offset] = r;
    colors[offset + 1] = g;
    colors[offset + 2] = b;
  }

  return colors;
}

export interface LegendTick {
  value: number;
  percent: number; // 0 to 100
  label: string;
}

/**
 * Generates evenly spaced legend ticks from API min/max values.
 */
export function generateLegendTicks(
  minTemp: number,
  maxTemp: number,
  tickCount: number = 5
): LegendTick[] {
  if (tickCount < 2) tickCount = 2;
  const ticks: LegendTick[] = [];
  const range = maxTemp - minTemp;

  for (let i = 0; i < tickCount; i++) {
    const factor = i / (tickCount - 1);
    const val = minTemp + factor * range;
    const percent = factor * 100;

    // Formatting: 2 decimal places if needed, otherwise clean
    let label: string;
    if (Math.abs(val) >= 100 || Number.isInteger(val)) {
      label = val.toFixed(1);
    } else {
      label = val.toFixed(2);
    }

    ticks.push({
      value: val,
      percent,
      label: `${label} °C`,
    });
  }

  return ticks;
}

