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
<title>QuarkWave – Web Panel</title>
<style>
  /* ---------- Layout ---------- */
  .layout{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:12px;margin-top:12px}
  .section{grid-column:1 / -1}
  @media (min-width:900px){
    .span-8{grid-column:span 8}
    .span-4{grid-column:span 4}
    .split-6{grid-column:span 6}
    .section[aria-label="Patch"].span-8 { grid-column: 1 / -1; }
    .section[aria-label="Performance"].span-8 { grid-column: 1 / -1; }
    .section[aria-label="Envelope"].span-8 { grid-column: 1 / -1; } 
  }

  /* ---------- Section headers ---------- */
  h3{margin:16px 0 6px 0;font:600 17px/1.25 system-ui;letter-spacing:.1px;opacity:.95}

  /* ---------- Bars (horizontal groups) ---------- */
  .bar{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:8px}

  /* ---------- Rounded-square pills (vertical by default) ---------- */
  .pill{
    display:flex;flex-direction:column;align-items:flex-start;gap:8px;
    padding:10px;min-width:160px;width:fit-content;
    border:1px solid #b8b8b8;border-radius:12px;background:#f7f7f7;
    font-size:12px;line-height:1.3;box-sizing:border-box
  }
  .pill b{font-size:13px;font-weight:600;display:block;margin:2px 0 4px}
  .pill--compact{ min-width:auto; padding:6px 8px; }

  /* Horizontal pill variant */
  .pill--h{flex-direction:row;align-items:center}
  .pill--h>b{margin-right:8px}

  /* Strong keyboard focus */
  .btn:focus-visible{
    outline:2px solid #3b82f6;
    outline-offset:2px;
  }

  /* Controls inside pills */
  .pill label{display:flex;flex-direction:column;gap:6px;width:100%}
  .pill input[type="range"], .pill select, .pill .btn{width:100%;margin:0}
  .pill input[type="range"]{vertical-align:middle}

  /* ---------- Buttons that look like buttons (not applied to keyboard) ---------- */
  .btn{
    appearance:none;user-select:none;cursor:pointer;
    height:30px;padding:0 12px;border-radius:10px;border:1px solid #b5b5b5;
    background:linear-gradient(#fff,#f1f1f1); box-shadow:0 1px 0 rgba(0,0,0,.05);
    font:500 12px/1 system-ui; color:#222;
    transition:transform .05s ease, box-shadow .1s ease, background .1s ease
  }
  .btn:hover{background:linear-gradient(#fff,#eaeaea)}
  .btn:active{transform:translateY(1px);box-shadow:inset 0 1px 2px rgba(0,0,0,.15)}
  .btn--small{height:26px;padding:0 10px}
  .btn--primary{border-color:#3b82f6;background:linear-gradient(#5ea1ff,#3b82f6);color:#fff}
  .btn--primary:hover{background:linear-gradient(#6aa9ff,#3b82f6)}
  .btn--primary[data-variant="danger"]{
    border-color:#e03131;
    background:linear-gradient(#ff6b6b,#e03131);
  }
  .btn--primary[data-variant="danger"]:hover{
    background:linear-gradient(#ff7777,#e03131);
  }
  .btn--primary[data-variant="danger"]:focus-visible{
    outline-color:#e03131;
  }

  /* Selects to match buttons lightly */
  .pill select{
    height:30px;padding:0 10px;border-radius:10px;border:1px solid #b5b5b5;background:#fff
  }

  /* ---------- Two-column pill grids (Tone/Modulation) ---------- */
  .pillGrid{display:grid;gap:12px;grid-template-columns:1fr}
  .pillGrid>div{display:flex;flex-direction:column;gap:8px}
  @media (min-width:900px){ .pillGrid{grid-template-columns:1fr 1fr 1fr 1fr} }

  /* ---------- Keyboard ---------- */
  #kb{display:flex;gap:0;margin-top:12px;position:relative;user-select:none}
  #kb button{position:relative;height:80px;border:1px solid #999;border-radius:4px;overflow:hidden;touch-action:none;transition:transform 40ms,filter 40ms,background 40ms}
  #kb button.b{background:#eee}
  #kb button.s{background:#333;color:#fff;height:50px;z-index:2}
  #kb button .lbl{position:absolute;left:0;right:0;bottom:4px;text-align:center;font:11px/1 system-ui;opacity:.8;pointer-events:none}
  #kb button.b.active{background:#ddd;transform:translateY(1px);filter:brightness(.95)}
  #kb button.s.active{background:#222;transform:translateY(1px);filter:brightness(1.15)}
  #kb.disabled{opacity:.6} #kb.disabled button{pointer-events:none}

  /* ---------- Small-screen tweaks ---------- */
  @media (max-width:700px){
    .pill{gap:6px;padding:8px;min-width:140px}
    .pill--h input[type="range"]{width:120px}
    .pill--h input[type="text"]{width:100px}
  }

  /* --- Ensure horizontal pills stay compact (override the 100% widths) --- */
  .pill.pill--h label{ width:auto; flex:0 0 auto; }
  .pill.pill--h .btn,
  .pill.pill--h select{ width:auto; }
</style>
</head>
<body style="font-family:system-ui;margin:16px;max-width:1280px">
  <h1 style="margin:0 0 8px 0">&#9883;&#127754; QuarkWave</h1>
  <div style="opacity:.8; font-size:16px">Web Synthesis Sound Design Studio</div>
  <div id="s" aria-live="polite" style="opacity:.8">Connecting…</div>
  <div class="layout">
    <!-- PATCH (left) + VIZ/PANIC/COMMIT (right) -->
    <div class="section span-8" aria-label="Patch">
      <div class="bar">
        <span class="pill pill--h"> <b>Patch</b>
          <select id="patch"></select>
          <button type="button" class="btn btn--primary" id="patchLoad">Load</button>
          <button type="button" class="btn" id="patchSave">Save</button>
          <input id="pname" type="text" placeholder="Name…">
          <button type="button" class="btn" id="randomize">Randomize</button>
        </span>

        <span class="pill pill--h"><b>Viz</b>
          <button type="button" class="btn btn--small" data-viz-mode="0">Status</button>
          <button type="button" class="btn btn--small" data-viz-mode="1">VU</button>
          <button type="button" class="btn btn--small" data-viz-mode="2">Scope</button>
          <button type="button" class="btn btn--small" data-viz-mode="3">Water</button>
        </span>

        <span class="pill pill--h pill--compact">
           <button type="button" class="btn btn--primary"
                  data-variant="danger"
                  title="Send All Notes Off"
                  aria-label="Panic – All Notes Off"
                  id="panic">
            Panic
          </button>
        </span>

        <span class="pill pill--h pill--compact">
           <button type="button" class="btn btn--primary"
                  title="Save commit patch"
                  aria-label="Commit – Save commit patch"
                  id="commitPatch">
            Commit
          </button>
        </span>


        <span class="pill pill--h pill--compact">
           <button type="button" class="btn btn--primary"
                  title="Load commit patch"
                  aria-label="Load commit patch"
                  id="loadCommitPatch">
            Load Commit
          </button>
        </span>
      </div>
    </div>

    <!-- ENVELOPE -->
    <div class="section span-8" aria-label="Envelope" >
      <div class="bar">
        <span class="pill pill--h"><b>Envelope</b>
          <label>Attack <input type="range" data-cc="73" min="0" max="127" value="16" id="a"></label>
          <label>Decay <input type="range" data-cc="75" min="0" max="127" value="35" id="d"></label>
          <label>Sustain <input type="range" data-cc="23" min="0" max="127" value="90" id="sust"></label>
          <label>Release <input type="range" data-cc="72" min="0" max="127" value="40" id="r"></label>
        </span>

        <span class="pill pill--h"><b>Tempo</b>
          <label>BPM
            <input id="bpm" type="range" min="40" max="240" value="120">
          </label>
          <small id="bpmLbl" style="margin-left:4px">120</small>
          <label style="margin-left:8px">
            <input type="checkbox" id="tempoSrc"> Clock
          </label>
        </span>
      </div>
    </div>

    <!-- TONE (left) + MODULATION (right) -->
    <div class="section split-6">
      <h3>Tone</h3>
      <span class="pill" style="width:100%">
        <div class="pillGrid">
          <div>
            <b>Osc</b>
            <label>Morph <input type="range" data-cc="76" min="0" max="127" value="0" id="morph"></label>
            <label>Detune <input type="range" data-cc="94" min="0" max="127" value="30" id="detune"></label>
          </div>
          <div>
            <b>&nbsp;</b>
            <label>Noise <input type="range" data-cc="93" min="0" max="127" value="0" id="noise"></label>
            <label>Unison
              <select id="unisonSel">
                <option value="0">1</option><option value="64">2</option><option value="127">3</option>
              </select>
            </label>
          </div>
          <div>
            <b>Amp</b>
            <label>Volume <input type="range" data-cc="7" min="0" max="127" value="100" id="vol"></label>
            <label>Glide  <input type="range" data-cc="5" min="0" max="127" value="0"   id="glide"></label>
          </div>
          <div>
            <b>Filter</b>
            <label>Cutoff    <input type="range" data-cc="74" min="0" max="127" value="70" id="cut"></label>
            <label>Resonance <input type="range" data-cc="71" min="0" max="127" value="30" id="res"></label>
            <input type="checkbox" id="filterOn" data-cc="91" checked> Filter On
          </div>
        </div>
      </span>
    </div>

    <div class="section split-6">
      <h3>Modulation</h3>
      <span class="pill" style="width:100%">
        <div class="pillGrid">
          <div>
            <b>LFO1</b>
            <label>Amt  <input type="range" min="0" max="127" value="40" id="lfoAmt"  oninput="enqueueCC(1, +this.value)"></label>
            <label>Rate <input type="range" min="0" max="127" value="60" id="lfoRate" oninput="enqueueCC(2, +this.value)"></label>
            <label >Sync <input type="checkbox" id="lfoSyncBox"></label>
          </div>
          <div>
            <b>LFO Routing</b>
            <label>→ Morph  <input id="lfoMorph"  type="range" min="0" max="127" value="0"></label>
            <label>→ Amp    <input id="lfoAmp"    type="range" min="0" max="127" value="0"></label>
            <label>→ Detune <input id="lfoDetune" type="range" data-cc="26" min="0" max="127" value="0"></label>
          </div>
          <div>
            <b>Env & Perf</b>
            <label>Vel→Cutoff   <input id="velCut" type="range" data-cc="24" min="0" max="127" value="0"></label>
            <label>Noise→Cutoff <input id="noiCut" type="range" data-cc="25" min="0" max="127" value="0"></label>
          </div>
          <div>
            <b>Response</b>
            <label for="stealMode">Voice Steal</label>
            <select id="stealMode">
              <option value="0">Quietest</option>
              <option value="1">Last</option>
            </select>

            <label for="velCurve">Velocity Curve</label>
            <select id="velCurve">
              <option value="0">Linear</option>
              <option value="1">Soft</option>
              <option value="2">Hard</option>
              <option value="3">Expo</option>
            </select>
          </div>
        </div>
      </span>
    </div>

    <!-- FX -->
    <div class="section">
      <h3>FX</h3>
      <div class="bar">
        <span class="pill pill--h"><b>Delay</b>
          <label>Mix <input id="dlyMix" type="range" data-cc="14" min="0" max="127" value="0" aria-label="Delay Mix"></label>
          <label>FB  <input id="dlyFb"  type="range" data-cc="13" min="0" max="127" value="0" aria-label="Delay Feedback"></label>
          <label>Time<input id="dlyT"   type="range" data-cc="12" min="0" max="127" value="0" aria-label="Delay Time"></label>
          <label style="margin-left:8px" title="Locks delay time to 1/16 note of the active tempo">
            Sync (1/16) <input type="checkbox" id="dlySync"> 
          </label>
        </span>

        <span class="pill pill--h"><b>Chorus</b>
          <label>Mix   <input id="chMix"   type="range" min="0" max="127" value="0"></label>
          <label>Depth <input id="chDepth" type="range" min="0" max="20"  value="5"></label>
        </span>

        <span class="pill pill--h"><b>Character</b>
          <label>Crush <input id="crush" type="range" data-cc="29" min="0" max="127" value="0"></label>
          <label>Drive <input id="drive" type="range" data-cc="33" min="0" max="127" value="0"></label>
          <label>Fold  <input id="fold"  type="range" data-cc="34" min="0" max="127" value="0"></label>
        </span>

        <span class="pill pill--h"><b>Tremolo</b>
          <label>Rate  <input id="tremRate"  type="range" data-cc="30" min="0" max="127" value="31"></label>
          <label>Depth <input id="tremDepth" type="range" data-cc="31" min="0" max="127" value="0"></label>
        </span>
  
        <span class="pill pill--h pill--compact"><b>Sustain</b>
          <input type="checkbox" id="ped">
        </span>

        <span class="pill pill--h"><b>Vibrato</b>
          <label>Rate <input id="l2Rate" type="range" min="0" max="127" value="31"></label>
          <label>Amt <input id="l2Amt"  type="range" min="0" max="127" value="10"></label>
          <select id="l2Wave">
            <option value="0">Sine</option><option value="1">Tri</option><option value="2">Square</option>
          </select>
        </span>

        <span class="pill pill--h"><b>Arp</b>
          <select id="arpMode">
            <option value="0">Off</option>
            <option value="1">Up</option>
            <option value="2">Down</option>
            <option value="3">Up-Down</option>
            <option value="4">Random</option>
          </select>
          <label>Div <input id="arpDiv"  type="range" min="0" max="7"  value="2"  aria-label="Arp Division"></label>
          <label>Gate <input id="arpGate" type="range" min="5" max="95" value="60" aria-label="Arp Gate"></label>
        </span>

        <span class="pill pill--h"><b>Octave</b>
          <button type="button" id="octD" class="btn btn--small">−</button>
          <span id="octLbl">3</span>
          <button type="button" id="octU" class="btn btn--small">+</button>
        </span>
      </div>
    </div>

    <!-- Keyboard + helpers -->
    <div class="section" id="kb"></div>
  </div>

  <footer style="margin-top:24px;font:12px system-ui;opacity:.7;text-align:center">
    <div>Powered by <b>Arduino Uno R4 WiFi</b></div>
    <div>Made with ♥ by Polyatomic</div>
    <div>QuarkWave 1.0</div>
  </footer>

<script>
let ws = null; 

function send(obj){
  if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
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
    send({ type: 'getState' });
  };

  sock.onclose = () => {
    for (const k of downKeys) { 
      const n = kbNoteFromKey(k); 
      if (n != null) send({ type: 'noteOff', note: n });
    }
    downKeys.clear();
    setKbEnabled();
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
        case 'patchList':
          // Handle the new structure with userPatches and factoryPatches
          updatePatchList({
            userPatches: msg.userPatches, 
            factoryPatches: msg.factoryPatches
          });
          break;

        case 'patchData':
          updateUIFromPatchData(msg.patch);
          break;
          
        case 'currentPatch':
          setCurrentPatch(msg.index);
          break;
          
        case 'state':
          updateUIFromState(msg);
          break;

        case 'status':
          if (s) s.textContent = msg.message || 'Connected';
          break;

        case 'synthMessage':
          console.debug('[synth]', msg.message);
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
  const sel = document.getElementById('patch');
  const saveBtn = document.getElementById('patchSave');
  const nameInput = document.getElementById('pname');
  
  if (sel && saveBtn && nameInput) {
    const selectedIndex = +sel.value;
    const isFactoryPreset = selectedIndex >= 100;
    
    if (isFactoryPreset) {
      // For factory presets, change button text and behavior to "Save As"
      saveBtn.textContent = "Save As";
      saveBtn.disabled = false;  // Enable save button
      nameInput.disabled = false;
      nameInput.placeholder = "Enter new patch name...";
      saveBtn.title = "Save as new user patch";
      
      // Change the onclick behavior for factory presets
      saveBtn.onclick = function() {
        const patchName = document.getElementById('pname').value;
        if (!patchName.trim()) {
          alert('Please enter a name for the new patch');
          return;
        }
        
        // Find first empty user slot (0-7) or ask user to choose
        let targetSlot = -1;
        for (let i = 0; i < 8; i++) {
          const option = document.querySelector(`option[value="${i}"]`);
          if (option && option.textContent.includes('P' + i)) {
            targetSlot = i;
            break;
          }
        }
        
        if (targetSlot === -1) {
          // All slots occupied, ask user which slot to overwrite
          targetSlot = prompt('All user slots are occupied. Which slot (0-7) would you like to overwrite?');
          targetSlot = parseInt(targetSlot);
          if (isNaN(targetSlot) || targetSlot < 0 || targetSlot > 7) {
            alert('Invalid slot number');
            return;
          }
        }
        
        // Save to user patch slot
        send({ type: 'savePatch', index: targetSlot, name: patchName });
        
        // Switch to the newly saved patch
        document.getElementById('patch').value = String(targetSlot);
        saveBtn.textContent = "Save";
        saveBtn.onclick = originalSaveFunction; // Restore original save function
        nameInput.placeholder = "Name...";
      };
      
    } else {
      // For user patches, normal save behavior
      saveBtn.textContent = "Save";
      saveBtn.disabled = false;
      nameInput.disabled = false;
      nameInput.placeholder = "Name...";
      saveBtn.title = "";
      saveBtn.onclick = originalSaveFunction;
    }
  }
}

// Store original save function
const originalSaveFunction = function() {
  const patchIndex = +document.getElementById('patch').value;
  const patchName = document.getElementById('pname').value || `P${patchIndex}`;
  send({ type: 'savePatch', index: patchIndex, name: patchName });
};

function updatePatchList(data) {
  const sel = document.getElementById('patch');
  if (!sel) return;
  
  // Clear existing options
  sel.innerHTML = '';
  
  // Add user patches first (0-7)
  if (data.userPatches) {
    data.userPatches.forEach((patch) => {
      const option = new Option(
        patch.name || `Empty ${patch.index}`, 
        String(patch.index)
      );
      sel.add(option);
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
  
  // Update save button state when selection changes
  updateSaveButtonState();
}

function setCurrentPatch(index) {
  const sel = document.getElementById('patch');
  if (sel) sel.value = String(index);
}

function updateUIFromPatchData(patch) {
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
  if (patch.crushAmt != null) setVal('crush', Math.round(patch.crushAmt * 127));
  if (patch.tremRateHz != null) setVal('tremRate', Math.round((patch.tremRateHz - 0.2) / 19.8 * 127));
  if (patch.tremDepth != null) setVal('tremDepth', Math.round(patch.tremDepth * 127));
  if (patch.driveAmt != null) setVal('drive', Math.round(patch.driveAmt * 127));
  if (patch.foldAmt != null) setVal('fold', Math.round(patch.foldAmt * 127));
  if (patch.noiseAmt != null) setVal('noise', Math.round(patch.noiseAmt * 127));

  if (patch.lfoToMorph != null) setVal('lfoMorph', Math.round(patch.lfoToMorph * 127));
  if (patch.lfoToAmp != null) setVal('lfoAmp', Math.round(patch.lfoToAmp * 127));
  if (patch.lfoToDetune != null) setVal('lfoDetune', Math.round(patch.lfoToDetune * 127));
  if (patch.velToCutoff != null) setVal('velCut', Math.round(patch.velToCutoff * 127));
  if (patch.noiseToCutoff != null) setVal('noiCut', Math.round(patch.noiseToCutoff * 127));
  
  if (patch.chMix != null) setVal('chMix', Math.round(patch.chMix * 127));
  if (patch.chDepth != null) setVal('chDepth', Math.round(patch.chDepth));
  
  if (patch.arpMode != null) setVal('arpMode', patch.arpMode);
  if (patch.arpDiv != null) setVal('arpDiv', patch.arpDiv);
  if (patch.arpGate != null) setVal('arpGate', patch.arpGate);
  
  if (patch.lfo2RateHz != null) setVal('l2Rate', Math.round((patch.lfo2RateHz - 0.05) / 19.95 * 127));
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
}

const s  = document.getElementById('s');
const kb = document.getElementById('kb');

const pending = new Map();
let rafPending = false;

let baseOct = 3;
const downKeys = new Set();
const octLbl = document.getElementById('octLbl');
document.getElementById('octD').onclick = ()=>{ baseOct = Math.max(0, baseOct-1); octLbl.textContent = baseOct; buildKeyboard(); };
document.getElementById('octU').onclick = ()=>{ baseOct = Math.min(8, baseOct+1); octLbl.textContent = baseOct; buildKeyboard(); };

function enqueueCC(cc, v){ 
  pending.set(cc, v);
  scheduleSend();
}

function scheduleSend(){
  if (rafPending) return;
  rafPending = true;
  requestAnimationFrame(() => {
    rafPending = false;
    if (!ws || ws.readyState !== 1 || pending.size === 0) return;
    const items = [];
    for (const [id, v] of pending) items.push({id, v});
    pending.clear();
    send({ type: 'controls', items });
  });
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
    enqueueCC(64, e.target.checked ? 127 : 0));

  const lfoSyncBox = document.getElementById('lfoSyncBox');
  if (lfoSyncBox) lfoSyncBox.addEventListener('change', e => 
    send({ type: 'control', param: 'lfoSync', value: e.target.checked }));

  const tempoSrc = document.getElementById('tempoSrc');
  if (tempoSrc) tempoSrc.addEventListener('change', e => 
    send({ type: 'control', param: 'tempoSrc', value: e.target.checked ? 1 : 0 }));

  const dlySync = document.getElementById('dlySync');
  if (dlySync) dlySync.addEventListener('change', e => 
    send({ type: 'control', param: 'dlySync', value: e.target.checked }));

  const patchSaveBtn = document.getElementById('patchSave');
  if (patchSaveBtn) {
    patchSaveBtn.onclick = function() {
      const patchIndex = +document.getElementById('patch').value;
      const patchName = document.getElementById('pname').value || `Patch ${patchIndex}`;
      send({ type: 'savePatch', index: patchIndex, name: patchName });
    };
  }

  const patchLoadBtn = document.getElementById('patchLoad');
  if (patchLoadBtn) {
    patchLoadBtn.onclick = function() {
      const patchIndex = +document.getElementById('patch').value;
      send({ type: 'loadPatch', index: patchIndex });
    };
  }

  function applyUIForParam(param, value){
  const set = (id, v) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.value = String(v);
	    el.dispatchEvent(new Event('input', {bubbles:true}));
  };

  // map param -> element + inverse scaling (value -> 0..127)
  if (param === 'attack')  set('a',     Math.round((value - 0.002) / 0.498 * 127));
  else if (param === 'decay')   set('d',    Math.round((value - 0.01)  / 0.99  * 127));
  else if (param === 'sustain') set('sust', Math.round((value - 0.10)  / 0.75  * 127));
  else if (param === 'release') set('r',    Math.round((value - 0.02)  / 1.48  * 127));
  else if (param === 'cutoff')  set('cut',  Math.round((value - 40)    / 9960  * 127));
  else if (param === 'resonance') set('res',   Math.round((value - 0.5) / 2.5   * 127));
  else if (param === 'morph')   set('morph',   Math.round(value * 127));
  else if (param === 'glide')   set('glide',   Math.round(value / 0.3 * 127));
  else if (param === 'detune')  set('detune',  Math.round((value - 2) / 18 * 127));
  else if (param === 'masterGain') set('vol',  Math.round((value - 0.2) / 0.8 * 127));
  else if (param === 'noiseAmt')   set('noise',Math.round(value * 127));
  else if (param === 'delayTime')  set('dlyT', Math.round((value - 0.02) / 0.146 * 127));
  else if (param === 'delayFeedback') set('dlyFb', Math.round((value - 0.01) / 0.88 * 127));
  else if (param === 'delayMix')  set('dlyMix', Math.round(value * 127));
  else if (param === 'crushAmt')  set('crush', Math.round(value * 127));
  else if (param === 'tremRateHz') set('tremRate', Math.round((value - 0.2) / 19.8 * 127));
  else if (param === 'tremDepth') set('tremDepth', Math.round(value * 127));
  else if (param === 'driveAmt')  set('drive', Math.round(value * 127));
  else if (param === 'foldAmt')   set('fold', Math.round(value * 127));
  else if (param === 'lfoAmtHz')  set('lfoAmt', Math.round(value / 3000 * 127));
  else if (param === 'lfoRateHz') set('lfoRate', Math.round((value - 0.1) / 12 * 127));
  else if (param === 'lfoSync')   document.getElementById('lfoSyncBox')?.toggleAttribute('checked', !!value);
  else if (param === 'dlySync')   document.getElementById('dlySync')?.toggleAttribute('checked', !!value);
  else if (param === 'tempoSrc') {
    const el = document.getElementById('tempoSrc');
    if (el) el.checked = (value > 0);
  } else if (param === 'bpm') {
    const bpmEl = document.getElementById('bpm');
    const bpmLbl = document.getElementById('bpmLbl');
    if (bpmEl) bpmEl.value = String(Math.round(value));
    if (bpmLbl) bpmLbl.textContent = Math.round(value);
  } else if (param === 'unison') {
    const el = document.getElementById('unisonSel');
    if (el) el.value = (value === 1 ? 0 : value === 2 ? 64 : 127);
  }
}

  const randomBtn = document.getElementById('randomize');
  if (randomBtn) {
    randomBtn.onclick = function() {
      const r = (min, max) => min + Math.random() * (max - min);

      const params = [
        // Envelope
        ['attack',       r(0.002, 0.5)],
        ['decay',        r(0.01, 1.0)],
        ['sustain',      r(0.10, 0.85)],
        ['release',      r(0.02, 2.0)],

        // Tone
        ['cutoff',       r(40, 10000)],
        ['resonance',    r(0.5, 3.0)],
        ['morph',        r(0, 1)],
        ['glide',        r(0, 0.3)],
        ['detune',       r(2, 20)],
        ['masterGain',   r(0.2, 1.0)],
        ['noiseAmt',     r(0, 1)],

        // Delay
        ['delayTime',    r(0.02, 0.166)],   // 0.02–0.166 ≈ 20–166 ms
        ['delayFeedback',r(0.01, 0.89)],    // 1–89%
        ['delayMix',     r(0, 1)],

        // Character FX
        ['crushAmt',     r(0, 0.45)],
        ['tremRateHz',   r(0.2, 12)],
        ['tremDepth',    r(0, 0.6)],
        ['driveAmt',     r(0, 0.65)],
        ['foldAmt',      r(0, 0.45)],

        // LFO1
        ['lfoAmtHz',     r(0, 3000)],       // amount in Hz-depth
        ['lfoRateHz',    r(0.1, 12)],       // rate in Hz
        ['lfoSync',      Math.random() > 0.5 ? 1 : 0],

        // LFO2 (Vibrato)
        ['lfo2RateHz',   r(0.05, 20)],      // Hz
        ['lfo2AmtSemi',  r(0, 2)],          // semitone depth
        ['lfo2Wave',     Math.floor(r(0, 3))], // sine, tri, square

        // Modulation routing
        ['lfoToMorph',   r(0, 1)],
        ['lfoToAmp',     r(0, 1)],
        ['lfoToDetune',  r(0, 1)],
        ['velToCutoff',  r(0, 1)],
        ['noiseToCutoff',r(0, 1)],

        // FX / extras
        ['chMix',        r(0, 1)],
        ['chDepth',      r(0, 20)],
        ['arpMode',      Math.floor(r(0, 5))],  // 0=off…4=random
        ['arpDiv',       Math.floor(r(0, 8))],
        ['arpGate',      r(5, 95)],

        // Performance / system
        ['tempoSrc',     Math.random() > 0.5 ? 1 : 0],
        ['bpm',          r(40, 240)],
        ['unison',       Math.floor(r(1, 4))],  // 1,2,3 voices
        ['stealMode',    Math.floor(r(0, 2))],  // 0=quietest,1=last
        ['velCurve',     Math.floor(r(0, 4))]   // 0–3
      ];

      for (const [param, value] of params) {
        send({ type: 'control', param, value });
        applyUIForParam(param, value);
      }
    };
  }

  const commitBtn = document.getElementById('commitPatch');
  if (commitBtn) {
    commitBtn.onclick = function() {
      send({ type: 'commitPatch' });
    };
  }

  const loadCommitBtn = document.getElementById('loadCommitPatch');
  if (loadCommitBtn) {
    loadCommitBtn.onclick = function() {
      send({ type: 'loadCommitPatch' });
    };
  }

  const panicBtn = document.getElementById('panic');
  if (panicBtn) {
    panicBtn.onclick = function() {
      send({ type: 'panic' });
    };
  }

  document.querySelectorAll('[data-viz-mode]').forEach(btn => {
    btn.onclick = function() {
      send({ type: 'visualization', mode: +btn.dataset.vizMode });
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
    send({ type: 'control', param: 'lfo2RateHz', value: 0.05 + ((+e.target.value) / 127) * 19.95 }));

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

function updateUIFromState(st){
  const setVal = (id, val) => {
    const el = document.getElementById(id);
    if (!el || val == null || Number.isNaN(val)) return;
    el.value = String(Math.max(+el.min || 0, Math.min(+el.max || 127, Math.round(val))));
  };
  const setRaw = (id, val) => {
    const el = document.getElementById(id);
    if (!el || val == null) return;
    el.value = String(val);
  };
  const setChecked = (id, val) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.checked = !!val;
  };
  const norm = (x, lo, hi) => ((Number(x) - lo) / (hi - lo)) * 127;

  if (st.a != null) setVal('a', norm(st.a, 0.002, 0.5));
  if (st.d != null) setVal('d', norm(st.d, 0.01, 1.0));
  if (st.s != null) setVal('sust', norm(st.s, 0.10, 0.95));
  if (st.r != null) setVal('r', norm(st.r, 0.02, 1.52));

  if (st.morph != null) setVal('morph', Number(st.morph) * 127);
  if (st.cutoff != null) setVal('cut', norm(st.cutoff, 40, 10000));
  if (st.res != null) setVal('res', norm(st.res, 0.5, 3.0));
  if (st.glide != null) setVal('glide', norm(st.glide, 0, 0.3));
  if (st.detune != null) setVal('detune', norm(st.detune, 2, 20));
  if (st.gain != null) setVal('vol', norm(st.gain, 0.2, 1.0));
  if (st.noise != null) setVal('noise', Number(st.noise) * 127);

  if (st.dlyT != null) setVal('dlyT', norm(st.dlyT, 0.02, 0.166));
  if (st.dlyFb != null) setVal('dlyFb', norm(st.dlyFb, 0.01, 0.89));
  if (st.dlyMix != null) setVal('dlyMix', Number(st.dlyMix) * 127);
  if (st.crush != null) setVal('crush', Number(st.crush) * 127);
  if (st.tremRate != null) setVal('tremRate', norm(st.tremRate, 0.2, 20.0));
  if (st.tremDepth != null) setVal('tremDepth', Number(st.tremDepth) * 127);
  if (st.drive != null) setVal('drive', Number(st.drive) * 127);
  if (st.fold != null) setVal('fold', Number(st.fold) * 127);

  if (st.lfoMorph != null) setVal('lfoMorph', Number(st.lfoMorph) * 127);
  if (st.lfoAmp != null) setVal('lfoAmp', Number(st.lfoAmp) * 127);
  if (st.lfoToDet != null) setVal('lfoDetune', Number(st.lfoToDet) * 127);
  if (st.velToCut != null) setVal('velCut', Number(st.velToCut) * 127);
  if (st.noiseToCut != null) setVal('noiCut', Number(st.noiseToCut) * 127);

  if (st.chorusMix != null) setVal('chMix', Number(st.chorusMix) * 127);
  if (st.lfo2Rate != null) setVal('l2Rate', norm(st.lfo2Rate, 0.05, 20.0));
  if (st.lfo2Amt != null) setVal('l2Amt', norm(st.lfo2Amt, 0, 2));

  if (st.unison != null) setRaw('unisonSel', st.unison <= 1 ? 0 : st.unison == 2 ? 64 : 127);
  if (st.lfo2Wave != null) setRaw('l2Wave', st.lfo2Wave);
  if (st.velCurve != null) setRaw('velCurve', st.velCurve);
  if (st.stealMode != null) setRaw('stealMode', st.stealMode);
  if (st.arpMode != null) setRaw('arpMode', st.arpMode);
  if (st.arpDiv != null) setRaw('arpDiv', st.arpDiv);
  if (st.arpGate != null) setRaw('arpGate', st.arpGate);

  setChecked('filterOn', st.filterOn == null ? document.getElementById('filterOn')?.checked : Number(st.filterOn) != 0);
  setChecked('lfoSyncBox', st.lfoSync == null ? document.getElementById('lfoSyncBox')?.checked : Number(st.lfoSync) != 0);
  setChecked('dlySync', st.dlySync == null ? document.getElementById('dlySync')?.checked : Number(st.dlySync) != 0);
  setChecked('tempoSrc', st.tempoSrc == null ? document.getElementById('tempoSrc')?.checked : Number(st.tempoSrc) != 0);

  if (st.bpmInt != null || st.bpm != null) {
    const bpm = Math.round(Number(st.bpmInt ?? st.bpm));
    setRaw('bpm', bpm);
    const bpmLbl = document.getElementById('bpmLbl');
    if (bpmLbl) bpmLbl.textContent = bpm;
  }
}

const velEl = document.getElementById('kvel') || { value:100 };

function buildKeyboard(){
  kb.innerHTML = '';
  const gap = 2;
  const maxW = kb.getBoundingClientRect().width || 700;
  let whites = Math.floor(maxW / 30);
  whites = Math.max(14, Math.min(24, whites));
  const whiteW = Math.floor((maxW - (whites - 1) * gap) / whites);

  const BASE = baseOct * 12;
  const names = ['C','','D','','E','F','','G','','A','','B'];
  const blackSet = new Set([1,3,6,8,10]);

  let n = BASE, whiteCount = 0;
  const toPlace = whites;

  while (whiteCount < toPlace) {
    const isBlack = blackSet.has(n % 12);
    const b = document.createElement('button');
    b.tabIndex = 0;

    if (isBlack) {
      const bw = Math.max(14, Math.round(whiteW * 0.6));
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
        delete b.dataset.d;
        b.classList.remove('active');
        b.setAttribute('aria-pressed', 'false');
      }
      if (e && e.pointerId != null) b.releasePointerCapture?.(e.pointerId);
    };

    b.onpointerdown = e => {
      e.preventDefault();
      b.setPointerCapture?.(e.pointerId);
      send({ type: 'noteOn', note: note, velocity: +velEl.value|0 });
      b.dataset.d = 1;
      b.classList.add('active');
      b.setAttribute('aria-pressed', 'true');
    };
    b.onpointerup = noteUp; b.onpointerleave = noteUp; b.onpointercancel = noteUp; b.onpointerout = noteUp;
    b.oncontextmenu = e => e.preventDefault();

    kb.appendChild(b);
    n++;
  }
}

const keyMap = { 'a':0, 'w':1, 's':2, 'e':3, 'd':4, 'f':5, 't':6, 'g':7, 'y':8, 'h':9, 'u':10, 'j':11 };
function kbNoteFromKey(k){ if (!(k in keyMap)) return null; return baseOct*12 + keyMap[k]; }

const isTypingTarget = el => {
  const t = el.tagName; return t === 'INPUT' || t === 'TEXTAREA' || el.isContentEditable || t === 'SELECT';
};

window.addEventListener('keydown', e => {
  if (isTypingTarget(e.target) || e.repeat) return;
  const k = e.key.toLowerCase();
  const n = kbNoteFromKey(k);
  if (n==null || downKeys.has(k)) return;
  downKeys.add(k);
  send({ type: 'noteOn', note: n, velocity: +velEl.value|0 });
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

function setKbEnabled(){
  const connected = ws && ws.readyState === 1;
  kb.classList.toggle('disabled', !connected);
  s.textContent = connected ? 'Connected' : 'Not connected';
}

document.addEventListener('DOMContentLoaded', () => {
  wireControls();
  buildKeyboard();
  octLbl.textContent = baseOct;
  ws = initWS();
});
</script>
</body>
</html>)HTML";
static const size_t INDEX_HTML_LEN = sizeof(INDEX_HTML) - 1;
