// Dumps the mockup's renders of every factory preset (plus the full parameter set) for the C++ parity test.
const {chromium}=require('playwright');const fs=require('fs');
(async()=>{const b=await chromium.launch();const pg=await b.newPage();
await pg.goto('file://'+process.cwd()+'/docs/mockups/KickKitten-v1.html');await pg.waitForTimeout(900);
const o=await pg.evaluate(()=>{const k=window.__kk;const out=[];
 k.FACTORY.forEach(f=>{k.load(f);const p={...k.P};const r=Array.from(k.renderKick(p,k.sr));
  out.push({name:f.name,p,r});});return {sr:k.sr,items:out};});
const keys=new Set();o.items.forEach(i=>Object.keys(i.p).forEach(k=>keys.add(k)));
fs.writeFileSync('tests/parity/keys.txt',[...keys].join('\n')+'\n');
let idx=[];
o.items.forEach((it,n)=>{
  const lines=[];for(const [k,v] of Object.entries(it.p)){if(k==='env'){lines.push('env '+(v?v.map(a=>a.join(',')).join(';'):'-'));}else if(typeof v==='number')lines.push(k+' '+v);}
  fs.writeFileSync(`tests/parity/p${n}.txt`,lines.join('\n')+'\n');
  fs.writeFileSync(`tests/parity/r${n}.f32`,Buffer.from(new Float32Array(it.r).buffer));
  idx.push(n+' '+it.name);});
fs.writeFileSync('tests/parity/index.txt',o.sr+'\n'+idx.join('\n')+'\n');
console.log(o.items.length,'presets, sr',o.sr,'keys',[...keys].join(' '));await b.close();})();
