'use strict';
(() => {
  const empty = () => ({version:1,revision:0,offset:-new Date().getTimezoneOffset(),flirt:1,events:[],classes:[]});
  let plan=empty(), mode='events', editing=null, dirty=false, syncing=false, pendingMerge=false, attemptedLoad=false;
  const days=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
  const now=()=>Math.floor(Date.now()/1000);
  const validText=(text,max,optional=false)=>text.length<=max && (optional||text.trim().length>0) && /^[\x20-\x7e]*$/.test(text);
  function persist(){dirty=true;plan._draft=true;native('savePlannerDraft',JSON.stringify(plan));$('planner-status').textContent='Changes saved on phone';render();}
  function dateLabel(at){return new Date(at*1000).toLocaleString([], {month:'short',day:'numeric',hour:'2-digit',minute:'2-digit'});}
  function classAt(c){const local=now()+plan.offset*60, day=Math.floor(local/86400), weekday=(day+4)%7;let at=(day+(c.day-weekday+7)%7)*86400+c.start*60-plan.offset*60;if(at-604800<=now()&&at-604800+c.duration*60>now())at-=604800;if(at+c.duration*60<=now())at+=604800;return at;}
  function countdown(at){const minutes=Math.ceil((at-now())/60);return minutes<=0?'Due now':minutes<60?`${minutes} min`:minutes<1440?`${Math.ceil(minutes/60)} hr`:`${Math.ceil(minutes/1440)} days`;}
  function button(icon,label,action){const b=document.createElement('button');b.className='icon';b.type='button';b.title=label;b.setAttribute('aria-label',label);const i=document.createElement('i');i.dataset.lucide=icon;b.append(i);b.onclick=action;return b;}
  function render(){
    $('flirt-level').value=String(plan.flirt);
    const pending=plan.events.filter(e=>!e.done && e.at+86400>now()).sort((a,b)=>a.at-b.at);
    const classes=plan.classes.map(c=>({...c,at:classAt(c)})).sort((a,b)=>a.at-b.at);
    const next=[...pending,...classes.filter(c=>c.at-now()<=7200)].sort((a,b)=>a.at-b.at)[0];
    $('next-event').textContent=next?next.title:'Nothing scheduled';
    $('next-event-time').textContent=next?`${dateLabel(next.at)} · ${countdown(next.at)}`:'';
    $('event-total').textContent=`${pending.length} upcoming`;
    const list=$('schedule-list');list.textContent='';
    const entries=mode==='events'?[...plan.events].sort((a,b)=>Number(a.done)-Number(b.done)||a.at-b.at):classes;
    if(!entries.length){const p=document.createElement('p');p.className='empty-schedule';p.textContent=mode==='events'?'No exams or reminders':'No classes scheduled';list.append(p);}
    entries.forEach(entry=>{
      const row=document.createElement('article');row.className='schedule-row';
      if(mode==='classes' && entry.at-now()<=7200)row.classList.add('up-next');
      if(entry.done)row.classList.add('completed');
      const marker=document.createElement('div');marker.className='date-marker';
      const date=new Date(entry.at*1000);
      marker.textContent=mode==='classes'?days[entry.day].slice(0,3):date.toLocaleString([], {month:'short'});
      const number=document.createElement('strong');number.textContent=mode==='classes'?`${Math.floor(entry.start/60).toString().padStart(2,'0')}:${(entry.start%60).toString().padStart(2,'0')}`:date.getDate();marker.append(number);
      const body=document.createElement('div');body.className='schedule-copy';const title=document.createElement('h3');title.textContent=entry.title;const detail=document.createElement('p');
      detail.textContent=mode==='classes'?`${entry.duration} min${entry.place?' · '+entry.place:''}`:`${entry.kind?'Reminder':'Exam'} · ${dateLabel(entry.at)}`;
      const status=document.createElement('p');status.className='entry-status';status.textContent=entry.done?'Completed':entry.quietUntil>now()?`Snoozed until ${new Date(entry.quietUntil*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})}`:countdown(entry.at);
      body.append(title,detail,status);
      const actions=document.createElement('div');actions.className='entry-actions';
      actions.append(button('pencil','Edit '+entry.title,()=>edit(entry)));
      if(mode==='events')actions.append(button(entry.done?'rotate-ccw':'check',entry.done?'Mark incomplete':'Complete',()=>{entry.done=!entry.done;persist();}));
      actions.append(button('bell-off','Snooze '+entry.title,()=>{const item=plan[mode].find(e=>e.id===entry.id);item.quietUntil=mode==='classes'?classAt(item)+item.duration*60:now()+3600;persist();}));
      actions.append(button('trash-2','Delete '+entry.title,()=>{if(confirm(`Delete ${entry.title}?`)){plan[mode]=plan[mode].filter(e=>e.id!==entry.id);persist();}}));
      row.append(marker,body,actions);list.append(row);
    });
    $('planner-add').disabled=syncing||plan[mode].length>=(mode==='events'?12:16);
    icons();requestAnimationFrame(drawGraph);
  }
  function drawGraph(){
    const canvas=$('exam-chart'), width=canvas.clientWidth;if(!width)return;
    const height=100, ratio=window.devicePixelRatio||1;canvas.width=Math.round(width*ratio);canvas.height=height*ratio;
    const ctx=canvas.getContext('2d');ctx.scale(ratio,ratio);ctx.clearRect(0,0,width,height);
    const bins=Array(14).fill(0), today=Math.floor((now()+plan.offset*60)/86400);
    for(const e of plan.events){const day=Math.floor((e.at+plan.offset*60)/86400)-today;if(!e.done&&day>=0&&day<14)bins[day]++;}
    const max=Math.max(1,...bins), step=width/14;
    ctx.strokeStyle='#363a3d';ctx.beginPath();ctx.moveTo(0,77);ctx.lineTo(width,77);ctx.stroke();
    bins.forEach((count,i)=>{const h=count/max*57;ctx.fillStyle=i<3?'#ff9699':'#72dcc2';if(h)ctx.fillRect(i*step+3,76-h,Math.max(2,step-6),h);if(count){ctx.font='11px system-ui';ctx.textAlign='center';ctx.fillText(String(count),(i+.5)*step,70-h);}});
    ctx.fillStyle='#a1a6ac';ctx.font='11px system-ui';ctx.textAlign='left';ctx.fillText('Today',0,96);ctx.textAlign='right';ctx.fillText('+13 days',width,96);
    canvas.setAttribute('aria-label',bins.map((n,i)=>`Day ${i}: ${n} events`).join(', '));
  }
  function closeEditor(){$('schedule-editor').hidden=true;editing=null;$('planner-add').focus();}
  function edit(entry=null){
    editing=entry?entry.id:null;const classes=mode==='classes';$('schedule-editor').hidden=false;$('editor-heading').textContent=entry?'Edit entry':classes?'New class':'New event';
    $('event-fields').hidden=classes;$('class-fields').hidden=!classes;$('entry-at').required=!classes;$('entry-time').required=classes;
    $('entry-title').value=entry?.title||'';$('entry-kind').value=entry?.kind||0;$('entry-place').value=entry?.place||'';
    const date=new Date((entry?.at||now()+3600)*1000);date.setMinutes(date.getMinutes()-date.getTimezoneOffset());$('entry-at').value=date.toISOString().slice(0,16);
    $('entry-day').value=entry?.day??new Date().getDay();const start=entry?.start??540;$('entry-time').value=`${Math.floor(start/60).toString().padStart(2,'0')}:${(start%60).toString().padStart(2,'0')}`;
    $('entry-duration').value=entry?.duration||60;$('entry-title').focus();$('schedule-editor').scrollIntoView({block:'start'});
  }
  $('schedule-editor').onsubmit=e=>{
    e.preventDefault();if(syncing)return;
    const title=$('entry-title').value.trim(), place=$('entry-place').value.trim();
    if(!validText(title,40)||!validText(place,24,true)){onResult({message:'Use printable English titles (40 characters) and room names (24 characters).'});return;}
    const id=editing||Math.floor(Math.random()*0x7fffffff)+1;
    const entry={id,title,quietUntil:0};
    if(mode==='events'){Object.assign(entry,{at:Math.floor(new Date($('entry-at').value).getTime()/1000),kind:Number($('entry-kind').value),done:false});if(!Number.isFinite(entry.at)||entry.at<1704067200||entry.at>4102444800)return;}
    else{const [hour,minute]=$('entry-time').value.split(':').map(Number);Object.assign(entry,{day:Number($('entry-day').value),start:hour*60+minute,duration:Number($('entry-duration').value),place});if(!Number.isFinite(entry.start)||entry.start<0||entry.start>=1440||!Number.isInteger(entry.duration)||entry.duration<1||entry.duration>720)return;}
    const index=plan[mode].findIndex(e=>e.id===id);if(index>=0)plan[mode][index]=entry;else if(plan[mode].length<(mode==='events'?12:16))plan[mode].push(entry);else return;
    closeEditor();persist();
  };
  function setMode(value){if(!$('schedule-editor').hidden&&!confirm('Discard this edit?'))return;closeEditor();mode=value;$('schedule-heading').textContent=mode==='events'?'Upcoming':'Weekly schedule';$('events-mode').setAttribute('aria-selected',String(mode==='events'));$('classes-mode').setAttribute('aria-selected',String(mode==='classes'));render();}
  $('events-mode').onclick=()=>setMode('events');$('classes-mode').onclick=()=>setMode('classes');$('planner-add').onclick=()=>edit();$('editor-close').onclick=closeEditor;
  function busy(value){syncing=value;['planner-sync','planner-refresh','personality-sync','flirt-level','events-mode','classes-mode','planner-add'].forEach(id=>$(id).disabled=value);$('schedule-editor').querySelectorAll('input,select,button').forEach(e=>e.disabled=value);$('schedule-list').querySelectorAll('button').forEach(e=>e.disabled=value);}
  function sync(){
    if(syncing)return;
    if(!online){onResult({message:'Draft saved on phone. Connect to sync.'});return;}
    if(!plan.device){pendingMerge=true;busy(true);native('loadPlanner');return;}
    plan.epoch=now();plan.offset=-new Date().getTimezoneOffset();busy(true);$('planner-status').textContent='Saving to bot...';native('syncPlanner',JSON.stringify(plan));
  }
  $('planner-sync').onclick=sync;$('personality-sync').onclick=sync;
  $('flirt-level').onchange=()=>{plan.flirt=Number($('flirt-level').value);persist();};
  $('planner-refresh').onclick=()=>{if(dirty&&!confirm('Replace this phone draft with the bot schedule?'))return;busy(true);native('loadPlanner');};
  window.onPlanner=value=>{
    if(pendingMerge){
      pendingMerge=false;
      const events=[...new Map([...value.events,...plan.events].map(e=>[e.id,e])).values()];
      const classes=[...new Map([...value.classes,...plan.classes].map(e=>[e.id,e])).values()];
      if(events.length>12||classes.length>16){busy(false);native('savePlannerDraft',JSON.stringify(plan));onResult({message:'Combined schedule exceeds bot capacity. Keep at most 12 events and 16 classes.'});return;}
      plan={...value,events,classes,flirt:plan.flirt};busy(false);persist();sync();return;
    }
    plan=value;dirty=false;closeEditor();busy(false);$('planner-status').textContent='Synced with deskbot';render();
  };
  window.onPlannerError=value=>{pendingMerge=false;busy(false);$('planner-status').textContent='Phone draft - not confirmed on bot';onResult(value);};
  try{const saved=bridge&&bridge.loadPlannerDraft?bridge.loadPlannerDraft():'';if(saved){const value=JSON.parse(saved);if(value.version===1&&Array.isArray(value.events)&&Array.isArray(value.classes)){plan=value;dirty=!!value._draft;}}}catch(_){}
  document.querySelector('nav [data-tab=planner]').addEventListener('click',()=>{render();if(!plan.device&&!dirty&&online&&!syncing){busy(true);native('loadPlanner');}});
  window.addEventListener('bot-online',()=>{if(!attemptedLoad&&!plan.device&&!dirty&&!syncing){attemptedLoad=true;busy(true);native('loadPlanner');}});
  document.addEventListener('keydown',e=>{if(e.key==='Escape')closeEditor();});window.addEventListener('resize',drawGraph);
  setInterval(()=>{if(!document.hidden&&!syncing&&$('schedule-editor').hidden)render();},60000);render();
})();
