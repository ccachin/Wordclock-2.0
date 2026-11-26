(() => {
    'use strict';
    function validate(data){
        data.name=data.name.trim();data.author=data.author.trim();data.description=data.description.trim();
        if(!data.name)throw new Error('Indiquez un nom.');
        for(const [key,max] of [['name',48],['author',48],['description',200]]) {
            if(new TextEncoder().encode(data[key]).length>max)throw new Error('Texte trop long : '+key+' (limite '+max+' octets UTF-8).');
            if(/[\x00-\x1f\x7f]/.test(data[key]))throw new Error('Les caractères de contrôle et retours à la ligne ne sont pas acceptés.');
        }
    }
    function validateImport(data) {
        if(!data || typeof data!=='object' || Array.isArray(data) || ![2,3].includes(data.version)) throw new Error('Format de thème version 2 ou 3 requis.');
        const bytes=s=>new TextEncoder().encode(s).length;
        function text(key,max,required=false,fallback) {
            const v=data[key]===undefined?fallback:data[key];
            if(typeof v!=='string'||bytes(v)>max||/[\x00-\x1f\x7f]/.test(v)||(required&&!v.trim())) throw new Error('Champ invalide : '+key);
            return v;
        }
        if(data.id!==undefined && (typeof data.id!=='string'||data.id.length>48||!/^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$/.test(data.id))) throw new Error('Identifiant invalide.');
        const result={version:3,name:text('name',48,true),author:text('author',48),description:text('description',200,false,'')};
        for(const key of ['front','rear']) {
            if(typeof data[key]!=='string'||!/^#[0-9a-fA-F]{6}$/.test(data[key])) throw new Error('Couleur invalide : '+key);
            result[key]=data[key];
        }
        for(const [key,options] of Object.entries({harmony:['manual','identical','complementary','analogous-plus','analogous-minus','triadic'],frontEffect:['solid','breathing','color-cycle'],rearEffect:['solid','breathing','color-cycle','rainbow','chase']})) {
            const value=key==='harmony'&&data[key]===undefined?'manual':data[key];
            if(!options.includes(value)) throw new Error('Valeur invalide : '+key);
            result[key]=value;
        }
        for(const key of ['frontSpeed','rearSpeed']) {
            if(!Number.isInteger(data[key])||data[key]<0||data[key]>100) throw new Error('Vitesse invalide : '+key);
            result[key]=data[key];
        }
        for(const key of ['frontBrightness','rearBrightness']) { const v=data.version===2?100:data[key]; if(!Number.isInteger(v)||v<0||v>100)throw new Error('Luminosité invalide : '+key);result[key]=v; }
        if(bytes(JSON.stringify(result))>2048) throw new Error('Thème trop volumineux.');
        return result;
    }
    // Compare portable fields, not device-specific identifiers.
    const nameKey=n=>n.trim().normalize('NFC').toLowerCase();
    function uniqueName(name,pool){
        const used=new Set(pool.map(t=>nameKey(t.name)));
        if(!used.has(nameKey(name)))return name;
        const base=name.replace(/ \(Copie(?: \d+)?\)$/i,'');
        for(let i=1;;i++){
            const suffix=i===1?' (Copie)':` (Copie ${i})`;
            let stem=base;
            while(new TextEncoder().encode(stem+suffix).length>48)stem=Array.from(stem).slice(0,-1).join('');
            const candidate=stem.trimEnd()+suffix;
            if(!used.has(nameKey(candidate)))return candidate;
        }
    }
    function fingerprint(t){
        const v=validateImport(t);validate(v);
        v.name=nameKey(v.name);v.front=v.front.toLowerCase();v.rear=v.rear.toLowerCase();
        return JSON.stringify(v);
    }
    function planImport(theme,pool){
        validate(theme);
        if(pool.some(t=>fingerprint(t)===fingerprint(theme)))return null;
        return {...theme,name:uniqueName(theme.name,pool)};
    }
    function prepareSave(theme,pool,checkIdentical){
        return checkIdentical?planImport(theme,pool):{...theme,name:uniqueName(theme.name,pool)};
    }
    window.WordclockThemeRules=Object.freeze({validate,validateImport,uniqueName,fingerprint,planImport,prepareSave});
})();
