#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------
//  Main page: alarms, music, settings. Served at http://alarmclock.local/
// ---------------------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="Alarm Clock">
<meta name="theme-color" content="#15120c">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="manifest" href="/manifest.json">
<title>Alarm Clock</title>
<style>
:root{
  --bg:#15120c;--card:#211c14;--card2:#2b2419;--line:#3a3124;
  --text:#f2eee4;--muted:#a39a8a;--accent:#ff453a;--accent-ink:#ffffff;
  --danger:#ff8a80;--ok:#5fd38a;--radius:16px;
}
@media (prefers-color-scheme: light){
  :root{--bg:#f6f2ea;--card:#ffffff;--card2:#f1ebdf;--line:#e2d9c8;
    --text:#1f1a12;--muted:#7a705f;--accent:#d70015;--accent-ink:#ffffff;}
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--text);
  font:16px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
body{padding:env(safe-area-inset-top) 16px calc(24px + env(safe-area-inset-bottom))}
.wrap{max-width:560px;margin:0 auto}
.hero{text-align:center;padding:28px 0 18px}
.clock{font-size:64px;font-weight:700;letter-spacing:-1px;font-variant-numeric:tabular-nums;line-height:1}
.clock small{font-size:22px;font-weight:600;margin-left:6px;color:var(--muted)}
.date{color:var(--muted);margin-top:8px}
.next{display:inline-block;margin-top:12px;padding:6px 14px;border-radius:999px;
  background:var(--card);color:var(--accent);font-weight:600;font-size:15px}
.hidden{display:none!important}
.ring{background:var(--accent);color:var(--accent-ink);border-radius:var(--radius);
  padding:18px;margin-bottom:16px;text-align:center}
.ring h2{margin:0 0 4px;font-size:22px}
.ring p{margin:0 0 14px;opacity:.85}
.ring .btn{background:rgba(0,0,0,.18);color:inherit;border:0}
.ring .btn.primary{background:var(--accent-ink);color:var(--accent)}
.ring .btn{background:rgba(0,0,0,.22)}
.tabs{display:flex;gap:6px;background:var(--card);padding:5px;border-radius:14px;margin-bottom:16px;
  position:sticky;top:calc(env(safe-area-inset-top) + 6px);z-index:5}
.tabs button{flex:1;border:0;background:none;color:var(--muted);font:inherit;font-weight:600;
  padding:10px 0;border-radius:10px}
.tabs button.on{background:var(--card2);color:var(--text)}
.card{background:var(--card);border-radius:var(--radius);padding:16px;margin-bottom:12px}
.card h3{margin:0 0 12px;font-size:15px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted)}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;
  min-height:46px;padding:0 18px;border-radius:12px;border:1px solid var(--line);
  background:var(--card2);color:var(--text);font:inherit;font-weight:600;cursor:pointer}
.btn.primary{background:var(--accent);border-color:var(--accent);color:var(--accent-ink)}
.btn.danger{color:var(--danger)}
.btn.wide{width:100%}
.btn.small{min-height:36px;padding:0 12px;font-size:14px}
.btn:disabled{opacity:.5}
.row{display:flex;gap:10px}
.row .btn{flex:1}
.alarm{display:flex;align-items:center;gap:12px;background:var(--card);border-radius:var(--radius);
  padding:14px 16px;margin-bottom:10px;cursor:pointer}
.alarm .info{flex:1;min-width:0}
.alarm .t{font-size:32px;font-weight:700;font-variant-numeric:tabular-nums;line-height:1.1}
.alarm .t small{font-size:16px;color:var(--muted);margin-left:4px}
.alarm .d{color:var(--muted);font-size:14px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.alarm.off .t{color:var(--muted)}
.empty{text-align:center;color:var(--muted);padding:24px 0}
.switch{position:relative;width:52px;height:32px;flex:none}
.switch input{opacity:0;width:0;height:0}
.switch span{position:absolute;inset:0;background:var(--line);border-radius:999px;transition:.2s}
.switch span:before{content:"";position:absolute;width:26px;height:26px;left:3px;top:3px;
  background:#fff;border-radius:50%;transition:.2s}
.switch input:checked+span{background:var(--accent)}
.switch input:checked+span:before{transform:translateX(20px)}
.song{display:flex;align-items:center;gap:10px;padding:12px 0;border-bottom:1px solid var(--line)}
.song:last-child{border-bottom:0}
.song .n{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.song .s{color:var(--muted);font-size:13px}
.icon-btn{width:40px;height:40px;border-radius:10px;border:1px solid var(--line);
  background:var(--card2);color:var(--text);font-size:15px;flex:none;cursor:pointer}
.icon-btn.del{color:var(--danger)}
.bar{height:8px;background:var(--card2);border-radius:99px;overflow:hidden;margin:4px 0 6px}
.bar div{height:100%;background:var(--accent);width:0}
.muted{color:var(--muted)}
.small{font-size:13px}
.field{display:block;margin:14px 0}
.field>span{display:flex;justify-content:space-between;margin-bottom:6px}
input[type=range]{width:100%;accent-color:var(--accent)}
select,input[type=text],input[type=password],input[type=search]{width:100%;min-height:46px;padding:0 12px;border-radius:12px;
  border:1px solid var(--line);background:var(--card2);color:var(--text);font:inherit}
.two{display:flex;gap:10px;align-items:center}
.two select{flex:1}
.check-row{display:flex;justify-content:space-between;align-items:center;margin:14px 0}
.upload{font-size:14px;margin-top:10px}
.upload .bar{height:6px}
.now{display:flex;align-items:center;gap:10px}
.vol{display:flex;align-items:center;gap:10px;margin-top:12px;font-size:14px;font-weight:600}
.vol input{flex:1}
.vol b{min-width:42px;text-align:right;font-variant-numeric:tabular-nums}
.ring .vol{margin-top:14px}
.ring input[type=range]{accent-color:var(--accent-ink)}
.search{position:relative;margin-bottom:4px}
.search input{padding-left:38px}
.search:before{content:"";position:absolute;left:14px;top:15px;width:11px;height:11px;
  border:2px solid var(--muted);border-radius:50%}
.search:after{content:"";position:absolute;left:27px;top:28px;width:7px;height:2px;
  background:var(--muted);transform:rotate(45deg)}
input[type=search]{-webkit-appearance:none;appearance:none}
.now span{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sheet{position:fixed;inset:0;background:rgba(0,0,0,.5);display:flex;align-items:flex-end;z-index:20}
.sheet-body{background:var(--card);width:100%;max-width:560px;margin:0 auto;border-radius:20px 20px 0 0;
  padding:20px 16px calc(20px + env(safe-area-inset-bottom))}
.sheet h3{margin:0 0 14px;text-align:center}
.time-input{width:100%;font-size:40px;font-weight:700;text-align:center;min-height:70px;
  border-radius:14px;border:1px solid var(--line);background:var(--card2);color:var(--text);margin-bottom:14px}
.days{display:flex;gap:6px;margin-bottom:6px}
.days button{flex:1;aspect-ratio:1;max-height:46px;border-radius:50%;border:1px solid var(--line);
  background:var(--card2);color:var(--muted);font:inherit;font-weight:700}
.days button.on{background:var(--accent);border-color:var(--accent);color:var(--accent-ink)}
.presets{display:flex;gap:6px;margin:8px 0 14px}
.presets button{flex:1;border:0;background:none;color:var(--accent);font:inherit;font-size:14px;font-weight:600}
.sheet input[type=text]{margin-bottom:14px}
.toast{position:fixed;left:50%;bottom:calc(24px + env(safe-area-inset-bottom));transform:translateX(-50%) translateY(120px);
  background:var(--text);color:var(--bg);padding:12px 18px;border-radius:12px;font-weight:600;
  transition:transform .25s;z-index:30;max-width:90%;text-align:center}
.toast.show{transform:translateX(-50%) translateY(0)}
.offline{background:var(--danger);color:#fff;text-align:center;padding:8px;border-radius:10px;margin-top:10px;font-size:14px}
</style>
</head>
<body>
<div class="wrap">
  <header class="hero">
    <div class="clock" id="clock">--:--</div>
    <div class="date" id="date">Connecting...</div>
    <div class="next hidden" id="next"></div>
    <div class="offline hidden" id="offline">Can't reach the clock. Is it plugged in?</div>
  </header>

  <div class="ring hidden" id="ring">
    <h2 id="ringTitle">Wake up!</h2>
    <p id="ringSub"></p>
    <div class="row">
      <button class="btn" id="btnSnooze">Snooze</button>
      <button class="btn primary" id="btnStop">Turn off</button>
    </div>
    <label class="vol" id="ringVolRow">Volume <input type="range" class="liveVol" min="5" max="100"><b class="liveVolOut"></b></label>
  </div>

  <nav class="tabs">
    <button data-tab="alarms" class="on">Alarms</button>
    <button data-tab="music">Music</button>
    <button data-tab="settings">Settings</button>
  </nav>

  <section id="tab-alarms">
    <div id="alarmList"></div>
    <button class="btn wide" id="addAlarm">+ Add alarm</button>
  </section>

  <section id="tab-music" class="hidden">
    <div class="card">
      <h3>Songs</h3>
      <div class="bar"><div id="storeBar"></div></div>
      <div class="muted small" id="storeText"></div>
      <label class="btn wide primary" style="margin-top:14px">Upload songs
        <input id="fileInput" type="file" accept=".mp3,audio/mpeg" multiple hidden>
      </label>
      <div id="uploads"></div>
      <p class="muted small" style="margin-bottom:0">MP3 files only. On iPhone, save them to the Files app first.
        A different song plays each morning until every song has played once.</p>
    </div>
    <div class="card hidden" id="nowBar">
      <div class="now">
        <span id="nowText"></span>
        <button class="btn small" id="btnStopPreview">Stop</button>
      </div>
      <label class="vol">Volume <input type="range" class="liveVol" min="5" max="100"><b class="liveVolOut"></b></label>
    </div>
    <div class="card" id="songCard">
      <div class="search"><input type="search" id="songSearch" placeholder="Search songs" autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false" aria-label="Search songs"></div>
      <div class="muted small hidden" id="searchInfo" style="margin-top:8px"></div>
      <div id="songList"></div>
    </div>
  </section>

  <section id="tab-settings" class="hidden">
    <div class="card">
      <h3>Alarm sound</h3>
      <label class="field"><span>Loudest volume <b id="volOut" class="liveVolOut"></b></span>
        <input type="range" id="setVolume" class="liveVol" min="5" max="100"></label>
      <p class="muted small" style="margin:-6px 0 0">Changes right away, even while a song is playing.</p>
      <label class="field"><span>Fade in</span>
        <select id="setRamp">
          <option value="0">Off, start at full volume</option>
          <option value="15">Over 15 seconds</option>
          <option value="30">Over 30 seconds</option>
          <option value="60">Over 1 minute</option>
          <option value="120">Over 2 minutes</option>
        </select></label>
      <label class="field"><span>Snooze length</span>
        <select id="setSnooze">
          <option value="5">5 minutes</option><option value="9">9 minutes</option>
          <option value="10">10 minutes</option><option value="15">15 minutes</option>
          <option value="20">20 minutes</option>
        </select></label>
      <button class="btn wide" id="btnTest">Save and test the alarm</button>
    </div>
    <div class="card">
      <h3>Screen</h3>
      <label class="check-row">24-hour clock
        <span class="switch"><input type="checkbox" id="set24"><span></span></span></label>
      <label class="field"><span>Daytime brightness</span>
        <input type="range" id="setDay" min="10" max="255"></label>
      <label class="field"><span>Night brightness</span>
        <input type="range" id="setNight" min="0" max="255"></label>
      <div class="field"><span>Night time</span>
        <div class="two"><select id="setNightStart"></select><span class="muted">to</span><select id="setNightEnd"></select></div>
      </div>
    </div>
    <button class="btn wide primary" id="saveSettings">Save settings</button>
    <div class="card" style="margin-top:12px">
      <h3>Device</h3>
      <div class="muted small" id="devInfo"></div>
      <div class="muted small" id="updStatus" style="margin-top:10px"></div>
      <button class="btn wide" id="btnUpdate" style="margin-top:12px">Check for updates</button>
      <div class="row" style="margin-top:10px">
        <button class="btn" id="btnWifi">Change Wi-Fi</button>
        <label class="btn">Install file<input id="fwInput" type="file" accept=".bin" hidden></label>
      </div>
      <div class="muted small" id="fwStatus" style="margin-top:8px"></div>
      <p class="muted small" style="margin-bottom:0">Updates install by themselves at night. They never run while an alarm is going off or within an hour of one.</p>
    </div>
  </section>
</div>

<div class="sheet hidden" id="editor">
  <div class="sheet-body">
    <h3 id="edTitle">New alarm</h3>
    <input type="time" class="time-input" id="edTime" value="07:00">
    <div class="days" id="edDays"></div>
    <div class="presets">
      <button data-days="62">Weekdays</button><button data-days="65">Weekends</button>
      <button data-days="127">Every day</button><button data-days="0">Once</button>
    </div>
    <input type="text" id="edLabel" placeholder="Label (optional)" maxlength="30">
    <div class="row">
      <button class="btn danger" id="edDelete">Delete</button>
      <button class="btn" id="edCancel">Cancel</button>
      <button class="btn primary" id="edSave">Save</button>
    </div>
  </div>
</div>
<div class="toast" id="toast"></div>

<script>
const $ = s => document.querySelector(s);
const DAYS = ["S","M","T","W","T","F","S"], DAYN = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"];
let alarms = [], settings = {}, st = {}, editing = -1, edDays = 0, base = 0, baseAt = 0, failCount = 0;

const pad = n => String(n).padStart(2, "0");
const esc = s => String(s).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));

async function api(path, body) {
  const opt = body === undefined ? {} :
    {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body)};
  const r = await fetch(path, opt);
  const j = await r.json().catch(() => ({ok: false, error: "Unexpected reply"}));
  if (!r.ok || j.ok === false) throw new Error(j.error || ("Error " + r.status));
  return j;
}
function toast(msg) {
  const t = $("#toast"); t.textContent = msg; t.classList.add("show");
  clearTimeout(t._h); t._h = setTimeout(() => t.classList.remove("show"), 2600);
}
function fmtHM(h, m, parts) {
  if (st.use24h) return parts ? [pad(h) + ":" + pad(m), ""] : pad(h) + ":" + pad(m);
  const t = ((h % 12) || 12) + ":" + pad(m), ap = h < 12 ? "AM" : "PM";
  return parts ? [t, ap] : t + " " + ap;
}
function daysText(d) {
  if (!d) return "Once";
  if (d === 127) return "Every day";
  if (d === 62) return "Weekdays";
  if (d === 65) return "Weekends";
  return DAYN.filter((_, i) => d & (1 << i)).join(", ");
}
function localDate(sec) { return new Date(sec * 1000); }  // read with getUTC*()

// ---------- status / clock ----------
function tick() {
  if (!st.valid) return;
  const d = localDate(base + Math.floor((Date.now() - baseAt) / 1000));
  const [t, ap] = fmtHM(d.getUTCHours(), d.getUTCMinutes(), true);
  $("#clock").innerHTML = esc(t) + (ap ? "<small>" + ap + "</small>" : "");
  $("#date").textContent = d.toLocaleDateString(undefined,
    {weekday: "long", month: "long", day: "numeric", timeZone: "UTC"});
}
function nextText() {
  if (!st.nextLocal) return "";
  const a = localDate(st.nextLocal), n = localDate(st.local);
  const dayDiff = Math.round((Date.UTC(a.getUTCFullYear(), a.getUTCMonth(), a.getUTCDate()) -
                              Date.UTC(n.getUTCFullYear(), n.getUTCMonth(), n.getUTCDate())) / 86400000);
  const when = dayDiff === 0 ? "Today" : dayDiff === 1 ? "Tomorrow" : DAYN[a.getUTCDay()];
  return "⏰ " + when + " " + fmtHM(a.getUTCHours(), a.getUTCMinutes());
}
async function poll() {
  try {
    st = await api("/api/status");
    failCount = 0;
    $("#offline").classList.add("hidden");
    base = st.local; baseAt = Date.now();
    if (!st.valid) { $("#clock").textContent = "--:--"; $("#date").textContent = "Waiting for the time..."; }
    tick();
    const nt = nextText();
    $("#next").textContent = nt; $("#next").classList.toggle("hidden", !nt);

    const ring = $("#ring");
    if (st.state === "ringing") {
      ring.classList.remove("hidden");
      $("#ringTitle").textContent = st.label || "Wake up!";
      $("#ringSub").textContent = st.nowPlaying ? "Playing " + st.nowPlaying : "";
      $("#btnSnooze").classList.remove("hidden");
    } else if (st.state === "snoozed") {
      ring.classList.remove("hidden");
      const s = localDate(st.snoozeLocal);
      $("#ringTitle").textContent = "Snoozing";
      $("#ringSub").textContent = "Rings again at " + fmtHM(s.getUTCHours(), s.getUTCMinutes());
      $("#btnSnooze").classList.add("hidden");
    } else ring.classList.add("hidden");

    $("#ringVolRow").classList.toggle("hidden", st.state !== "ringing");
    if (typeof st.volume === "number") showVolume(st.volume, false);
    $("#nowBar").classList.toggle("hidden", st.state !== "preview");
    $("#nowText").textContent = "Playing: " + (st.nowPlaying || "");
    $("#devInfo").innerHTML =
      "Wi-Fi: " + esc(st.ssid || "-") + (st.rssi ? " (" + st.rssi + " dBm)" : "") +
      "<br>Address: http://alarmclock.local (" + esc(st.ip || "-") + ")" +
      "<br>Time source: " + (st.ntp ? "internet" : st.rtc ? "backup clock chip" : "not set yet") +
      (st.rtc ? "" : " - no RTC chip found") +
      "<br>SD card: " + (st.sdOk ? "OK" : "not found") + " - Firmware v" + esc(st.version);
    if (st.update) {
      const u = st.update;
      $("#updStatus").textContent = u.progress >= 0 ? "Installing update: " + u.progress + "%" : u.status;
      $("#btnUpdate").disabled = !!u.busy;
      $("#btnUpdate").textContent = u.busy ? "Checking..." : "Check for updates";
    }
  } catch (e) {
    if (++failCount >= 2) $("#offline").classList.remove("hidden");
  }
}

// ---------- alarms ----------
function renderAlarms() {
  const list = $("#alarmList");
  if (!alarms.length) { list.innerHTML = '<div class="empty">No alarms yet</div>'; return; }
  list.innerHTML = alarms.map((a, i) => {
    const [t, ap] = fmtHM(a.hour, a.minute, true);
    return '<div class="alarm' + (a.enabled ? "" : " off") + '" data-i="' + i + '">' +
      '<div class="info"><div class="t">' + t + (ap ? "<small>" + ap + "</small>" : "") + '</div>' +
      '<div class="d">' + esc(a.label ? a.label + " · " : "") + daysText(a.days) + '</div></div>' +
      '<label class="switch" onclick="event.stopPropagation()"><input type="checkbox" data-t="' + i + '"' +
      (a.enabled ? " checked" : "") + '><span></span></label></div>';
  }).join("");
  list.querySelectorAll(".alarm").forEach(el => el.onclick = () => openEditor(+el.dataset.i));
  list.querySelectorAll("input[data-t]").forEach(el => el.onchange = async () => {
    alarms[+el.dataset.t].enabled = el.checked;
    await saveAlarms(el.checked ? "Alarm on" : "Alarm off");
  });
  $("#addAlarm").disabled = alarms.length >= 8;
}
async function loadAlarms() {
  try { alarms = (await api("/api/alarms")).alarms; renderAlarms(); } catch (e) { toast(e.message); }
}
async function saveAlarms(msg) {
  try {
    alarms = (await api("/api/alarms", alarms)).alarms;
    renderAlarms(); poll(); if (msg) toast(msg);
  } catch (e) { toast(e.message); loadAlarms(); }
}
function renderDays() {
  $("#edDays").innerHTML = DAYS.map((d, i) =>
    '<button data-d="' + i + '" class="' + (edDays & (1 << i) ? "on" : "") + '">' + d + "</button>").join("");
  $("#edDays").querySelectorAll("button").forEach(b => b.onclick = () => { edDays ^= 1 << +b.dataset.d; renderDays(); });
}
function openEditor(i) {
  editing = i;
  const a = i >= 0 ? alarms[i] : {hour: 7, minute: 0, days: 62, label: "", enabled: true};
  $("#edTitle").textContent = i >= 0 ? "Edit alarm" : "New alarm";
  $("#edTime").value = pad(a.hour) + ":" + pad(a.minute);
  $("#edLabel").value = a.label || "";
  edDays = a.days; renderDays();
  $("#edDelete").classList.toggle("hidden", i < 0);
  $("#editor").classList.remove("hidden");
}
function closeEditor() { $("#editor").classList.add("hidden"); }
document.querySelectorAll(".presets button").forEach(b => b.onclick = () => { edDays = +b.dataset.days; renderDays(); });
$("#addAlarm").onclick = () => openEditor(-1);
$("#edCancel").onclick = closeEditor;
$("#editor").onclick = e => { if (e.target.id === "editor") closeEditor(); };
$("#edSave").onclick = async () => {
  const [h, m] = ($("#edTime").value || "07:00").split(":").map(Number);
  const a = {hour: h, minute: m, days: edDays, label: $("#edLabel").value.trim(), enabled: true};
  if (editing >= 0) alarms[editing] = a; else alarms.push(a);
  closeEditor();
  await saveAlarms("Alarm saved");
};
$("#edDelete").onclick = async () => {
  if (editing < 0) return;
  alarms.splice(editing, 1); closeEditor();
  await saveAlarms("Alarm deleted");
};

// ---------- music ----------
const mb = b => b >= 1073741824 ? (b / 1073741824).toFixed(1) + " GB"
  : (b / 1048576).toFixed(b < 10485760 ? 1 : 0) + " MB";
let songs = [];
// Compare letters and digits only, so "dont stop" also finds "Don_t Stop.mp3"
// (curly apostrophes become "_" when a song is uploaded) and "mr brightside"
// finds "Mr. Brightside"
const squash = s => s.toLowerCase().replace(/\.mp3$/, "").replace(/[^a-z0-9]+/g, "");
function renderSongs() {
  const raw = $("#songSearch").value.trim();
  const words = raw.split(/\s+/).map(squash).filter(Boolean);
  const shown = songs.map((s, i) => ({s, i})).filter(({s}) => {
    const n = squash(s.name);
    return words.every(w => n.includes(w));
  });
  const q = raw;
  const info = $("#searchInfo");
  info.classList.toggle("hidden", !q);
  info.textContent = shown.length ? shown.length + " of " + songs.length + " songs" : "No songs match \u201c" + $("#songSearch").value.trim() + "\u201d";
  const list = $("#songList");
  list.innerHTML = shown.map(({s, i}) =>
    '<div class="song"><div class="n">' + esc(s.name.replace(/\.mp3$/i, "")) +
    '<div class="s">' + mb(s.size) + '</div></div>' +
    '<button class="icon-btn" data-play="' + i + '" aria-label="Play">&#9654;</button>' +
    '<button class="icon-btn del" data-del="' + i + '" aria-label="Delete">&#10005;</button></div>').join("");
  list.querySelectorAll("[data-play]").forEach(b => b.onclick = async () => {
    try { await api("/api/preview", {name: songs[+b.dataset.play].name}); setTimeout(poll, 400); }
    catch (e) { toast(e.message); }
  });
  list.querySelectorAll("[data-del]").forEach(b => b.onclick = async () => {
    const s = songs[+b.dataset.del];
    if (!confirm('Delete "' + s.name.replace(/\.mp3$/i, "") + '"?')) return;
    try { await api("/api/song-delete", {name: s.name}); toast("Deleted"); loadSongs(); }
    catch (e) { toast(e.message); }
  });
}
async function loadSongs() {
  try {
    const j = await api("/api/songs");
    const pct = j.total ? Math.min(100, j.used / j.total * 100) : 0;
    $("#storeBar").style.width = pct + "%";
    $("#storeText").textContent = !j.sdOk ? "No SD card found. Songs can't be added until one is inserted."
      : j.songs.length + " song" + (j.songs.length === 1 ? "" : "s") + " · " + mb(j.total - j.used) + " free";
    $("#fileInput").disabled = !j.sdOk;
    songs = j.songs;
    $("#songCard").classList.toggle("hidden", !songs.length);
    renderSongs();
  } catch (e) { toast(e.message); }
}
$("#songSearch").oninput = renderSongs;
function uploadOne(file, row) {
  return new Promise(resolve => {
    const xhr = new XMLHttpRequest(), fd = new FormData();
    fd.append("file", file, file.name);
    xhr.upload.onprogress = e => {
      if (e.lengthComputable) row.querySelector(".bar div").style.width = (e.loaded / e.total * 100) + "%";
    };
    xhr.onload = () => {
      let j = {}; try { j = JSON.parse(xhr.responseText); } catch (e) {}
      const ok = xhr.status === 200 && j.ok;
      row.querySelector(".lbl").textContent = (ok ? "✓ " : "✗ ") + file.name + (ok ? "" : " - " + (j.error || "failed"));
      resolve(ok);
    };
    xhr.onerror = () => { row.querySelector(".lbl").textContent = "✗ " + file.name + " - connection lost"; resolve(false); };
    xhr.open("POST", "/api/upload");
    xhr.send(fd);
  });
}
$("#fileInput").onchange = async e => {
  const files = [...e.target.files]; e.target.value = "";
  const box = $("#uploads"); box.innerHTML = "";
  let okCount = 0;
  for (const f of files) {
    const row = document.createElement("div");
    row.className = "upload";
    row.innerHTML = '<div class="lbl"></div><div class="bar"><div></div></div>';
    row.querySelector(".lbl").textContent = f.name;
    box.appendChild(row);
    if (await uploadOne(f, row)) okCount++;
    loadSongs();
  }
  toast(okCount + " of " + files.length + " uploaded");
};
$("#btnStopPreview").onclick = async () => { await api("/api/preview-stop", {}).catch(() => {}); poll(); };

// ---------- settings ----------
function fillHours(sel) {
  sel.innerHTML = [...Array(24).keys()].map(h => '<option value="' + h + '">' + fmtHM(h, 0) + "</option>").join("");
}
async function loadSettings() {
  try {
    settings = (await api("/api/settings")).settings;
    st.use24h = settings.use24h;
    fillHours($("#setNightStart")); fillHours($("#setNightEnd"));
    showVolume(settings.volume, true);
    $("#setRamp").value = settings.rampSec; $("#setSnooze").value = settings.snoozeMin;
    $("#set24").checked = settings.use24h;
    $("#setDay").value = settings.dayBright; $("#setNight").value = settings.nightBright;
    $("#setNightStart").value = settings.nightStart; $("#setNightEnd").value = settings.nightEnd;
  } catch (e) { toast(e.message); }
}
function readSettings() {
  return {volume: +$("#setVolume").value, rampSec: +$("#setRamp").value, snoozeMin: +$("#setSnooze").value,
    use24h: $("#set24").checked, dayBright: +$("#setDay").value, nightBright: +$("#setNight").value,
    nightStart: +$("#setNightStart").value, nightEnd: +$("#setNightEnd").value};
}
async function saveSettings(quiet) {
  try {
    settings = (await api("/api/settings", readSettings())).settings;
    st.use24h = settings.use24h; renderAlarms(); tick();
    if (!quiet) toast("Settings saved");
    return true;
  } catch (e) { toast(e.message); return false; }
}
// ---------- live volume ----------
// Every volume slider (ringing card, now-playing bar, settings) is the same
// setting. Moving one saves it and changes the sound straight away.
let volDragging = false, volSending = false, volPending = null, volHoldUntil = 0;
function showVolume(v, force) {
  if (!force && (volDragging || volSending || Date.now() < volHoldUntil)) return;  // don't fight the finger
  document.querySelectorAll(".liveVol").forEach(el => { if (!(volDragging && el === document.activeElement)) el.value = v; });
  document.querySelectorAll(".liveVolOut").forEach(el => el.textContent = v + "%");
  settings.volume = v;
}
async function sendVolume(v) {
  volPending = v;
  if (volSending) return;             // the loop below sends the newest value next
  volSending = true;
  while (volPending !== null) {
    const x = volPending; volPending = null;
    try { await api("/api/volume", {volume: x}); } catch (e) { toast(e.message); break; }
  }
  volSending = false;
  volHoldUntil = Date.now() + 1500;   // let the next status poll catch up
}
document.querySelectorAll(".liveVol").forEach(el => {
  el.addEventListener("input", () => {
    volDragging = true;
    const v = +el.value;
    document.querySelectorAll(".liveVol").forEach(o => { if (o !== el) o.value = v; });
    document.querySelectorAll(".liveVolOut").forEach(o => o.textContent = v + "%");
    settings.volume = v;
    sendVolume(v);
  });
  el.addEventListener("change", () => { volDragging = false; sendVolume(+el.value); });
});
$("#saveSettings").onclick = () => saveSettings(false);
$("#btnTest").onclick = async () => {
  if (!await saveSettings(true)) return;
  try { await api("/api/test-alarm", {}); toast("Ringing now. Use Snooze or Turn off."); setTimeout(poll, 400); }
  catch (e) { toast(e.message); }
};
$("#btnSnooze").onclick = async () => { await api("/api/snooze", {}).catch(e => toast(e.message)); poll(); };
$("#btnStop").onclick = async () => { await api("/api/stop", {}).catch(e => toast(e.message)); poll(); };
$("#btnUpdate").onclick = async () => {
  try { await api("/api/update-check", {}); toast("Checking for updates..."); setTimeout(poll, 800); }
  catch (e) { toast(e.message); }
};
$("#btnWifi").onclick = async () => {
  if (!confirm("The clock will forget its Wi-Fi and restart into setup mode. Follow the steps on its screen to pick a new network. Continue?")) return;
  try { await api("/api/wifi-forget", {}); toast("Restarting into Wi-Fi setup..."); } catch (e) { toast(e.message); }
};
$("#fwInput").onchange = e => {
  const f = e.target.files[0]; e.target.value = "";
  if (!f || !confirm("Install " + f.name + "? The clock restarts when it's done.")) return;
  const xhr = new XMLHttpRequest(), fd = new FormData(), out = $("#fwStatus");
  fd.append("firmware", f, f.name);
  xhr.upload.onprogress = ev => { if (ev.lengthComputable) out.textContent = "Uploading " + Math.round(ev.loaded / ev.total * 100) + "%"; };
  xhr.onload = () => {
    let j = {}; try { j = JSON.parse(xhr.responseText); } catch (err) {}
    if (j.ok) { out.textContent = "Installed. Restarting..."; setTimeout(() => location.reload(), 12000); }
    else out.textContent = "Update failed: " + (j.error || xhr.status);
  };
  xhr.onerror = () => out.textContent = "Upload failed";
  xhr.open("POST", "/api/update"); xhr.send(fd);
};

// ---------- tabs ----------
document.querySelectorAll(".tabs button").forEach(b => b.onclick = () => {
  document.querySelectorAll(".tabs button").forEach(x => x.classList.toggle("on", x === b));
  ["alarms", "music", "settings"].forEach(t => $("#tab-" + t).classList.toggle("hidden", t !== b.dataset.tab));
  if (b.dataset.tab === "music") loadSongs();
  if (b.dataset.tab === "settings") loadSettings();
});

(async () => {
  await poll();
  await loadSettings();
  await loadAlarms();
  setInterval(poll, 2500);
  setInterval(tick, 1000);
  document.addEventListener("visibilitychange", () => { if (!document.hidden) { poll(); loadAlarms(); } });
})();
</script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------
//  Wi-Fi setup page, shown on the "AlarmClock-Setup" hotspot
// ---------------------------------------------------------------------
const char SETUP_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Alarm Clock Wi-Fi setup</title>
<style>
:root{--bg:#15120c;--card:#211c14;--card2:#2b2419;--line:#3a3124;--text:#f2eee4;--muted:#a39a8a;--accent:#ff453a;--ink:#ffffff}
@media (prefers-color-scheme: light){:root{--bg:#f6f2ea;--card:#fff;--card2:#f1ebdf;--line:#e2d9c8;--text:#1f1a12;--muted:#7a705f;--accent:#d70015;--ink:#fff}}
*{box-sizing:border-box}
body{margin:0;padding:20px 16px 40px;background:var(--bg);color:var(--text);font:16px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:480px;margin:0 auto}
h1{font-size:26px;margin:8px 0 4px}
p{color:var(--muted);margin:0 0 18px}
.card{background:var(--card);border-radius:16px;padding:6px 0;margin-bottom:16px}
.net{display:flex;align-items:center;gap:10px;padding:14px 16px;border-bottom:1px solid var(--line);cursor:pointer}
.net:last-child{border-bottom:0}
.net span{flex:1}
.net.sel{color:var(--accent);font-weight:700}
.sig{color:var(--muted);font-size:13px}
.muted{color:var(--muted);text-align:center;padding:16px}
label{display:block;font-weight:600;margin:14px 0 6px}
input{width:100%;min-height:48px;padding:0 12px;border-radius:12px;border:1px solid var(--line);background:var(--card2);color:var(--text);font:inherit}
button{width:100%;min-height:50px;margin-top:18px;border:0;border-radius:12px;background:var(--accent);color:var(--ink);font:inherit;font-weight:700}
button:disabled{opacity:.5}
.show{display:flex;align-items:center;gap:8px;margin-top:10px;color:var(--muted);font-weight:400}
.show input{width:auto;min-height:0}
.done{background:var(--card);border-radius:16px;padding:20px}
.done h2{margin-top:0;color:var(--accent)}
.done ol{padding-left:20px;margin-bottom:0}
.done li{margin-bottom:8px}
</style>
</head>
<body>
<div class="wrap" id="app">
  <h1>Connect the alarm clock</h1>
  <p>Pick the home Wi-Fi network the clock should use.</p>
  <div class="card" id="nets"><div class="muted">Looking for networks...</div></div>
  <label for="ssid">Network name</label>
  <input id="ssid" autocapitalize="none" autocorrect="off" placeholder="Tap one above or type it">
  <label for="pass">Password</label>
  <input id="pass" type="password" placeholder="Leave empty if there isn't one">
  <label class="show"><input type="checkbox" id="showPass"> Show password</label>
  <button id="go">Connect</button>
</div>
<script>
const $ = s => document.querySelector(s);
const esc = s => String(s).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
let nets = [], timer;
function bars(r) { return r > -55 ? "Strong" : r > -70 ? "Good" : "Weak"; }
function render() {
  const box = $("#nets");
  if (!nets.length) { box.innerHTML = '<div class="muted">Looking for networks...</div>'; return; }
  box.innerHTML = nets.map((n, i) =>
    '<div class="net' + (n.ssid === $("#ssid").value ? " sel" : "") + '" data-i="' + i + '"><span>' +
    esc(n.ssid) + (n.lock ? " &#128274;" : "") + '</span><span class="sig">' + bars(n.rssi) + '</span></div>').join("") +
    '<div class="net" id="rescan"><span class="sig">Refresh list</span></div>';
  box.querySelectorAll(".net[data-i]").forEach(el => el.onclick = () => {
    $("#ssid").value = nets[+el.dataset.i].ssid; render(); $("#pass").focus();
  });
  $("#rescan").onclick = () => { fetch("/api/wifi-scan?refresh=1"); nets = []; render(); };
}
async function scan() {
  try {
    const r = await fetch("/api/wifi-scan"); const j = await r.json();
    if (j.length) { nets = j; render(); }
  } catch (e) {}
  timer = setTimeout(scan, nets.length ? 8000 : 2500);
}
$("#showPass").onchange = e => $("#pass").type = e.target.checked ? "text" : "password";
$("#go").onclick = async () => {
  const ssid = $("#ssid").value.trim(), pass = $("#pass").value;
  if (!ssid) { $("#ssid").focus(); return; }
  $("#go").disabled = true; $("#go").textContent = "Saving...";
  try {
    const r = await fetch("/api/wifi-save", {method: "POST", headers: {"Content-Type": "application/json"},
      body: JSON.stringify({ssid, pass})});
    const j = await r.json();
    if (!j.ok) throw new Error(j.error || "Failed");
    clearTimeout(timer);
    $("#app").innerHTML = '<div class="done"><h2>Saved!</h2><p>The clock is restarting and joining <b>' + esc(ssid) +
      '</b>.</p><ol><li>Close this page and switch your phone back to your home Wi-Fi.</li>' +
      '<li>Wait for the time to show on the clock.</li>' +
      '<li>Press <b>SNOOZE</b> on the clock and scan the code to set alarms and add music.</li></ol>' +
      '<p style="margin-top:14px">If the setup screen comes back, the password was probably wrong. Just try again.</p></div>';
  } catch (e) {
    $("#go").disabled = false; $("#go").textContent = "Connect"; alert(e.message);
  }
};
scan();
</script>
</body>
</html>
)HTML";

const char MANIFEST_JSON[] PROGMEM = R"JSON({
  "name": "Alarm Clock",
  "short_name": "Alarm Clock",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#15120c",
  "theme_color": "#15120c",
  "icons": [{"src": "/icon.png", "sizes": "180x180", "type": "image/png"}]
})JSON";
