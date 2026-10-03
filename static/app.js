// Shared constants for UI
const LINE_COLORS = {
  'Purple': '#7F77DD',
  'Aqua': '#378ADD',
  'Line 4': '#D4537E'
};

// ==========================================
// STEP 1: Toast Notification System
// ==========================================
function showToast(message, type = 'info') {
  const container = document.getElementById('toast-container');
  const toast = document.createElement('div');
  toast.className = `toast ${type}`;
  toast.textContent = message;
  container.appendChild(toast);
  setTimeout(() => toast.remove(), 3400); // 3s delay + 0.3s animation
}

// API wrapper with Try/Catch & Loading
async function api(method, path, body) {
  const opts = { method, headers: {} };
  if (body !== undefined) {
    opts.headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(body);
  }
  try {
    const res = await fetch(path, opts);
    let data;
    try { data = await res.json(); } catch (e) {
      data = { success: false, message: 'Invalid response from server.' };
    }
    return { status: res.status, data };
  } catch (err) {
    showToast('Cannot reach the server. Is the backend running?', 'error');
    return { status: 0, data: { success: false, message: 'Network error.' } };
  }
}

// ==========================================
// STEP 2: Tab Navigation Logic
// ==========================================
function setupTabs() {
  document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
      // Deactivate all
      document.querySelectorAll('.tab').forEach(t => {
        t.classList.remove('active');
        t.setAttribute('aria-selected', 'false');
      });
      document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));

      // Activate clicked
      tab.classList.add('active');
      tab.setAttribute('aria-selected', 'true');
      document.getElementById(`tab-${tab.dataset.tab}`).classList.add('active');
    });
  });
}

// ==========================================
// STEP 3: Load Stations for Dropdowns
// ==========================================
let stationNames = []; 

async function loadStations() {
  // If your C++ API is ready on this endpoint:
  const { data, status } = await api('GET', '/api/stations');
  
  if (status === 0) return; // Silent fail if network down, wait for user action
  if (data && data.success && Array.isArray(data.data)) {
    stationNames = data.data.map(s => s.name).sort();
  } else {
    // Fallback stub just for UI preview if API doesn't exist yet
    stationNames = ["Vanaz", "Anand Nagar", "Ideal Colony", "Nal Stop", "Garware College", "Deccan Gymkhana", "PMC", "Shivaji Nagar", "District Court"];
  }

  // Populate every station <select>
  document.querySelectorAll('.station-select').forEach(select => {
    const placeholder = select.querySelector('option');
    select.innerHTML = '';
    select.appendChild(placeholder);
    
    stationNames.forEach(name => {
      const opt = document.createElement('option');
      opt.value = name;
      opt.textContent = name;
      select.appendChild(opt);
    });
  });
}

function swapStations() {
  const fromEl = document.getElementById('route-from');
  const toEl = document.getElementById('route-to');
  [fromEl.value, toEl.value] = [toEl.value, fromEl.value];
}

// ==========================================
// STEP 4: Journey Planner Render
// ==========================================
async function searchRoute() {
  const from = document.getElementById('route-from').value;
  const to = document.getElementById('route-to').value;
  const strategy = document.getElementById('route-strategy').value;

  if (!from || !to) { showToast('Please select both origin and destination.', 'warning'); return; }

  const container = document.getElementById('route-result');
  container.innerHTML = '<div class="loading-state"><span class="spinner"></span> Finding best route…</div>';

  const params = new URLSearchParams({ from, to, strategy });
  const result = await api('GET', `/api/routes?${params}`);
  
  if (!result.data.success) {
    container.innerHTML = `<div class="result-error">❌ ${result.data.message || 'Routing failed'}</div>`;
    return;
  }

  const { stations, metric, metricType } = result.data;
  const unitLabel = metricType === 'minutes' ? 'min' : 'stops';
  const metricLabel = metricType === 'minutes' ? '⏱️ Travel Time' : '🚏 Interchanges';

  container.innerHTML = `
    <div class="route-summary">
      <div class="stat">
        <span class="stat-label">${metricLabel}</span>
        <span class="stat-value">${metric} ${unitLabel}</span>
      </div>
      <div class="stat">
        <span class="stat-label">🛤️ Stations</span>
        <span class="stat-value">${stations.length}</span>
      </div>
    </div>
    <div class="route-timeline">
      ${stations.map(name => `
        <div class="timeline-stop">
          <span class="station-name">${name}</span>
        </div>
      `).join('')}
    </div>
  `;
}

// ==========================================
// STEP 5: Smart Card Widget Render
// ==========================================
async function handleCardAction(isRecharge) {
  const cardId = document.getElementById('wallet-card-id').value.trim();
  const amount = parseFloat(document.getElementById('wallet-amount').value || 0);
  const type = document.getElementById('wallet-card-type').value;
  const container = document.getElementById('card-result');

  if (!cardId) { showToast('Card ID is required.', 'warning'); return; }
  
  container.innerHTML = '<div class="loading-state"><span class="spinner"></span> Processing...</div>';

  let result;
  if (isRecharge) {
    result = await api('POST', '/api/cards', { cardId, cardType: type, initialBalance: amount });
  } else {
    result = await api('GET', `/api/cards/${cardId}`);
  }

  if (!result.data.success) {
    container.innerHTML = `<div class="result-error">❌ ${result.data.message}</div>`;
    return;
  }

  const d = result.data;
  if (d.message) showToast(d.message, 'success');

  container.innerHTML = `
    <div class="smart-card-widget">
      <div class="card-widget-header">
        <span class="card-widget-id">🚇 ${d.cardId}</span>
        <span class="card-type-badge">${d.cardType}</span>
      </div>
      <div class="card-balance-label">Available Balance</div>
      <div class="card-balance-amount">₹${parseFloat(d.balance).toFixed(2)}</div>
      <div class="card-status-row">
        ${d.checkedIn
          ? `<span class="card-checked-in-badge">📍 Tapped in at ${d.checkedInStation}</span>`
          : `<span class="card-checked-in-badge">Ready for travel</span>`
        }
      </div>
    </div>
  `;
}

// ==========================================
// STEP 6: Turnstile Tap-In / Tap-Out Render
// ==========================================
async function handleTap(isExit) {
  const cardId = document.getElementById('tap-card-id').value.trim();
  const stationName = document.getElementById('tap-station').value;
  const container = document.getElementById('tap-result');

  if (!cardId || !stationName) { showToast('Card ID and Station are required.', 'warning'); return; }

  container.innerHTML = '<div class="loading-state"><span class="spinner"></span> Communicating with gate...</div>';

  const path = isExit ? '/api/turnstile/tap-out' : '/api/turnstile/tap-in';
  const result = await api('POST', path, { cardId, stationName });
  const d = result.data;

  if (!d.success) {
    container.innerHTML = `<div class="tap-card tap-error"><div class="tap-card-title">❌ ${d.message}</div></div>`;
    return;
  }

  container.innerHTML = `
    <div class="tap-card ${isExit ? 'tap-out' : 'tap-in'}">
      <div class="tap-card-title">${isExit ? '🚪 Tap Out Successful' : '✅ Tap In Successful'}</div>
      <div class="tap-card-detail">
        <span>Entry Station</span>
        <span>${d.entryStation}</span>
      </div>
      ${isExit ? `
        <div class="tap-card-detail">
          <span>Exit Station</span>
          <span>${d.exitStation}</span>
        </div>
        <div class="tap-card-detail">
          <span>Fare Charged</span>
          <span>₹${parseFloat(d.fareCharged).toFixed(2)}</span>
        </div>
      ` : ''}
      <div class="tap-card-detail">
        <span>Remaining Balance</span>
        <span>₹${parseFloat(d.remainingBalance).toFixed(2)}</span>
      </div>
    </div>
  `;
}

// ==========================================
// STEP 7: Admin Panel Results & Incidents
// ==========================================
function showAdminResult(elId, result) {
  const container = document.getElementById(elId);
  const d = result.data;
  const isOk = d.success !== false;

  container.innerHTML = `
    <div style="
      margin-top: 12px; padding: 14px 18px; border-radius: 8px; font-size: 0.9rem; font-weight: 500;
      background: ${isOk ? 'rgba(26,158,92,0.06)' : 'rgba(209,69,61,0.06)'};
      border-left: 3px solid ${isOk ? 'var(--success)' : 'var(--error)'};
      color: ${isOk ? 'var(--success)' : 'var(--error)'};
    ">
      ${isOk ? '✅' : '❌'} ${d.message || (isOk ? 'Action successful.' : 'Action failed.')}
    </div>
  `;
}

async function reportIncident() {
  const from = document.getElementById('inc-from').value;
  const to = document.getElementById('inc-to').value;
  const type = parseInt(document.getElementById('inc-type').value);
  const severity = parseInt(document.getElementById('inc-severity').value);
  const delay = parseInt(document.getElementById('inc-delay').value || 0);

  if (!from || !to) { showToast('Select both stations.', 'warning'); return; }

  const res = await api('POST', '/api/admin/incident', { from, to, type, severity, delayMinutes: delay });
  showAdminResult('incident-result', res);
  if (res.data.success) loadIncidents(); // Refresh feed
}

async function toggleTrack(close) {
  const from = document.getElementById('track-from').value;
  const to = document.getElementById('track-to').value;
  if (!from || !to) { showToast('Select both stations.', 'warning'); return; }

  const endpoint = close ? '/api/admin/track/close' : '/api/admin/track/reopen';
  const res = await api('POST', endpoint, { from, to });
  showAdminResult('track-result', res);
  if (res.data.success) loadIncidents();
}

async function loadIncidents() {
  const container = document.getElementById('incidents-list');
  container.innerHTML = '<div class="loading-state"><span class="spinner"></span> Fetching events...</div>';
  
  const { data, status } = await api('GET', '/api/admin/incidents');
  if (status === 0 || !data.success) {
    container.innerHTML = '<p class="result-error">Failed to load incidents.</p>';
    return;
  }

  if (!data.data || data.data.length === 0) {
    container.innerHTML = '<p style="color:var(--muted);font-size:0.9rem;padding:10px 0;">✅ All lines operating normally. No active incidents.</p>';
    return;
  }

  container.innerHTML = data.data.map(ev => `
    <div class="incident-item">
      <div class="incident-info">
        <div class="incident-route">
          ${ev.fromStation} ↔ ${ev.toStation}
          ${ev.lineName ? `<span class="line-tag" style="background:${LINE_COLORS[ev.lineName] || '#999'}">${ev.lineName}</span>` : ''}
        </div>
        <div class="incident-meta">
          ${ev.incidentType}
          ${ev.trackClosed ? ' · 🚫 TRACK CLOSED' : ` · +${ev.delayMinutes} min delay`}
        </div>
      </div>
      <span class="severity-badge ${ev.severity.toLowerCase().replace(' ', '_')}">${ev.severity}</span>
    </div>
  `).join('');
}


// ==========================================
// Initialization & Event Listeners
// ==========================================
document.addEventListener('DOMContentLoaded', () => {
  setupTabs();
  loadStations();
  loadIncidents();

  // Attach all event listeners to replace inline onclicks
  document.getElementById('btn-swap-stations').addEventListener('click', swapStations);
  document.getElementById('btn-search-route').addEventListener('click', searchRoute);
  
  document.getElementById('btn-create-card').addEventListener('click', () => handleCardAction(true));
  document.getElementById('btn-lookup-card').addEventListener('click', () => handleCardAction(false));
  
  document.getElementById('btn-tap-in').addEventListener('click', () => handleTap(false));
  document.getElementById('btn-tap-out').addEventListener('click', () => handleTap(true));

  document.getElementById('btn-report-incident').addEventListener('click', reportIncident);
  document.getElementById('btn-close-track').addEventListener('click', () => toggleTrack(true));
  document.getElementById('btn-reopen-track').addEventListener('click', () => toggleTrack(false));
  document.getElementById('btn-refresh-incidents').addEventListener('click', loadIncidents);
});