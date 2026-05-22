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
    style: 'mapbox://styles/mapbox/standard',
    center: [-87.3243, 39.4833],
    zoom: 15
  });
  map.addControl(new mapboxgl.NavigationControl(), 'bottom-right');

  var colors = ['#0af', '#0f9', '#fa0', '#f55'];
  var behaviors = ['Idle', 'Walking', 'Running', 'Sitting', 'Rolling'];
  var trackerRenders = {};
  var isRunActive = false;
  var currentRunId = '';
  var activeStyle = 'standard';
  var mapLoaded = false;
  var pendingAdds = [];
  var lastSeen = {};

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

    if (coords.length >= 2) {
      if (map.getSource('line-' + id)) {
        map.getSource('line-' + id).setData({ type: 'Feature', geometry: { type: 'LineString', coordinates: coords } });
      } else {
        map.addSource('line-' + id, {
          type: 'geojson',
          data: { type: 'Feature', geometry: { type: 'LineString', coordinates: coords } }
        });
        map.addLayer({
          id: 'layer-' + id, type: 'line', source: 'line-' + id,
          paint: { 'line-color': color, 'line-width': 3, 'line-opacity': 0.85 }
        });
      }
    }

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
        if (!tr.points || tr.points.length === 0) return;
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

  function fetchT(url, opts) {
    var controller = new AbortController();
    var timer = setTimeout(function() { controller.abort(); }, 6000);
    return fetch(url, Object.assign({}, opts || {}, { signal: controller.signal }))
      .finally(function() { clearTimeout(timer); });
  }

  var ws;
  function connectWS() {
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen = function() {
      document.getElementById('status').style.color = '#fa0';
      loadHistory();
    };
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

  function renderTelemetry(data) {
    var id = data.id;
    lastSeen[id] = Date.now();
    var card = document.getElementById('card-' + id);
    if (!card) {
      card = document.createElement('div');
      card.id = 'card-' + id;
      card.className = 'card';
      card.style.cursor = 'pointer';
      document.getElementById('tracker-list').appendChild(card);
    }
    card.onclick = function() { flyToTracker(id); };
    var batColor = data.bat > 50 ? '#0f9' : data.bat > 20 ? '#fa0' : '#f55';
    card.innerHTML = '<b>NODE-' + id + '</b> | Bat: <span style="color:' + batColor + '">' + data.bat + '%</span><br>' +
      'Behavior: ' + (behaviors[data.beh] || '?') + '<br>' +
      'Speed: ' + data.spd.toFixed(1) + ' mph | Hdg: ' + data.hd.toFixed(0) + '&deg;<br>' +
      'RSSI: ' + (data.rssi !== undefined ? data.rssi + ' dBm' : '&mdash;') + '<br>' +
      'Fix: ' + (data.fix ? '<span style="color:#0f9">VALID</span>' : '<span style="color:#f55">NO FIX</span>') + '<br>' +
      'Last: <span id="seen-' + id + '">just now</span>';
    card.style.outline = followingId == id ? '1px solid #0af' : 'none';

    if (data.fix) {
      var latlng = [data.lat, data.lng];
      var lnglat = [data.lng, data.lat];

      if (viewingHistory) {
        if (!liveTrackers[id]) {
          liveTrackers[id] = { points: [], lnglat: { lng: data.lng, lat: data.lat } };
          var n = Object.keys(liveTrackers).length;
          document.getElementById('node-count').innerText = n + ' node' + (n !== 1 ? 's' : '') + ' active';
        } else {
          liveTrackers[id].lnglat = { lng: data.lng, lat: data.lat };
        }
        if (isRunActive) {
          liveTrackers[id].points.push(latlng);
          var snap = {};
          Object.keys(liveTrackers).forEach(function(k) { snap[k] = liveTrackers[k].points; });
          localStorage.setItem('currentTrack', JSON.stringify(snap));
        }
        return;
      }

      if (!trackerRenders[id]) {
        var color = colors[id % colors.length];
        var el = document.createElement('div');
        el.style.cssText = 'width:12px;height:12px;border-radius:50%;background:' + color + ';border:2px solid #fff;cursor:pointer;';
        var marker = new mapboxgl.Marker({ element: el })
          .setLngLat(lnglat)
          .setPopup(new mapboxgl.Popup().setText('Node ' + id))
          .addTo(map);
        trackerRenders[id] = { points: [], marker: marker };
        var activeIds = Object.keys(trackerRenders);
        if (activeIds.length > 1) {
          var bounds = new mapboxgl.LngLatBounds();
          activeIds.forEach(function(k) { bounds.extend(trackerRenders[k].marker.getLngLat()); });
          map.fitBounds(bounds, { padding: 60 });
        } else {
          map.panTo(lnglat);
        }
        var n = activeIds.length;
        document.getElementById('node-count').innerText = n + ' node' + (n !== 1 ? 's' : '') + ' active';
      } else {
        trackerRenders[id].marker.setLngLat(lnglat);
        if (followingId == id) map.easeTo({ center: lnglat, duration: 300 });
      }

      if (isRunActive) {
        trackerRenders[id].points.push(latlng);
        var coords = trackerRenders[id].points.map(function(p) { return [p[1], p[0]]; });
        if (!map.getSource('line-' + id)) {
          var color = colors[id % colors.length];
          map.addSource('line-' + id, {
            type: 'geojson',
            data: { type: 'Feature', geometry: { type: 'LineString', coordinates: coords } }
          });
          map.addLayer({
            id: 'layer-' + id, type: 'line', source: 'line-' + id,
            paint: { 'line-color': color, 'line-width': 3, 'line-opacity': 0.85 }
          });
        } else {
          var src = map.getSource('line-' + id);
          if (src) src.setData({
            type: 'Feature', geometry: { type: 'LineString', coordinates: coords }
          });
        }
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

  function makeRunId() {
    var now = new Date();
    var h = now.getHours(), ampm = h >= 12 ? 'PM' : 'AM';
    h = h % 12 || 12;
    var m = String(now.getMinutes()).padStart(2,'0');
    var mo = now.getMonth() + 1;
    var d = String(now.getDate()).padStart(2,'0');
    var y = String(now.getFullYear()).slice(2);
    return 'run_' + h + '-' + m + ampm + '_' + mo + '-' + d + '-' + y;
  }

  function formatRunLabel(id) {
    var m = id.match(/^run_(\d+)-(\d{2})(AM|PM)_(\d+)-(\d{2})-(\d{2})$/);
    if (!m) return id;
    return m[1] + ':' + m[2] + m[3] + ' ' + m[4] + '/' + m[5] + '/' + m[6];
  }

  function startRun() {
    var liveCount = viewingHistory ? Object.keys(liveTrackers).length : Object.keys(trackerRenders).length;
    if (liveCount === 0) {
      alert('No active nodes detected.');
      return;
    }
    var btn = document.getElementById('startBtn');
    btn.disabled = true;
    btn.innerText = 'Starting…';
    localStorage.removeItem('currentTrack');
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
        btn.disabled = false;
        btn.innerText = 'Failed — Try Again';
        btn.style.borderColor = '#f55';
        btn.style.color = '#f55';
        setTimeout(function() {
          btn.innerText = 'Start Session';
          btn.style.borderColor = '';
          btn.style.color = '';
        }, 3000);
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
    btn.disabled = true;
    btn.innerText = 'Saving…';
    var saveId = currentRunId;
    var snap = {};
    if (viewingHistory) {
      Object.keys(liveTrackers).forEach(function(k) {
        snap[k] = downsample(liveTrackers[k].points);
        liveTrackers[k].points = [];
      });
    } else {
      Object.keys(trackerRenders).forEach(function(k) { snap[k] = downsample(trackerRenders[k].points); });
    }
    var saved = JSON.parse(localStorage.getItem('savedRuns') || '{}');
    saved[saveId] = snap;
    localStorage.setItem('savedRuns', JSON.stringify(saved));
    localStorage.removeItem('currentTrack');
    fetchT('/api/run/stop', { method: 'POST' });
    if (!viewingHistory) {
      Object.keys(trackerRenders).forEach(function(id) {
        if (map.getLayer('layer-' + id)) map.removeLayer('layer-' + id);
        if (map.getSource('line-' + id)) map.removeSource('line-' + id);
        trackerRenders[id].points = [];
      });
    }
    loadHistory();
  }

  function updateHistHighlight() {
    document.querySelectorAll('.hist[data-rid]').forEach(function(el) {
      el.style.background = el.dataset.rid === activeViewId ? '#1a3a1a' : '';
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
      el.className = 'hist';
      el.dataset.rid = f;
      el.style.cssText = 'display:flex;justify-content:space-between;align-items:center;';
      if (f === activeViewId) el.style.background = '#1a3a1a';
      var name = document.createElement('span');
      name.innerText = formatRunLabel(f);
      name.style.flex = '1';
      name.onclick = function() { viewLog(f); };
      var del = document.createElement('span');
      del.innerText = '✕';
      del.style.cssText = 'color:#f55;cursor:pointer;padding-left:8px;';
      del.onclick = function(e) {
        e.stopPropagation();
        if (activeViewId === f) { exitHistoricalView(); activeViewId = null; }
        var saved = JSON.parse(localStorage.getItem('savedRuns') || '{}');
        delete saved[f];
        localStorage.setItem('savedRuns', JSON.stringify(saved));
        loadHistory();
      };
      el.appendChild(name);
      el.appendChild(del);
      list.appendChild(el);
    });
  }

  function exitHistoricalView() {
    Object.keys(trackerRenders).forEach(function(k) { removeTracker(k); });
    trackerRenders = {};
    viewingHistory = false;
    Object.keys(liveTrackers).forEach(function(id) {
      var lt = liveTrackers[id];
      var color = colors[id % colors.length];
      var el = document.createElement('div');
      el.style.cssText = 'width:12px;height:12px;border-radius:50%;background:' + color + ';border:2px solid #fff;cursor:pointer;';
      var lnglat = [lt.lnglat.lng, lt.lnglat.lat];
      var marker = new mapboxgl.Marker({ element: el })
        .setLngLat(lnglat)
        .setPopup(new mapboxgl.Popup().setText('Node ' + id))
        .addTo(map);
      trackerRenders[id] = { points: lt.points, marker: marker };
      if (isRunActive && lt.points.length > 0) {
        var coords = lt.points.map(function(p) { return [p[1], p[0]]; });
        map.addSource('line-' + id, {
          type: 'geojson',
          data: { type: 'Feature', geometry: { type: 'LineString', coordinates: coords } }
        });
        map.addLayer({
          id: 'layer-' + id, type: 'line', source: 'line-' + id,
          paint: { 'line-color': color, 'line-width': 3, 'line-opacity': 0.85 }
        });
      }
    });
    var n = Object.keys(trackerRenders).length;
    document.getElementById('node-count').innerText = n + ' node' + (n !== 1 ? 's' : '') + ' active';
    liveTrackers = {};
  }

  function viewLog(id) {
    if (activeViewId === id) {
      exitHistoricalView();
      activeViewId = null;
      updateHistHighlight();
      return;
    }
    liveTrackers = {};
    Object.keys(trackerRenders).forEach(function(k) {
      var tr = trackerRenders[k];
      liveTrackers[k] = { points: tr.points.slice(), lnglat: tr.marker.getLngLat() };
      tr.marker.remove();
      if (map.getLayer('layer-' + k)) map.removeLayer('layer-' + k);
      if (map.getSource('line-' + k)) map.removeSource('line-' + k);
    });
    trackerRenders = {};
    viewingHistory = true;
    activeViewId = id;
    updateHistHighlight();
    var saved = JSON.parse(localStorage.getItem('savedRuns') || '{}');
    var paths = saved[id];
    if (!paths) { exitHistoricalView(); activeViewId = null; updateHistHighlight(); return; }
    var allCoords = [];
    Object.keys(paths).forEach(function(tId) {
      var points = paths[tId];
      if (points && points.length > 0) {
        addTrackerToMap(tId, points);
        points.forEach(function(p) { allCoords.push([p[1], p[0]]); });
      }
    });
    if (allCoords.length > 0) {
      var bounds = allCoords.reduce(function(b, c) {
        return b.extend(c);
      }, new mapboxgl.LngLatBounds(allCoords[0], allCoords[0]));
      map.fitBounds(bounds, { padding: 40 });
    }
  }

  var followingId = null;
  var activeViewId = null;
  var viewingHistory = false;
  var liveTrackers = {};

  function flyToTracker(id) {
    if (viewingHistory) {
      exitHistoricalView();
      activeViewId = null;
      updateHistHighlight();
    }
    if (followingId === id) {
      followingId = null;
      return;
    }
    var tr = trackerRenders[id];
    if (!tr) return;
    followingId = id;
    var lnglat = tr.marker.getLngLat();
    map.flyTo({ center: [lnglat.lng, lnglat.lat], zoom: Math.max(map.getZoom(), 17) });
  }

  map.on('dragstart', function(e) {
    if (e.originalEvent) followingId = null;
  });

  setInterval(function() {
    Object.keys(lastSeen).forEach(function(id) {
      var el = document.getElementById('seen-' + id);
      if (!el) return;
      var secs = Math.floor((Date.now() - lastSeen[id]) / 1000);
      el.innerText = secs < 60 ? secs + 's ago' : Math.floor(secs / 60) + 'm ago';
    });
  }, 1000);

  window.onload = loadHistory;
</script>
</body>
</html>

)rawliteral";