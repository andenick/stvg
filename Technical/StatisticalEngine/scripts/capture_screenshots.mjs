import { chromium } from 'playwright';

const SCREENSHOTS_DIR = '../../../Outputs/screenshots';
const BASE_URL = 'http://localhost:8080';

async function delay(ms) { return new Promise(r => setTimeout(r, ms)); }

async function capture() {
  const browser = await chromium.launch({ headless: true });
  const context = await browser.newContext({
    viewport: { width: 1920, height: 1080 },
    deviceScaleFactor: 1,
  });
  const page = await context.newPage();

  // 1. Title Screen
  console.log('1/5: Title Screen...');
  await page.goto(BASE_URL, { waitUntil: 'networkidle' });
  await delay(1000);
  await page.screenshot({ path: `${SCREENSHOTS_DIR}/01_title_screen.png`, fullPage: false });
  console.log('  saved 01_title_screen.png');

  // 2. Create a game and capture the game view
  console.log('2/5: Creating game...');
  const resp = await page.evaluate(async () => {
    const r = await fetch('/api/game/new', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ seed: 42, ceoId: 'dimon' })
    });
    return r.json();
  });
  const gameId = resp.data.gameId;
  console.log(`  Game created: ${gameId}`);

  // Advance through QuarterStart to get decisions
  await page.evaluate(async (gid) => {
    await fetch(`/api/game/${gid}/advance`, { method: 'POST' });
  }, gameId);
  await delay(500);

  // Reload to pick up the game state
  await page.goto(BASE_URL, { waitUntil: 'networkidle' });
  await delay(2000);
  await page.screenshot({ path: `${SCREENSHOTS_DIR}/02_game_loading.png`, fullPage: false });
  console.log('  saved 02_game_loading.png');

  // 3. Hit the state API and capture raw JSON for reference
  console.log('3/5: Game state...');
  const stateResp = await page.evaluate(async (gid) => {
    const r = await fetch(`/api/game/${gid}/state`);
    return r.json();
  }, gameId);
  console.log(`  Phase: ${stateResp.data.phase}, Year: ${stateResp.data.state?.currentYear}`);

  // 4. Try to interact — click any visible button
  console.log('4/5: Interacting with UI...');
  try {
    // Look for start/play buttons on title screen
    const buttons = await page.$$('button');
    console.log(`  Found ${buttons.length} buttons`);
    if (buttons.length > 0) {
      await buttons[0].click();
      await delay(2000);
      await page.screenshot({ path: `${SCREENSHOTS_DIR}/03_after_click.png`, fullPage: false });
      console.log('  saved 03_after_click.png');
    }
  } catch (e) {
    console.log(`  Interaction failed: ${e.message}`);
  }

  // 5. Mobile viewport
  console.log('5/5: Mobile view...');
  await page.setViewportSize({ width: 412, height: 915 });
  await page.goto(BASE_URL, { waitUntil: 'networkidle' });
  await delay(1000);
  await page.screenshot({ path: `${SCREENSHOTS_DIR}/04_mobile.png`, fullPage: false });
  console.log('  saved 04_mobile.png');

  // Back to desktop for one more
  await page.setViewportSize({ width: 1920, height: 1080 });
  await page.goto(BASE_URL, { waitUntil: 'networkidle' });
  await delay(1000);
  await page.screenshot({ path: `${SCREENSHOTS_DIR}/05_desktop_final.png`, fullPage: false });
  console.log('  saved 05_desktop_final.png');

  await browser.close();
  console.log('\nDone! Screenshots saved to Outputs/screenshots/');
}

capture().catch(e => { console.error(e); process.exit(1); });
