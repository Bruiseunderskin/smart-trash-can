const fs=require('node:fs');const {Resvg}=require('@resvg/resvg-js');
const [source,target,size]=process.argv.slice(2);
const svg=fs.readFileSync(source,'utf8').replaceAll('currentColor','#000000');
fs.writeFileSync(target,new Resvg(svg,{fitTo:{mode:'width',value:Number(size)}}).render().asPng());
