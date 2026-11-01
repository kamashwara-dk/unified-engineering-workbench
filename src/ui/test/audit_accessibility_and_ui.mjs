import { spawn } from 'child_process';
import path from 'path';
import fs from 'fs';
import { fileURLToPath } from 'url';
import { chromium } from 'playwright';
import { AxeBuilder } from '@axe-core/playwright';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const WORKBENCH_ROOT = path.resolve(__dirname, '../../..');
const SERVER_EXE = path.join(WORKBENCH_ROOT, 'build/vs2022/src/server/Release/workbench_server.exe');
const DOCS_IMG_DIR = path.join(WORKBENCH_ROOT, 'docs/img/ui');
const PORT = 9096;
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
        console.log(`[AUDIT] Server ready! Version: ${json.core_version}`);
        return true;
      }
    } catch {}
    await sleep(250);
  }
  throw new Error(`Server failed to start on port ${PORT}`);
}

async function runAxeScan(page, contextName) {
  console.log(`[AXE] Running WCAG accessibility scan on: ${contextName}...`);
  const results = await new AxeBuilder({ page })
    .withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa'])
    .analyze();

  const seriousOrCritical = results.violations.filter(
    (v) => v.impact === 'serious' || v.impact === 'critical'
  );

  console.log(`[AXE] ${contextName}: ${results.violations.length} total issues (${seriousOrCritical.length} critical/serious)`);
  if (seriousOrCritical.length > 0) {
    for (const v of seriousOrCritical) {
      console.log(`  - [${v.impact.toUpperCase()}] ${v.id}: ${v.description} (${v.nodes.length} nodes)`);
      for (const node of v.nodes.slice(0, 3)) {
        console.log(`    Target: ${node.target.join(' ')}`);
      }
    }
  }
  return results;
}

async function main() {
  console.log('[AUDIT] Starting workbench_server on port', PORT);
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

  const allScanResults = [];

  try {
    await waitForServer();

    console.log('[AUDIT] Launching Chromium for comprehensive UI and Accessibility Audit...');
    const browser = await chromium.launch({
      headless: true,
      args: ['--no-sandbox', '--disable-setuid-sandbox'],
    });

    const context = await browser.newContext({
      viewport: { width: 1280, height: 850 },
    });

    const page = await context.newPage();

    console.log('[AUDIT] Navigating to Workbench...');
    await page.goto(BASE_URL, { waitUntil: 'networkidle' });
    await sleep(500);

    // ── Audit 1: Geometry View ────────────────────────────────────────────────
    console.log('\n── Audit 1: Geometry View ──');
    const geoScan = await runAxeScan(page, 'Geometry View (Default Empty State)');
    allScanResults.push({ view: 'Geometry', scan: geoScan });

    // Keyboard navigation in Geometry View
    await page.keyboard.press('Tab'); // focus first interactive element
    await page.keyboard.press('Tab');
    await sleep(200);

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '25_ui_audit_keyboard_geometry.png'),
      fullPage: true,
    });
    console.log('[AUDIT] Saved: 25_ui_audit_keyboard_geometry.png');

    // ── Audit 2: Thermal View (1024px & Reduced Motion) ──────────────────────
    console.log('\n── Audit 2: Thermal View (1024px & Reduced-Motion) ──');
    await page.locator('button#tab-thermal').click();
    await sleep(400);

    // Set 1024px layout and prefers-reduced-motion
    await page.setViewportSize({ width: 1024, height: 768 });
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await sleep(300);

    const thermalScan = await runAxeScan(page, 'Thermal View (1024px, Reduced Motion)');
    allScanResults.push({ view: 'Thermal', scan: thermalScan });

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '26_ui_audit_1024px_thermal.png'),
      fullPage: true,
    });
    console.log('[AUDIT] Saved: 26_ui_audit_1024px_thermal.png');

    // Restore desktop viewport
    await page.setViewportSize({ width: 1280, height: 850 });
    await page.emulateMedia({ reducedMotion: 'no-preference' });
    await sleep(300);

    // ── Audit 3: Performance View & Learning Demo ─────────────────────────────
    console.log('\n── Audit 3: Performance View & Learning Demo ──');
    await page.locator('button#tab-performance').click();
    await sleep(400);

    const perfBenchScan = await runAxeScan(page, 'Performance View (Benchmark Tab)');
    allScanResults.push({ view: 'Performance-Benchmark', scan: perfBenchScan });

    // Switch to Learning Demo sub-tab
    await page.locator('button#tab-sub-backprop').click();
    await sleep(400);

    const perfBackpropScan = await runAxeScan(page, 'Performance View (Learning Demo Tab)');
    allScanResults.push({ view: 'Performance-LearningDemo', scan: perfBackpropScan });

    // Run backprop to audit results state
    await page.locator('button#btn-run-backprop').click();
    await page.waitForSelector('text=ALL CHECKS PASSED', { timeout: 10000 });
    await sleep(500);

    const trainedScan = await runAxeScan(page, 'Performance View (Learning Demo Trained State)');
    allScanResults.push({ view: 'Performance-LearningDemo-Trained', scan: trainedScan });

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '28_ui_audit_learning_demo_a11y.png'),
      fullPage: true,
    });
    console.log('[AUDIT] Saved: 28_ui_audit_learning_demo_a11y.png');

    // ── Audit 4: Hardware View ────────────────────────────────────────────────
    console.log('\n── Audit 4: Hardware View ──');
    await page.locator('button#tab-hardware').click();
    await sleep(400);

    const hwScan = await runAxeScan(page, 'Hardware View (Simulated Mocks)');
    allScanResults.push({ view: 'Hardware', scan: hwScan });

    // Connect devices and verify live telemetry state scan
    await page.locator('button#btn-connect-mcu').click();
    await sleep(250);
    await page.locator('button#btn-telemetry-mcu').click();
    await sleep(400);

    const hwConnectedScan = await runAxeScan(page, 'Hardware View (Connected & Telemetry)');
    allScanResults.push({ view: 'Hardware-Connected', scan: hwConnectedScan });

    await page.screenshot({
      path: path.join(DOCS_IMG_DIR, '27_ui_audit_axe_clean_hardware.png'),
      fullPage: true,
    });
    console.log('[AUDIT] Saved: 27_ui_audit_axe_clean_hardware.png');

    await browser.close();

    console.log('\n═══════════════════════════════════════════════════════');
    console.log('         ACCESSIBILITY AUDIT SUMMARY REPORT            ');
    console.log('═══════════════════════════════════════════════════════');
    let totalViolations = 0;
    let criticalCount = 0;
    for (const r of allScanResults) {
      const crits = r.scan.violations.filter((v) => v.impact === 'critical' || v.impact === 'serious');
      totalViolations += r.scan.violations.length;
      criticalCount += crits.length;
      console.log(`- ${r.view.padEnd(35)}: ${r.scan.violations.length} violations (${crits.length} critical/serious)`);
    }
    console.log(`\nTotal Axe Violations: ${totalViolations}`);
    console.log(`Total Critical/Serious Issues: ${criticalCount}`);
    console.log('═══════════════════════════════════════════════════════\n');

  } finally {
    console.log('[AUDIT] Stopping workbench_server...');
    serverProc.kill('SIGINT');
    await sleep(300);
    try {
      serverProc.kill('SIGKILL');
    } catch {}
  }
}

main().catch((err) => {
  console.error('[AUDIT ERROR]', err);
  process.exit(1);
});

