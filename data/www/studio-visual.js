(() => {
    'use strict';
    const byId=id=>document.getElementById(id);
    const matrix = ['ILAESTBDEUX','QUATREDCINQ','SIXNEUFONZE','UNEHSEPTDIX','HUITFTROISZ','UMIDIHEURES','MINUITSETFA','MOINSOVINGT','DIXTLESDEMI','QUARTUCINQO','WGBSDLO****'];
    // Static example: IL EST HUIT HEURES. No device writes.
    const lit = new Set([0,1,3,4,5,44,45,46,47,60,61,62,63,64,65]);
    const front = byId('studioFrontPreview');
    const rear = byId('studioRearPreview');
    Array.from(matrix.join('')).forEach((letter, index) => {
        const cell = document.createElement('span');
        cell.textContent = letter;
        if (lit.has(index)) cell.classList.add('lit');
        front.appendChild(cell);
    });
    const rgb = hex => [1,3,5].map(i => parseInt(hex.slice(i,i+2),16));
    const css = c => '#' + c.map(x => Math.max(0,Math.min(255,Math.round(x))).toString(16).padStart(2,'0')).join('');
    function hsv(c) {
        const [r,g,b] = c.map(x=>x/255), v=Math.max(r,g,b), d=v-Math.min(r,g,b);
        let h=0;
        if(d) h=60*(v===r?(g-b)/d:v===g?(b-r)/d+2:(r-g)/d+4);
        return [(h+360)%360,v?d/v:0,v];
    }
    function fromHsv(h,s,v) {
        h=((h%360)+360)%360;
        const c=v*s,x=c*(1-Math.abs(h/60%2-1)),m=v-c;
        return [[c,x,0],[x,c,0],[0,c,x],[0,x,c],[x,0,c],[c,0,x]][Math.floor(h/60)].map(n=>Math.round((n+m)*255));
    }
    function rotate(c,degrees) { const [h,s,v]=hsv(c); return fromHsv(h+degrees,s,v); }
    const harmony=byId('studioHarmony');
    const segments=[];
    const svg=rear.querySelector('svg');
    // Five trapezoids on each side, bottom, left, top, right.
    for(let side=0;side<4;side++) {
        for(let i=0;i<5;i++) {
            const path=document.createElementNS('http://www.w3.org/2000/svg','path');
            const a=i*20,b=(i+1)*20,c=10+i*16,d=10+(i+1)*16;
            path.setAttribute('d',`M ${100-a} 100 L ${100-b} 100 L ${100-d} 90 L ${100-c} 90 Z`);
            path.setAttribute('transform',`rotate(${side*90} 50 50)`);
            svg.appendChild(path); segments.push(path);
        }
    }
    const controls=['Front','Rear'].map(face=>({effect:byId(`studio${face}Effect`),speed:byId(`studio${face}Speed`),row:byId(`studio${face}SpeedRow`),value:byId(`studio${face}SpeedValue`),period:byId(`studio${face}Period`),start:performance.now()}));
    const period=c=>30000-290*Number(c.speed.value);
    function sync(c) {
        c.row.hidden=c.effect.value==='solid';
        c.value.textContent=c.speed.value+' %';
        c.period.textContent=`Période : ${(period(c)/1000).toFixed(1)} s`;
    }
    function effect(base,c,now,i=0) {
        const t=((now-c.start)%period(c))/period(c);
        const phase=Math.floor(t*256)/256;
        let level=255;
        switch(c.effect.value) {
            case 'solid': return base;
            case 'breathing': level=40+215*(1+Math.cos(phase*2*Math.PI))/2; break;
            case 'color-cycle': return rotate(base,t*360);
            case 'rainbow': return fromHsv((Math.floor(i*256/20)/256-phase)*360,1,Math.max(...base)/255);
            case 'chase': level=12+243*Math.pow((1+Math.cos((Math.floor(i*256/20)/256-phase)*2*Math.PI))/2,4); break;
        }
        return base.map(n=>n?Math.min(255,Math.floor(n*Math.floor(level)/256)+1):0);
    }
    function bases() {
        const f=rgb(byId('studioFrontColor').value), r=rgb(byId('studioRearColor').value);
        const offsets={identical:0,complementary:180,'analogous-plus':30,'analogous-minus':-30,triadic:120};
        return [f,harmony.value==='manual'?r:rotate(f,offsets[harmony.value])];
    }
    function render(now) {
        const [f,r]=bases();
        front.style.setProperty('--studio-front',css(effect(f,controls[0],now)));
        segments.forEach((p,i)=>{const col=css(effect(r,controls[1],now,i));p.setAttribute('fill',col);p.setAttribute('stroke',col);p.style.filter=`drop-shadow(0 0 2px ${col})`;});
    }
    function updatePreview() {
        const manual=harmony.value==='manual';
        byId('studioManualRear').hidden=!manual;
        byId('studioAutoRear').hidden=manual;
        byId('studioEffectiveRear').style.backgroundColor=css(bases()[1]);
        render(performance.now());
    }
    controls.forEach(c=>{for(const el of [c.effect,c.speed]) el.addEventListener('input',()=>{c.start=performance.now();sync(c);updatePreview();});sync(c);});
    for(const id of ['studioFrontColor','studioRearColor','studioHarmony']) byId(id).addEventListener('input',updatePreview);
    window.WordclockStudioVisual=Object.freeze({controls,sync,render,updatePreview});
})();
