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
    <button onclick="setStyle('standard')" id="btn-standard" style="width:auto;padding:6px 10px;">Street</button>
    <button onclick="setStyle('satellite')" id="btn-satellite" style="width:auto;padding:6px 10px;">Satellite</button>
  </div>
</div>

<script>
  mapboxgl.accessToken = ")rawliteral"
MAPBOX_TOKEN
R"rawliteral(";

  var map = new mapboxgl.Map({
    container: 'map',
    style: 'mapbox://styles/mapbox/satellite-streets-v12',
    center: [-87.4139, 39.4667],
    zoom: 15
  });
  map.addControl(new mapboxgl.NavigationControl(), 'bottom-right');

  var colors = ['#0af', '#0f9', '#fa0', '#f55'];
  var behaviors = ['Idle', 'Walking', 'Running', 'Sitting', 'Rolling'];
  var trackerRenders = {};
  var isRunActive = false;
  var currentRunId = '';
  var activeStyle = 'satellite';
  var mapLoaded = false;
  var pendingAdds = [];

  map.on('load', function() {
    mapLoaded = true;
    var saved = localStorage.getItem('currentTrack');
    if (saved) {
      try {
        var restored = JSON.parse(saved);
        Object.keys(restored).forEach(function(id) {
          var points = restored[id];
          if (points && points.length > 0) addTrackerToMap(id, points);
        });
      } catch(e) {}
    }
    pendingAdds.forEach(function(fn) { fn(); });
    pendingAdds = [];
  });

  // points is [[lat, lng], ...] — Mapbox needs [lng, lat]
  function addTrackerToMap(id, points) {
    if (!mapLoaded) { pendingAdds.push(function() { addTrackerToMap(id, points); }); return; }
    var color = colors[id % colors.length];
    var coords = points.map(function(p) { return [p[1], p[0]]; });

    map.addSource('line-' + id, {
      type: 'geojson',
      data: { type: 'Feature', geometry: { type: 'LineString', coordinates: coords } }
    });
    map.addLayer({
      id: 'layer-' + id, type: 'line', source: 'line-' + id,
      paint: { 'line-color': color, 'line-width': 3, 'line-opacity': 0.85 }
    });

    var el = document.createElement('div');
    el.style.cssText = 'width:12px;height:12px;border-radius:50%;background:' + color + ';border:2px solid #fff;cursor:pointer;';
    var marker = new mapboxgl.Marker({ element: el })
      .setLngLat(coords[coords.length - 1])
      .setPopup(new mapboxgl.Popup().setText('Node ' + id))
      .addTo(map);

    trackerRenders[id] = { points: points, marker: marker };
  }

  function removeTracker(id) {
    var tr = trackerRenders[id];
    if (!tr) return;
    tr.marker.remove();
    if (map.getLayer('layer-' + id)) map.removeLayer('layer-' + id);
    if (map.getSource('line-' + id)) map.removeSource('line-' + id);
    delete trackerRenders[id];
  }

  var styles = {
    standard: 'mapbox://styles/mapbox/standard',
    satellite: 'mapbox://styles/mapbox/standard-satellite'
  };

  function setStyle(name) {
    if (name === activeStyle) return;
    activeStyle = name;
    mapLoaded = false;
    map.setStyle(styles[name]);
    map.once('style.load', function() {
      mapLoaded = true;
      Object.keys(trackerRenders).forEach(function(id) {
        var tr = trackerRenders[id];
        var color = colors[id % colors.length];
        var coords = tr.points.map(function(p) { return [p[1], p[0]]; });
        map.addSource('line-' + id, {
          type: 'geojson',
          data: { type: 'Feature', geometry: { type: 'LineString', coordinates: coords } }
        });
        map.addLayer({
          id: 'layer-' + id, type: 'line', source: 'line-' + id,
          paint: { 'line-color': color, 'line-width': 3, 'line-opacity': 0.85 }
        });
      });
    });
  }

  var ws;
  function connectWS() {
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onmessage = function(e) {
      var msg = JSON.parse(e.data);
      if (msg.type === 'STATE') updateState(msg.active, msg.run_id);
      else if (msg.type === 'DATA') renderTelemetry(msg.payload);
    };
    ws.onclose = function() { setTimeout(connectWS, 2000); };
  }
  connectWS();

  function renderTelemetry(data) {
    var id = data.id;
    var card = document.getElementById('card-' + id);
    if (!card) {
      card = document.createElement('div');
      card.id = 'card-' + id;
      card.className = 'card';
      document.getElementById('tracker-list').appendChild(card);
    }
    card.innerHTML = '<b>NODE-' + id + '</b> | Bat: ' + data.bat + '%<br>' +
      'Mode: ' + (behaviors[data.beh] || '?') + '<br>' +
      'Speed: ' + data.spd.toFixed(1) + ' mph | Hdg: ' + data.hd.toFixed(0) + '&deg;<br>' +
      'RSSI: ' + (data.rssi !== undefined ? data.rssi + ' dBm' : '&mdash;') + '<br>' +
      'Fix: ' + (data.fix ? '<span style="color:#0f9">VALID</span>' : '<span style="color:#f55">NO FIX</span>');

    if (data.fix) {
      var latlng = [data.lat, data.lng];
      var lnglat = [data.lng, data.lat];
      if (!trackerRenders[id]) {
        addTrackerToMap(id, [latlng]);
        map.panTo(lnglat);
      } else {
        trackerRenders[id].points.push(latlng);
        trackerRenders[id].marker.setLngLat(lnglat);
        var coords = trackerRenders[id].points.map(function(p) { return [p[1], p[0]]; });
        map.getSource('line-' + id).setData({
          type: 'Feature', geometry: { type: 'LineString', coordinates: coords }
        });
      }
      if (isRunActive) {
        var snap = {};
        Object.keys(trackerRenders).forEach(function(k) { snap[k] = trackerRenders[k].points; });
        localStorage.setItem('currentTrack', JSON.stringify(snap));
      }
    }
  }

  function updateState(active, runId) {
    isRunActive = active;
    currentRunId = runId;
    document.getElementById('status').innerText = active ? 'Recording: ' + runId : 'Monitoring';
    document.getElementById('startBtn').style.display = active ? 'none' : 'block';
    document.getElementById('stopBtn').style.display = active ? 'block' : 'none';
  }

  function startRun() {
    localStorage.removeItem('currentTrack');
    fetch('/api/run/start', { method: 'POST' });
  }

  function stopRun() {
    var snap = {};
    Object.keys(trackerRenders).forEach(function(k) { snap[k] = trackerRenders[k].points; });
    fetch('/api/run/save?id=' + currentRunId, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ paths: snap })
    }).then(function() {
      localStorage.removeItem('currentTrack');
      fetch('/api/run/stop', { method: 'POST' });
      loadHistory();
    });
  }

  function loadHistory() {
    fetch('/api/results').then(function(r) { return r.json(); }).then(function(files) {
      var list = document.getElementById('history-list');
      list.innerHTML = '';
      if (!files.length) { list.innerHTML = '<div style="font-size:11px;color:#555;">No runs saved</div>'; return; }
      files.forEach(function(f) {
        var el = document.createElement('div');
        el.className = 'hist';
        el.innerText = f;
        el.onclick = function() { viewLog(f); };
        list.appendChild(el);
      });
    }).catch(function() {});
  }

  function viewLog(id) {
    fetch('/api/results/view?id=' + id).then(function(r) { return r.json(); }).then(function(data) {
      Object.keys(trackerRenders).forEach(function(k) { removeTracker(k); });
      trackerRenders = {};
      Object.keys(data.paths).forEach(function(tId) {
        var points = data.paths[tId];
        if (points.length > 0) {
          addTrackerToMap(tId, points);
          var coords = points.map(function(p) { return [p[1], p[0]]; });
          var bounds = coords.reduce(function(b, c) {
            return b.extend(c);
          }, new mapboxgl.LngLatBounds(coords[0], coords[0]));
          map.fitBounds(bounds, { padding: 40 });
        }
      });
    }).catch(function() {});
  }

  window.onload = loadHistory;
</script>
</body>
</html>

)rawliteral";