'use strict';
const $ = id => document.getElementById(id);
const bridge = window.AndroidBridge;
const pages = [['Eyes','bot'],['Spotify','disc-3'],['Lyrics','captions'],['Pomodoro','timer'],['Weather','cloud-sun'],['Notifications','bell'],['Notes','notebook-pen'],['Stocks','chart-no-axes-combined'],['Gaming','gamepad-2'],['Social','messages-square'],['Productivity','list-checks'],['Network','wifi'],['System','sliders-horizontal'],['PC dashboard','monitor'],['RuView','radar'],['Clock','clock-3'],['Watch','watch'],['Dr. Hack','radio']];
let online=false, capture=true, activeTab='deskbot', pending=-1, pendingAt=0, noticeTimer, networks=[], captureUnavailable=false;
const fields=['spotify_id','spotify_secret','spotify_refresh','spotify_access','github_user','github_token','weather_city','stocks_list'];
function native(method,...args){if(bridge && typeof bridge[method]==='function') return bridge[method](...args);onResult({message:'Android controller unavailable in this preview'});}
function icons(){if(window.lucide)lucide.createIcons();}
function onResult(data){$('notice').textContent=data.message;$('notice').hidden=false;clearTimeout(noticeTimer);noticeTimer=setTimeout(()=>$('notice').hidden=true,6000);}
window.onResult=onResult;
function command(value){if(!online){onResult({message:'Bot offline; command not sent'});return;}native('sendUdp',value);if(value.startsWith('PAGE:')){pending=Number(value.slice(5));pendingAt=Date.now();$('pending').textContent='Switching...';}setTimeout(poll,200);}
pages.forEach(([name,icon],id)=>{const button=document.createElement('button');button.className='tile';button.dataset.command=`PAGE:${id}`;button.setAttribute('aria-pressed','false');const glyph=document.createElement('i');glyph.dataset.lucide=icon;button.append(glyph,document.createTextNode(name));$('tiles').append(button);});
document.addEventListener('click',event=>{const button=event.target.closest('button');if(!button)return;if(button.dataset.command)command(button.dataset.command);if(button.dataset.tab){activeTab=button.dataset.tab;document.querySelectorAll('.tab-panel').forEach(el=>el.hidden=el.id!==activeTab);document.querySelectorAll('nav button').forEach(el=>el.setAttribute('aria-current',el===button?'page':'false'));window.scrollTo(0,0);poll();}});
window.onBotState=data=>{
  online=!!data.online;$('connection').classList.toggle('online',online);$('connection').querySelector('b').textContent=online?'Connected':'Offline';
  $('device-address').textContent=data.ip||'Searching on WiFi';
  const page=online && Number.isInteger(data.page)?data.page:-1;
  $('current-page').textContent=pages[page]?pages[page][0]:'No tile connected';
  document.querySelectorAll('.tile').forEach((el,i)=>el.setAttribute('aria-pressed',String(i===page)));
  document.querySelectorAll('#themes button').forEach((el,i)=>el.setAttribute('aria-pressed',String(online && i===data.theme)));
  if(page===pending){pending=-1;$('pending').textContent='';}else if(pending>=0 && Date.now()-pendingAt>7000){pending=-1;$('pending').textContent='No tile confirmation';}
  if(data.screen){$('screen').src=data.screen;$('screen').hidden=false;$('screen-empty').hidden=true;$('screen-time').textContent='Captured '+new Date().toLocaleTimeString();}
  else if(!online || data.legacy || data.screen_error){$('screen').hidden=true;$('screen-empty').hidden=false;$('screen-message').textContent=!online?'Waiting for deskbot':data.legacy?'Screen preview requires updated bot firmware':data.screen_error;$('screen-time').textContent='No current screen';if(data.screen_error && data.screen_error.includes('MISO'))captureUnavailable=true;}
  document.querySelectorAll('[data-command]').forEach(button=>button.disabled=!online);
};
window.onSettings=data=>{for(const key of fields)if(!key.includes('secret')&&!key.includes('token')&&!key.includes('refresh')&&!key.includes('access')&&data[key]!==undefined)$(key).value=data[key];$('manual_bot_ip').value=data.manual_bot_ip||'';$('phone_relays').checked=!!data.phone_relays;$('spotify-status').textContent=data.spotify_saved?'Saved on phone':'Not configured';try{networks=JSON.parse(data.networks||'{}').networks||[];}catch(_){networks=[];}renderNetworks();};
function saveSettings(){const data={};fields.forEach(key=>data[key]=$(key).value);data.phone_relays=$('phone_relays').checked;native('saveSettings',JSON.stringify(data));['spotify_secret','spotify_refresh','spotify_access','github_token'].forEach(key=>$(key).value='');}
function renderNetworks(){const list=$('networks');list.textContent='';networks.forEach((network,i)=>{const row=document.createElement('div');row.className='network';const heading=document.createElement('div');heading.className='network-heading';heading.textContent=`Priority ${i+1}`;const tools=document.createElement('div');tools.className='network-tools';[['arrow-up','Move up',()=>{if(i>0){[networks[i-1],networks[i]]=[networks[i],networks[i-1]];renderNetworks();}}],['trash-2','Remove network',()=>{networks.splice(i,1);renderNetworks();}]].forEach(([icon,label,action])=>{const button=document.createElement('button');button.className='icon';button.title=label;button.setAttribute('aria-label',label);const glyph=document.createElement('i');glyph.dataset.lucide=icon;button.append(glyph);button.onclick=action;if(icon==='arrow-up')button.disabled=i===0;tools.append(button);});heading.append(tools);row.append(heading);['ssid','pass'].forEach(key=>{const input=document.createElement('input');input.type=key==='pass'?'password':'text';input.autocomplete='off';input.placeholder=key==='pass'?'Password (empty for open network)':'Network name';input.setAttribute('aria-label',`${key==='pass'?'Password':'SSID'} ${i+1}`);input.value=network[key]||'';input.oninput=()=>network[key]=input.value;row.append(input);});list.append(row);});$('add-network').disabled=networks.length>=5;icons();}
$('add-network').onclick=()=>{if(networks.length<5){networks.push({ssid:'',pass:''});renderNetworks();}};
$('sync-wifi').onclick=()=>{if(confirm('Send these WiFi passwords to the selected bot over your trusted local network?'))native('saveWifi',JSON.stringify({networks}));};
$('sync').onclick=()=>{if(confirm('Send the saved Spotify credentials to this bot over your trusted local network?'))native('syncSpotify');};
$('spotify-save').onclick=saveSettings;$('settings-save').onclick=saveSettings;
$('authorize').onclick=()=>{saveSettings();native('startSpotifyAuth');};
$('emotion').onchange=()=>command('EMOTION:'+$('emotion').value);
$('thought-form').onsubmit=event=>{event.preventDefault();const value=$('thought').value.trim();if(value){command('THOUGHT:'+value);if(online)$('thought').value='';}};
$('ip-form').onsubmit=event=>{event.preventDefault();native('setManualBotIp',$('manual_bot_ip').value);captureUnavailable=false;poll();};
$('photo').onclick=()=>{if(online)native('pickAndUploadImage');else onResult({message:'Connect to the bot first'});};
$('capture-toggle').onclick=()=>{capture=!capture;captureUnavailable=false;$('capture-toggle').setAttribute('aria-pressed',String(capture));$('capture-toggle').setAttribute('aria-label',capture?'Pause screen capture':'Resume screen capture');$('capture-toggle').title=capture?'Pause screen capture':'Resume screen capture';$('capture-toggle').textContent='';const icon=document.createElement('i');icon.dataset.lucide=capture?'pause':'play';$('capture-toggle').append(icon);icons();if(capture)poll();};
$('notification-settings').onclick=()=>native('openNotificationSettings');$('battery-settings').onclick=()=>native('openBatterySettings');$('disconnect').onclick=()=>{if(confirm('Stop the phone controller?'))native('exitApp');};
function poll(){if(bridge&&!document.hidden)native('pollBot',activeTab==='deskbot'&&capture&&!captureUnavailable);}
if(bridge){try{window.onSettings(JSON.parse(native('loadSettings')));}catch(_){onResult({message:'Unable to load saved settings'});}poll();}
document.querySelectorAll('[data-command]').forEach(el=>el.disabled=true);icons();setInterval(poll,1600);document.addEventListener('visibilitychange',poll);
