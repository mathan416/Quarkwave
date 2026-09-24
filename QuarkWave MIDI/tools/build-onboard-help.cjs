#!/usr/bin/env node
// Regenerate the Pico's offline Help pages from the Markdown guides.
// Run from the repository root with: npm install && npm run build:help
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const {marked} = require('marked');

const midi = path.resolve(__dirname, '..');
const docsDir = path.join(midi, 'docs');
const uiPath = path.join(midi, 'QuarkWave_UI_MIDI', 'QuarkWave_UI.h');
const outPath = path.join(midi, 'QuarkWave_UI_MIDI', 'QuarkWave_Help.h');
const repoUrl = 'https://github.com/mathan416/Quarkwave/blob/dev/QuarkWave%20MIDI/';
const guides = [
  {id:'start', file:'quick-start.md', title:'Quick start', group:'User Guide'},
  {id:'musician', file:'user-guide.md', title:'Musician guide', group:'User Guide'},
  {id:'patches', file:'patch-book.md', title:'Patch book', group:'User Guide'},
  {id:'lessons', file:'sound-design.md', title:'Sound design lessons', group:'User Guide'},
  {id:'led', file:'led-display-guide.md', title:'Uno LED display', group:'User Guide'},
  {id:'external', file:'external-midi-guide.md', title:'External MIDI', group:'User Guide'},
  {id:'network', file:'network-midi-setup.md', title:'Mac, Windows & Logic', group:'User Guide'},
  {id:'technical', file:'technical-guide.md', title:'Technical guide', group:'Technical details'},
  {id:'connections', file:'connection-guide.md', title:'Connection guide', group:'Technical details'},
];
const byFile = new Map(guides.map(g => [g.file, g]));
const images = new Map();
const escapeHtml = s => String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
const slug = s => s.toLowerCase().normalize('NFKD').replace(/[\u0300-\u036f]/g,'').replace(/[^a-z0-9 -]/g,'').trim().replace(/\s+/g,'-');
const repoLink = relative => repoUrl + relative.split('/').map(encodeURIComponent).join('/');

function renderGuide(guide) {
  const raw = fs.readFileSync(path.join(docsDir, guide.file), 'utf8');
  const seen = new Map();
  const renderer = new marked.Renderer();
  renderer.heading = function(token) {
    if (token.depth === 1) return '';
    const plain = token.text.replace(/<[^>]+>/g,'');
    const base = slug(plain);
    const count = seen.get(base) || 0;
    seen.set(base, count + 1);
    const id = `help-${guide.id}-${base}${count ? '-' + count : ''}`;
    const level = Math.min(6, token.depth + 1);
    return `<h${level} id="${id}">${this.parser.parseInline(token.tokens)}</h${level}>\n`;
  };
  renderer.link = function(token) {
    const href = token.href || '';
    const label = this.parser.parseInline(token.tokens);
    if (/^(https?:|mailto:)/i.test(href)) {
      return `<a href="${escapeHtml(href)}" target="_blank" rel="noopener noreferrer">${label}</a>`;
    }
    const [file, fragment = ''] = href.split('#');
    const normalized = file ? path.posix.normalize(path.posix.join('docs', file)) : `docs/${guide.file}`;
    const target = byFile.get(path.posix.basename(normalized));
    if (target && normalized === `docs/${target.file}`) {
      const anchor = fragment ? `help-${target.id}-${fragment.replace(/--+/g,'-')}` : `help-${target.id}`;
      return `<a href="#${escapeHtml(anchor)}" data-help-link="${target.id}" data-help-anchor="${escapeHtml(anchor)}">${label}</a>`;
    }
    const url = repoLink(normalized) + (fragment ? '#' + encodeURIComponent(fragment) : '');
    return `<a href="${url}" target="_blank" rel="noopener noreferrer">${label}</a>`;
  };
  renderer.image = function(token) {
    const original = path.posix.basename(token.href || '');
    const extension = path.extname(original).toLowerCase();
    const onboard = extension === '.svg' ? original : path.parse(original).name + '.jpg';
    const asset = extension === '.svg'
      ? path.join(docsDir, 'images', original)
      : path.join(docsDir, 'images', 'onboard', onboard);
    if (!fs.existsSync(asset)) throw new Error(`Missing onboard image: ${asset}`);
    images.set(onboard, {file:asset, mime:extension === '.svg' ? 'image/svg+xml' : 'image/jpeg'});
    const alt = escapeHtml(token.text || 'Guide illustration');
    const full = repoLink(`docs/images/${original}`);
    return `<a class="help-illustration" href="${full}" target="_blank" rel="noopener noreferrer" aria-label="Open full-size image: ${alt}"><img loading="lazy" decoding="async" src="/help/images/${encodeURIComponent(onboard)}" alt="${alt}"><span>${alt} · open full size</span></a>`;
  };
  let html = marked.parse(raw, {renderer, gfm:true});
  html = html.replace(/<table>/g,'<div class="help-table-wrap"><table>')
             .replace(/<\/table>/g,'</table></div>');
  return `<article class="help-page help-doc" id="help-${guide.id}" data-help-page="${guide.id}" aria-labelledby="help-${guide.id}-title"><span class="eyebrow">${escapeHtml(guide.group)} / Full guide</span><h2 id="help-${guide.id}-title" tabindex="-1">${escapeHtml(guide.title)}</h2>${html}</article>`;
}

const pages = guides.map(guide => ({...guide, html:renderGuide(guide)}));
let group = '';
const nav = pages.map(guide => {
  const heading = guide.group === group ? '' : `<span class="help-nav-label">${escapeHtml(guide.group)}</span>`;
  group = guide.group;
  return `${heading}<button type="button" data-help-target="${guide.id}"${guide.id === 'start' ? ' aria-current="page"' : ''}>${escapeHtml(guide.title)}</button>`;
}).join('\n          ');
const layout = `      <div class="help-layout">\n        <nav class="help-nav" aria-label="Help guides">\n          ${nav}\n        </nav>\n        <div class="help-content" id="helpContent"><div class="card help-loading" id="helpLoading" role="status">Choose a guide to load it from the Pico.</div></div>\n      </div>`;
const begin = '      <!-- BEGIN GENERATED HELP -->';
const end = '      <!-- END GENERATED HELP -->';
let ui = fs.readFileSync(uiPath, 'utf8');
if (!ui.includes(begin)) {
  const start = ui.indexOf('      <div class="help-layout">');
  const finish = ui.indexOf('\n    </section>\n  </main>', start);
  if (start < 0 || finish < 0) throw new Error('Could not find existing Help layout');
  ui = ui.slice(0,start) + begin + '\n' + layout + '\n' + end + ui.slice(finish);
} else {
  const start = ui.indexOf(begin);
  const finish = ui.indexOf(end,start) + end.length;
  if (finish < end.length) throw new Error('Missing generated Help end marker');
  ui = ui.slice(0,start) + begin + '\n' + layout + '\n' + end + ui.slice(finish);
}
fs.writeFileSync(uiPath, ui);

const source = ['#pragma once', '#include <Arduino.h>', '#include <ESPAsyncWebServer.h>', '// Generated from docs/*.md by tools/build-onboard-help.cjs.', ''];
function emitBytes(symbol, bytes) {
  const rows = [];
  for (let i=0;i<bytes.length;i+=20) rows.push('  ' + Array.from(bytes.subarray(i,i+20), b => '0x'+b.toString(16).padStart(2,'0')).join(',') + ',');
  source.push(`static const uint8_t ${symbol}[] PROGMEM = {`, ...rows, '};', '');
}
for (const page of pages) {
  emitBytes(`HELP_DOC_${page.id.toUpperCase()}`, zlib.gzipSync(Buffer.from(page.html), {level:9}));
}
for (const [name, asset] of images) {
  const symbol = 'HELP_IMG_' + path.parse(name).name.replace(/[^a-z0-9]/gi,'_').toUpperCase();
  asset.symbol = symbol;
  if (asset.mime === 'image/svg+xml') {
    emitBytes(symbol, zlib.gzipSync(fs.readFileSync(asset.file), {level:9}));
  } else {
    emitBytes(symbol, fs.readFileSync(asset.file));
  }
}
source.push('inline void registerOnboardHelpRoutes(AsyncWebServer& server) {');
for (const page of pages) {
  const symbol = `HELP_DOC_${page.id.toUpperCase()}`;
  source.push(`  server.on("/help/${page.id}", HTTP_GET, [](AsyncWebServerRequest* request) { auto* response = request->beginResponse(200, "text/html; charset=utf-8", ${symbol}, sizeof(${symbol})); response->addHeader("Content-Encoding", "gzip"); request->send(response); });`);
}
for (const [name, asset] of images) {
  const call = `auto* response = request->beginResponse(200, "${asset.mime}", ${asset.symbol}, sizeof(${asset.symbol})); ${asset.mime === 'image/svg+xml' ? 'response->addHeader("Content-Encoding", "gzip"); ' : ''}request->send(response);`;
  source.push(`  server.on("/help/images/${name}", HTTP_GET, [](AsyncWebServerRequest* request) { ${call} });`);
}
source.push('}', '');
fs.writeFileSync(outPath, source.join('\n'));
console.log(`Generated ${pages.length} complete guides and ${images.size} images; ${Buffer.byteLength(ui)} main-page bytes, ${fs.statSync(outPath).size} generated-header bytes`);
