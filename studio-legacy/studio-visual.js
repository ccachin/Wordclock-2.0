(() => {
    'use strict';
    const byId=id=>document.getElementById(id);
    const matrix = ['ILAESTBDEUX','QUATREDCINQ','SIXNEUFONZE','UNEHSEPTDIX','HUITFTROISZ','UMIDIHEURES','MINUITSETFA','MOINSOVINGT','DIXTLESDEMI','QUARTUCINQO','WGBSDLO****'];
    function timePixels(hour, minute) {
        const lit=new Set();
        if(!Number.isInteger(hour)||hour<0||hour>23||!Number.isInteger(minute)||minute<0||minute>59)return lit;
        const add=(start,length)=>{for(let i=0;i<length;i++)lit.add(start+i);};
        const step=Math.floor(minute/5);
        if(step>=7)hour=(hour+1)%24;
        add(0,2);add(3,3);
        const hours=[[66,6],[33,3],[7,4],[49,5],[11,6],[18,4],[22,3],[37,4],[44,4],[25,4],[41,3],[29,4]];
        if(hour===12)add(56,4);else {add(...hours[hour%12]);if(hour!==0)add(60,hour%12===1?5:6);}
        const mins=[[],[[105,4]],[[88,3]],[[73,2],[99,5]],[[83,5]],[[83,5],[105,4]],[[73,2],[95,4]],[[77,5],[83,5],[105,4]],[[77,5],[83,5]],[[77,5],[92,2],[99,5]],[[77,5],[88,3]],[[77,5],[105,4]]];
        mins[step].forEach(w=>add(...w));add(117,minute%5);return lit;
    }
    function matrixPreview(element,hour=8,minute=0) {
        const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox','0 0 110 110');svg.setAttribute('aria-hidden','true');
        const cells=Array.from(matrix.join(''),(letter,i)=>{const t=document.createElementNS(svg.namespaceURI,'text');t.setAttribute('x',String(5+i%11*10));t.setAttribute('y',String(8+Math.floor(i/11)*10));t.textContent=letter==='*'?'·':letter;svg.append(t);return t;});
        element.replaceChildren(svg);element.classList.add('matrix-preview');
        function setTime(h,m){const lit=timePixels(h,m);cells.forEach((c,i)=>c.classList.toggle('lit',lit.has(i)));}
        setTime(hour,minute);return setTime;
    }
    // Static example: IL EST HUIT HEURES. No device writes.
    const lit = timePixels(8,0);
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
    const level=face=>Number(byId(`studio${face}Brightness`).value)/100;
    const dim=(color,face)=>color.map(n=>n*level(face));
    function render(now) {
        const [f,r]=bases();
        front.style.setProperty('--studio-front',css(dim(effect(f,controls[0],now),'Front')));
        segments.forEach((p,i)=>{const col=css(dim(effect(r,controls[1],now,i),'Rear'));p.setAttribute('fill',col);p.setAttribute('stroke',col);p.style.filter=`drop-shadow(0 0 2px ${col})`;});
    }
    function updatePreview() {
        for(const face of ['Front','Rear'])byId(`studio${face}BrightnessValue`).textContent=byId(`studio${face}Brightness`).value+' %';
        const manual=harmony.value==='manual';
        byId('studioManualRear').hidden=!manual;
        byId('studioAutoRear').hidden=manual;
        byId('studioEffectiveRear').style.backgroundColor=css(bases()[1]);
        render(performance.now());
    }
    controls.forEach(c=>{for(const el of [c.effect,c.speed]) el.addEventListener('input',()=>{c.start=performance.now();sync(c);updatePreview();});sync(c);});
    for(const id of ['studioFrontColor','studioRearColor','studioHarmony','studioFrontBrightness','studioRearBrightness']) byId(id).addEventListener('input',updatePreview);
    window.WordclockStudioVisual=Object.freeze({controls,sync,render,updatePreview,timePixels,matrixPreview,rgb,css,rotate,effect});
})();
