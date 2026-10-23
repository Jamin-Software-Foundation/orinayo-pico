"use strict";

// Protocol: F0 7D 4F <cmd> [data...] F7  (Universal SysEx, non-commercial ID 0x7D). See README "Web controller".
const SYSEX_ID = 0x7D, DEV_ID = 0x4F;
const CMD_KEY = 0x01, CMD_REQUEST_STATE = 0x02, CMD_SET_VOLUME = 0x03, EVT_STATE = 0x10;
const NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const TYPES = ["", "min", "sus"];

const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];

let access = null, input = null, output = null, pollTimer = null;
const dragging = new Set();

function setConn(on, text) {
  $("#conn").className = "conn " + (on ? "on" : "off");
  $("#connText").textContent = text;
}

function notice(msg) {
  const n = $("#notice");
  n.hidden = !msg;
  n.textContent = msg || "";
}

function send(data) {
  if (!output) return false;
  output.send([0xF0, SYSEX_ID, DEV_ID, ...data, 0xF7]);
  return true;
}

const sendKey = (code) => send([CMD_KEY, code & 0x7F]);
const requestState = () => send([CMD_REQUEST_STATE]);
const setVolume = (track, v) => send([CMD_SET_VOLUME, track, v & 0x7F]);

function onMidi(ev) {
  const d = ev.data;
  if (d[0] !== 0xF0 || d[1] !== SYSEX_ID || d[2] !== DEV_ID || d[3] !== EVT_STATE || d.length < 15) return;
  render({
    group: d[4], key: d[5], section: d[6], chord_root: d[14], chord_bass: d[15], chord_type: d[16],
    started: !!(d[7] & 1), drums: !!(d[7] & 2), bass: !!(d[7] & 4), chords: !!(d[7] & 8),
    neck: d[8], pattern: d[9] - 1,
    vol: [d[10], d[11], d[12], d[13]],
  });
}

function render(s) {
  $("#styleGroup").textContent = s.group + 1;
  $("#styleSection").textContent = (s.section % 8) + 1;
  $$("#sectionLeds i").forEach((el, i) => el.classList.toggle("on", s.started && i === s.section % 4));
  $("#songKey").textContent = NOTES[s.key % 12];
  $("#activeChord").textContent = NOTES[s.chord_root % 12] + "<small>" + YPES[s.chord_type] + "</small>";
  
  if (s.chord_root != s.chord_bass) {
	  $("#activeChord").innerHTML = NOTES[s.chord_root % 12] + "<small>" + TYPES[s.chord_type] + "</small>/" + NOTES[s.chord_bass % 12];
  }
  
  const b = $("#startBtn");
  b.classList.toggle("running", s.started);
  $("#startText").textContent = s.started ? "STOP" : "START";

  $$("#patterns [data-pat]").forEach(el => el.classList.toggle("active", +el.dataset.pat === s.pattern));
  $$("#neck [data-neck]").forEach(el => el.classList.toggle("active", +el.dataset.neck === s.neck));

  const on = [s.drums, s.bass, s.chords];
  $$(".ch").forEach(ch => {
    const t = +ch.dataset.track;
    if (t < 3) $(".mute", ch).classList.toggle("muted", !on[t]);
    const r = $("input", ch);
    if (!dragging.has(r)) r.value = s.vol[t];
    $("output", ch).textContent = s.vol[t];
  });
}

function bindUi() {
  $$("[data-key]").forEach(el => {
    const press = (e) => { e.preventDefault(); el.classList.add("pressed"); sendKey(+el.dataset.key); if (navigator.vibrate) navigator.vibrate(10); };
    const release = () => el.classList.remove("pressed");
    el.addEventListener("pointerdown", press);
    ["pointerup", "pointerleave", "pointercancel"].forEach(t => el.addEventListener(t, release));
  });

  let last = 0;
  $$(".ch input").forEach(r => {
    const track = +r.closest(".ch").dataset.track;
    r.addEventListener("pointerdown", () => dragging.add(r));
    ["pointerup", "pointercancel"].forEach(t => r.addEventListener(t, () => dragging.delete(r)));
    r.addEventListener("input", () => {
      $("output", r.parentElement).textContent = r.value;
      const now = performance.now();
      if (now - last > 40) { last = now; setVolume(track, +r.value); }
    });
    r.addEventListener("change", () => setVolume(track, +r.value));
  });

  $("#connectBtn").addEventListener("click", connect);
  $("#conn").addEventListener("click", () => access && $("#devDialog").showModal());
  $("#inSel").addEventListener("change", () => { localStorage.setItem("midiIn", $("#inSel").value); pickPorts(); });
  $("#outSel").addEventListener("change", () => { localStorage.setItem("midiOut", $("#outSel").value); pickPorts(); });
}

function fillSelect(sel, ports, saved) {
  sel.innerHTML = "";
  const list = [...ports.values()];
  list.forEach(p => sel.add(new Option(p.name, p.id)));
  const pick = list.find(p => p.id === saved) || list.find(p => /orinayo|pico/i.test(p.name)) || list[0];
  if (pick) sel.value = pick.id;
}

function pickPorts() {
  if (input) input.onmidimessage = null;
  input = access.inputs.get($("#inSel").value) || null;
  output = access.outputs.get($("#outSel").value) || null;
  if (input) input.onmidimessage = onMidi;

  clearInterval(pollTimer);
  if (input && output) {
    setConn(true, output.name);
    requestState();
    pollTimer = setInterval(requestState, 3000);
  } else {
    setConn(false, access.outputs.size ? "Select ports" : "No MIDI device");
  }
}

function refreshPorts() {
  fillSelect($("#inSel"), access.inputs, localStorage.getItem("midiIn"));
  fillSelect($("#outSel"), access.outputs, localStorage.getItem("midiOut"));
  pickPorts();
}

async function connect() {
  if (!navigator.requestMIDIAccess) {
    notice("WebMIDI is not supported in this browser. Use Chrome, Edge or Opera (Android or desktop).");
    return;
  }
  try {
    access = await navigator.requestMIDIAccess({ sysex: true });
    notice("");
    access.onstatechange = refreshPorts;
    refreshPorts();
    $("#connectBtn").hidden = true;
  } catch (e) {
    notice("MIDI access denied: " + e.message);
  }
}

bindUi();
if (!navigator.requestMIDIAccess) notice("WebMIDI is not supported in this browser. Use Chrome, Edge or Opera.");
else if (navigator.permissions) {
  // Auto-connect when the user already granted SysEx access.
  navigator.permissions.query({ name: "midi", sysex: true }).then(p => { if (p.state === "granted") connect(); }).catch(() => {});
}

if ("serviceWorker" in navigator) navigator.serviceWorker.register("sw.js").catch(() => {});
