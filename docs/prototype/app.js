(() => {
  const $=(s,r=document)=>r.querySelector(s), $$=(s,r=document)=>[...r.querySelectorAll(s)];
  const graph=$('#graph'), svg=$('#wires'), status=$('#status');
  const nodeMeta={
    firefox:['Firefox','Playback stream · running','84','Firefox has 3 outgoing links. Select a wire on the canvas to inspect or remove it.'],
    spotify:['Spotify','Playback stream · running','68','Spotify has 1 outgoing link. Select its wire on the canvas to inspect it.'],
    mic:['USB microphone','Capture device · active','76','The microphone feeds Stream Mix directly.'],
    streammix:['Stream Mix','Virtual source · running','100','Stream Mix has 2 incoming links and 1 outgoing link.'],
    headphones:['Built-in headphones','Playback device · default','70','Two streams are currently connected. Default affects new automatic connections.'],
    scarlett:['Scarlett 18i20','Audio interface · Pro Audio','100','Firefox is also connected to playback 1–2. Expand the node to work with individual ports.'],
    obs:['OBS Studio','Recording stream · active','100','OBS receives the Stream Mix virtual microphone.']
  };
  let links=[
    {from:'firefox-out',to:'headphones-in',kind:'policy',owner:'WirePlumber policy',persistence:'Live link · follows default'},
    {from:'firefox-out',to:'scarlett-in',kind:'user',owner:'You',persistence:'Live link · current session'},
    {from:'spotify-out',to:'headphones-in',kind:'policy',owner:'WirePlumber policy',persistence:'Live link · follows default'},
    {from:'mic-out',to:'mix-in',kind:'capture user',owner:'You',persistence:'Remembered link'},
    {from:'firefox-out',to:'mix-in',kind:'capture user',owner:'You',persistence:'Remembered link'},
    {from:'mix-out',to:'obs-in',kind:'capture user',owner:'You',persistence:'Live link · current session'}
  ];
  let selectedNode='firefox', selectedLink=null, drag=null, history=[];
  const port=id=>$(`[data-port="${id}"]`);
  function point(el){const r=el.getBoundingClientRect(),g=graph.getBoundingClientRect();return{x:(el.classList.contains('in')?r.left:r.right)-g.left,y:r.top+r.height/2-g.top}}
  function curve(a,b){const dx=Math.max(55,Math.abs(b.x-a.x)*.44);return `M${a.x} ${a.y} C${a.x+dx} ${a.y} ${b.x-dx} ${b.y} ${b.x} ${b.y}`}
  function draw(){svg.innerHTML='';links.forEach((l,i)=>{const a=port(l.from),b=port(l.to);if(!a||!b)return;const d=curve(point(a),point(b));const wire=document.createElementNS('http://www.w3.org/2000/svg','path');wire.setAttribute('d',d);wire.setAttribute('class',`wire ${l.kind} ${selectedLink===i?'selected':''}`);const hit=document.createElementNS('http://www.w3.org/2000/svg','path');hit.setAttribute('d',d);hit.setAttribute('class','wire-hit');hit.dataset.link=i;hit.addEventListener('click',e=>{e.stopPropagation();selectLink(Number(e.currentTarget.dataset.link))});svg.append(wire,hit)});}
  function selectNode(id){selectedNode=id;selectedLink=null;$$('.node').forEach(n=>n.classList.toggle('selected',n.id===id));$('#nodeInspector').hidden=false;$('#linkInspector').hidden=true;const m=nodeMeta[id];$('#inspectTitle').textContent=m[0];$('#inspectKind').textContent=m[1];$('#inspectVolume').value=m[2];$('#inspectVolumeValue').textContent=m[2]+'%';$('#connectionSummary').textContent=m[3];status.textContent=`${m[0]} selected`;draw()}
  function nodeName(portId){const n=port(portId).closest('.node');return n.querySelector('strong').textContent}
  function nodeIcon(portId){return port(portId).closest('.node').querySelector('.icon').textContent}
  function selectLink(i){selectedLink=i;$$('.node').forEach(n=>n.classList.remove('selected'));const l=links[i];$('#nodeInspector').hidden=true;$('#linkInspector').hidden=false;$('#inspectTitle').textContent='Audio link';$('#inspectKind').textContent=l.kind.includes('capture')?'Capture path · active':'Playback path · active';$('#linkFrom').textContent=nodeName(l.from);$('#linkTo').textContent=nodeName(l.to);$('#linkFromIcon').textContent=nodeIcon(l.from);$('#linkToIcon').textContent=nodeIcon(l.to);$('#linkOwner').textContent=l.owner;$('#linkPersistence').textContent=l.persistence;status.textContent=`${nodeName(l.from)} → ${nodeName(l.to)} selected`;draw()}
  function record(label){history.push(label);status.textContent=label}
  $$('.node').forEach(n=>n.addEventListener('click',e=>{if(!e.target.closest('.port,.mute,input,.expand,.bypass'))selectNode(n.id)}));
  $$('.level input').forEach(r=>r.addEventListener('input',e=>{e.target.nextElementSibling.value=e.target.value+'%';if(e.target.closest('.node').id===selectedNode){$('#inspectVolume').value=e.target.value;$('#inspectVolumeValue').textContent=e.target.value+'%'}}));
  $$('.mute').forEach(b=>b.addEventListener('click',()=>{b.classList.toggle('on');record(`${b.classList.contains('on')?'Muted':'Unmuted'} ${b.closest('.node').querySelector('strong').textContent}`)}));
  $('#inspectVolume').addEventListener('input',e=>{const value=e.target.value;$('#inspectVolumeValue').textContent=value+'%';const n=$('#'+selectedNode);if(n){const r=$('.level input',n);if(r){r.value=value;r.nextElementSibling.value=value+'%'}}});
  $$('.port.out').forEach(p=>p.addEventListener('pointerdown',e=>{e.stopPropagation();drag={from:p.dataset.port,capture:p.classList.contains('capture')};p.setPointerCapture(e.pointerId);$$('.port.in').forEach(x=>x.classList.toggle('possible',x.classList.contains('capture')===drag.capture));status.textContent='Choose a compatible input. Existing links stay connected.'}));
  graph.addEventListener('pointerup',e=>{if(!drag)return;const target=document.elementFromPoint(e.clientX,e.clientY)?.closest('.port.in');if(target&&target.classList.contains('capture')===drag.capture){if(!links.some(l=>l.from===drag.from&&l.to===target.dataset.port)){links.push({from:drag.from,to:target.dataset.port,kind:(drag.capture?'capture ':'')+'user',owner:'You',persistence:'Live link · current session'});record(`Connected ${nodeName(drag.from)} → ${nodeName(target.dataset.port)}`);selectLink(links.length-1)}else status.textContent='That link already exists.'}else status.textContent='Connection cancelled';$$('.possible').forEach(x=>x.classList.remove('possible'));drag=null;draw()});
  $('#removeLink').addEventListener('click',()=>{if(selectedLink===null)return;const l=links[selectedLink],label=`Removed ${nodeName(l.from)} → ${nodeName(l.to)}`;links.splice(selectedLink,1);selectedLink=null;selectNode(selectedNode||'firefox');record(label);draw()});
  $('#rememberLink').addEventListener('click',()=>{if(selectedLink===null)return;links[selectedLink].owner='You';links[selectedLink].persistence='Remembered rule · future matching streams';links[selectedLink].kind=links[selectedLink].kind.replace('policy','user');selectLink(selectedLink);record('Link remembered for future matching streams')});
  $('#focusLinks').addEventListener('click',e=>{const ids=new Set();links.forEach(l=>{const a=port(l.from).closest('.node').id,b=port(l.to).closest('.node').id;if(a===selectedNode||b===selectedNode){ids.add(a);ids.add(b)}});const on=e.target.dataset.on!=='true';e.target.dataset.on=on;e.target.textContent=on?'Show everything':'Show only these';$$('.node').forEach(n=>n.classList.toggle('dimmed',on&&!ids.has(n.id)));status.textContent=on?'Focused connected signal path':'Showing the full graph'});
  $('#expandScarlett').addEventListener('click',e=>{const n=$('#scarlett');n.classList.toggle('expanded');e.target.textContent=n.classList.contains('expanded')?'Collapse ports':'12 ports';setTimeout(draw,0)});
  $('#dismissNotice').addEventListener('click',()=>{$('#notice').classList.add('closed');setTimeout(draw,0)});
  $('#addButton').addEventListener('click',()=>$('#addDialog').showModal());
  $('#addDialog').addEventListener('close',()=>{const v=$('#addDialog').returnValue;if(v&&v!=='cancel')record(`Ready to add ${v.replace('-',' ')}`)});
  $('#searchButton').addEventListener('click',()=>{const p=$('#searchPopover');p.hidden=!p.hidden;if(!p.hidden){$('#searchInput').focus();renderSearch('')}});
  function renderSearch(q){const result=Object.entries(nodeMeta).filter(([,m])=>m[0].toLowerCase().includes(q.toLowerCase()));$('#searchResults').innerHTML=result.map(([id,m])=>`<button data-id="${id}"><strong>${m[0]}</strong><small>${m[1]}</small></button>`).join('');$$('#searchResults button').forEach(b=>b.addEventListener('click',()=>{$('#searchPopover').hidden=true;selectNode(b.dataset.id)}))}
  $('#searchInput').addEventListener('input',e=>renderSearch(e.target.value));
  $('#closeInspector').addEventListener('click',()=>{$('#inspector').style.display='none';$('.main').style.gridTemplateColumns='1fr';setTimeout(draw,0)});
  $('#undo').addEventListener('click',()=>{const h=history.pop();status.textContent=h?`Undid: ${h}`:'Nothing to undo'});
  $('#fit').addEventListener('click',()=>{status.textContent='Graph fitted to the window'});
  window.addEventListener('resize',draw);graph.addEventListener('click',e=>{if(e.target===graph){$$('.node').forEach(n=>n.classList.remove('selected'));selectedLink=null;draw()}});
  requestAnimationFrame(draw);setTimeout(draw,80);
})();
