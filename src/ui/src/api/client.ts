import {
  HealthResponse,
  StlValidationSummary,
  CsvValidationSummary,
  RodProblemParams,
  JobStatusResponse,
  StlBinaryMesh,
  CsvBinaryPoints,
  BatchSpecParams,
  DevicesResponse,
  DeviceActionResponse,
  DeviceTelemetry,
  DeviceEventsResponse,
  BackpropJobParams,
} from './types';

export class WorkbenchApiError extends Error {
  public status: number;
  public field?: string;
  public lineNumber?: number;

  constructor(message: string, status: number, field?: string, lineNumber?: number) {
    super(message);
    this.name = 'WorkbenchApiError';
    this.status = status;
    this.field = field;
    this.lineNumber = lineNumber;
  }
}

export class WorkbenchNetworkError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'WorkbenchNetworkError';
  }
}

export class WorkbenchAuthError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'WorkbenchAuthError';
  }
}

/**
 * Extracts line number from backend error messages like:
 * "line 15: malformed vertex"
 * "CSV line 10: non-finite float"
 * "vertex count on line 42 is invalid"
 */
export function extractLineNumber(errorMsg: string): number | undefined {
  const match = errorMsg.match(/(?:line|row)\s*(\d+)/i);
  if (match && match[1]) {
    const num = parseInt(match[1], 10);
    return isNaN(num) ? undefined : num;
  }
  return undefined;
}

export function extractBatchFieldError(errorMsg: string): { field?: keyof BatchSpecParams; message: string } {
  const msg = errorMsg.trim();
  if (/^seed\b/i.test(msg)) return { field: 'seed', message: msg };
  if (/^case_count\b/i.test(msg)) return { field: 'case_count', message: msg };
  if (/^n_elements\b/i.test(msg)) return { field: 'n_elements', message: msg };
  if (/^k_min\b/i.test(msg)) return { field: 'k_min', message: msg };
  if (/^k_max\b/i.test(msg)) return { field: 'k_max', message: msg };
  if (/^q_min\b/i.test(msg)) return { field: 'q_min', message: msg };
  if (/^q_max\b/i.test(msg)) return { field: 'q_max', message: msg };
  if (/nodal memory/i.test(msg)) return { field: 'case_count', message: msg };
  return { message: msg };
}

export class WorkbenchApiClient {
  private baseUrl: string;

  constructor(baseUrl: string = '') {
    this.baseUrl = baseUrl;
  }

  private async request<T>(
    endpoint: string,
    options: RequestInit = {}
  ): Promise<T> {
    const url = `${this.baseUrl}${endpoint}`;
    let res: Response;

    try {
      res = await fetch(url, options);
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      throw new WorkbenchNetworkError(
        `Backend server unreachable at ${url}: ${msg}. Ensure workbench_server is running.`
      );
    }

    if (res.status === 401 || res.status === 403) {
      throw new WorkbenchAuthError(
        res.status === 401
          ? 'Session unauthorized (missing or expired wb_session token cookie).'
          : 'Access forbidden (invalid launch token).'
      );
    }

    if (!res.ok) {
      let errorText = await res.text();
      let field: string | undefined;

      try {
        const json = JSON.parse(errorText);
        if (json.error) {
          errorText = json.error;
        }
        if (json.field) {
          field = json.field;
        }
      } catch {
        // use raw text
      }

      const line = extractLineNumber(errorText);
      throw new WorkbenchApiError(errorText, res.status, field, line);
    }

    return (await res.json()) as T;
  }

  public async getHealth(): Promise<HealthResponse> {
    return this.request<HealthResponse>('/api/health');
  }

  public async uploadStl(fileContent: string | ArrayBuffer): Promise<StlValidationSummary> {
    const body = typeof fileContent === 'string' ? fileContent : new Uint8Array(fileContent);
    return this.request<StlValidationSummary>('/api/files/stl', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/octet-stream',
      },
      body: body as any,
    });
  }

  public async uploadCsv(fileContent: string | ArrayBuffer): Promise<CsvValidationSummary> {
    const body = typeof fileContent === 'string' ? fileContent : new Uint8Array(fileContent);
    return this.request<CsvValidationSummary>('/api/files/csv', {
      method: 'POST',
      headers: {
        'Content-Type': 'text/plain',
      },
      body: body as any,
    });
  }

  public async getStlBinary(fileId: string): Promise<StlBinaryMesh> {
    const url = `${this.baseUrl}/api/files/${fileId}/stl.bin`;
    let res: Response;
    try {
      res = await fetch(url);
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      throw new WorkbenchNetworkError(`Failed to fetch STL binary from ${url}: ${msg}`);
    }

    if (!res.ok) {
      throw new WorkbenchApiError(`Failed to fetch STL binary: HTTP ${res.status}`, res.status);
    }

    const buffer = await res.arrayBuffer();
    return parseStlBinary(buffer);
  }

  public async getPointsBinary(fileId: string): Promise<CsvBinaryPoints> {
    const url = `${this.baseUrl}/api/files/${fileId}/points.bin`;
    let res: Response;
    try {
      res = await fetch(url);
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      throw new WorkbenchNetworkError(`Failed to fetch Points binary from ${url}: ${msg}`);
    }

    if (!res.ok) {
      throw new WorkbenchApiError(`Failed to fetch Points binary: HTTP ${res.status}`, res.status);
    }

    const buffer = await res.arrayBuffer();
    return parsePointsBinary(buffer);
  }

  public async createFemJob(params?: RodProblemParams): Promise<{ id: string; state: string }> {
    return this.request<{ id: string; state: string }>('/api/jobs', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        kind: 'fem',
        params: params || {},
      }),
    });
  }

  public async getJob(jobId: string): Promise<JobStatusResponse> {
    return this.request<JobStatusResponse>(`/api/jobs/${jobId}`);
  }

  public async cancelJob(jobId: string): Promise<{ cancelled: boolean }> {
    const res = await fetch(`${this.baseUrl}/api/jobs/${jobId}`, {
      method: 'DELETE',
    });
    if (!res.ok) {
      throw new WorkbenchApiError(`Failed to cancel job: HTTP ${res.status}`, res.status);
    }
    return (await res.json()) as { cancelled: boolean };
  }

  public getSolutionCsvUrl(jobId: string): string {
    return `${this.baseUrl}/api/jobs/${jobId}/export/solution.csv`;
  }

  public getConvergenceCsvUrl(jobId: string): string {
    return `${this.baseUrl}/api/jobs/${jobId}/export/convergence.csv`;
  }

  public async createBenchmarkJob(params: BatchSpecParams): Promise<{ id: string; state: string }> {
    return this.request<{ id: string; state: string }>('/api/jobs', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        kind: 'benchmark',
        params,
      }),
    });
  }

  public async createCalibrateJob(params: BatchSpecParams): Promise<{ id: string; state: string }> {
    return this.request<{ id: string; state: string }>('/api/jobs', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        kind: 'calibrate',
        params,
      }),
    });
  }

  public getBenchmarkCsvUrl(jobId: string): string {
    return `${this.baseUrl}/api/jobs/${jobId}/export/benchmark.csv`;
  }

  public getBenchmarkJsonUrl(jobId: string): string {
    return `${this.baseUrl}/api/jobs/${jobId}/export/benchmark.json`;
  }

  public async getDevices(): Promise<DevicesResponse> {
    return this.request<DevicesResponse>('/api/devices');
  }

  public async connectDevice(id: string): Promise<DeviceActionResponse> {
    return this.request<DeviceActionResponse>(`/api/devices/${id}/connect`, {
      method: 'POST',
    });
  }

  public async disconnectDevice(id: string): Promise<DeviceActionResponse> {
    return this.request<DeviceActionResponse>(`/api/devices/${id}/disconnect`, {
      method: 'POST',
    });
  }

  public async readDeviceTelemetry(id: string): Promise<DeviceTelemetry> {
    return this.request<DeviceTelemetry>(`/api/devices/${id}/telemetry`, {
      method: 'POST',
    });
  }

  public async injectDeviceTimeout(id: string, onNthRead: number = 1): Promise<DeviceActionResponse> {
    return this.request<DeviceActionResponse>(`/api/devices/${id}/inject-timeout`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ on_nth_read: onNthRead }),
    });
  }

  public async getDeviceEvents(): Promise<DeviceEventsResponse> {
    return this.request<DeviceEventsResponse>('/api/devices/events');
  }

  public async createBackpropJob(params: BackpropJobParams = {}): Promise<{ id: string; state: string }> {
    return this.request<{ id: string; state: string }>('/api/jobs', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        kind: 'backprop',
        params,
      }),
    });
  }

  public getBackpropCsvUrl(jobId: string): string {
    return `${this.baseUrl}/api/jobs/${jobId}/export/backprop.csv`;
  }
}

/**
 * Binary parsers for little-endian layouts defined in docs/API.md:
 * STL: [n_tri:u32][positions: f32 * 9 * n_tri][normals: f32 * 3 * n_tri]
 */
export function parseStlBinary(buffer: ArrayBuffer): StlBinaryMesh {
  if (buffer.byteLength < 4) {
    throw new Error('STL binary buffer too small for triangle count header');
  }

  const view = new DataView(buffer);
  const triangleCount = view.getUint32(0, true);
  const expectedBytes = 4 + triangleCount * 48; // 4 + 9*4 + 3*4 = 4 + 48*n_tri

  if (buffer.byteLength < expectedBytes) {
    throw new Error(
      `Truncated STL binary buffer: expected ${expectedBytes} bytes, got ${buffer.byteLength}`
    );
  }

  // Float32Array offsets must be multiples of 4 (4-byte alignment guaranteed here)
  const posCount = triangleCount * 9;
  const normCount = triangleCount * 3;

  const positions = new Float32Array(buffer, 4, posCount);
  const normals = new Float32Array(buffer, 4 + posCount * 4, normCount);

  return {
    triangleCount,
    positions,
    normals,
  };
}

/**
 * CSV Points: [n_nodes:u32][xyz: f32 * 3 * n_nodes][temp: f32 * n_nodes][id: u32 * n_nodes]
 */
export function parsePointsBinary(buffer: ArrayBuffer): CsvBinaryPoints {
  if (buffer.byteLength < 4) {
    throw new Error('Points binary buffer too small for node count header');
  }

  const view = new DataView(buffer);
  const nodeCount = view.getUint32(0, true);
  const expectedBytes = 4 + nodeCount * 20; // 4 + 3*4 + 1*4 + 1*4 = 4 + 20*n_nodes

  if (buffer.byteLength < expectedBytes) {
    throw new Error(
      `Truncated Points binary buffer: expected ${expectedBytes} bytes, got ${buffer.byteLength}`
    );
  }

  const xyzCount = nodeCount * 3;
  const tempCount = nodeCount;
  const idCount = nodeCount;

  let byteOffset = 4;
  const xyz = new Float32Array(buffer, byteOffset, xyzCount);
  byteOffset += xyzCount * 4;

  const temperatures = new Float32Array(buffer, byteOffset, tempCount);
  byteOffset += tempCount * 4;

  const nodeIds = new Uint32Array(buffer, byteOffset, idCount);

  return {
    nodeCount,
    xyz,
    temperatures,
    nodeIds,
  };
}

export const apiClient = new WorkbenchApiClient();

