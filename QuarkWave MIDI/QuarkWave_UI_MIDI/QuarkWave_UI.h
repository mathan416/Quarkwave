#pragma once
// Keep in flash on AVR; harmless elsewhere
#include <Arduino.h>

// Definition lives below as a UTF-8 raw string.
static const char INDEX_HTML[] PROGMEM = u8R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0b1422">
<title>⚛🌊 QuarkWave — Sound Design Studio</title>
<style>
  :root{color-scheme:dark;font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
  *{box-sizing:border-box}
  body{--accent:#51d9e8;--accent-soft:rgba(81,217,232,.18);--glow:rgba(33,176,210,.17);margin:0;min-width:280px;color:#edf3fb;background:radial-gradient(circle at 72% -12%,var(--glow),transparent 42%),#0a101b;line-height:1.45;transition:background-color .2s}
  body[data-view="shape"]{--accent:#f4bd79;--accent-soft:rgba(244,189,121,.17);--glow:rgba(182,111,39,.16)}
  body[data-view="explore"]{--accent:#c49afa;--accent-soft:rgba(196,154,250,.18);--glow:rgba(130,74,185,.17)}
  body[data-view="all"]{--accent:#8de9c2;--accent-soft:rgba(141,233,194,.16);--glow:rgba(62,161,123,.15)}
  body[data-view="help"]{--accent:#8de9c2;--accent-soft:rgba(141,233,194,.16);--glow:rgba(62,161,123,.15)}
  button,input,select{font:inherit}
  button{cursor:pointer}
  button:disabled{cursor:not-allowed;opacity:.48}
  button:focus-visible,input:focus-visible,select:focus-visible,summary:focus-visible,a:focus-visible{outline:3px solid var(--accent);outline-offset:3px}
  [hidden]{display:none!important}
  .app-shell{max-width:1512px;margin:auto;padding:28px clamp(16px,2.6vw,42px) 32px}
  .masthead{display:flex;align-items:center;justify-content:space-between;gap:20px;margin-bottom:23px}
  .brand{display:flex;align-items:center;gap:15px;min-width:0}
  .brand-mark{flex:none;width:64px;height:48px;font-size:23px;letter-spacing:-.14em;white-space:nowrap;border:1px solid rgba(117,221,234,.34);border-radius:16px;display:grid;place-items:center;color:#86e6ed;background:linear-gradient(145deg,#1a394d,#132332 62%,#172338);box-shadow:0 12px 30px rgba(0,0,0,.22)}
  .brand-name{font-size:clamp(24px,2.5vw,34px);line-height:1;font-weight:750;letter-spacing:-.055em}
  .brand-name span{color:#8de9ef}
  .brand-sub{margin-top:4px;color:#a9b9cd;font-size:12px;letter-spacing:.16em;text-transform:uppercase;font-weight:700}
  .connection{display:flex;align-items:center;gap:10px;padding:10px 14px;border:1px solid #345165;background:#132334;border-radius:999px;color:#d5edf4;font-size:13px;font-weight:700;white-space:nowrap}
  .link-statuses{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}
  .connection--uno{border-color:#574d66;background:#211d31}
  .connection-dot{width:8px;height:8px;border-radius:50%;background:#8092a5;box-shadow:0 0 0 4px rgba(143,158,173,.12)}
  .connection[data-connected="true"] .connection-dot{background:#52d6c9;box-shadow:0 0 0 4px rgba(82,214,201,.13)}
  .eyebrow{display:block;color:var(--accent);font-size:11px;text-transform:uppercase;letter-spacing:.19em;font-weight:800}
  .topbar{display:flex;align-items:end;gap:12px;flex-wrap:wrap;padding:15px;background:#101d2b;border:1px solid #253d50;border-radius:22px;box-shadow:0 16px 38px rgba(0,0,0,.15)}
  .field{display:flex;flex-direction:column;gap:6px;min-width:0}
  .field label{color:#aabbd1;font-size:11px;letter-spacing:.13em;text-transform:uppercase;font-weight:800}
  .field--patch{flex:1 1 200px}.field--name{flex:1 1 180px}
  select,input[type="text"]{height:44px;min-width:0;width:100%;padding:0 13px;border:1px solid #3d5369;border-radius:12px;background:#17293b;color:#f1f6fd}
  select:hover,input[type="text"]:hover{border-color:#7390a4}
  .topbar-actions{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
  .action-status{flex-basis:100%;min-height:18px;color:#a9c9d9;font-size:12px}
  .save-dialog{width:min(440px,calc(100vw - 28px));padding:22px;border:1px solid #587489;border-radius:20px;background:#152637;color:#edf3fb;box-shadow:0 24px 65px rgba(0,0,0,.6)}
  .save-dialog::backdrop{background:rgba(3,9,17,.75)}
  .save-dialog h2{margin:0 0 8px;font-size:22px}.save-dialog p{color:#b7cadb;font-size:14px}
  .save-dialog label{display:block;margin:17px 0 6px;font-size:13px;font-weight:700}.save-dialog-actions{display:flex;justify-content:flex-end;gap:9px;margin-top:20px}
  .btn{min-height:44px;border:1px solid #456076;border-radius:12px;padding:0 16px;background:#1c3041;color:#eef6ff;font-size:13px;font-weight:750;white-space:nowrap;transition:background .16s,border-color .16s,transform .16s}
  .btn:hover{background:#2b465c;border-color:#6c92a9}.btn:active{transform:translateY(1px)}
  .btn--primary{background:var(--accent);color:#071621;border-color:var(--accent)}
  .btn--primary:hover{background:#fff;border-color:#fff}
  .btn--danger{background:#632b3a;border-color:#9b5362;color:#fff}
  .btn--danger:hover{background:#8a354a;border-color:#dc7180}
  .btn--small{min-width:44px;padding:0 12px}
  .more{position:relative}
  .more summary{list-style:none;min-height:44px;padding:0 15px;display:flex;align-items:center;justify-content:center;border:1px solid #456076;border-radius:12px;background:#1c3041;font-size:13px;font-weight:750;cursor:pointer}
  .more summary::-webkit-details-marker{display:none}.more[open] summary{border-color:var(--accent)}
  .more-panel{position:absolute;z-index:9;right:0;top:calc(100% + 10px);width:min(270px,90vw);display:grid;gap:8px;padding:12px;border:1px solid #52637b;background:#17283a;border-radius:16px;box-shadow:0 20px 42px rgba(0,0,0,.4)}
  .more-panel .btn{width:100%;text-align:left}
  .more-panel p{margin:5px 4px;color:#aabbd1;font-size:12px}
  .view-tabs{display:flex;gap:8px;margin:22px 0 20px;padding:6px;width:max-content;max-width:100%;border:1px solid #33465c;border-radius:16px;background:#101b29;overflow-x:auto}
  .view-tab{min-height:48px;min-width:135px;border:0;border-radius:11px;padding:8px 16px;background:transparent;color:#b7c9db;text-align:left;white-space:nowrap}
  .view-tab strong{display:block;color:inherit;font-size:14px}.view-tab small{display:block;margin-top:2px;font-size:11px;opacity:.82}
  .view-tab[aria-selected="true"]{background:var(--accent-soft);color:#f8fcff;box-shadow:inset 0 0 0 1px var(--accent)}
  .view-tab[aria-selected="true"] strong{color:var(--accent)}
  .view-panel{animation:appear .2s ease}
  @keyframes appear{from{opacity:.5;transform:translateY(5px)}to{opacity:1;transform:none}}
  .view-intro{display:flex;align-items:end;justify-content:space-between;gap:16px;margin:0 0 17px}
  .view-intro h1{margin:4px 0 3px;font-size:clamp(29px,3.3vw,44px);line-height:1.05;letter-spacing:-.055em}
  .view-intro p{margin:0;color:#b8c7d9;font-size:14px}
  main{margin-top:19px}
  .panel-grid{display:grid;gap:16px}
  .perform-grid{grid-template-columns:minmax(0,1.15fr) minmax(320px,.85fr)}
  .shape-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
  .explore-grid{grid-template-columns:repeat(3,minmax(0,1fr))}
  .all-shape-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
  .all-explore-grid{grid-template-columns:repeat(3,minmax(0,1fr))}
  .all-section{margin-top:25px}.all-section h2{margin:0 0 13px;font-size:23px;letter-spacing:-.035em}
  .help-links{display:flex;flex-wrap:wrap;gap:9px;margin:0 0 19px}
  .help-links a{min-height:44px;display:inline-flex;align-items:center;padding:0 15px;border:1px solid #4d6e70;border-radius:12px;background:#1a3338;color:#d5f7ea;font-size:13px;font-weight:750;text-decoration:none}
  .help-links a:hover{border-color:var(--accent);background:#23444a}
  .help-section{margin-top:27px;scroll-margin-top:20px}
  .help-section h2{margin:0 0 6px;font-size:25px;letter-spacing:-.04em}
  .help-section>p{margin:0 0 15px;color:#b7c9d5;font-size:14px}
  .help-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}
  .help-card h3{margin:0 0 10px;font-size:18px}
  .help-card p,.help-card li,.help-card dd{color:#c9d7e3;font-size:14px;line-height:1.55}
  .help-card p{margin:8px 0 0}.help-card ol,.help-card ul{margin:0;padding-left:22px}.help-card li+li{margin-top:6px}
  .help-card strong,.help-card dt{color:#f2f8fd}.help-card dl{margin:0}.help-card dt{font-size:13px;font-weight:800}.help-card dd{margin:0 0 10px}
  .help-wide{grid-column:1/-1}
  .help-flow{display:flex;align-items:center;flex-wrap:wrap;gap:8px;margin:12px 0;color:#d5f7ea;font-size:13px;font-weight:750}
  .help-flow span{padding:8px 11px;border:1px solid #486b70;border-radius:10px;background:#142c35}
  .help-flow b{color:var(--accent)}
  .help-layout{display:grid;grid-template-columns:220px minmax(0,1fr);align-items:start;gap:20px}
  .help-content{min-width:0}
  .help-nav{position:sticky;top:16px;display:grid;gap:4px;padding:15px;border:1px solid #34545a;border-radius:19px;background:#132730}
  .help-nav-label{margin:10px 10px 3px;color:#91acae;font-size:11px;font-weight:800;letter-spacing:.14em;text-transform:uppercase}
  .help-nav-label:first-child{margin-top:0}
  .help-nav button{min-height:42px;padding:9px 11px;border:1px solid transparent;border-radius:10px;background:transparent;color:#c7d9dc;text-align:left;font-size:13px;font-weight:700}
  .help-nav button:hover{background:#204047}
  .help-nav button[aria-current="page"]{border-color:var(--accent);background:var(--accent-soft);color:#f7fffb}
  .help-page{min-width:0}
  .help-page h2{margin:2px 0 7px;font-size:clamp(25px,2.5vw,34px);letter-spacing:-.045em}
  .help-page h3{margin:0 0 9px;font-size:18px}
  .help-page-lead{margin:0 0 17px;color:#b8ced1;font-size:14px}
  .help-page .card{margin-bottom:15px}
  .help-page .card:last-child{margin-bottom:0}
  .help-page p,.help-page li,.help-page td,.help-page th{font-size:14px;line-height:1.55}
  .help-page p{margin:7px 0 0;color:#cbd9e3}
  .help-page ol,.help-page ul{margin:8px 0 0;padding-left:22px;color:#cbd9e3}
  .help-page li+li{margin-top:7px}
  .help-page strong{color:#f3fbfd}
  .help-page .help-result{margin-top:13px;padding:11px 13px;border-left:3px solid var(--accent);border-radius:0 9px 9px 0;background:#173238;color:#e2f6f1}
  .help-table-wrap{width:100%;overflow-x:auto;margin-top:11px}
  .help-page table{border-collapse:collapse;width:100%;min-width:480px;text-align:left}
  .help-page th,.help-page td{padding:9px 11px;border-bottom:1px solid #405365;vertical-align:top}
  .help-page th{color:#b9f2db;font-weight:800}
  .help-page code{padding:2px 5px;border-radius:5px;background:#0d202b;color:#b9f2db;font-size:.92em;overflow-wrap:anywhere}
  .help-page pre{overflow-x:auto;padding:13px;border:1px solid #405365;border-radius:11px;background:#0d202b}
  .help-page pre code{padding:0;white-space:pre}
  .help-note{color:#a9bfca!important;font-size:12px!important}
  .card{min-width:0;padding:22px;border:1px solid #30465a;border-radius:23px;background:linear-gradient(145deg,rgba(28,46,63,.96),rgba(17,30,45,.97));box-shadow:0 14px 32px rgba(0,0,0,.12)}
  body[data-view="shape"] .card{background:linear-gradient(145deg,#2b292a,#1d222b);border-color:#58483e}
  body[data-view="explore"] .card{background:linear-gradient(145deg,#292640,#1a1e33);border-color:#53466d}
  .card--hero{min-height:265px;display:flex;flex-direction:column;justify-content:space-between;overflow:hidden;position:relative;background:radial-gradient(circle at 68% 45%,var(--accent-soft),transparent 36%),linear-gradient(125deg,#14283c,#0e1b2c 72%)}
  .hero-copy{position:relative;z-index:1;max-width:480px}
  .hero-copy h2{margin:8px 0 10px;font-size:clamp(31px,3.1vw,50px);line-height:1.03;letter-spacing:-.06em}
  .hero-copy p{margin:0;color:#c0cfde;font-size:15px;max-width:390px}
  .hero-art{position:absolute;right:-30px;bottom:-58px;width:min(56%,400px);aspect-ratio:1;border:1px solid rgba(81,217,232,.28);border-radius:50%;box-shadow:0 0 0 34px rgba(81,217,232,.055),0 0 0 70px rgba(81,217,232,.04),inset 0 0 70px rgba(81,217,232,.09)}
  .hero-art:before,.hero-art:after{content:"";position:absolute;border:1px solid rgba(81,217,232,.24);border-radius:50%;inset:17%}.hero-art:after{inset:33%;background:rgba(81,217,232,.08)}
  .hero-foot{position:relative;z-index:1;display:flex;gap:16px;flex-wrap:wrap;color:#a9c9d9;font-size:12px;font-weight:700}
  .hero-foot span:before{content:"•";color:var(--accent);margin-right:8px}
  .card-head{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;margin-bottom:21px}
  .card-head h2{margin:2px 0 0;font-size:20px;letter-spacing:-.035em}
  .card-head p{margin:3px 0 0;color:#a9bad0;font-size:12px}
  .card-index{color:var(--accent);font-size:12px;font-weight:800;letter-spacing:.1em}
  .control-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px 20px}
  .control-grid--three{grid-template-columns:repeat(3,minmax(0,1fr))}
  .control-grid--one{grid-template-columns:1fr}
  .control{min-width:0;display:grid;grid-template-columns:minmax(0,1fr) auto;gap:8px 10px;align-items:center;color:#dce8f3;font-size:13px;font-weight:650}
  .control input[type="range"]{grid-column:1 / -1;grid-row:2;width:100%;height:24px;margin:0;accent-color:var(--accent);cursor:pointer}
  .control-value{grid-column:2;grid-row:1;font-size:12px;color:var(--accent);font-variant-numeric:tabular-nums;white-space:nowrap}
  .control select{grid-column:1 / -1}
  .switch-row{display:flex;align-items:center;gap:11px;min-height:44px;color:#dce8f3;font-size:13px;font-weight:650}
  .switch-row input[type="checkbox"]{width:19px;height:19px;margin:0;accent-color:var(--accent)}
  .select-control{display:grid;gap:7px;color:#dce8f3;font-size:13px;font-weight:650}
  .select-control select{width:100%}
  .section-note{margin:14px 0 0;color:#a5b8cd;font-size:12px}
  .visual-card{display:grid;align-content:space-between;gap:14px;min-height:185px;border:1px solid #33495c;border-radius:18px;padding:18px;background:linear-gradient(135deg,#0e1c2d,#14263b)}
  .visual-card svg{display:block;width:100%;height:90px;overflow:visible}
  .visual-card path{fill:none;stroke:var(--accent);stroke-width:2.5;stroke-linecap:round;stroke-linejoin:round}
  .visual-card .gridline{stroke:#426074;stroke-width:1;opacity:.55}
  .visual-card figcaption{color:#aabdd0;font-size:12px}
  .shape-card{display:grid;grid-template-columns:minmax(0,1fr) minmax(150px,.8fr);gap:20px}
  .shape-card .card-head{grid-column:1/-1;margin-bottom:0}
  .fx-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:13px}
  .effect{border:1px solid #384765;border-radius:17px;padding:16px;background:#1c2540}
  .effect h3{margin:0 0 14px;font-size:14px;color:#f3eaff}
  .effect .control+.control{margin-top:15px}
  .effect--wide{grid-column:1/-1}
  .tempo-value{display:inline-flex;align-items:center;justify-content:center;min-width:50px;padding:5px 8px;color:var(--accent);font-size:13px;font-variant-numeric:tabular-nums;font-weight:750;background:var(--accent-soft);border-radius:8px}
  .viz-picks{display:flex;flex-wrap:wrap;gap:8px}
  .viz-picks .btn{flex:1 1 90px}
  .keyboard-dock{margin-top:18px;padding:20px;border:1px solid #30485b;border-radius:24px;background:#101d2b}
  .keyboard-head{display:flex;justify-content:space-between;align-items:center;gap:16px;margin-bottom:14px}
  .keyboard-head h2{margin:2px 0 0;font-size:20px}
  .keyboard-head p{margin:3px 0 0;color:#a5b7c9;font-size:12px}
  .keyboard-actions{display:flex;align-items:center;justify-content:flex-end;gap:18px;flex-wrap:wrap}
  .velocity-control{display:grid;grid-template-columns:auto 118px auto;align-items:center;gap:9px;color:#c9d9e7;font-size:12px;font-weight:700}
  .velocity-control input{width:118px;min-height:32px;accent-color:var(--accent)}
  .velocity-control output{min-width:25px;text-align:right;color:var(--accent);font-size:16px;font-variant-numeric:tabular-nums}
  .octave{display:flex;align-items:center;gap:8px;color:#c9d9e7;font-size:12px;font-weight:700;white-space:nowrap}
  #octLbl{min-width:24px;text-align:center;color:var(--accent);font-size:18px;font-variant-numeric:tabular-nums}
  .keyboard-scroll{overflow-x:auto;overscroll-behavior-inline:contain;padding:4px 2px 8px}
  #kb{display:flex;position:relative;user-select:none;min-height:113px;touch-action:pan-x}
  #kb button{position:relative;flex:none;height:108px;border:1px solid #657588;border-radius:0 0 7px 7px;overflow:hidden;touch-action:none;transition:background .08s,transform .08s}
  body[data-view="perform"] #kb{min-height:145px}
  body[data-view="perform"] #kb button.b{height:140px}
  body[data-view="perform"] #kb button.s{height:90px}
  #kb button.b{background:linear-gradient(#fcfdff,#dce7ee);color:#35485a}
  #kb button.s{background:linear-gradient(#31465a,#142231);color:#fff;height:70px;z-index:2;border-color:#091826}
  #kb button .lbl{position:absolute;left:0;right:0;bottom:8px;text-align:center;font-size:11px;font-weight:750;opacity:.8;pointer-events:none}
  #kb button.b.active{background:#9eeaf0;transform:translateY(2px)}
  #kb button.s.active{background:#207e93;transform:translateY(2px)}
  #kb.disabled{opacity:.55}#kb.disabled button{pointer-events:none}
  #kb button:focus-visible{outline:3px solid #eea64b;outline-offset:-4px;z-index:4}
  .footer{margin-top:23px;display:flex;justify-content:space-between;gap:12px;color:#8fa4b9;font-size:11px}
  .footer-credit{text-align:right}
  .footer-credit strong{color:#dbe9f5;font-weight:700}
  @media(max-width:1080px){.explore-grid,.all-explore-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.perform-grid{grid-template-columns:1fr 1fr}.shape-card{grid-template-columns:1fr}.shape-card .card-head{grid-column:1}.control-grid--three{grid-template-columns:repeat(2,minmax(0,1fr))}}
  @media(max-width:760px){.app-shell{padding:18px 14px 28px}.masthead{align-items:flex-start}.brand-mark{width:58px;height:42px;font-size:20px}.brand-name{font-size:26px}.brand-sub{font-size:10px}.topbar{border-radius:18px}.topbar-actions{width:100%}.topbar-actions .btn,.topbar-actions .more{flex:1}.more summary{width:100%}.view-tabs{width:100%}.view-tab{flex:0 0 auto;min-width:120px;padding:8px 12px}.view-tab small{display:none}.perform-grid,.shape-grid,.explore-grid,.all-shape-grid,.all-explore-grid,.help-grid{grid-template-columns:1fr}.help-layout{grid-template-columns:minmax(0,1fr)}.help-nav{position:static;grid-template-columns:repeat(2,minmax(0,1fr))}.help-nav-label{grid-column:1/-1}.card{padding:19px}.card--hero{min-height:225px}.hero-art{width:270px;opacity:.6}.keyboard-head{flex-wrap:wrap}.keyboard-actions{width:100%;justify-content:space-between}.keyboard-dock{padding:16px}.footer{flex-direction:column}.footer-credit{text-align:left}}
  @media(max-width:450px){.masthead{flex-direction:column;gap:12px}.link-statuses{justify-content:flex-start}.connection{align-self:flex-start;max-width:100%;white-space:normal}.field--patch,.field--name{flex-basis:100%}.topbar-actions{display:grid;grid-template-columns:1fr 1fr}.topbar-actions .more{grid-column:1/-1}.control-grid,.control-grid--three,.fx-grid{grid-template-columns:1fr}.shape-card{grid-template-columns:1fr}.view-intro h1{font-size:31px}.keyboard-head{align-items:flex-start}.keyboard-head p{max-width:170px}.effect{padding:14px}}
  @media(prefers-reduced-motion:reduce){*,*:before,*:after{scroll-behavior:auto!important;animation:none!important;transition:none!important}}
</style>
</head>
<body data-view="perform">
<div class="app-shell">
  <header class="masthead">
    <div class="brand">
      <div class="brand-mark" aria-hidden="true">⚛🌊</div>
      <div><div class="brand-name">Quark<span>Wave</span></div><div class="brand-sub">Sound design studio</div></div>
    </div>
    <div class="link-statuses"><div class="connection" id="connection" data-connected="false"><span class="connection-dot" aria-hidden="true"></span><span id="s" aria-live="polite">Pico: Connecting…</span></div><div class="connection connection--uno" id="unoConnection" data-connected="false"><span class="connection-dot" aria-hidden="true"></span><span id="unoLabel" aria-live="polite">Uno: Checking…</span></div><div class="connection" id="rtpConnection" data-connected="false"><span class="connection-dot" aria-hidden="true"></span><span id="rtpLabel" aria-live="polite">RTP-MIDI: Checking…</span></div></div>
  </header>

  <section class="topbar" aria-label="Patch and session">
    <div class="field field--patch"><label for="patch">Current patch</label><select id="patch"></select></div>
    <div class="field field--name"><label for="pname">Patch name</label><input id="pname" type="text" maxlength="16" placeholder="Name your sound" autocomplete="off"></div>
    <div class="topbar-actions">
      <button id="patchLoad" type="button" class="btn">Load patch</button>
      <button id="patchSave" type="button" class="btn btn--primary">Save patch</button>
      <button id="randomBtn" type="button" class="btn">Randomize</button>
      <button id="panicBtn" type="button" class="btn btn--danger" aria-label="Panic — all sound off">Panic</button>
      <details class="more" id="moreActions"><summary>More actions</summary><div class="more-panel">
        <button id="commitBtn" type="button" class="btn">Commit snapshot</button>
        <button id="loadCommitBtn" type="button" class="btn">Load committed sound</button>
        <button id="syncUnoBtn" type="button" class="btn" disabled>Sync Pico patch to Uno</button>
        <p>These actions affect the current sound. Select a user slot before saving.</p>
      </div></details>
    </div>
    <div id="actionStatus" class="action-status" role="status" aria-live="polite"></div>
  </section>

  <dialog id="saveAsDialog" class="save-dialog" aria-labelledby="saveAsTitle">
    <h2 id="saveAsTitle">Save factory sound as a user patch</h2>
    <p>Choose a user slot. Occupied slots require confirmation before replacement.</p>
    <label for="saveAsSlot">User slot</label>
    <select id="saveAsSlot"></select>
    <div class="save-dialog-actions"><button id="saveAsCancel" type="button" class="btn">Cancel</button><button id="saveAsConfirm" type="button" class="btn btn--primary">Save to slot</button></div>
  </dialog>

  <nav class="view-tabs" role="tablist" aria-label="QuarkWave views">
    <button class="view-tab" type="button" role="tab" id="tab-perform" aria-controls="view-perform" aria-selected="true" data-view="perform"><strong>Perform</strong><small>Play and react</small></button>
    <button class="view-tab" type="button" role="tab" id="tab-shape" aria-controls="view-shape" aria-selected="false" data-view="shape" tabindex="-1"><strong>Shape</strong><small>Sculpt the core sound</small></button>
    <button class="view-tab" type="button" role="tab" id="tab-explore" aria-controls="view-explore" aria-selected="false" data-view="explore" tabindex="-1"><strong>Explore</strong><small>Movement and effects</small></button>
    <button class="view-tab" type="button" role="tab" id="tab-all" aria-controls="view-all" aria-selected="false" data-view="all" tabindex="-1"><strong>All controls</strong><small>Classic synth panel</small></button>
    <button class="view-tab" type="button" role="tab" id="tab-help" aria-controls="view-help" aria-selected="false" data-view="help" tabindex="-1"><strong>Help</strong><small>Guides and details</small></button>
  </nav>

  <section class="keyboard-dock" aria-label="Playable keyboard"><div class="keyboard-head"><div><span class="eyebrow">Always ready</span><h2>Keyboard</h2><p>Play here or use A W S E D F T G Y H U J.</p></div><div class="keyboard-actions"><label class="velocity-control" for="kvel"><span>Velocity</span><input id="kvel" type="range" min="1" max="127" value="100" aria-describedby="kvelValue"><output id="kvelValue" for="kvel">100</output></label><div class="octave"><span>Octave</span><button type="button" id="octD" class="btn btn--small" aria-label="Octave down">−</button><span id="octLbl">3</span><button type="button" id="octU" class="btn btn--small" aria-label="Octave up">+</button></div></div></div><div class="keyboard-scroll"><div id="kb"></div></div></section>

  <main>
    <section class="view-panel" id="view-perform" role="tabpanel" aria-labelledby="tab-perform">
      <div class="view-intro"><div><span class="eyebrow">01 / Live desk</span><h1>Perform</h1><p>Play first. Keep the essential sound controls within reach.</p></div></div>
      <div class="panel-grid perform-grid">
        <div class="card card--hero"><div class="hero-copy"><span class="eyebrow">Your instrument is ready</span><h2>Find a sound.<br>Make it yours.</h2><p>Choose a patch, hold a note, and shape the moment with just a few moves.</p></div><div class="hero-art" aria-hidden="true"></div><div class="hero-foot"><span>4 voice synthesis</span><span>Live browser keyboard</span><span>Patch recall</span></div></div>
        <div class="card"><div class="card-head"><div><span class="eyebrow">Quick controls</span><h2>At your fingertips</h2><p>Linked to the detailed controls in Shape and Explore.</p></div><span class="card-index">LIVE</span></div>
          <div class="control-grid">
            <label class="control" for="perfVol">Volume<input id="perfVol" type="range" min="0" max="127" value="100"></label>
            <label class="control" for="perfCut">Filter cutoff<input id="perfCut" type="range" min="0" max="127" value="70"></label>
            <label class="control" for="perfMorph">Osc morph<input id="perfMorph" type="range" min="0" max="127" value="0"></label>
            <label class="select-control" for="perfArp">Arpeggiator<select id="perfArp"><option value="0">Off</option><option value="1">Up</option><option value="2">Down</option><option value="3">Up–Down</option><option value="4">Random</option></select></label>
          </div>
          <label class="switch-row" for="perfPed" style="margin-top:18px"><input type="checkbox" id="perfPed">Sustain notes</label>
          <p class="section-note">The sound stays the same when you change views.</p>
        </div>
      </div>
    </section>

    <section class="view-panel" id="view-shape" role="tabpanel" aria-labelledby="tab-shape" hidden>
      <div class="view-intro"><div><span class="eyebrow">02 / Sound workbench</span><h1>Shape</h1><p>Build the sound from oscillator to envelope to filter.</p></div></div>
      <div class="panel-grid shape-grid">
        <div class="card shape-card"><div class="card-head"><div><span class="eyebrow">Source</span><h2>Oscillator & texture</h2><p>Choose the wave character, then give it width.</p></div><span class="card-index">01</span></div>
          <div class="control-grid control-grid--one">
            <label class="control" for="morph">Morph<input type="range" data-cc="76" min="0" max="127" value="0" id="morph"></label>
            <label class="select-control" for="unisonSel">Unison<select id="unisonSel"><option value="0">1 oscillator</option><option value="64">2 oscillators</option><option value="127">3 oscillators</option></select></label>
            <label class="control" for="detune">Detune<input type="range" data-cc="94" min="0" max="127" value="30" id="detune"></label>
            <label class="control" for="noise">Noise<input type="range" data-cc="93" min="0" max="127" value="0" id="noise"></label>
          </div>
          <figure class="visual-card" style="margin:0"><span class="eyebrow">Wave preview</span><svg viewBox="0 0 300 90" role="img" aria-label="Oscillator waveform preview"><path class="gridline" d="M0 45H300"></path><path id="wavePath" d=""></path></svg><figcaption>Sine → triangle → saw → square</figcaption></figure>
        </div>
        <div class="card shape-card"><div class="card-head"><div><span class="eyebrow">Contour</span><h2>Envelope</h2><p>How a note starts, settles, holds and fades.</p></div><span class="card-index">02</span></div>
          <div class="control-grid control-grid--one">
            <label class="control" for="a">Attack<input type="range" data-cc="73" min="0" max="127" value="16" id="a"></label>
            <label class="control" for="d">Decay<input type="range" data-cc="75" min="0" max="127" value="35" id="d"></label>
            <label class="control" for="sust">Sustain<input type="range" data-cc="23" min="0" max="127" value="90" id="sust"></label>
            <label class="control" for="r">Release<input type="range" data-cc="72" min="0" max="127" value="40" id="r"></label>
          </div>
          <figure class="visual-card" style="margin:0"><span class="eyebrow">Envelope preview</span><svg viewBox="0 0 300 90" role="img" aria-label="Attack, decay, sustain, release preview"><path class="gridline" d="M0 74H300"></path><path id="envelopePath" d=""></path></svg><figcaption>Preview shows the control shape, not measured audio.</figcaption></figure>
        </div>
        <div class="card"><div class="card-head"><div><span class="eyebrow">Tone</span><h2>Filter</h2><p>Remove brightness and emphasize the cutoff.</p></div><span class="card-index">03</span></div><div class="control-grid"><label class="control" for="cut">Cutoff<input type="range" data-cc="74" min="0" max="127" value="70" id="cut"></label><label class="control" for="res">Resonance<input type="range" data-cc="71" min="0" max="127" value="30" id="res"></label></div><label class="switch-row" for="filterOn" style="margin-top:16px"><input type="checkbox" id="filterOn" checked>Filter on</label></div>
        <div class="card"><div class="card-head"><div><span class="eyebrow">Response</span><h2>Voice & motion</h2><p>Control transitions and how notes share voices.</p></div><span class="card-index">04</span></div><div class="control-grid"><label class="control" for="vol">Master volume<input type="range" data-cc="7" min="0" max="127" value="100" id="vol"></label><label class="control" for="glide">Glide<input type="range" data-cc="5" min="0" max="127" value="0" id="glide"></label><label class="select-control" for="stealMode">Voice steal<select id="stealMode"><option value="0">Quietest</option><option value="1">Last</option></select></label><label class="select-control" for="velCurve">Velocity curve<select id="velCurve"><option value="0">Linear</option><option value="1">Soft</option><option value="2">Hard</option><option value="3">Exponential</option></select></label></div></div>
      </div>
    </section>

    <section class="view-panel" id="view-explore" role="tabpanel" aria-labelledby="tab-explore" hidden>
      <div class="view-intro"><div><span class="eyebrow">03 / Motion lab</span><h1>Explore</h1><p>Give the sound movement, rhythm and a little edge.</p></div></div>
      <div class="panel-grid explore-grid">
        <div class="card"><div class="card-head"><div><span class="eyebrow">Clock</span><h2>Tempo & rhythm</h2><p>Choose the source for synced movement.</p></div></div><div class="control-grid control-grid--one"><label class="control" for="bpm">Internal BPM<input id="bpm" type="range" min="40" max="240" value="120"></label><span class="tempo-value" id="bpmLbl">120</span><label class="switch-row" for="tempoSrc"><input type="checkbox" id="tempoSrc">Use external MIDI clock</label><label class="select-control" for="arpMode">Arpeggiator<select id="arpMode"><option value="0">Off</option><option value="1">Up</option><option value="2">Down</option><option value="3">Up–Down</option><option value="4">Random</option></select></label><label class="control" for="arpDiv">Arp division<input id="arpDiv" type="range" min="0" max="7" value="2"></label><label class="control" for="arpGate">Arp gate<input id="arpGate" type="range" min="5" max="95" value="60"></label></div></div>
        <div class="card"><div class="card-head"><div><span class="eyebrow">LFO 01</span><h2>Modulation</h2><p>Animate the filter, wave shape and level.</p></div></div><div class="control-grid control-grid--one"><label class="control" for="lfoAmt">Cutoff amount<input type="range" min="0" max="127" value="40" id="lfoAmt"></label><label class="control" for="lfoRate">Rate<input type="range" min="0" max="127" value="60" id="lfoRate"></label><label class="switch-row" for="lfoSyncBox"><input type="checkbox" id="lfoSyncBox">Sync LFO to tempo</label><label class="control" for="lfoMorph">To morph<input id="lfoMorph" type="range" min="0" max="127" value="0"></label><label class="control" for="lfoAmp">To amplitude<input id="lfoAmp" type="range" min="0" max="127" value="0"></label><label class="control" for="lfoDetune">To detune<input id="lfoDetune" type="range" min="0" max="127" value="0"></label></div></div>
        <div class="card"><div class="card-head"><div><span class="eyebrow">Expression</span><h2>Vibrato & touch</h2><p>Motion that responds to playing.</p></div></div><div class="control-grid control-grid--one"><label class="control" for="l2Rate">Vibrato rate<input id="l2Rate" type="range" min="0" max="127" value="31"></label><label class="control" for="l2Amt">Vibrato amount<input id="l2Amt" type="range" min="0" max="127" value="10"></label><label class="select-control" for="l2Wave">Vibrato wave<select id="l2Wave"><option value="0">Sine</option><option value="1">Triangle</option><option value="2">Square</option></select></label><label class="control" for="velCut">Velocity to cutoff<input id="velCut" type="range" min="0" max="127" value="0"></label><label class="control" for="noiCut">Noise to cutoff<input id="noiCut" type="range" min="0" max="127" value="0"></label><label class="switch-row" for="ped"><input type="checkbox" id="ped">Sustain notes</label></div></div>
        <div class="card" style="grid-column:1/-1"><div class="card-head"><div><span class="eyebrow">Signal color</span><h2>Effects</h2><p>Chorus and insert effects feed the delay stage.</p></div></div><div class="fx-grid">
          <div class="effect"><h3>Delay</h3><label class="control" for="dlyMix">Mix<input id="dlyMix" type="range" data-cc="14" min="0" max="127" value="0"></label><label class="control" for="dlyFb">Feedback<input id="dlyFb" type="range" data-cc="13" min="0" max="127" value="0"></label><label class="control" for="dlyT">Time<input id="dlyT" type="range" data-cc="12" min="0" max="127" value="0"></label><label class="switch-row" for="dlySync"><input type="checkbox" id="dlySync">Sync to 1/16 note</label></div>
          <div class="effect"><h3>Chorus</h3><label class="control" for="chMix">Mix<input id="chMix" type="range" min="0" max="127" value="0"></label><label class="control" for="chDepth">Depth<input id="chDepth" type="range" min="0" max="20" value="5"></label></div>
          <div class="effect"><h3>Bitcrush</h3><label class="control" for="bcMix">Mix<input id="bcMix" type="range" data-cc="29" min="0" max="127" value="0"></label><label class="control" for="bcBits">Bits<input id="bcBits" type="range" data-cc="30" min="0" max="127" value="127"></label><label class="control" for="bcRate">Rate divider<input id="bcRate" type="range" data-cc="31" min="0" max="127" value="0"></label></div>
          <div class="effect"><h3>Tremolo</h3><label class="control" for="tremDepth">Depth<input id="tremDepth" type="range" data-cc="77" min="0" max="127" value="0"></label><label class="control" for="tremRate">Rate<input id="tremRate" type="range" data-cc="78" min="0" max="127" value="42"></label></div>
          <div class="effect effect--wide"><h3>Drive & wavefold</h3><div class="control-grid"><label class="control" for="driveAmt">Drive<input id="driveAmt" type="range" data-cc="79" min="0" max="127" value="0"></label><label class="control" for="foldAmt">Fold<input id="foldAmt" type="range" data-cc="80" min="0" max="127" value="0"></label></div></div>
        </div>
        </div>
        <div class="card" style="grid-column:1/-1"><div class="card-head"><div><span class="eyebrow">Hardware display</span><h2>LED matrix view</h2><p>Choose what the Uno's matrix shows while you play.</p></div></div><div class="viz-picks"><button type="button" class="btn" data-viz="0">Status</button><button type="button" class="btn" data-viz="1">VU meter</button><button type="button" class="btn" data-viz="2">Scope</button></div></div>
      </div>
    </section>
    <section class="view-panel" id="view-all" role="tabpanel" aria-labelledby="tab-all" hidden>
      <div class="view-intro"><div><span class="eyebrow">04 / Complete instrument</span><h1>All controls</h1><p>Every sound setting on one scrollable panel.</p></div></div>
      <div class="all-section"><h2>Sound engine</h2><div class="panel-grid all-shape-grid" id="allShapeCards"></div></div>
      <div class="all-section"><h2>Movement, rhythm & effects</h2><div class="panel-grid all-explore-grid" id="allExploreCards"></div></div>
    </section>
    <section class="view-panel" id="view-help" role="tabpanel" aria-labelledby="tab-help" hidden>
      <div class="view-intro"><div><span class="eyebrow">05 / Onboard reference</span><h1>Help</h1><p>Choose a page below. This guide lives in the Pico panel and needs no PDF or separate download.</p></div></div>
      <div class="help-layout">
        <nav class="help-nav" aria-label="Help pages">
          <span class="help-nav-label">User Guide</span>
          <button type="button" data-help-target="start" aria-current="page">Start here</button>
          <button type="button" data-help-target="play">Play the panel</button>
          <button type="button" data-help-target="sound">Shape a sound</button>
          <button type="button" data-help-target="patches">Patches &amp; presets</button>
          <button type="button" data-help-target="network">MIDI &amp; Logic</button>
          <button type="button" data-help-target="led">LED display</button>
          <button type="button" data-help-target="troubleshoot">Troubleshooting</button>
          <span class="help-nav-label">Technical details</span>
          <button type="button" data-help-target="connections">Connections</button>
          <button type="button" data-help-target="messages">Messages &amp; MIDI</button>
          <button type="button" data-help-target="engine">Sound engine</button>
        </nav>
        <div class="help-content">
          <section class="help-page" id="help-start" data-help-page="start" aria-labelledby="help-start-title">
            <span class="eyebrow">User Guide / 01</span><h2 id="help-start-title" tabindex="-1">Start here</h2><p class="help-page-lead">The Pico hosts this panel and remembers patches. The Uno R4 makes the sound. Both boards are powered through their own USB cables in the photographed development build.</p>
            <div class="card"><h3>Play your first note</h3><ol><li>Power both boards. Open <strong>http://quarkwave.local/</strong> from a device on the same local network as the Pico, or use the Pico IP address if the name does not resolve.</li><li>Wait for <strong>Pico: Connected</strong> and <strong>Uno: Connected to Pico</strong>. <strong>RTP-MIDI: No controller</strong> is normal when playing from this page.</li><li>In Perform, set <strong>Volume</strong> low and play a key above. The default browser Velocity is 100. Select a factory sound and choose <strong>Load patch</strong> to change sounds.</li></ol><p class="help-result"><strong>Expected:</strong> the Uno LED display responds to the note. A clean Uno startup receives the Pico's selected sound without waiting for a browser to open.</p></div>
            <div class="card"><h3>Hear the current build</h3><p>The physical A0 line-output circuit is <strong>not built</strong> on the photographed instrument. If the optional Uno USB-audio firmware is installed, select <strong>QuarkWave USB Audio</strong> as a mono input in Logic Pro, enable Input Monitoring, and start with your listening level low. The owner has confirmed this route in Logic. Without that firmware or a built audio circuit, use the matrix for note checks; the connection labels alone do not prove audible output.</p></div>
          </section>
          <section class="help-page" id="help-play" data-help-page="play" aria-labelledby="help-play-title" hidden>
            <span class="eyebrow">User Guide / 02</span><h2 id="help-play-title" tabindex="-1">Play the panel</h2><p class="help-page-lead">The patch strip and keyboard above remain mounted in every view. Switching views does not reset the sound or interrupt held notes.</p>
            <div class="card"><h3>Keyboard and expression</h3><ol><li>Play the on-screen keys or computer keys <strong>A W S E D F T G Y H U J</strong>. Use − and + to shift the browser keyboard octave.</li><li>Set <strong>Velocity</strong> from 1 to 127 before a new note. Notes already held keep their original velocity; the setting resets to 100 on page reload and is not part of a patch.</li><li>In Perform, try <strong>Volume</strong>, <strong>Filter cutoff</strong>, <strong>Osc morph</strong>, <strong>Arpeggiator</strong>, and <strong>Sustain notes</strong>.</li></ol><p class="help-result"><strong>Expected:</strong> Perform's quick controls adjust the same live sound settings as Shape and Explore.</p></div>
            <div class="card"><h3>Choose a view</h3><p><strong>Perform</strong> keeps a large keyboard and quick controls in reach. <strong>Shape</strong> follows oscillator, envelope, filter, and voice response. <strong>Explore</strong> contains tempo, modulation, effects, and matrix views. <strong>All controls</strong> puts the same Shape and Explore controls on one scrollable panel. <strong>Help</strong> is this reference. The selected view is remembered in this browser, not saved with a patch.</p><p>If a note hangs, choose <strong>Panic</strong> in the patch strip. It sends All Sound Off and clears browser-held notes.</p></div>
          </section>
          <section class="help-page" id="help-sound" data-help-page="sound" aria-labelledby="help-sound-title" hidden>
            <span class="eyebrow">User Guide / 03</span><h2 id="help-sound-title" tabindex="-1">Shape a sound</h2><p class="help-page-lead">Load a factory sound first, then change one section at a time. Descriptions here follow the source; audible differences are for you to test.</p>
            <div class="card"><h3>Source, contour, and tone</h3><ol><li>In Shape, move <strong>Morph</strong> while holding a note. It travels through sine, triangle, saw, and square character. <strong>Unison</strong> layers one to three oscillators per note; <strong>Detune</strong> spreads them.</li><li>Change <strong>Attack</strong> for a slower start, <strong>Decay</strong> and <strong>Sustain</strong> for the held level, and <strong>Release</strong> for the tail. The drawing previews settings, not measured audio.</li><li>Enable <strong>Filter on</strong>, lower <strong>Cutoff</strong> for a darker sound, and raise <strong>Resonance</strong> gradually. Try <strong>Glide</strong> and <strong>Noise</strong> last.</li></ol></div>
            <div class="card"><h3>Movement, rhythm, and effects</h3><ol><li>In Explore, raise LFO 01 <strong>Cutoff amount</strong> and adjust <strong>Rate</strong>. Its other routes move morph, amplitude, or detune. Vibrato is a separate LFO.</li><li>For repeated notes, select an <strong>Arpeggiator</strong> mode, then set division and gate. Internal BPM works without a clock source. For external timing, enable <strong>Use external MIDI clock</strong> and supply MIDI Clock.</li><li>Add effects gradually. Signal order is <strong>chorus → drive/fold → bitcrush → tremolo → delay</strong>. Delay Sync uses a sixteenth note of the active tempo.</li></ol><p class="help-result"><strong>Try:</strong> load Pluck, lengthen Release, then add a little Chorus Mix and Delay Mix. Save As to keep the variation.</p></div>
          </section>
          <section class="help-page" id="help-patches" data-help-page="patches" aria-labelledby="help-patches-title" hidden>
            <span class="eyebrow">User Guide / 04</span><h2 id="help-patches-title" tabindex="-1">Patches &amp; presets</h2><p class="help-page-lead">Eight factory presets are read-only. Eight user slots on the Pico are empty on a new unit until saved; an existing file is occupied even if its name looks like P0.</p>
            <div class="card"><h3>Factory sounds</h3><div class="help-table-wrap"><table><thead><tr><th>Sound</th><th>Starting character</th></tr></thead><tbody><tr><td>Warm Pad</td><td>Wide, slow chords</td></tr><tr><td>Pluck</td><td>Short notes and velocity response</td></tr><tr><td>Solid Bass</td><td>Single-note bass and glide</td></tr><tr><td>PWM Lead</td><td>Morph movement and lead lines</td></tr><tr><td>EP Keys</td><td>Velocity-sensitive keys</td></tr><tr><td>Sweep Pad</td><td>Slow filter movement</td></tr><tr><td>Noise Perc</td><td>Noisy short notes</td></tr><tr><td>Chrs Strngs</td><td>Layered strings and chorus</td></tr></tbody></table></div><p class="help-note">These are suggested uses from preset settings, not verified acoustic descriptions.</p></div>
            <div class="card"><h3>Keep or recover a sound</h3><ol><li>Select a patch and choose <strong>Load patch</strong>. Edits to the current sound are lost when another patch is loaded unless saved first.</li><li>For an edited factory sound, enter a name, choose <strong>Save As</strong>, select a user slot, and choose <strong>Save to slot</strong>. Confirm before replacing an occupied slot. An ordinary <strong>Save patch</strong> updates the selected user slot.</li><li>Choose <strong>Randomize</strong> in the patch strip to explore a new live sound. It can use the full valid range, including effects; it does not change the selected slot, sustain pedal, or browser Velocity.</li><li><strong>More actions → Commit snapshot</strong> keeps a separate Pico snapshot. <strong>Load committed sound</strong> restores it. Check the action status for success.</li></ol></div>
            <div class="card"><h3>Standalone sound and Pico sync</h3><p>If the Uno reports <strong>Connected to Pico · standalone sound</strong>, an external MIDI input has controlled it. The Pico leaves that sound alone. To replace it, stop playing, choose <strong>More actions → Sync Pico patch to Uno</strong>, read the warning, and confirm. Sync resets the sound and held notes; wait for <strong>Uno: Connected to Pico</strong>.</p></div>
          </section>
          <section class="help-page" id="help-network" data-help-page="network" aria-labelledby="help-network-title" hidden>
            <span class="eyebrow">User Guide / 05</span><h2 id="help-network-title" tabindex="-1">MIDI &amp; Logic</h2><p class="help-page-lead">RTP-MIDI is hosted by the Pico. The Uno's Wi-Fi and BLE-MIDI are disabled in this build; the proposed physical DIN input is not yet built.</p>
            <div class="card"><h3>Connect a wireless controller</h3><ol><li>Power both boards and check <strong>Uno: Connected to Pico</strong>. Put the computer and Pico on the same reachable network.</li><li>On a Mac, open <strong>Audio MIDI Setup → Window → Show MIDI Studio → Configure Network Driver</strong>. Add and enable an <strong>RTP</strong> session. In Directory, connect to <strong>QuarkWave</strong>, or manually enter the Pico IP and port <strong>5004</strong>.</li><li>On Windows, use an RTP-MIDI driver such as rtpMIDI. Add and enable a session, connect to <strong>QuarkWave</strong> in Directory, then choose your local session as the music app's MIDI output. Windows setup has not been accepted end to end on this build.</li></ol><p class="help-result"><strong>Expected:</strong> the browser reports <strong>RTP-MIDI: Controller connected</strong>. This Pico build admits one RTP controller session at a time.</p></div>
            <div class="card"><h3>Logic Pro: MIDI and sound are separate</h3><p>To hear the current experimental audio route, connect Uno USB and select <strong>QuarkWave USB Audio</strong> as Logic's mono input, then enable Input Monitoring. To sequence notes, connect Logic's External MIDI track to the Mac RTP session; the Pico forwards them to the Uno. Leave ordinary Program Change sending off for a first test: QuarkWave uses selected numbers for sound commands, not factory-preset selection. Use a real-time audio recording or bounce for the Uno's output. The owner confirmed Logic USB audio; Logic-to-Pico RTP sequencing still needs an end-to-end check.</p></div>
            <div class="card"><h3>Clock and disconnects</h3><p>Explore can choose external MIDI clock. Clock pulses alone do not change that setting. The Uno accepts measured 40–240 BPM, prefers valid recent DIN clock over RTP, and holds the last valid tempo if pulses stop. Start resets the arp pattern, Continue resumes, and Stop releases its current arp note. On a recognized RTP disconnect, the Pico releases that controller's notes and pedal without releasing notes held by another input; a silent network loss waits for session timeout.</p></div>
          </section>
          <section class="help-page" id="help-led" data-help-page="led" aria-labelledby="help-led-title" hidden>
            <span class="eyebrow">User Guide / 06</span><h2 id="help-led-title" tabindex="-1">Uno LED display</h2><p class="help-page-lead">The Uno's 12 × 8 red matrix shows activity while playing. Choose Status, VU meter, or Scope in Explore or All controls; the scrolling “Viz:” label temporarily replaces the view.</p>
            <div class="card"><h3>Three views</h3><p><strong>Status</strong> draws up to four voice-envelope bars. <strong>VU meter</strong> draws eleven recent mono output levels over time, not frequency bands or stereo channels. <strong>Scope</strong> draws an automatically scaled trace of synthesized output. These are firmware views before the DAC, not calibrated readings at an audio jack.</p></div>
            <div class="card"><h3>Top row, left to right</h3><div class="help-table-wrap"><table><thead><tr><th>Column</th><th>Indicator</th><th>Column</th><th>Indicator</th></tr></thead><tbody><tr><td>1</td><td>Pico sound loaded</td><td>7</td><td>Pico MIDI received · brief</td></tr><tr><td>2</td><td>External MIDI used</td><td>8</td><td>Voice stolen · brief</td></tr><tr><td>3</td><td>Valid external clock active</td><td>9</td><td>External sound activity · brief</td></tr><tr><td>4</td><td>Sustain held</td><td>10</td><td>Note started · brief</td></tr><tr><td>5</td><td>Arpeggiator enabled</td><td>11</td><td>Audio timing slip · brief</td></tr><tr><td>6</td><td>Recent Pico handshake</td><td>12</td><td>Heartbeat</td></tr></tbody></table></div><p>Count columns from left to right as viewed on the board. The bottom-right pixel can mark a slow LED update. A scrolling label hides the top row until it finishes. The current photo predates these new status meanings; each new pixel still needs individual hardware acceptance.</p></div>
          </section>
          <section class="help-page" id="help-troubleshoot" data-help-page="troubleshoot" aria-labelledby="help-troubleshoot-title" hidden>
            <span class="eyebrow">User Guide / 07</span><h2 id="help-troubleshoot-title" tabindex="-1">Troubleshooting</h2><p class="help-page-lead">Check the three connection labels separately: browser ↔ Pico, Pico ↔ Uno, and RTP controller ↔ Pico.</p>
            <div class="card"><div class="help-table-wrap"><table><thead><tr><th>What you see</th><th>First check</th></tr></thead><tbody><tr><td>Page will not open</td><td>Check Pico power and Wi-Fi. Try its IP address if quarkwave.local does not resolve.</td></tr><tr><td>Pico: Not connected</td><td>Reload the page and check Pico Wi-Fi. The browser's WebSocket uses port 8081.</td></tr><tr><td>Uno: Not connected to Pico</td><td>Check Uno power, both UART signal wires, common ground, and the Uno-TX level shifter.</td></tr><tr><td>Connected, but silent</td><td>Check the listening route. The photographed build has no physical A0 jack; the optional USB-audio firmware offers the tested Logic input.</td></tr><tr><td>Standalone sound</td><td>Keep playing it, or use explicit Sync Pico patch to Uno after reading its warning.</td></tr><tr><td>Sound sync failed</td><td>Check the return MIDI wire, then try explicit sync again.</td></tr><tr><td>Note stays on</td><td>Choose Panic. For a silent wireless loss, the RTP session may take time to expire.</td></tr><tr><td>Save As failed</td><td>Read the action status. An existing unreadable file counts as occupied; do not assume it is empty.</td></tr></tbody></table></div><p class="help-note">The physical audio circuit, Windows walkthrough, external-clock timing, and DIN input have not passed full hardware acceptance.</p></div>
          </section>
          <section class="help-page" id="help-connections" data-help-page="connections" aria-labelledby="help-connections-title" hidden>
            <span class="eyebrow">Technical details / 01</span><h2 id="help-connections-title" tabindex="-1">Connections</h2><p class="help-page-lead">Current wiring and proposed additions are different. The photographed build has two USB-powered boards and a bidirectional level-shifted MIDI UART.</p>
            <div class="card"><h3>Current Pico–Uno wiring</h3><div class="help-flow" role="img" aria-label="Pico GPIO zero transmits MIDI to Uno D0. Uno D1 transmits MIDI through a five-volt to three-point-three-volt level shifter to Pico GPIO one. Grounds are common; power comes from separate USB ports."><span>Pico GPIO 0 TX</span><b>→</b><span>Uno D0 RX</span><b>·</b><span>Uno D1 TX</span><b>→</b><span>5 V → 3.3 V shifter</span><b>→</b><span>Pico GPIO 1 RX</span></div><p>Both UART directions run at <strong>31,250 baud</strong>; common ground is required. The owner confirms the Uno return-wire level shift. Exact shifter channel endpoints and voltage at Pico RX have not been measured. The two 5 V USB rails are not joined.</p></div>
            <div class="card"><h3>Audio and DIN plans</h3><p>The Uno writes 12-bit samples to <strong>A0</strong>. Its physical line jack is unbuilt. The proposed mono line stage uses an MCP6002 buffer, AC coupling, and an output divider before a powered speaker or mixer <strong>line input</strong>. The owner's TDA1308 board is a separate proposed headphone amplifier fed after that buffered node, with two separate channels for both ears. Neither circuit is an as-built audio path. The Uno firmware also listens on D2 through SoftwareSerial for a future opto-isolated DIN MIDI IN; a direct DIN-jack-to-pin wire is not the proposed circuit.</p><p class="help-result"><strong>Before building:</strong> use the detailed repository connection schematic and verify the actual part pinouts, continuity, power, and output DC. The onboard Help is an orientation guide, not an as-built wiring netlist.</p></div>
          </section>
          <section class="help-page" id="help-messages" data-help-page="messages" aria-labelledby="help-messages-title" hidden>
            <span class="eyebrow">Technical details / 02</span><h2 id="help-messages-title" tabindex="-1">Messages &amp; MIDI</h2><p class="help-page-lead">The browser sends JSON to the Pico WebSocket on port 8081. The Pico converts sound controls to MIDI on the wired Uno link.</p>
            <div class="card"><h3>Browser → Pico examples</h3><pre><code>{"type":"noteOn","note":60,"velocity":100}
{"type":"noteOff","note":60}
{"type":"control","param":"cutoff","value":1400}
{"type":"randomize"}</code></pre><p>Patch actions use <code>loadPatch</code> with an <code>index</code>, <code>savePatch</code> with user <code>index</code>, <code>name</code>, and <code>overwrite</code>, plus <code>commitPatch</code>, <code>loadCommitPatch</code>, and <code>syncUno</code>. The Pico returns <code>patchData</code>, <code>patchList</code>, <code>patchSaveResult</code>, and <code>unoStatus</code>. Check <code>patchSaveResult.success</code> before treating a save as complete. Factory indexes 100–107 cannot be saved.</p></div>
            <div class="card"><h3>Uno MIDI control groups</h3><p>MIDI CC values are 0–127. Main sound CCs: <strong>7</strong> gain, <strong>5</strong> glide, <strong>71/74/91</strong> filter, <strong>72/73/75/23</strong> ADSR, <strong>76/94/95/93</strong> oscillator and noise, <strong>1/2/3/26</strong> LFO1, <strong>27/28</strong> vibrato, <strong>12/13/14</strong> delay, <strong>21</strong> chorus mix, <strong>29/30/31</strong> bitcrush, <strong>77/78/79/80</strong> tremolo/drive/fold, and <strong>18/19/20</strong> arpeggiator. CC64 is sustain; CC120 is All Sound Off. The Pico stores controls at effective MIDI steps, so a recalled value can be rounded.</p></div>
            <div class="card"><h3>Program Change</h3><div class="help-table-wrap"><table><thead><tr><th>Message</th><th>Action</th><th>Accepted input</th></tr></thead><tbody><tr><td>PC 0</td><td>Reset synth controls</td><td>Pico only</td></tr><tr><td>PC 10–12</td><td>Status, VU, Scope</td><td>Pico only</td></tr><tr><td>PC 100/101</td><td>External/internal tempo</td><td>Pico, DIN, RTP</td></tr><tr><td>PC 102/103</td><td>Delay sync on/off</td><td>Pico, DIN, RTP</td></tr><tr><td>PC 110–112</td><td>Vibrato sine/triangle/square</td><td>Pico, DIN, RTP</td></tr></tbody></table></div><p>Generic Program Changes do not select the Pico's factory patches. Some controller menus show program numbers one higher than the transmitted MIDI byte; verify the byte if a command seems wrong.</p></div>
            <div class="card"><h3>QuarkWave SysEx bytes</h3><p>Every message starts <code>F0 7D 00</code> and ends <code>F7</code>. <code>7D</code> is the non-commercial MIDI identifier used by this project. The command and payload between those boundaries are:</p><div class="help-table-wrap"><table><thead><tr><th>Command + payload</th><th>Meaning</th><th>Direction / input</th></tr></thead><tbody><tr><td><code>01 &lt;BPM low&gt; &lt;BPM high&gt;</code></td><td>Internal BPM 40–240, little-endian seven-bit bytes</td><td>Pico, DIN, RTP → Uno</td></tr><tr><td><code>02 &lt;route&gt; &lt;amount&gt;</code></td><td>LFO1 route 0=morph or 1=amplitude; amount 0–127</td><td>Pico, DIN, RTP → Uno</td></tr><tr><td><code>03 &lt;depth&gt;</code></td><td>Chorus depth 0–20</td><td>Pico, DIN, RTP → Uno</td></tr><tr><td><code>10</code> / <code>11 &lt;flags&gt;</code></td><td>Readiness request / status reply</td><td>Pico → Uno / Uno → Pico</td></tr><tr><td><code>12</code> / <code>13 &lt;flags&gt;</code></td><td>Patch completion / acknowledgment</td><td>Pico → Uno / Uno → Pico</td></tr><tr><td><code>14</code></td><td>Forwarded RTP sound-activity marker</td><td>Pico → Uno only</td></tr><tr><td><code>15 &lt;action&gt; &lt;note&gt; &lt;velocity&gt;</code></td><td>Forwarded RTP Note On/Off; action 0/1</td><td>Pico → Uno only</td></tr><tr><td><code>16 &lt;on&gt;</code></td><td>Forwarded RTP sustain; 1/0</td><td>Pico → Uno only</td></tr></tbody></table></div><p>In status flags, bit 0 marks standalone sound activity and bit 1 marks a completed Pico patch. Clock alone does not set bit 0. Pico-only commands are not accepted from external RTP peers.</p></div>
          </section>
          <section class="help-page" id="help-engine" data-help-page="engine" aria-labelledby="help-engine-title" hidden>
            <span class="eyebrow">Technical details / 03</span><h2 id="help-engine-title" tabindex="-1">Sound engine</h2><p class="help-page-lead">The Uno owns live synthesis state. The Pico owns patch files, browser control, and the RTP-MIDI gateway.</p>
            <div class="card"><h3>Audio path</h3><div class="help-flow" role="img" aria-label="MIDI notes drive four voices; voices pass through a global filter, chorus, drive and fold, bitcrush, tremolo, delay, master gain, and the A0 DAC."><span>MIDI notes</span><b>→</b><span>4 voices</span><b>→</b><span>Filter</span><b>→</b><span>Chorus</span><b>→</b><span>Drive / fold</span><b>→</b><span>Bitcrush</span><b>→</b><span>Tremolo</span><b>→</b><span>Delay</span><b>→</b><span>A0 DAC</span></div><p>Each voice can use up to three unison oscillators. The current build uses a global two-pole state-variable low-pass filter. LFOs, velocity, and the arpeggiator modulate the voice and filter stages. The 22,050 samples/s rate is a scheduling target; a demanding four-voice/all-effects diagnostic met that <strong>average write count</strong> with Uno Wi-Fi off, but some writes were late. Evenly spaced DAC timing and physical output quality remain unmeasured.</p></div>
            <div class="card"><h3>Patch lifecycle and link state</h3><p>The Pico stores eight user files in LittleFS; eight factory sounds are compiled into its firmware. It remembers the last explicitly loaded factory or user patch. On a clean Uno startup, the Pico probes readiness over the return MIDI wire, resets and transfers that selected patch, then waits for completion acknowledgment. Browser opening does not trigger the sync. If the Uno reports standalone sound activity, the Pico preserves it until an explicit browser sync or patch load. Clock alone does not mark standalone activity. A recognized RTP disconnect releases only that session's owned notes and sustain state.</p></div>
            <div class="card"><h3>Build limits</h3><p>Uno Wi-Fi, Uno RTP-MIDI, and BLE-MIDI are disabled in this source build; Pico RTP-MIDI is enabled for one peer. The separate physical DIN interface, A0 line output, and headphone amplifier are proposed hardware. The optional USB-audio experiment has worked on the owner's Mac in Logic Pro, but it is a distinct Uno build and not a promise for every host or workload.</p></div>
          </section>
        </div>
      </div>
    </section>
  </main>


  <footer class="footer"><span>QuarkWave · Pico controller + Uno R4 sound engine</span><span class="footer-credit">Powered by <strong>Arduino Uno R4 WiFi</strong><br>Made with ♥ by Polyatomic</span></footer>
</div>
<script>
let ws = null; 
let currentPatchData = {};
let pendingSave = false;
let pendingRandomize = false;
let userPatchSlots = null;
let pendingUnoSync = false;
let unoLink = { connected:false, mode:'offline', syncing:false, syncFailed:false };

function announce(message) {
  document.getElementById('actionStatus').textContent = message;
}

function send(obj){
  if (!ws || ws.readyState !== 1) return false;
  ws.send(JSON.stringify(obj));
  return true;
}

let _reconnectDelay = 800;

function initWS(){
  const wsProto = location.protocol === 'https:' ? 'wss' : 'ws';
  const sock = new WebSocket(`${wsProto}://${location.hostname}:8081`);

  sock.onopen = () => {
    _reconnectDelay = 800;
    setKbEnabled();
    // Request patch list on connect
    send({ type: 'getPatchList' });
  };

  sock.onclose = () => {
    // The Pico tracks notes per WebSocket client and releases them server-side.
    downKeys.clear();
    pendingSave = false;
    pendingRandomize = false;
    userPatchSlots = null;
    updateSaveButtonState();
    pendingUnoSync = false;
    unoLink = { connected:false, mode:'offline', syncing:false, syncFailed:false };
    setKbEnabled();
    setUnoStatus();
    setTimeout(() => { ws = initWS(); }, _reconnectDelay);

    // exponential backoff + small random jitter (50–250ms)
    const jitter = 50 + Math.floor(Math.random() * 200);
    _reconnectDelay = Math.min(Math.round(_reconnectDelay * 1.7), 8000) + jitter;
  };

  sock.onerror = () => { console.debug('WS Error'); };

  sock.onmessage = (ev) => {
    try {
      const msg = JSON.parse(ev.data);
      if (!msg || !msg.type) return;

      // Handle different message types from Pico 2W
      switch (msg.type) {
        case 'unoStatus':
          unoLink = msg;
          setUnoStatus();
          if (pendingUnoSync && msg.mode === 'pico') {
            announce('Selected sound loaded on Uno.');
            pendingUnoSync = false;
          } else if (pendingUnoSync && msg.syncFailed) {
            announce('Uno did not confirm the patch sync. Try again.');
            pendingUnoSync = false;
          } else if (pendingUnoSync && !msg.connected) {
            announce('Uno connection lost before patch sync completed.');
            pendingUnoSync = false;
          }
          break;
        case 'patchList':
          // Handle the new structure with userPatches and factoryPatches
          updatePatchList({
            userPatches: msg.userPatches, 
            factoryPatches: msg.factoryPatches
          });
          break;
        case 'patchSaveResult':
          pendingSave = false;
          updateSaveButtonState();
          announce(msg.success ? `Saved to user slot ${msg.index}.` : (msg.error || 'Patch save failed.'));
          break;

        case 'patchData':
          updateUIFromPatchData(msg.patch);
          if (pendingRandomize) {
            pendingRandomize = false;
            announce('Randomized the current sound. Save a patch to keep it.');
          }
          break;
          
        case 'currentPatch':
          setCurrentPatch(msg.index);
          break;
          
        case 'pname':
          const i = (+msg.i) | 0;
          const name = String(msg.name || ('P' + i));
          const sel = document.getElementById('patch');
          if (sel && ((+sel.value | 0) === i)) {
            const pn = document.getElementById('pname');
            if (pn) pn.value = name;
          }
          break;
        case 'commitSaved':
          announce('Committed sound saved.');
          break;
        case 'commitLoaded':
          announce('Committed sound loaded.');
          break;
      }
    } catch(e) {
      console.error('[sock.onmessage] Error parsing WebSocket message:', e);
    }
  };

  return sock;
}

const patchSelector = document.getElementById('patch');
if (patchSelector) {
  patchSelector.addEventListener('change', updateSaveButtonState);
}

function updateSaveButtonState() {
  const selectedIndex = +document.getElementById('patch').value;
  const saveBtn = document.getElementById('patchSave');
  const isFactory = selectedIndex >= 100;
  saveBtn.textContent = isFactory ? 'Save As' : 'Save patch';
  saveBtn.title = isFactory ? 'Save this sound to a user slot' : '';
  saveBtn.disabled = pendingSave || (isFactory && !userPatchSlots);
}

function sendPatchSave(index, name, overwrite) {
  pendingSave = send({ type:'savePatch', index, name, overwrite });
  announce(pendingSave ? 'Saving patch…' : 'Connect to QuarkWave before saving.');
  updateSaveButtonState();
}

function openSaveAsDialog() {
  if (!userPatchSlots) {
    announce('Waiting for the Pico patch list.');
    return;
  }
  const name = document.getElementById('pname').value.trim();
  if (!name) {
    announce('Enter a patch name before saving.');
    document.getElementById('pname').focus();
    return;
  }
  const slotSelect = document.getElementById('saveAsSlot');
  slotSelect.innerHTML = '';
  userPatchSlots.forEach(slot => slotSelect.add(new Option(
    `${slot.index}: ${slot.exists ? slot.name + ' (occupied)' : 'Empty'}`, String(slot.index))));
  document.getElementById('saveAsDialog').showModal();
  slotSelect.focus();
}

function saveSelectedPatch() {
  const selectedIndex = +document.getElementById('patch').value;
  if (selectedIndex >= 100) {
    openSaveAsDialog();
    return;
  }
  if (!Number.isInteger(selectedIndex) || selectedIndex < 0 || selectedIndex > 7) return;
  const name = document.getElementById('pname').value.trim() || `P${selectedIndex}`;
  // Ordinary Save is an explicit update of the selected user slot.
  sendPatchSave(selectedIndex, name, true);
}

function saveAsChosenSlot() {
  const slot = +document.getElementById('saveAsSlot').value;
  const target = userPatchSlots.find(item => item.index === slot);
  if (!target) return;
  if (target.exists && !confirm(`Replace user slot ${slot} (${target.name})? This cannot be undone.`)) return;
  const name = document.getElementById('pname').value.trim();
  if (!name) return;
  document.getElementById('saveAsDialog').close();
  sendPatchSave(slot, name, target.exists);
}

function updatePatchList(data) {
  const sel = document.getElementById('patch');
  if (!sel) return;
  const previousSelection = sel.value;
  
  // Clear existing options
  sel.innerHTML = '';
  
  // Occupancy comes from the Pico filesystem, never from a patch name.
  if (data.userPatches) {
    userPatchSlots = data.userPatches;
    data.userPatches.forEach((patch) => {
      const label = patch.exists ? patch.name : `Empty ${patch.index}`;
      sel.add(new Option(`${patch.index}: ${label}`, String(patch.index)));
    });
  }
  
  // Add factory presets (100+) with visual indication
  if (data.factoryPatches) {
    data.factoryPatches.forEach((preset) => {
      const option = new Option(
        `\u{1F3ED} ${preset.name}`, // Factory icon prefix
        String(preset.index)
      );
      sel.add(option);
    });
  }

  if (Array.from(sel.options).some(option => option.value === previousSelection))
    sel.value = previousSelection;
  
  // Update save button state when selection changes
  updateSaveButtonState();
}

function setCurrentPatch(index) {
  const sel = document.getElementById('patch');
  if (sel) {
    sel.value = String(index);
    updateSaveButtonState();
  }
}

function updateUIFromPatchData(patch) {
  currentPatchData = patch;
  
  // Update all controls with patch data
  const setVal = (id, val) => {
    const el = document.getElementById(id);
    if (!el || val == null) return;
    if (el.type === 'checkbox') el.checked = !!val;
    else el.value = String(val);
  };

  // Map patch data to UI controls
  if (patch.a != null) setVal('a', Math.round((patch.a - 0.002) / 0.498 * 127));
  if (patch.d != null) setVal('d', Math.round((patch.d - 0.01) / 0.99 * 127));
  if (patch.s != null) setVal('sust', Math.round((patch.s - 0.10) / 0.75 * 127));
  if (patch.r != null) setVal('r', Math.round((patch.r - 0.02) / 1.48 * 127));
  
  if (patch.cutoff != null) setVal('cut', Math.round((patch.cutoff - 40) / 9960 * 127));
  if (patch.resonance != null) setVal('res', Math.round((patch.resonance - 0.5) / 2.5 * 127));
  if (patch.morph != null) setVal('morph', Math.round(patch.morph * 127));
  if (patch.filterOn != null) setVal('filterOn', patch.filterOn);
  
  if (patch.glide != null) setVal('glide', Math.round(patch.glide / 0.3 * 127));
  if (patch.detune != null) setVal('detune', Math.round((patch.detune - 2) / 18 * 127));
  
  if (patch.masterGain != null) setVal('vol', Math.round((patch.masterGain - 0.2) / 0.8 * 127));
  
  if (patch.lfoAmtHz != null) setVal('lfoAmt', Math.round(patch.lfoAmtHz / 3000 * 127));
  if (patch.lfoRateHz != null) setVal('lfoRate', Math.round((patch.lfoRateHz - 0.1) / 12 * 127));
  
  if (patch.lfoSync != null) setVal('lfoSyncBox', patch.lfoSync);
  
  // Unison mapping: 1->0, 2->64, 3->127
  if (patch.unison != null) {
    const unisonVal = patch.unison === 1 ? 0 : patch.unison === 2 ? 64 : 127;
    setVal('unisonSel', unisonVal);
  }
  
  if (patch.dlyTime != null) setVal('dlyT', Math.round((patch.dlyTime - 0.02) / 0.146 * 127));
  if (patch.dlyFb != null) setVal('dlyFb', Math.round((patch.dlyFb - 0.01) / 0.88 * 127));
  if (patch.dlyMix != null) setVal('dlyMix', Math.round(patch.dlyMix * 127));
  if (patch.noiseAmt != null) setVal('noise', Math.round(patch.noiseAmt * 127));

  if (patch.lfoToMorph != null) setVal('lfoMorph', Math.round(patch.lfoToMorph * 127));
  if (patch.lfoToAmp != null) setVal('lfoAmp', Math.round(patch.lfoToAmp * 127));
  if (patch.lfoToDetune != null) setVal('lfoDetune', Math.round(patch.lfoToDetune * 127));
  if (patch.velToCutoff != null) setVal('velCut', Math.round(patch.velToCutoff * 127));
  if (patch.noiseToCutoff != null) setVal('noiCut', Math.round(patch.noiseToCutoff * 127));
  
  if (patch.chMix != null) setVal('chMix', Math.round(patch.chMix * 127));
  if (patch.chDepth != null) setVal('chDepth', Math.round(patch.chDepth));

  if (patch.bitcrushMix != null) setVal('bcMix', Math.round(patch.bitcrushMix * 127));
  if (patch.bitcrushBits != null) setVal('bcBits', Math.round((patch.bitcrushBits - 4) / 12 * 127));
  if (patch.bitcrushRateDiv != null) setVal('bcRate', Math.round((patch.bitcrushRateDiv - 1) / 15 * 127));
  if (patch.tremDepth != null) setVal('tremDepth', Math.round(patch.tremDepth * 127));
  if (patch.tremRateHz != null) setVal('tremRate', Math.round((patch.tremRateHz - 0.1) / 12 * 127));
  if (patch.driveAmount != null) setVal('driveAmt', Math.round(patch.driveAmount * 127));
  if (patch.foldAmount != null) setVal('foldAmt', Math.round(patch.foldAmount * 127));
  
  if (patch.arpMode != null) setVal('arpMode', patch.arpMode);
  if (patch.arpDiv != null) setVal('arpDiv', patch.arpDiv);
  if (patch.arpGate != null) setVal('arpGate', patch.arpGate);
  
  if (patch.lfo2RateHz != null) setVal('l2Rate', Math.round((patch.lfo2RateHz - 0.1) / 20 * 127));
  if (patch.lfo2AmtSemi != null) setVal('l2Amt', Math.round(patch.lfo2AmtSemi / 2 * 127));
  if (patch.lfo2Wave != null) setVal('l2Wave', patch.lfo2Wave);
  
  if (patch.stealMode != null) setVal('stealMode', patch.stealMode);
  if (patch.velCurve != null) setVal('velCurve', patch.velCurve);
  
  if (patch.dlySync != null) setVal('dlySync', patch.dlySync);
  
  if (patch.tempoSrc != null) setVal('tempoSrc', patch.tempoSrc > 0);
  
  if (patch.bpmInt != null) {
    setVal('bpm', Math.round(patch.bpmInt));
    const bpmLbl = document.getElementById('bpmLbl');
    if (bpmLbl) bpmLbl.textContent = Math.round(patch.bpmInt);
  }
  
  // Set patch name
  const pn = document.getElementById('pname');
  if (pn && patch.name) pn.value = patch.name;
  refreshPanel();
}

const s  = document.getElementById('s');
const kb = document.getElementById('kb');

let baseOct = 3;
const downKeys = new Set();
const octLbl = document.getElementById('octLbl');
document.getElementById('octD').onclick = ()=>{ baseOct = Math.max(0, baseOct-1); octLbl.textContent = baseOct; buildKeyboard(); };
document.getElementById('octU').onclick = ()=>{ baseOct = Math.min(8, baseOct+1); octLbl.textContent = baseOct; buildKeyboard(); };

function enqueueCC(cc, v){ 
  // Convert CC to control parameter for Pico 2W
  const param = ccToParam(cc);
  if (param) {
    send({ type: 'control', param: param, value: ccToValue(cc, v) });
  }
}

function ccToParam(cc) {
  const ccMap = {
    73: 'attack', 75: 'decay', 23: 'sustain', 72: 'release',
    74: 'cutoff', 71: 'resonance', 76: 'morph', 91: 'filterOn',
    5: 'glide', 94: 'detune', 7: 'masterGain', 93: 'noiseAmt',
    12: 'delayTime', 13: 'delayFeedback', 14: 'delayMix',
    1: 'lfoAmtHz', 2: 'lfoRateHz', 3: 'lfoSync',
    24: 'velToCutoff', 25: 'noiseToCutoff', 26: 'lfoToDetune',
    27: 'lfo2RateHz', 29: 'bitcrushMix', 30: 'bitcrushBits', 31: 'bitcrushRateDiv',
    77: 'tremDepth', 78: 'tremRateHz', 79: 'driveAmount', 80: 'foldAmount',
    95: 'unison', 64: 'sustainPedal'
  };
  return ccMap[cc];
}

function ccToValue(cc, v) {
  // Convert 0-127 CC values to actual parameter ranges
  const t = v / 127;
  switch(cc) {
    case 73: return 0.002 + 0.498 * t; // attack
    case 75: return 0.01 + 0.99 * t;   // decay  
    case 23: return 0.10 + 0.75 * t;   // sustain
    case 72: return 0.02 + 1.48 * t;   // release
    case 74: return 40 + 9960 * t;     // cutoff
    case 71: return 0.5 + 2.5 * t;     // resonance
    case 76: return t;                 // morph
    case 5:  return 0.3 * t;           // glide
    case 94: return 2 + 18 * t;        // detune
    case 7:  return 0.2 + 0.8 * t;     // gain
    case 93: return t;                 // noise
    case 12: return 0.02 + 0.146 * t;  // delay time
    case 13: return 0.01 + 0.88 * t;   // delay feedback
    case 14: return t;                 // delay mix
    case 1:  return 3000 * t;          // lfo amount
    case 2:  return 0.1 + 12 * t;      // lfo rate
    case 91: return v >= 64 ? 1 : 0;   // filter on
    case 3:  return v >= 64 ? 1 : 0;   // lfo sync
    case 95: return v < 32 ? 1 : v < 96 ? 2 : 3; // unison
    case 64: return v >= 64 ? 1 : 0;   // sustain pedal
    case 18: return Math.round(t * 4);  // arp mode
    case 19: return Math.round(t * 7);  // arp division
    case 20: return 5 + 90 * t;         // arp gate
    case 27: return 0.1 + 20 * t;       // lfo2 rate
    case 28: return 2 * t;              // lfo2 amount
    case 29: return t;                  // bitcrush mix
    case 30: return 4 + 12 * t;         // bit depth
    case 31: return 1 + 15 * t;         // sample-hold divider
    case 77: return t;                  // tremolo depth
    case 78: return 0.1 + 12 * t;       // tremolo rate
    case 79: return t;                  // drive amount
    case 80: return t;                  // fold amount
    default: return t;
  }
}

function wireControls(){
  // Generic CC sliders
  document.querySelectorAll('input[type="range"][data-cc]').forEach(el => {
    const cc = (+el.dataset.cc) | 0;
    el.addEventListener('input', e => enqueueCC(cc, (+e.target.value) | 0), { passive: true });
  });

  // Toggles and special controls
  const filterOn = document.getElementById('filterOn');
  if (filterOn) filterOn.addEventListener('change', e => 
    send({ type: 'control', param: 'filterOn', value: e.target.checked }));

  const unisonSel = document.getElementById('unisonSel');
  if (unisonSel) unisonSel.addEventListener('change', e => {
    const val = (+e.target.value) | 0;
    const unison = val === 0 ? 1 : val === 64 ? 2 : 3;
    send({ type: 'control', param: 'unison', value: unison });
  });

  const ped = document.getElementById('ped');
  if (ped) ped.addEventListener('change', e => 
    send({ type: 'control', param: 'sustainPedal', value: e.target.checked }));

  const lfoSyncBox = document.getElementById('lfoSyncBox');
  if (lfoSyncBox) lfoSyncBox.addEventListener('change', e => 
    send({ type: 'control', param: 'lfoSync', value: e.target.checked }));

  const tempoSrc = document.getElementById('tempoSrc');
  if (tempoSrc) tempoSrc.addEventListener('change', e => 
    send({ type: 'control', param: 'tempoSrc', value: e.target.checked ? 1 : 0 }));

  const dlySync = document.getElementById('dlySync');
  if (dlySync) dlySync.addEventListener('change', e => 
    send({ type: 'control', param: 'dlySync', value: e.target.checked }));

  document.getElementById('lfoAmt').addEventListener('input', e => enqueueCC(1, +e.target.value));
  document.getElementById('lfoRate').addEventListener('input', e => enqueueCC(2, +e.target.value));

  // Patch management
  document.getElementById('patchSave').onclick = saveSelectedPatch;
  document.getElementById('saveAsCancel').onclick = () => document.getElementById('saveAsDialog').close();
  document.getElementById('saveAsConfirm').onclick = saveAsChosenSlot;

  const patchLoadBtn = document.getElementById('patchLoad');
  if (patchLoadBtn) {
    patchLoadBtn.onclick = function() {
      const patchIndex = +document.getElementById('patch').value;
      if (!send({ type: 'loadPatch', index: patchIndex }))
        announce('Connect to QuarkWave before loading.');
    };
  }

  // The Pico generates and normalizes a whole sound, then returns patchData.
  document.getElementById('randomBtn').onclick = () => {
    pendingRandomize = send({ type: 'randomize' });
    announce(pendingRandomize ? 'Randomizing sound…' : 'Connect to QuarkWave before randomizing.');
  };

  // Commit buttons  
  const commitBtn = document.getElementById('commitBtn');
  if (commitBtn) {
    commitBtn.onclick = function() {
      announce(send({ type: 'commitPatch' }) ? 'Saving committed sound…' : 'Connect to QuarkWave first.');
    };
  }

  const loadCommitBtn = document.getElementById('loadCommitBtn');
  if (loadCommitBtn) {
    loadCommitBtn.onclick = function() {
      announce(send({ type: 'loadCommitPatch' }) ? 'Loading committed sound…' : 'Connect to QuarkWave first.');
    };
  }

  // Panic button
  const panicBtn = document.getElementById('panicBtn');
  if (panicBtn) {
    panicBtn.onclick = function() {
      announce(send({ type: 'panic' }) ? 'Panic sent: all sound off.' : 'Connect to QuarkWave first.');
    };
  }

  // Visualization buttons
  document.querySelectorAll('button[data-viz]').forEach(btn => {
    btn.onclick = function() {
      send({ type: 'visualization', mode: +btn.dataset.viz });
    };
  });

  // BPM control
  const bpmEl = document.getElementById('bpm');
  const bpmLbl = document.getElementById('bpmLbl');
  if (bpmEl && bpmLbl) {
    bpmEl.addEventListener('input', e => { bpmLbl.textContent = e.target.value; });
    bpmEl.addEventListener('change', e => { 
      send({ type: 'control', param: 'bpm', value: +e.target.value });
    });
  }

  // Additional controls (LFO routing, etc.)
  const lfoMorph = document.getElementById('lfoMorph');
  if (lfoMorph) lfoMorph.addEventListener('input', e => 
    send({ type: 'control', param: 'lfoToMorph', value: (+e.target.value) / 127 }));

  const lfoAmp = document.getElementById('lfoAmp');
  if (lfoAmp) lfoAmp.addEventListener('input', e => 
    send({ type: 'control', param: 'lfoToAmp', value: (+e.target.value) / 127 }));


  const lfoDetune = document.getElementById('lfoDetune');
  if (lfoDetune) lfoDetune.addEventListener('input', e => 
    send({ type: 'control', param: 'lfoToDetune', value: (+e.target.value) / 127 }));

  // Performance controls
  const velCut = document.getElementById('velCut');
  if (velCut) velCut.addEventListener('input', e => 
    send({ type: 'control', param: 'velToCutoff', value: (+e.target.value) / 127 }));

  const noiCut = document.getElementById('noiCut');
  if (noiCut) noiCut.addEventListener('input', e => 
    send({ type: 'control', param: 'noiseToCutoff', value: (+e.target.value) / 127 }));

  // Voice management controls
  const stealMode = document.getElementById('stealMode');
  if (stealMode) stealMode.addEventListener('change', e => 
    send({ type: 'control', param: 'stealMode', value: +e.target.value }));

  const velCurve = document.getElementById('velCurve');
  if (velCurve) velCurve.addEventListener('change', e => 
    send({ type: 'control', param: 'velCurve', value: +e.target.value }));

  // Chorus controls
  const chMix = document.getElementById('chMix');
  if (chMix) chMix.addEventListener('input', e => 
    send({ type: 'control', param: 'chMix', value: (+e.target.value) / 127 }));

  const chDepth = document.getElementById('chDepth');
  if (chDepth) chDepth.addEventListener('input', e => 
    send({ type: 'control', param: 'chDepth', value: +e.target.value }));

  // LFO2/Vibrato controls
  const l2Rate = document.getElementById('l2Rate');
  if (l2Rate) l2Rate.addEventListener('input', e => 
    enqueueCC(27, +e.target.value));

  const l2Amt = document.getElementById('l2Amt');
  if (l2Amt) l2Amt.addEventListener('input', e => 
    send({ type: 'control', param: 'lfo2AmtSemi', value: ((+e.target.value) / 127) * 2 }));

  const l2Wave = document.getElementById('l2Wave');
  if (l2Wave) l2Wave.addEventListener('change', e => 
    send({ type: 'control', param: 'lfo2Wave', value: +e.target.value }));

  // Arp controls
  const arpMode = document.getElementById('arpMode');
  if (arpMode) arpMode.addEventListener('change', e => 
    send({ type: 'control', param: 'arpMode', value: +e.target.value }));

  const arpDiv = document.getElementById('arpDiv');
  if (arpDiv) arpDiv.addEventListener('input', e => 
    send({ type: 'control', param: 'arpDiv', value: +e.target.value }));

  const arpGate = document.getElementById('arpGate');
  if (arpGate) arpGate.addEventListener('input', e => 
    send({ type: 'control', param: 'arpGate', value: +e.target.value }));
}

const velEl = document.getElementById('kvel');
const velValue = document.getElementById('kvelValue');
velEl.addEventListener('input', () => { velValue.textContent = velEl.value; });
const keyboardVelocity = () => Math.min(127, Math.max(1, Math.round(+velEl.value || 100)));
const pointerHeldNotes = new Set();

function buildKeyboard(){
  // Rebuilding on resize or octave change must not leave a pointer-held note on.
  for (const note of pointerHeldNotes) send({ type: 'noteOff', note });
  pointerHeldNotes.clear();
  kb.innerHTML = '';
  const gap = 2;
  const maxW = kb.parentElement.getBoundingClientRect().width || 700;
  let whites = Math.floor(maxW / 46);
  whites = Math.max(14, Math.min(24, whites));
  const whiteW = Math.max(44, Math.floor((maxW - (whites - 1) * gap) / whites));
  kb.style.width = (whites * whiteW + (whites - 1) * gap) + 'px';

  const BASE = baseOct * 12;
  const names = ['C','','D','','E','F','','G','','A','','B'];
  const blackSet = new Set([1,3,6,8,10]);

  let n = BASE, whiteCount = 0;
  const toPlace = whites;

  while (whiteCount < toPlace && n <= 127) {
    const isBlack = blackSet.has(n % 12);
    const b = document.createElement('button');
    b.type = 'button';
    b.tabIndex = 0;
    b.setAttribute('aria-label', 'MIDI note ' + n);
    b.setAttribute('aria-pressed', 'false');

    if (isBlack) {
      const bw = Math.max(44, Math.round(whiteW * 0.68));
      b.style.width = bw + 'px';
      b.className = 's';
      const half = Math.round(bw / 2);
      b.style.marginLeft  = (-half + Math.round(gap/2)) + 'px';
      b.style.marginRight = (-half + Math.round(gap/2)) + 'px';
    } else {
      b.style.width = whiteW + 'px';
      b.className = 'b';
      b.style.marginRight = (whiteCount === toPlace - 1) ? '0px' : gap + 'px';

      const lbl = document.createElement('span');
      lbl.className = 'lbl';
      lbl.textContent = names[n % 12] || '';
      b.appendChild(lbl);
      whiteCount++;
    }

    const note = n;
    const noteUp = (e) => {
      if (b.dataset.d) {
        send({ type: 'noteOff', note: note });
        pointerHeldNotes.delete(note);
        delete b.dataset.d;
        b.classList.remove('active');
        b.setAttribute('aria-pressed', 'false');
      }
      if (e && e.pointerId != null) b.releasePointerCapture?.(e.pointerId);
    };

    b.onpointerdown = e => {
      if (!unoLink.connected || unoLink.syncing) return;
      e.preventDefault();
      b.setPointerCapture?.(e.pointerId);
      send({ type: 'noteOn', note: note, velocity: keyboardVelocity() });
      pointerHeldNotes.add(note);
      b.dataset.d = 1;
      b.classList.add('active');
      b.setAttribute('aria-pressed', 'true');
    };
    b.onpointerup = noteUp; b.onpointerleave = noteUp; b.onpointercancel = noteUp; b.onpointerout = noteUp;
    b.onkeydown = e => {
      if ((e.key !== 'Enter' && e.key !== ' ') || b.dataset.d) return;
      e.preventDefault();
      send({ type: 'noteOn', note, velocity: keyboardVelocity() });
      pointerHeldNotes.add(note);
      b.dataset.d = 1;
      b.classList.add('active');
      b.setAttribute('aria-pressed', 'true');
    };
    b.onkeyup = e => {
      if (e.key !== 'Enter' && e.key !== ' ') return;
      e.preventDefault();
      noteUp();
    };
    b.onblur = noteUp;
    b.oncontextmenu = e => e.preventDefault();

    kb.appendChild(b);
    n++;
  }
}

const keyMap = { 'a':0, 'w':1, 's':2, 'e':3, 'd':4, 'f':5, 't':6, 'g':7, 'y':8, 'h':9, 'u':10, 'j':11 };
function kbNoteFromKey(k){ if (!(k in keyMap)) return null; return baseOct*12 + keyMap[k]; }

const isTypingTarget = el => {
  const t = el.tagName;
  return t === 'TEXTAREA' || t === 'SELECT' || el.isContentEditable ||
    (t === 'INPUT' && el.type !== 'range' && el.type !== 'checkbox');
};

window.addEventListener('keydown', e => {
  if (isTypingTarget(e.target) || e.repeat || !unoLink.connected || unoLink.syncing) return;
  const k = e.key.toLowerCase();
  const n = kbNoteFromKey(k);
  if (n==null || downKeys.has(k)) return;
  downKeys.add(k);
  send({ type: 'noteOn', note: n, velocity: keyboardVelocity() });
  e.preventDefault();
});

window.addEventListener('keyup', e => {
  if (isTypingTarget(e.target)) return;
  const k = e.key.toLowerCase();
  const n = kbNoteFromKey(k);
  if (n==null) return;
  downKeys.delete(k);
  send({ type: 'noteOff', note: n });
  e.preventDefault();
});

let _kbResizeTO = 0;
window.addEventListener('resize', () => { 
  clearTimeout(_kbResizeTO); 
  _kbResizeTO = setTimeout(buildKeyboard, 120); 
});

window.addEventListener('beforeunload', () => { 
  try { ws?.close(); } catch {} 
});

const quickPairs = [
  ['perfVol', 'vol'], ['perfCut', 'cut'],
  ['perfMorph', 'morph'], ['perfArp', 'arpMode'], ['perfPed', 'ped']
];

function syncQuickControls() {
  for (const [quickId, fullId] of quickPairs) {
    const quick = document.getElementById(quickId);
    const full = document.getElementById(fullId);
    if (quick.type === 'checkbox') quick.checked = full.checked;
    else quick.value = full.value;
  }
}

function wireQuickControls() {
  for (const [quickId, fullId] of quickPairs) {
    const quick = document.getElementById(quickId);
    const full = document.getElementById(fullId);
    const eventName = quick.type === 'range' ? 'input' : 'change';
    quick.addEventListener(eventName, () => {
      if (quick.type === 'checkbox') full.checked = quick.checked;
      else full.value = quick.value;
      full.dispatchEvent(new Event(eventName, { bubbles: true }));
    });
  }
}

function formatControlValue(el) {
  const id = ({perfVol:'vol',perfCut:'cut',perfMorph:'morph'})[el.id] || el.id;
  const n = +el.value, t = n / 127;
  const pct = Math.round(t * 100) + '%';
  const hz = value => value >= 1000 ? (value / 1000).toFixed(1) + ' kHz' : Math.round(value) + ' Hz';
  switch (id) {
    case 'a': return Math.round(2 + 498 * t) + ' ms';
    case 'd': return Math.round(10 + 990 * t) + ' ms';
    case 'r': return Math.round(20 + 1480 * t) + ' ms';
    case 'sust': return Math.round(10 + 75 * t) + '%';
    case 'cut': return hz(40 + 9960 * t);
    case 'res': return 'Q ' + (0.5 + 2.5 * t).toFixed(2);
    case 'detune': return (2 + 18 * t).toFixed(1) + ' ct';
    case 'glide': return Math.round(300 * t) + ' ms';
    case 'dlyT': return Math.round(20 + 146 * t) + ' ms';
    case 'dlyFb': return Math.round(1 + 88 * t) + '%';
    case 'lfoAmt': return hz(3000 * t);
    case 'lfoRate': return (0.1 + 12 * t).toFixed(1) + ' Hz';
    case 'l2Rate': return (0.1 + 20 * t).toFixed(1) + ' Hz';
    case 'vol': return Math.round(20 + 80 * t) + '%';
    case 'l2Amt': return (2 * t).toFixed(2) + ' st';
    case 'tremRate': return (0.1 + 12 * t).toFixed(1) + ' Hz';
    case 'bcBits': return Math.round(4 + 12 * t) + ' bits';
    case 'bcRate': return Math.round(1 + 15 * t) + '×';
    case 'arpDiv': return 'Step ' + n;
    case 'arpGate': return n + '%';
    case 'bpm': return n + ' BPM';
    case 'chDepth': return n.toFixed(0);
    default: return pct;
  }
}

function updateControlReadouts() {
  document.querySelectorAll('.control input[type="range"]').forEach(input => {
    const output = input.parentElement.querySelector('.control-value');
    if (output) output.textContent = formatControlValue(input);
  });
}

function drawWaveform() {
  const path = document.getElementById('wavePath');
  const morph = (+document.getElementById('morph').value) / 127;
  const points = [];
  for (let i = 0; i <= 96; ++i) {
    const phase = ((i / 96) * 2) % 1;
    const sine = Math.sin(phase * Math.PI * 2);
    const saw = 2 * phase - 1;
    const tri = 2 * Math.abs(saw) - 1;
    const signedTri = saw >= 0 ? tri : -tri;
    const square = sine >= 0 ? 1 : -1;
    const blend = (a, b, t) => a + (b - a) * t;
    const wave = morph < 1/3 ? blend(sine, signedTri, morph * 3)
      : morph < 2/3 ? blend(signedTri, saw, (morph - 1/3) * 3)
      : blend(saw, square, (morph - 2/3) * 3);
    points.push((i ? 'L' : 'M') + (i * 300 / 96).toFixed(1) + ' ' + (45 - wave * 29).toFixed(1));
  }
  path.setAttribute('d', points.join(' '));
}

function drawEnvelope() {
  const a = +document.getElementById('a').value / 127;
  const d = +document.getElementById('d').value / 127;
  const s = +document.getElementById('sust').value / 127;
  const r = +document.getElementById('r').value / 127;
  const attackEnd = 12 + 48 * a;
  const decayEnd = attackEnd + 28 + 48 * d;
  const sustainY = 68 - (10 + 75 * s) * .5;
  const releaseStart = Math.max(207, decayEnd + 46);
  const releaseEnd = Math.min(295, releaseStart + 24 + 52 * r);
  document.getElementById('envelopePath').setAttribute('d',
    `M4 74 L${attackEnd.toFixed(1)} 9 L${decayEnd.toFixed(1)} ${sustainY.toFixed(1)} L${releaseStart.toFixed(1)} ${sustainY.toFixed(1)} L${releaseEnd.toFixed(1)} 74 L296 74`);
}

function refreshPanel() {
  syncQuickControls();
  updateControlReadouts();
  drawWaveform();
  drawEnvelope();
}

function activateHelpPage(page, remember = true, focusHeading = false) {
  const target = document.getElementById('help-' + page);
  if (!target || !target.classList.contains('help-page')) {
    page = 'start';
  }
  document.querySelectorAll('.help-page').forEach(section => {
    section.hidden = section.dataset.helpPage !== page;
  });
  document.querySelectorAll('.help-nav button[data-help-target]').forEach(button => {
    if (button.dataset.helpTarget === page) button.setAttribute('aria-current', 'page');
    else button.removeAttribute('aria-current');
  });
  if (remember) {
    try { localStorage.setItem('quarkwave-help-page', page); } catch (_) {}
  }
  if (focusHeading) {
    document.getElementById('help-' + page).querySelector('h2').focus();
  }
}

function initHelpPages() {
  let saved = 'start';
  try { saved = localStorage.getItem('quarkwave-help-page') || saved; } catch (_) {}
  activateHelpPage(saved, false);
  document.querySelectorAll('.help-nav button[data-help-target]').forEach(button => {
    button.setAttribute('aria-controls', 'help-' + button.dataset.helpTarget);
    button.addEventListener('click', () => activateHelpPage(button.dataset.helpTarget, true, true));
  });
}

function activateView(view, remember = true) {
  if (!['perform', 'shape', 'explore', 'all', 'help'].includes(view)) view = 'perform';
  // Move the real cards, including their inputs and listeners. No duplicate controls or IDs.
  const groups = [
    [document.querySelector('#view-shape .shape-grid'), document.getElementById('allShapeCards')],
    [document.querySelector('#view-explore .explore-grid'), document.getElementById('allExploreCards')]
  ];
  for (const [home, combined] of groups) {
    const from = view === 'all' ? home : combined;
    const to = view === 'all' ? combined : home;
    while (from.firstElementChild) to.appendChild(from.firstElementChild);
  }
  document.body.dataset.view = view;
  document.querySelectorAll('.view-tab').forEach(tab => {
    const active = tab.dataset.view === view;
    tab.setAttribute('aria-selected', String(active));
    tab.tabIndex = active ? 0 : -1;
    document.getElementById('view-' + tab.dataset.view).hidden = !active;
  });
  if (remember) {
    try { localStorage.setItem('quarkwave-view', view); } catch (_) {}
  }
}

function initViewTabs() {
  let saved = 'perform';
  try { saved = localStorage.getItem('quarkwave-view') || saved; } catch (_) {}
  activateView(saved, false);
  const tabs = Array.from(document.querySelectorAll('.view-tab'));
  tabs.forEach((tab, index) => {
    tab.addEventListener('click', () => activateView(tab.dataset.view));
    tab.addEventListener('keydown', e => {
      const next = e.key === 'ArrowRight' ? (index + 1) % tabs.length
        : e.key === 'ArrowLeft' ? (index + tabs.length - 1) % tabs.length
        : e.key === 'Home' ? 0 : e.key === 'End' ? tabs.length - 1 : -1;
      if (next < 0) return;
      e.preventDefault();
      tabs[next].focus();
      activateView(tabs[next].dataset.view);
    });
  });
}

function setKbEnabled(){
  const connected = ws && ws.readyState === 1;
  kb.classList.toggle('disabled', !connected || !unoLink.connected || unoLink.syncing);
  s.textContent = connected ? 'Pico: Connected' : 'Pico: Not connected';
  document.getElementById('connection').dataset.connected = String(connected);
}

function setUnoStatus(){
  const label = document.getElementById('unoLabel');
  const state = unoLink.syncFailed ? 'Sound sync failed' : unoLink.mode === 'standalone' ? 'Connected to Pico · standalone sound'
    : unoLink.mode === 'syncing' ? 'Connected to Pico · loading sound…' : unoLink.mode === 'pico' ? 'Connected to Pico'
    : unoLink.mode === 'unsynced' ? 'Connected to Pico · awaiting sound' : 'Not connected to Pico';
  label.textContent = 'Uno: ' + state;
  document.getElementById('unoConnection').dataset.connected = String(!!unoLink.connected);
  document.getElementById('rtpLabel').textContent = 'RTP-MIDI: ' + (unoLink.rtpConnected ? 'Controller connected' : 'No controller');
  document.getElementById('rtpConnection').dataset.connected = String(!!unoLink.rtpConnected);
  document.getElementById('syncUnoBtn').disabled = !unoLink.connected || !!unoLink.syncing;
  setKbEnabled();
}

document.addEventListener('DOMContentLoaded', () => {
  initHelpPages();
  initViewTabs();
  wireControls();
  wireQuickControls();
  document.getElementById('syncUnoBtn').onclick = () => {
    if (!unoLink.connected || unoLink.syncing) return;
    if (!confirm('Sync the Pico patch to the Uno? This will stop held notes and replace the Uno’s current sound.')) return;
    pendingUnoSync = send({ type:'syncUno' });
    announce(pendingUnoSync ? 'Syncing Pico patch to Uno…' : 'Connect to the Pico first.');
  };
  document.querySelectorAll('.control input[type="range"]').forEach(input => {
    const output = document.createElement('output');
    output.className = 'control-value';
    output.setAttribute('for', input.id);
    input.parentElement.appendChild(output);
  });
  document.addEventListener('input', refreshPanel);
  document.addEventListener('change', refreshPanel);
  refreshPanel();
  buildKeyboard();
  octLbl.textContent = baseOct;
  ws = initWS();
  setKbEnabled();
  setUnoStatus();
});
</script>
</body>
</html>)HTML";
static const size_t INDEX_HTML_LEN = sizeof(INDEX_HTML) - 1;
