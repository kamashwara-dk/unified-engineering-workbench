import { describe, it, expect } from 'vitest';
import {
  extractLineNumber,
  extractBatchFieldError,
  parseStlBinary,
  parsePointsBinary,
  WorkbenchApiError,
  WorkbenchNetworkError,
  WorkbenchAuthError,
} from '../src/api/client';

describe('API Client & Error Mapping', () => {
  it('extracts line numbers from various C++ error messages', () => {
    expect(extractLineNumber('line 42: malformed vertex coordinate')).toBe(42);
    expect(extractLineNumber('CSV line 105: non-finite float')).toBe(105);
    expect(extractLineNumber('Error on Row 3: missing columns')).toBe(3);
    expect(extractLineNumber('General syntax error without line')).toBeUndefined();
  });

  it('constructs WorkbenchApiError with proper fields', () => {
    const err = new WorkbenchApiError('conductivity must be positive', 422, 'conductivity_W_per_mK');
    expect(err.message).toBe('conductivity must be positive');
    expect(err.status).toBe(422);
    expect(err.field).toBe('conductivity_W_per_mK');
    expect(err.name).toBe('WorkbenchApiError');
  });

  it('constructs WorkbenchNetworkError and WorkbenchAuthError', () => {
    const netErr = new WorkbenchNetworkError('Connection refused');
    expect(netErr.name).toBe('WorkbenchNetworkError');

    const authErr = new WorkbenchAuthError('Unauthorized');
    expect(authErr.name).toBe('WorkbenchAuthError');
  });

  it('correctly parses binary STL buffers conforming to little-endian spec', () => {
    // 1 triangle = 4 + 9*4 + 3*4 = 52 bytes
    const buffer = new ArrayBuffer(52);
    const view = new DataView(buffer);

    view.setUint32(0, 1, true); // 1 triangle

    // 9 floats for vertices: (0,0,0), (1,0,0), (1,1,0)
    const positions = [0, 0, 0, 1, 0, 0, 1, 1, 0];
    for (let i = 0; i < 9; i++) {
      view.setFloat32(4 + i * 4, positions[i], true);
    }

    // 3 floats for normal: (0, 0, 1)
    const normal = [0, 0, 1];
    for (let i = 0; i < 3; i++) {
      view.setFloat32(4 + 36 + i * 4, normal[i], true);
    }

    const mesh = parseStlBinary(buffer);
    expect(mesh.triangleCount).toBe(1);
    expect(mesh.positions.length).toBe(9);
    expect(mesh.normals.length).toBe(3);
    expect(mesh.positions[3]).toBeCloseTo(1.0);
    expect(mesh.normals[2]).toBeCloseTo(1.0);
  });

  it('rejects truncated binary STL buffers', () => {
    const truncated = new ArrayBuffer(20);
    const view = new DataView(truncated);
    view.setUint32(0, 1, true); // claims 1 triangle, but buffer is only 20 bytes instead of 52
    expect(() => parseStlBinary(truncated)).toThrow('Truncated STL binary buffer');
  });

  it('correctly parses binary CSV points buffers conforming to little-endian spec', () => {
    // 2 nodes = 4 + 2 * (3*4 + 1*4 + 1*4) = 4 + 2 * 20 = 44 bytes
    const buffer = new ArrayBuffer(44);
    const view = new DataView(buffer);

    view.setUint32(0, 2, true); // 2 nodes

    // Node 0: xyz = (0, 0, 0), temp = 25.5, id = 100
    // Node 1: xyz = (1, 2, 3), temp = 80.0, id = 101

    // xyz: 6 floats starting at byte 4
    const xyz = [0, 0, 0, 1, 2, 3];
    for (let i = 0; i < 6; i++) {
      view.setFloat32(4 + i * 4, xyz[i], true);
    }

    // temps: 2 floats starting at byte 4 + 24 = 28
    view.setFloat32(28, 25.5, true);
    view.setFloat32(32, 80.0, true);

    // ids: 2 uint32 starting at byte 36
    view.setUint32(36, 100, true);
    view.setUint32(40, 101, true);

    const points = parsePointsBinary(buffer);
    expect(points.nodeCount).toBe(2);
    expect(points.xyz.length).toBe(6);
    expect(points.temperatures.length).toBe(2);
    expect(points.nodeIds.length).toBe(2);

    expect(points.temperatures[0]).toBeCloseTo(25.5);
    expect(points.temperatures[1]).toBeCloseTo(80.0);
    expect(points.nodeIds[0]).toBe(100);
    expect(points.nodeIds[1]).toBe(101);
  });

  it('maps backend batch validation error strings to correct form fields', () => {
    expect(extractBatchFieldError('seed must be an unsigned integer')).toEqual({
      field: 'seed',
      message: 'seed must be an unsigned integer',
    });
    expect(extractBatchFieldError('case_count must be positive')).toEqual({
      field: 'case_count',
      message: 'case_count must be positive',
    });
    expect(extractBatchFieldError('n_elements must be at least 1')).toEqual({
      field: 'n_elements',
      message: 'n_elements must be at least 1',
    });
    expect(extractBatchFieldError('k_max must be >= k_min')).toEqual({
      field: 'k_max',
      message: 'k_max must be >= k_min',
    });
    expect(extractBatchFieldError('q_min must be positive')).toEqual({
      field: 'q_min',
      message: 'q_min must be positive',
    });
    expect(extractBatchFieldError('total batch nodal memory exceeds limit of 10000000 nodes')).toEqual({
      field: 'case_count',
      message: 'total batch nodal memory exceeds limit of 10000000 nodes',
    });
    expect(extractBatchFieldError('generic internal server error')).toEqual({
      message: 'generic internal server error',
    });
  });
});

