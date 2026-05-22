#include "secrets.h"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Pet Tracker</title>
<link href='https://api.mapbox.com/mapbox-gl-js/v3.3.0/mapbox-gl.css' rel='stylesheet' />
<script src='https://api.mapbox.com/mapbox-gl-js/v3.3.0/mapbox-gl.js'></script>
<style>
  body { margin: 0; font-family: monospace; background: #111; color: #eee; display: flex; height: 100vh; }
  #sidebar { width: 260px; padding: 12px; background: #1a1a1a; border-right: 1px solid #333; display: flex; flex-direction: column; gap: 10px; overflow-y: auto; }
  #map { flex: 1; }
  button { width: 100%; padding: 8px; background: #222; color: #eee; border: 1px solid #555; cursor: pointer; font-family: monospace; }
  button:hover { background: #333; }
  .card { background: #222; border: 1px solid #333; padding: 8px; font-size: 12px; }
  .card b { color: #0af; }
  h3 { margin: 0; font-size: 13px; color: #aaa; border-bottom: 1px solid #333; padding-bottom: 4px; }
  #status { font-size: 12px; color: #fa0; }
  .hist { font-size: 11px; padding: 4px; cursor: pointer; border-bottom: 1px solid #2a2a2a; }
  .hist:hover { background: #2a2a2a; }
</style>
</head>
<body>
<div id="sidebar">
  <h2 style="margin:0;font-size:14px;">Pet Tracker</h2>
  <div id="status">Monitoring</div>
  <div id="node-count" style="font-size:11px;color:#888;">0 nodes active</div>
  <button id="startBtn" onclick="startRun()">Start Session</button>
  <button id="stopBtn" onclick="stopRun()" style="display:none;border-color:#f55;color:#f55;">Stop + Save</button>
  <h3>Live Nodes</h3>
  <div id="tracker-list"></div>
  <h3>Saved Runs</h3>
  <div id="history-list"></div>
</div>
<div style="position:relative;flex:1;">
  <div id="map" style="width:100%;height:100%;"></div>
  <div style="position:absolute;top:10px;left:10px;z-index:10;display:flex;gap:4px;">
    <button onclick="setStyle('standard')" style="width:auto;padding:6px 10px;">Street</button>
    <button onclick="setStyle('satellite')" style="width:auto;padding:6px 10px;">Satellite</button>
  </div>
</div>

<script>
  mapboxgl.accessToken = ")rawliteral"
MAPBOX_TOKEN
R"rawliteral(";

  var map = new mapboxgl.Map({
    container: 'map',
    style: 'mapbox://styles/mapbox/standard',
    center: [-87.3243, 39.4833],
    zoom: 15
  });
  map.addControl(new mapboxgl.NavigationControl(), 'bottom-right');

  var colors    = ['#0af', '#0f9', '#fa0', '#f55'];
  var names     = ['Bryce', 'Ely', 'Node 2', 'Node 3'];
  var behaviors = ['Idle', 'Walking', 'Running', 'Sitting', 'Rolling'];
  var styles    = { standard: 'mapbox://styles/mapbox/standard', satellite: 'mapbox://styles/mapbox/standard-satellite' };

  function toCardinal(deg) {
    return ['N','NE','E','SE','S','SW','W','NW'][Math.round(deg / 45) % 8];
  }

  // ── State ──────────────────────────────────────────────────────
  var liveMarkers  = {};    // id → Marker, always on map when GPS valid
  var recordPoints = {};    // id → [[lat,lng],...], current run, memory only
  var histOverlay  = null;  // { sources:[], layers:[], markers:[] } or null
  var viewRunId    = null;  // run ID currently shown as overlay
  var followId     = null;
  var isRunActive  = false;
  var currentRunId = '';
  var lastSeen     = {};
  var activeStyle  = 'standard';
  var mapLoaded    = false;

  map.on('load', function() { mapLoaded = true; });

  // ── Map helpers ────────────────────────────────────────────────
  function safeRemoveLayer(id)  { if (map.getLayer(id))  map.removeLayer(id);  }
  function safeRemoveSource(id) { if (map.getSource(id)) map.removeSource(id); }

  function addLineToMap(srcId, lyrId, coords, color, dash) {
    map.addSource(srcId, { type: 'geojson', data: { type: 'Feature', geometry: { type: 'LineString', coordinates: coords } } });
    var paint = { 'line-color': color, 'line-width': dash ? 2 : 3, 'line-opacity': dash ? 0.6 : 0.85 };
    if (dash) paint['line-dasharray'] = [3, 3];
    map.addLayer({ id: lyrId, type: 'line', source: srcId, paint: paint });
  }

  // ── Live track lines (current run) ────────────────────────────
  function updateLiveLine(id) {
    if (!mapLoaded) return;
    var pts = recordPoints[id];
    if (!pts || pts.length < 2) return;
    var coords = pts.map(function(p) { return [p[1], p[0]]; });
    var src = 'live-' + id, lyr = 'live-lyr-' + id;
    if (map.getSource(src)) {
      map.getSource(src).setData({ type: 'Feature', geometry: { type: 'LineString', coordinates: coords } });
    } else {
      addLineToMap(src, lyr, coords, colors[id % colors.length], false);
    }
  }

  function clearLiveLines() {
    Object.keys(liveMarkers).forEach(function(id) {
      safeRemoveLayer('live-lyr-' + id);
      safeRemoveSource('live-' + id);
    });
  }

  // ── History overlay ────────────────────────────────────────────
  function showHistoryOverlay(runId) {
    if (viewRunId === runId) { hideHistoryOverlay(); return; }
    hideHistoryOverlay();
    var saved = JSON.parse(localStorage.getItem('savedRuns') || '{}');
    var paths = saved[runId];
    if (!paths) return;
    viewRunId = runId;
    histOverlay = { sources: [], layers: [], markers: [] };
    updateHistHighlight();
    var allCoords = [];
    Object.keys(paths).forEach(function(tId) {
      var pts = paths[tId];
      if (!pts || pts.length < 2) return;
      var coords = pts.map(function(p) { return [p[1], p[0]]; });
      allCoords = allCoords.concat(coords);
      var color = colors[tId % colors.length];
      var src = 'hist-' + tId, lyr = 'hist-lyr-' + tId;
      addLineToMap(src, lyr, coords, color, true);
      histOverlay.sources.push(src);
      histOverlay.layers.push(lyr);
      var s = document.createElement('div');
      s.style.cssText = 'width:8px;height:8px;border-radius:50%;background:' + color + ';border:2px solid #fff;opacity:0.7;';
      histOverlay.markers.push(new mapboxgl.Marker({ element: s }).setLngLat(coords[0]).addTo(map));
      var e = document.createElement('div');
      e.style.cssText = 'width:12px;height:12px;border-radius:50%;background:' + color + ';border:2px solid #fff;opacity:0.9;';
      histOverlay.markers.push(new mapboxgl.Marker({ element: e }).setLngLat(coords[coords.length - 1]).addTo(map));
    });
    if (allCoords.length > 0) {
      var b = allCoords.reduce(function(acc, c) { return acc.extend(c); }, new mapboxgl.LngLatBounds(allCoords[0], allCoords[0]));
      map.fitBounds(b, { padding: 40 });
    }
  }

  function hideHistoryOverlay() {
    if (!histOverlay) return;
    histOverlay.markers.forEach(function(m) { m.remove(); });
    histOverlay.layers.forEach(function(l)  { safeRemoveLayer(l);  });
    histOverlay.sources.forEach(function(s) { safeRemoveSource(s); });
    histOverlay = null;
    viewRunId = null;
    updateHistHighlight();
  }

  // ── Telemetry ──────────────────────────────────────────────────
  function renderTelemetry(data) {
    var id = data.id;
    lastSeen[id] = Date.now();

    var card = document.getElementById('card-' + id);
    if (!card) {
      card = document.createElement('div');
      card.id = 'card-' + id;
      card.className = 'card';
      card.style.cursor = 'pointer';
      card.onclick = function() { flyTo(id); };
      document.getElementById('tracker-list').appendChild(card);
    }
    var batColor = data.bat > 50 ? '#0f9' : data.bat > 20 ? '#fa0' : '#f55';
    card.innerHTML =
      '<b>' + names[id] + '</b> | Bat: <span style="color:' + batColor + '">' + data.bat + '%</span><br>' +
      'Behavior: ' + (behaviors[data.beh] || '?') + '<br>' +
      'Speed: ' + data.spd.toFixed(1) + ' mph | Hdg: ' + toCardinal(data.hd) + '<br>' +
      'RSSI: ' + data.rssi + ' dBm | SNR: ' + data.snr + ' dB<br>' +
      'Fix: ' + (!data.fix ? '<span style="color:#f55">NO FIX</span>' :
                 data.fresh ? '<span style="color:#0f9">FRESH</span>' :
                              '<span style="color:#fa0">STALE</span>') + '<br>' +
      'Last: <span id="seen-' + id + '">just now</span>';
    card.style.outline = followId == id ? '1px solid #0af' : 'none';

    if (!data.fix || !data.fresh) return;
    var lnglat = [data.lng, data.lat];

    if (!liveMarkers[id]) {
      var color = colors[id % colors.length];
      var el = document.createElement('div');
      el.style.cssText = 'width:12px;height:12px;border-radius:50%;background:' + color + ';border:2px solid #fff;cursor:pointer;';
      liveMarkers[id] = new mapboxgl.Marker({ element: el })
        .setLngLat(lnglat)
        .setPopup(new mapboxgl.Popup().setText(names[id]))
        .addTo(map);
      var ids = Object.keys(liveMarkers);
      if (ids.length > 1) {
        var bounds = new mapboxgl.LngLatBounds();
        ids.forEach(function(k) { bounds.extend(liveMarkers[k].getLngLat()); });
        map.fitBounds(bounds, { padding: 60 });
      } else {
        map.panTo(lnglat);
      }
    } else {
      liveMarkers[id].setLngLat(lnglat);
      if (followId == id) map.easeTo({ center: lnglat, duration: 300 });
    }
    var n = Object.keys(liveMarkers).length;
    document.getElementById('node-count').innerText = n + ' node' + (n !== 1 ? 's' : '') + ' active';

    if (isRunActive && data.beh !== 0) {
      if (!recordPoints[id]) recordPoints[id] = [];
      recordPoints[id].push([data.lat, data.lng]);
      updateLiveLine(id);
    }
  }

  // ── Run state ──────────────────────────────────────────────────
  function updateState(active, runId) {
    isRunActive  = active;
    currentRunId = runId;
    document.getElementById('status').innerText = active ? 'Recording: ' + runId : 'Monitoring';
    document.getElementById('startBtn').style.display = active ? 'none'  : 'block';
    document.getElementById('stopBtn').style.display  = active ? 'block' : 'none';
    if (!active) {
      var b = document.getElementById('startBtn');
      b.disabled = false; b.innerText = 'Start Session'; b.style.borderColor = ''; b.style.color = '';
    } else {
      var b = document.getElementById('stopBtn');
      b.disabled = false; b.innerText = 'Stop + Save';
    }
  }

  // ── Run control ────────────────────────────────────────────────
  function makeRunId() {
    var now = new Date();
    var h = now.getHours(), ampm = h >= 12 ? 'PM' : 'AM';
    h = h % 12 || 12;
    var m = String(now.getMinutes()).padStart(2, '0');
    return 'run_' + h + '-' + m + ampm + '_' + (now.getMonth() + 1) + '-' + String(now.getDate()).padStart(2, '0') + '-' + String(now.getFullYear()).slice(2);
  }

  function startRun() {
    if (Object.keys(liveMarkers).length === 0) { alert('No active nodes.'); return; }
    var btn = document.getElementById('startBtn');
    btn.disabled = true; btn.innerText = 'Starting…';
    recordPoints = {};
    clearLiveLines();
    var runId = makeRunId();
    fetchT('/api/run/start?id=' + encodeURIComponent(runId), { method: 'POST' })
      .then(function(r) {
        if (r.status === 400) {
          fetchT('/api/run/stop', { method: 'POST' }).then(function() {
            fetchT('/api/run/start?id=' + encodeURIComponent(runId), { method: 'POST' });
          });
        }
      })
      .catch(function() {
        btn.disabled = false; btn.innerText = 'Failed — Retry';
        btn.style.borderColor = '#f55'; btn.style.color = '#f55';
        setTimeout(function() { btn.innerText = 'Start Session'; btn.style.borderColor = ''; btn.style.color = ''; }, 3000);
      });
  }

  function downsample(pts) {
    if (pts.length <= 2) return pts;
    var out = [pts[0]];
    for (var i = 3; i < pts.length - 1; i += 3) out.push(pts[i]);
    out.push(pts[pts.length - 1]);
    return out;
  }

  function stopRun() {
    var btn = document.getElementById('stopBtn');
    btn.disabled = true; btn.innerText = 'Saving…';
    var snap = {};
    Object.keys(recordPoints).forEach(function(id) {
      if (recordPoints[id] && recordPoints[id].length > 0) snap[id] = downsample(recordPoints[id]);
    });
    var saved = JSON.parse(localStorage.getItem('savedRuns') || '{}');
    saved[currentRunId] = snap;
    localStorage.setItem('savedRuns', JSON.stringify(saved));
    recordPoints = {};
    clearLiveLines();
    fetchT('/api/run/stop', { method: 'POST' });
    loadHistory();
  }

  // ── History list ───────────────────────────────────────────────
  function formatRunLabel(id) {
    var m = id.match(/^run_(\d+)-(\d{2})(AM|PM)_(\d+)-(\d{2})-(\d{2})$/);
    if (!m) return id;
    return m[1] + ':' + m[2] + m[3] + ' ' + m[4] + '/' + m[5] + '/' + m[6];
  }

  function updateHistHighlight() {
    document.querySelectorAll('.hist[data-rid]').forEach(function(el) {
      el.style.background = el.dataset.rid === viewRunId ? '#1a3a1a' : '';
    });
  }

  function loadHistory() {
    var saved = JSON.parse(localStorage.getItem('savedRuns') || '{}');
    var ids = Object.keys(saved).sort().reverse();
    var list = document.getElementById('history-list');
    list.innerHTML = '';
    if (!ids.length) { list.innerHTML = '<div style="font-size:11px;color:#555;">No runs saved</div>'; return; }
    ids.forEach(function(f) {
      var el = document.createElement('div');
      el.className = 'hist'; el.dataset.rid = f;
      el.style.cssText = 'display:flex;justify-content:space-between;align-items:center;';
      if (f === viewRunId) el.style.background = '#1a3a1a';
      var name = document.createElement('span');
      name.innerText = formatRunLabel(f); name.style.flex = '1';
      name.onclick = function() { showHistoryOverlay(f); };
      var del = document.createElement('span');
      del.innerText = '✕'; del.style.cssText = 'color:#f55;cursor:pointer;padding-left:8px;';
      del.onclick = function(e) {
        e.stopPropagation();
        if (viewRunId === f) hideHistoryOverlay();
        var s = JSON.parse(localStorage.getItem('savedRuns') || '{}');
        delete s[f];
        localStorage.setItem('savedRuns', JSON.stringify(s));
        loadHistory();
      };
      el.appendChild(name); el.appendChild(del); list.appendChild(el);
    });
  }

  // ── Camera ─────────────────────────────────────────────────────
  function flyTo(id) {
    if (followId === id) {
      followId = null;
      document.querySelectorAll('.card').forEach(function(c) { c.style.outline = 'none'; });
      return;
    }
    var m = liveMarkers[id];
    if (!m) return;
    followId = id;
    document.querySelectorAll('.card').forEach(function(c) { c.style.outline = 'none'; });
    var card = document.getElementById('card-' + id);
    if (card) card.style.outline = '1px solid #0af';
    var ll = m.getLngLat();
    map.flyTo({ center: [ll.lng, ll.lat], zoom: Math.max(map.getZoom(), 17) });
  }

  map.on('dragstart', function(e) {
    if (e.originalEvent) {
      followId = null;
      document.querySelectorAll('.card').forEach(function(c) { c.style.outline = 'none'; });
    }
  });

  // ── Map style ──────────────────────────────────────────────────
  function setStyle(name) {
    if (name === activeStyle) return;
    activeStyle = name;
    mapLoaded = false;
    map.setStyle(styles[name]);
    map.once('style.load', function() {
      mapLoaded = true;
      Object.keys(recordPoints).forEach(function(id) { updateLiveLine(id); });
      if (viewRunId) { var r = viewRunId; viewRunId = null; histOverlay = null; showHistoryOverlay(r); }
    });
  }

  // ── WebSocket ──────────────────────────────────────────────────
  function fetchT(url, opts) {
    var ctrl = new AbortController();
    var t = setTimeout(function() { ctrl.abort(); }, 6000);
    return fetch(url, Object.assign({}, opts || {}, { signal: ctrl.signal })).finally(function() { clearTimeout(t); });
  }

  var ws;
  function connectWS() {
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen  = function() { document.getElementById('status').style.color = '#fa0'; loadHistory(); };
    ws.onmessage = function(e) {
      var msg = JSON.parse(e.data);
      if (msg.type === 'STATE') updateState(msg.active, msg.run_id);
      else if (msg.type === 'DATA') renderTelemetry(msg.payload);
    };
    ws.onclose = function() {
      document.getElementById('status').innerText = 'Disconnected';
      document.getElementById('status').style.color = '#f55';
      setTimeout(connectWS, 2000);
    };
  }
  connectWS();

  // ── Stale grey-out ─────────────────────────────────────────────
  setInterval(function() {
    Object.keys(lastSeen).forEach(function(id) {
      var el = document.getElementById('seen-' + id);
      if (!el) return;
      var secs = Math.floor((Date.now() - lastSeen[id]) / 1000);
      el.innerText = secs < 60 ? secs + 's ago' : Math.floor(secs / 60) + 'm ago';
      var stale = secs > 30;
      var card = document.getElementById('card-' + id);
      if (card) card.style.opacity = stale ? '0.4' : '1';
      var m = liveMarkers[id];
      if (m) m.getElement().style.opacity = stale ? '0.4' : '1';
    });
  }, 1000);

  window.onload = loadHistory;
</script>
</body>
</html>

)rawliteral";
