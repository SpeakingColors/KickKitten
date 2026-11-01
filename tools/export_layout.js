// Exports the mockup layout, theme tokens, sound bank and factory presets for the JUCE port.
// Usage: node tools/export_layout.js   (needs playwright)
const {chromium}=require('playwright');const fs=require('fs');const path=require('path');
(async()=>{const b=await chromium.launch();const pg=await b.newPage({viewport:{width:1800,height:1100}});
await pg.goto('file://'+path.resolve('docs/mockups/KickKitten-v1.html'));await pg.waitForTimeout(1200);
const out=await pg.evaluate(()=>{
  const k=window.__kk;
  const win=document.getElementById('win');const wr0=win.getBoundingClientRect();const sc=wr0.width/1640;
  const R=e=>{const r=e.getBoundingClientRect();return [Math.round((r.left-wr0.left)/sc*10)/10,Math.round((r.top-wr0.top)/sc*10)/10,Math.round(r.width/sc*10)/10,Math.round(r.height/sc*10)/10];};
  const items=[];
  win.querySelectorAll('section.panel,header,footer,canvas,.knob,button,select,.chip,.kl,.grp>i,h2,.wk,.bk,#bpm,input,.sub,label').forEach(e=>{
    if(e.offsetParent===null&&e.tagName!=='CANVAS')return;
    const cs=getComputedStyle(e);if(cs.display==='none'||cs.visibility==='hidden')return;
    const r=R(e);if(r[2]<1||r[3]<1)return;
    items.push({tag:e.tagName.toLowerCase(),cls:e.className&&e.className.baseVal===undefined?String(e.className):'',id:e.id||'',kid:(e.dataset&&e.dataset.id)||'',m:e.dataset&&e.dataset.m||'',text:(e.tagName==='CANVAS'||e.tagName==='SECTION')?'':(e.innerText||'').slice(0,40).replace(/\n/g,' '),tip:(e.dataset&&e.dataset.tip)||'',r});
  });
  const css={};const cs=getComputedStyle(document.documentElement);
  ['ground','panel','display','line','text','dim','cap','grid','curve','curve2','fill','read','c-click','c-body','c-sub','m-ok','m-warn','m-clip','title'].forEach(n=>css[n]=cs.getPropertyValue('--'+n).trim());
  return {W:1640,H:946,items,css,scale:sc};
});
fs.writeFileSync('assets/layout.json',JSON.stringify(out,null,1));
const data=await pg.evaluate(()=>{const k=window.__kk;
  const spec=k.SPEC.map(s=>({id:s.id,label:s.label,min:s.min,max:s.max,unit:s.unit,dec:s.dec,def:s.def,log:!!s.log,tip:s.tip}));
  const bank=k.BANK.map((b,i)=>({name:b.name,group:b.group,sub:b.sub,len:b.len}));
  const pre=k.FACTORY.map(f=>({name:f.name,tag:f.tag,p:f.p}));
  const names=k.SRCN,lens=k.SRCLEN;
  const pcm=k.BANK.map((b,i)=>{const x=k.bankBuf(i);return {sr:x.sr,d:Array.from(x.data)};});
  return {spec,bank,pre,names,lens,pcm};});
// bank.bin: per entry: uint32 sr, uint32 n, then n float32
const parts=[];const meta=[];let off=0;
for(let i=0;i<data.pcm.length;i++){const e=data.pcm[i];const hdr=Buffer.alloc(8);hdr.writeUInt32LE(e.sr,0);hdr.writeUInt32LE(e.d.length,4);const f=new Float32Array(e.d);parts.push(hdr,Buffer.from(f.buffer));meta.push({...data.bank[i],sr:e.sr,n:e.d.length,off});off+=8+4*e.d.length;}
fs.writeFileSync('assets/bank.bin',Buffer.concat(parts));
fs.writeFileSync('assets/bank.json',JSON.stringify({bank:meta,names:data.names,lens:data.lens}));
fs.writeFileSync('assets/spec.json',JSON.stringify(data.spec));
fs.writeFileSync('assets/presets.json',JSON.stringify(data.pre));
console.log('bank entries',meta.length,'bytes',off,'presets',data.pre.length,'spec',data.spec.length);
console.log(out.items.length,'items');
const knobs=out.items.filter(i=>i.cls.includes('knob'));console.log('knobs',knobs.length,knobs.slice(0,3));
await b.close();})();
