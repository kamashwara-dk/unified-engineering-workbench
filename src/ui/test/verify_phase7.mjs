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
const PORT = 9098;
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
        console.log(`[VERIFY-7] Server ready! Version: ${json.core_version}`);
        return true;
      }
    } catch {}
    await sleep(250);
  }
  throw new Error(`Server failed to start on port ${PORT}`);
}

async function main() {
  console.log('[VERIFY-7] Starting workbench_server on port', PORT);
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

    console.log('[VERIFY-7] Launching Chromium...');
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

    console.log('[VERIFY-7] Navigating to workbench...');
    await page.goto(BASE_URL, { waitUntil: 'networkidle' });
    await sleep(400);

    // Switch to Performance Tab
    console.log('[VERIFY-7] Selecting Performance tab...');
    await page.locator('button#tab-performance').click();
    await sleep(400);

    // Switch to Learning Demo (Backprop) Sub-tab
    console.log('[VERIFY-7] Selecting Learning Demo (Backprop) sub-tab...');
    await page.locator('button#tab-sub-backprop').click();
    await sleep(400);

    // ── Scenario 1: Initial View & Configuration ──────────────────────────────
    console.log('[VERIFY-7] Scenario 1: Verifying Initial State & Model Equations...');
    await page.waitForSelector('#bp_x');
    await page.waitForSelector('#bp_y');
    await page.waitForSelector('#bp_w');
    await page.waitForSelector('#btn-run-backprop');

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '22_backprop_learning_demo_initial.png'),
      fullPage: true,
    });
    console.log('[VERIFY-7] Saved: 22_backprop_learning_demo_initial.png');

    // ── Scenario 2: Run Gradient Check & Training ─────────────────────────────
    console.log('[VERIFY-7] Scenario 2: Running Gradient Check & Training...');
    await page.locator('button#btn-run-backprop').click();

    // Wait for gradient check pass badge and results table
    console.log('[VERIFY-7] Waiting for computation and results...');
    await page.waitForSelector('text=ALL CHECKS PASSED', { timeout: 10000 });
    await page.waitForSelector('#btn-export-bp-csv');
    await page.waitForSelector('#btn-export-bp-png');
    await sleep(600); // allow Plotly to complete render

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '23_backprop_learning_demo_trained.png'),
      fullPage: true,
    });
    console.log('[VERIFY-7] Saved: 23_backprop_learning_demo_trained.png');

    // ── Scenario 3: Verify Export Endpoint & Scale Toggle ────────────────────
    console.log('[VERIFY-7] Scenario 3: Toggling Linear Scale...');
    await page.locator('button:has-text("Scale: Logarithmic")').click();
    await sleep(400);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '24_backprop_linear_scale.png'),
      fullPage: true,
    });
    console.log('[VERIFY-7] Saved: 24_backprop_linear_scale.png');

    // ── Check Server API Backprop Direct Route ─────────────────────────────────
    console.log('[VERIFY-7] Testing POST /api/jobs {kind: "backprop"} directly...');
    const apiJobRes = await fetch(`${BASE_URL}/api/jobs`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Host: `127.0.0.1:${PORT}`,
      },
      body: JSON.stringify({
        kind: 'backprop',
        params: {
          x: 1.0,
          y: 2.0,
          w: 0.5,
          b: 0.0,
          v: 1.0,
          c: 0.0,
          learning_rate: 0.1,
          subsequent_steps: 100,
        },
      }),
    });

    if (!apiJobRes.ok) {
      throw new Error(`Direct POST /api/jobs backprop failed: ${apiJobRes.status}`);
    }
    const apiJobJson = await apiJobRes.json();
    console.log(`[VERIFY-7] Submitted job ID: ${apiJobJson.id}`);

    // Poll until succeeded
    let jobData = null;
    for (let i = 0; i < 20; i++) {
      await sleep(150);
      const pollRes = await fetch(`${BASE_URL}/api/jobs/${apiJobJson.id}`, {
        headers: { Host: `127.0.0.1:${PORT}` },
      });
      const pollJson = await pollRes.json();
      if (pollJson.state === 'succeeded') {
        jobData = pollJson;
        break;
      }
    }

    if (!jobData || !jobData.result) {
      throw new Error('Backprop job did not succeed in time');
    }

    console.log(`[VERIFY-7] Backprop Job Result: ${jobData.result.history.length} records`);
    console.log(`[VERIFY-7] Initial Loss: ${jobData.result.history[0].loss}`);
    console.log(`[VERIFY-7] Step 1 Loss: ${jobData.result.history[1].loss}`);
    console.log(`[VERIFY-7] Final Loss: ${jobData.result.history[101].loss}`);

    // Verify CSV Export route
    const exportRes = await fetch(`${BASE_URL}/api/jobs/${apiJobJson.id}/export/backprop.csv`, {
      headers: { Host: `127.0.0.1:${PORT}` },
    });
    if (!exportRes.ok) {
      throw new Error(`Export /export/backprop.csv failed: ${exportRes.status}`);
    }
    const csvContent = await exportRes.text();
    const csvLines = csvContent.trim().split('\n');
    console.log(`[VERIFY-7] Exported CSV line count: ${csvLines.length} (expected 103: 1 header + 102 rows)`);
    if (csvLines.length !== 103) {
      throw new Error(`Expected 103 CSV lines, got ${csvLines.length}`);
    }

    await browser.close();

    console.log('\n[VERIFY-7] ── Results Summary ──');
    console.log(`CSP Violations count: ${cspViolations.length}`);
    console.log(`Console Errors count: ${consoleErrors.length}`);

    if (cspViolations.length > 0) {
      throw new Error(`Encountered ${cspViolations.length} CSP violations!`);
    }

    console.log('\n[VERIFY-7 SUCCESS] All Phase 7 Backprop & Learning Demo scenarios verified!');
  } finally {
    console.log('[VERIFY-7] Stopping workbench_server...');
    serverProc.kill('SIGINT');
    await sleep(300);
    try {
      serverProc.kill('SIGKILL');
    } catch {}
  }
}

main().catch((err) => {
  console.error('[VERIFY-7 ERROR]', err);
  process.exit(1);
});
