import { describe, it, expect } from 'vitest';
import { infernoRgb, buildPointsColors, generateLegendTicks } from '../src/core/colormap';

describe('Inferno Colormap Math', () => {
  it('returns valid RGB values within [0, 1] range for t in [0, 1]', () => {
    const testPoints = [0, 0.25, 0.5, 0.75, 1.0, -0.1, 1.2];
    for (const t of testPoints) {
      const [r, g, b] = infernoRgb(t);
      expect(r).toBeGreaterThanOrEqual(0);
      expect(r).toBeLessThanOrEqual(1);
      expect(g).toBeGreaterThanOrEqual(0);
      expect(g).toBeLessThanOrEqual(1);
      expect(b).toBeGreaterThanOrEqual(0);
      expect(b).toBeLessThanOrEqual(1);
    }
  });

  it('correctly clamps out-of-range inputs', () => {
    const cLow = infernoRgb(-10);
    const c0 = infernoRgb(0);
    expect(cLow).toEqual(c0);

    const cHigh = infernoRgb(10);
    const c1 = infernoRgb(1);
    expect(cHigh).toEqual(c1);
  });

  it('correctly maps temperatures to RGB buffer using API min and max', () => {
    const temps = new Float32Array([10, 20, 30]);
    const minTemp = 10;
    const maxTemp = 30;

    const colors = buildPointsColors(temps, minTemp, maxTemp);
    expect(colors.length).toBe(9); // 3 points * 3 components

    // First point is at minTemp (t=0)
    const c0 = infernoRgb(0);
    expect(colors[0]).toBeCloseTo(c0[0], 5);
    expect(colors[1]).toBeCloseTo(c0[1], 5);
    expect(colors[2]).toBeCloseTo(c0[2], 5);

    // Last point is at maxTemp (t=1)
    const c1 = infernoRgb(1);
    expect(colors[6]).toBeCloseTo(c1[0], 5);
    expect(colors[7]).toBeCloseTo(c1[1], 5);
    expect(colors[8]).toBeCloseTo(c1[2], 5);
  });

  it('handles degenerate minTemp == maxTemp gracefully', () => {
    const temps = new Float32Array([50, 50, 50]);
    const colors = buildPointsColors(temps, 50, 50);
    expect(colors.length).toBe(9);
    for (let i = 0; i < 9; i++) {
      expect(Number.isFinite(colors[i])).toBe(true);
    }
  });

  it('generates accurate legend ticks with °C unit', () => {
    const ticks = generateLegendTicks(20.0, 100.0, 5);
    expect(ticks.length).toBe(5);
    expect(ticks[0].value).toBeCloseTo(20.0);
    expect(ticks[0].percent).toBe(0);
    expect(ticks[0].label).toContain('°C');

    expect(ticks[4].value).toBeCloseTo(100.0);
    expect(ticks[4].percent).toBe(100);
    expect(ticks[4].label).toContain('°C');

    expect(ticks[2].value).toBeCloseTo(60.0);
    expect(ticks[2].percent).toBe(50);
  });
});

