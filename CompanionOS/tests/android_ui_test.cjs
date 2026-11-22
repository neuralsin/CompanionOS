const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {pathToFileURL} = require('node:url');
const path = require('node:path');
const fs = require('node:fs');
const assert = require('node:assert/strict');
(async () => {
  const browser = await chromium.launch({headless:true, executablePath:process.env.CHROME_PATH});
  const output = path.join(__dirname,'host-output','android');
  fs.mkdirSync(output,{recursive:true});
  let checks=0;
  try {
    for (const width of [320,393,720]) {
      const page=await browser.newPage({viewport:{width,height:852},deviceScaleFactor:1});
      const errors=[];
      page.on('pageerror', e=>errors.push(e.message));
      const external=[];
      page.on('request', request=>{if(/^https?:/.test(request.url()))external.push(request.url());});
      await page.addInitScript(() => {
        window.commands=[];
        window.AndroidBridge={loadSettings:()=>JSON.stringify({spotify_id:'testclient12345678',networks:JSON.stringify({networks:[{ssid:'<img src=x onerror=alert(1)>',pass:'test-only'}]})}),
          pollBot:()=>{},sendUdp:cmd=>window.commands.push(cmd),saveSettings:()=>{},setManualBotIp:()=>{},
          loadPlannerDraft:()=>'',savePlannerDraft:json=>window.draft=JSON.parse(json),
          loadPlanner:()=>setTimeout(()=>window.onPlanner({version:1,revision:0,device:'test-bot',offset:-new Date().getTimezoneOffset(),flirt:1,events:[],classes:[]}),0),
          syncPlanner:json=>{window.synced=JSON.parse(json);setTimeout(()=>window.onPlanner({...window.synced,revision:1}),0);}};
      });
      await page.goto(pathToFileURL(path.resolve(__dirname,'../android/CompanionApp/app/src/main/assets/www/index.html')).href);
      assert.equal(await page.locator('.tile').count(),20); checks++;
      assert.equal(await page.locator('.tile:disabled').count(),20); checks++;
      assert.equal(await page.locator('#networks img').count(),0); checks++;
      await page.evaluate(()=>onBotState({online:true,ip:'192.168.1.50',page:2,theme:2}));
      assert.equal(await page.locator('.tile[aria-pressed=true]').innerText(),'Lyrics');checks++;
      await page.locator('.tile').nth(0).click();
      assert.equal(await page.locator('.tile[aria-pressed=true]').innerText(),'Lyrics');checks++;
      assert.equal(await page.evaluate(()=>commands.at(-1)),'PAGE:0'); checks++;
      await page.evaluate(()=>onBotState({online:true,page:0,theme:2,legacy:true}));
      assert.match(await page.locator('#screen-message').innerText(),/firmware/);checks++;
      await page.locator('nav [data-tab=planner]').click();
      await page.waitForFunction(()=>document.getElementById('planner-status').textContent==='Synced with deskbot');
      await page.locator('#planner-add').click();
      await page.locator('#entry-title').fill('Calculus final exam - integration');
      await page.locator('#entry-at').fill('2027-02-01T09:00');
      await page.locator('#schedule-editor button.primary').click();
      assert.equal(await page.locator('.schedule-row').count(),1);checks++;
      assert.equal(await page.evaluate(()=>draft.events[0].title),'Calculus final exam - integration');checks++;
      await page.locator('#classes-mode').click();
      await page.locator('#planner-add').click();
      await page.locator('#entry-title').fill('Physics laboratory');
      await page.locator('#entry-time').fill('10:00');
      await page.locator('#entry-place').fill('Laboratory 402');
      await page.locator('#schedule-editor button.primary').click();
      assert.equal(await page.evaluate(()=>draft.classes[0].start),600);checks++;
      await page.locator('#planner-sync').click();
      await page.waitForFunction(()=>window.synced?.classes?.length===1);
      assert.equal(await page.evaluate(()=>synced.events.length),1);checks++;
      await page.locator('#events-mode').click();
      await page.locator('[aria-label=Complete]').click();
      assert.equal(await page.evaluate(()=>draft.events[0].done),true);checks++;
      await page.locator('[aria-label="Mark incomplete"]').click();
      await page.locator('[aria-label="Snooze Calculus final exam - integration"]').click();
      assert.ok(await page.evaluate(()=>draft.events[0].quietUntil>Date.now()/1000));checks++;
      for (const tab of ['deskbot','planner','music','settings']) {
        await page.locator(`nav [data-tab=${tab}]`).click();
        if(tab==='music')await page.locator('details').evaluate(el=>el.open=true);
        const overflow=await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth);
        assert.equal(overflow,false,`horizontal overflow at ${width}/${tab}`);checks++;
        const clipped=await page.locator('button:visible').evaluateAll(buttons=>buttons.filter(b=>b.scrollWidth>b.clientWidth+1).map(b=>b.textContent));
        assert.deepEqual(clipped,[],`clipped buttons at ${width}/${tab}`);checks++;
        await page.screenshot({path:path.join(output,`${width}-${tab}.png`),fullPage:true});
      }
      await page.locator('nav [data-tab=music]').click();
      await page.locator('[data-command="CMD:NEXT"]').click();
      assert.equal(await page.evaluate(()=>commands.at(-1)),'CMD:NEXT');checks++;
      await page.locator('nav [data-tab=deskbot]').click();
      await page.selectOption('#emotion','AUTO');
      assert.equal(await page.evaluate(()=>commands.at(-1)),'EMOTION:AUTO');checks++;
      await page.selectOption('#flirt-level','2');
      await page.locator('#personality-sync').click();
      await page.waitForFunction(()=>window.synced.flirt===2);checks++;
      await page.locator('[data-command="BTN:DISMISS"]').first().click();
      assert.equal(await page.evaluate(()=>commands.at(-1)),'BTN:DISMISS');checks++;
      await page.evaluate(()=>onBotState({online:false}));
      assert.equal(await page.locator('.tile:disabled').count(),20);checks++;
      assert.deepEqual(errors,[]);assert.deepEqual(external,[]);checks+=2;
      await page.close();
    }
    console.log(`PASS ${checks} Android UI checks; three viewport sizes, no external assets. Mock bridge only, not hardware verification.`);
  } finally { await browser.close(); }
})().catch(error=>{console.error(error);process.exitCode=1;});
