const { test, expect } = require('@playwright/test');

test('graph is the canonical routing surface', async ({ page }) => {
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.setViewportSize({ width: 1440, height: 900 });
  await page.goto('file:///home/tucker/Projects/WireRunner/docs/prototype/index.html');
  await page.waitForTimeout(100);

  await expect(page.locator('#wires .wire')).toHaveCount(6);
  await page.locator('#wires .wire-hit').nth(1).evaluate(element => element.dispatchEvent(new MouseEvent('click', { bubbles: true })));
  await expect(page.locator('#linkInspector')).toBeVisible();
  await expect(page.locator('#linkFrom')).toHaveText('Firefox');
  await expect(page.locator('#linkTo')).toHaveText('Scarlett 18i20');

  await page.locator('#rememberLink').click();
  await expect(page.locator('#linkPersistence')).toContainText('Remembered rule');
  await page.locator('#scarlett').click();
  await page.locator('#expandScarlett').click();
  await expect(page.locator('#scarlett')).toHaveClass(/expanded/);

  expect(errors).toEqual([]);
});

test('search focuses the existing canvas object', async ({ page }) => {
  await page.setViewportSize({ width: 1100, height: 720 });
  await page.goto('file:///home/tucker/Projects/WireRunner/docs/prototype/index.html');
  await page.locator('#dismissNotice').click();
  await page.locator('#searchButton').click();
  await page.locator('#searchInput').fill('OBS');
  await page.locator('#searchResults button').click();
  await expect(page.locator('#obs')).toHaveClass(/selected/);
  await expect(page.locator('#inspectTitle')).toHaveText('OBS Studio');
});
