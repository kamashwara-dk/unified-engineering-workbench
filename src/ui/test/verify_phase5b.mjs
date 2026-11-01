import { spawn } from 'child_process';
import path from 'path';
import fs from 'fs';
import { fileURLToPath } from 'url';
import { chromium } from 'playwright';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const WORKBENCH_ROOT = path.resolve(__dirname, '../../..');
const SERVER_EXE = path.join(WORKBENCH_ROOT, 'build/vs2022/src/server/Release/workbench_server.exe');
const DOCS_IMG_DIR = path.join(WORKBENCH_ROOT, 'docs/img/ui');
const PORT = 9097;
const BASE_URL = `http://127.0.0.1:${PORT}`;

fs.mkdirSync(DOCS_IMG_DIR, { recursive: true });

async function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForServer(maxAttempts = 30) {
  for (let i = 0; i < maxAttempts; i++) {
    try {
      const res = await fetch(`${BASE_URL}/api/health`, {
        headers: { Host: `127.0.0.1:${PORT}` },
      });
      if (res.ok) {
        const json = await res.json();
        console.log(`[VERIFY-5B] Server ready! Version: ${json.core_version}`);
        return true;
      }
    } catch {}
    await sleep(250);
  }
  throw new Error(`Server failed to start on port ${PORT}`);
}

async function main() {
  console.log('[VERIFY-5B] Starting workbench_server on port', PORT);
  const serverProc = spawn(
    SERVER_EXE,
    ['--port', String(PORT), '--dev', '--ui-dir', 'src/ui/dist'],
    {
      cwd: WORKBENCH_ROOT,
      stdio: ['pipe', 'pipe', 'pipe'],
    }
  );

  serverProc.stdout.on('data', (d) => console.log(`[SERVER STDOUT] ${d.toString().trim()}`));
  serverProc.stderr.on('data', (d) => console.log(`[SERVER STDERR] ${d.toString().trim()}`));

  const cspViolations = [];
  const consoleErrors = [];

  try {
    await waitForServer();

    console.log('[VERIFY-5B] Launching Chromium...');
    const browser = await chromium.launch({
      headless: true,
      args: ['--no-sandbox', '--disable-setuid-sandbox'],
    });

    const context = await browser.newContext({
      viewport: { width: 1280, height: 850 },
    });

    const page = await context.newPage();

    page.on('console', (msg) => {
      const text = msg.text();
      if (msg.type() === 'error') {
        console.log(`[PAGE ERROR] ${text}`);
        consoleErrors.push(text);
      }
      if (text.toLowerCase().includes('content security policy') || text.toLowerCase().includes('csp')) {
        console.log(`[PAGE CSP WARNING/ERROR] ${text}`);
        cspViolations.push(text);
      }
    });

    page.on('pageerror', (err) => {
      console.log(`[PAGE UNHANDLED] ${err.message}`);
      consoleErrors.push(err.message);
    });

    console.log('[VERIFY-5B] Navigating to workbench...');
    await page.goto(BASE_URL, { waitUntil: 'networkidle' });
    await sleep(500);

    // Switch to Performance Tab
    console.log('[VERIFY-5B] Selecting Performance tab...');
    await page.locator('button#tab-performance').click();
    await sleep(400);

    // ── Scenario 1: Empty state & Hardware card ──────────────────────────
    console.log('[VERIFY-5B] Scenario 1: Checking Empty State & Hardware Card...');
    await page.waitForSelector('#empty-benchmark-state');
    await page.waitForSelector('#hw-heading');
    await page.waitForSelector('#card-launch-to-ready');

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '12_performance_view_empty_state.png'),
      fullPage: true,
    });
    console.log('[VERIFY-5B] Saved: 12_performance_view_empty_state.png');

    // ── Scenario 2: Calibration ──────────────────────────────────────────
    console.log('[VERIFY-5B] Scenario 2: Running Workload Calibration...');
    await page.locator('button#btn-calibrate').click();
    await page.waitForSelector('text=Calibration Advisory', { timeout: 8000 });
    await sleep(400);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '13_performance_calibration_advisory.png'),
      fullPage: true,
    });
    console.log('[VERIFY-5B] Saved: 13_performance_calibration_advisory.png');

    // ── Scenario 3: Force Invalid Spec (Inline API Validation Error) ───────
    console.log('[VERIFY-5B] Scenario 3: Testing Invalid Spec (k_max < k_min)...');
    await page.locator('input#k_min').fill('50.0');
    await page.locator('input#k_max').fill('10.0');
    await page.locator('button#btn-run-benchmark').click();

    await page.waitForSelector('text=Validation Error', { timeout: 4000 });
    await page.waitForSelector('text=k_max must be >= k_min', { timeout: 4000 });
    await sleep(300);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '14_performance_invalid_spec_error.png'),
      fullPage: true,
    });
    console.log('[VERIFY-5B] Saved: 14_performance_invalid_spec_error.png');

    // Restore valid conductivity
    await page.locator('input#k_min').fill('1.0');
    await page.locator('input#k_max').fill('100.0');

    // ── Scenario 4: Benchmark Progress & Cancellation ─────────────────────
    console.log('[VERIFY-5B] Scenario 4: Testing Benchmark Job Cancellation...');
    await page.locator('input#case_count').fill('600000');
    await page.locator('button#btn-run-benchmark').click();

    // Wait for cancel button to appear
    const cancelBtn = page.locator('button#btn-cancel');
    await cancelBtn.waitFor({ state: 'visible', timeout: 5000 });
    await sleep(150);
    await cancelBtn.click();
    console.log('[VERIFY-5B] Clicked Cancel button');
    await sleep(500);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '15_performance_benchmark_cancellation.png'),
      fullPage: true,
    });
    console.log('[VERIFY-5B] Saved: 15_performance_benchmark_cancellation.png');

    // ── Scenario 5: Successful Benchmark Run & Chart / Table ──────────────
    console.log('[VERIFY-5B] Scenario 5: Running Small Benchmark (200 cases)...');
    await page.locator('input#case_count').fill('200');
    await page.locator('button#btn-run-benchmark').click();

    // Wait for results table and Plotly chart
    await page.waitForSelector('#benchmark-results-table', { timeout: 20000 });
    await page.waitForSelector('#speedup-plot', { timeout: 10000 });
    await page.waitForSelector('#panel-honest-notes', { timeout: 10000 });
    await sleep(600);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '16_performance_benchmark_results_and_charts.png'),
      fullPage: true,
    });
    console.log('[VERIFY-5B] Saved: 16_performance_benchmark_results_and_charts.png');

    // ── Scenario 6: Single-CPU Simulation Disclosure ─────────────────────
    console.log('[VERIFY-5B] Scenario 6: Testing Single-CPU Disclosure...');
    await page.locator('input#sim_cpu').check();
    await page.locator('input#case_count').fill('100');
    await page.locator('button#btn-run-benchmark').click();

    await page.waitForSelector('#benchmark-results-table', { timeout: 20000 });
    await page.waitForSelector('text=Single-CPU Disclosure', { timeout: 10000 });
    await sleep(600);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '17_performance_single_cpu_disclosure.png'),
      fullPage: true,
    });
    console.log('[VERIFY-5B] Saved: 17_performance_single_cpu_disclosure.png');

    // ── Verification Assertions ──────────────────────────────────────────
    console.log('\n[VERIFY-5B] ── Results Summary ──');
    console.log(`CSP Violations count: ${cspViolations.length}`);
    console.log(`Console Errors count: ${consoleErrors.length}`);

    if (cspViolations.length > 0) {
      throw new Error(`Strict CSP was violated: ${JSON.stringify(cspViolations)}`);
    }

    await browser.close();
    console.log('\n[VERIFY-5B SUCCESS] All 6 Phase 5B scenarios verified with screenshots!');
  } finally {
    serverProc.kill();
  }
}

main().catch((err) => {
  console.error('[VERIFY-5B FAILED]', err);
  process.exit(1);
});

