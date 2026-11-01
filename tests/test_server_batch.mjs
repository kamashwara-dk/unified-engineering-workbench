import { spawn } from 'child_process';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const WORKBENCH_ROOT = path.resolve(__dirname, '..');
const SERVER_EXE = path.join(WORKBENCH_ROOT, 'build/vs2022/src/server/Release/workbench_server.exe');
const PORT = 9098;
const BASE_URL = `http://127.0.0.1:${PORT}`;

async function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForServer() {
  for (let i = 0; i < 30; i++) {
    try {
      const res = await fetch(`${BASE_URL}/api/health`, {
        headers: { Host: `127.0.0.1:${PORT}` },
      });
      if (res.ok) return true;
    } catch {}
    await sleep(200);
  }
  throw new Error('Server start timeout');
}

async function run() {
  console.log('[TEST] Starting server on port', PORT);
  const server = spawn(
    SERVER_EXE,
    ['--port', String(PORT), '--dev', '--ui-dir', 'src/ui/dist'],
    { cwd: WORKBENCH_ROOT }
  );

  try {
    await waitForServer();
    console.log('[TEST] Server started successfully!');

    // 1. Test POST /api/jobs with kind: "calibrate"
    console.log('[TEST] Submitting calibrate job...');
    const calibRes = await fetch(`${BASE_URL}/api/jobs`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Host: `127.0.0.1:${PORT}` },
      body: JSON.stringify({
        kind: 'calibrate',
        params: { seed: 1337, n_elements: 10 },
      }),
    });
    if (!calibRes.ok) throw new Error(`Calibrate failed: HTTP ${calibRes.status}`);
    const calibJob = await calibRes.json();
    console.log('[TEST] Calibrate Job submitted:', calibJob.id);

    // Poll until complete
    let calibResult = null;
    for (let i = 0; i < 20; i++) {
      const r = await fetch(`${BASE_URL}/api/jobs/${calibJob.id}`, {
        headers: { Host: `127.0.0.1:${PORT}` },
      });
      const data = await r.json();
      if (data.state === 'succeeded') {
        calibResult = data.result;
        break;
      }
      await sleep(150);
    }
    console.log('[TEST] Calibration result:', calibResult);
    if (!calibResult || !calibResult.suggested_case_count) {
      throw new Error('Calibrate result missing suggested_case_count');
    }

    // 2. Test POST /api/jobs with kind: "batch"
    console.log('[TEST] Submitting batch job (30 cases, 4 workers)...');
    const batchRes = await fetch(`${BASE_URL}/api/jobs`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Host: `127.0.0.1:${PORT}` },
      body: JSON.stringify({
        kind: 'batch',
        params: { seed: 42, case_count: 30, n_elements: 10, workers: 4 },
      }),
    });
    if (!batchRes.ok) throw new Error(`Batch submit failed: HTTP ${batchRes.status}`);
    const batchJob = await batchRes.json();

    let batchResult = null;
    for (let i = 0; i < 20; i++) {
      const r = await fetch(`${BASE_URL}/api/jobs/${batchJob.id}`, {
        headers: { Host: `127.0.0.1:${PORT}` },
      });
      const data = await r.json();
      if (data.state === 'succeeded') {
        batchResult = data.result;
        break;
      }
      await sleep(150);
    }
    console.log('[TEST] Batch result:', batchResult);
    if (!batchResult || batchResult.max_diff_vs_serial > 1e-8) {
      throw new Error('Batch agreement check failed!');
    }

    // 3. Test POST /api/jobs with kind: "benchmark"
    console.log('[TEST] Submitting benchmark job...');
    const benchRes = await fetch(`${BASE_URL}/api/jobs`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Host: `127.0.0.1:${PORT}` },
      body: JSON.stringify({
        kind: 'benchmark',
        params: { seed: 777, case_count: 15, n_elements: 5 },
      }),
    });
    if (!benchRes.ok) throw new Error(`Benchmark submit failed: HTTP ${benchRes.status}`);
    const benchJob = await benchRes.json();

    let benchResult = null;
    for (let i = 0; i < 30; i++) {
      const r = await fetch(`${BASE_URL}/api/jobs/${benchJob.id}`, {
        headers: { Host: `127.0.0.1:${PORT}` },
      });
      const data = await r.json();
      if (data.state === 'succeeded') {
        benchResult = data.result;
        break;
      }
      await sleep(200);
    }
    console.log('[TEST] Benchmark completed! CPU:', benchResult.hardware.cpu_brand);
    console.log('[TEST] Workers evaluated:', benchResult.worker_results.map((w) => w.worker_count));

    // 4. Test export endpoints
    const csvExport = await fetch(`${BASE_URL}/api/jobs/${benchJob.id}/export/benchmark.csv`, {
      headers: { Host: `127.0.0.1:${PORT}` },
    });
    if (!csvExport.ok) throw new Error('Benchmark CSV export failed');
    const csvText = await csvExport.text();
    console.log('[TEST] Benchmark CSV export lines:', csvText.trim().split('\n').length);

    const jsonExport = await fetch(`${BASE_URL}/api/jobs/${benchJob.id}/export/benchmark.json`, {
      headers: { Host: `127.0.0.1:${PORT}` },
    });
    if (!jsonExport.ok) throw new Error('Benchmark JSON export failed');
    const jsonText = await jsonExport.text();
    console.log('[TEST] Benchmark JSON export bytes:', jsonText.length);

    console.log('\n[TEST SUCCESS] All server batch & benchmark routes verified!');
  } finally {
    server.kill();
  }
}

run().catch((e) => {
  console.error('[TEST FAILED]', e);
  process.exit(1);
});

