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
const PORT = 9099;
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
        console.log(`[VERIFY-6B] Server ready! Version: ${json.core_version}`);
        return true;
      }
    } catch {}
    await sleep(250);
  }
  throw new Error(`Server failed to start on port ${PORT}`);
}

async function main() {
  console.log('[VERIFY-6B] Starting workbench_server on port', PORT);
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

    console.log('[VERIFY-6B] Launching Chromium...');
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

    console.log('[VERIFY-6B] Navigating to workbench...');
    await page.goto(BASE_URL, { waitUntil: 'networkidle' });
    await sleep(400);

    // Switch to Hardware Tab
    console.log('[VERIFY-6B] Selecting Hardware tab...');
    await page.locator('button#tab-hardware').click();
    await sleep(400);

    // ── Scenario 1: Initial View & Persistent SIMULATED Labels ───────────
    console.log('[VERIFY-6B] Scenario 1: Verifying Initial State & SIMULATED Labels...');
    await page.waitForSelector('#simulated-view-banner');
    await page.waitForSelector('#card-mcu');
    await page.waitForSelector('#card-fpga');
    await page.waitForSelector('#card-jtag');

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '18_hardware_initial_disconnected_state.png'),
      fullPage: true,
    });
    console.log('[VERIFY-6B] Saved: 18_hardware_initial_disconnected_state.png');

    // ── Scenario 2: Attempt Illegal Action (Read Telemetry while Disconnected) ─
    console.log('[VERIFY-6B] Scenario 2: Attempting illegal action (telemetry while disconnected)...');
    await page.locator('button#btn-telemetry-mcu').click();
    await page.waitForSelector('text=cannot read telemetry while disconnected', { timeout: 4000 });
    await sleep(300);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '19_hardware_illegal_action_error.png'),
      fullPage: true,
    });
    console.log('[VERIFY-6B] Saved: 19_hardware_illegal_action_error.png');

    // ── Scenario 3: Connect All Three & Read Telemetry ─────────────────────
    console.log('[VERIFY-6B] Scenario 3: Connecting all three devices...');
    await page.locator('button#btn-connect-mcu').click();
    await sleep(250);
    await page.locator('button#btn-connect-fpga').click();
    await sleep(250);
    await page.locator('button#btn-connect-jtag').click();
    await sleep(400);

    console.log('[VERIFY-6B] Reading telemetry from all three devices...');
    await page.locator('button#btn-telemetry-mcu').click();
    await sleep(250);
    await page.locator('button#btn-telemetry-fpga').click();
    await sleep(250);
    await page.locator('button#btn-telemetry-jtag').click();
    await sleep(500);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '20_hardware_connected_and_telemetry.png'),
      fullPage: true,
    });
    console.log('[VERIFY-6B] Saved: 20_hardware_connected_and_telemetry.png');

    // ── Scenario 4: Simulate Timeout, Fault Injection & Recovery Timeline ─
    console.log('[VERIFY-6B] Scenario 4: Arming simulate timeout on MCU...');
    await page.locator('button#btn-timeout-mcu').click();
    await page.waitForSelector('text=Timeout Armed!', { timeout: 4000 });
    await sleep(300);

    console.log('[VERIFY-6B] Triggering read telemetry to execute timeout and recovery...');
    await page.locator('button#btn-telemetry-mcu').click();
    // Wait for recovery to complete
    await sleep(1200);

    // Verify recovery events in timeline
    await page.waitForSelector('text=Telemetry timeout', { timeout: 6000 });
    await page.waitForSelector('text=Recovered after retry', { timeout: 6000 });

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '21_hardware_timeout_and_recovery_timeline.png'),
      fullPage: true,
    });
    console.log('[VERIFY-6B] Saved: 21_hardware_timeout_and_recovery_timeline.png');

    // ── Results Summary ──────────────────────────────────────────────────
    console.log('\n[VERIFY-6B] ── Results Summary ──');
    console.log(`CSP Violations count: ${cspViolations.length}`);
    console.log(`Console Errors count: ${consoleErrors.length}`);

    if (cspViolations.length > 0) {
      throw new Error(`Strict CSP was violated: ${JSON.stringify(cspViolations)}`);
    }

    await browser.close();
    console.log('\n[VERIFY-6B SUCCESS] All Phase 6B Hardware View scenarios verified!');
  } finally {
    serverProc.kill();
  }
}

main().catch((err) => {
  console.error('[VERIFY-6B FAILED]', err);
  process.exit(1);
});

