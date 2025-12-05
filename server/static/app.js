/**
 * LoRa Network Monitor - Frontend JavaScript
 */

const API_BASE = '';
const UPDATE_INTERVAL = 5000;

// Estado da aplicacao
let currentPage = 'dashboard';
let devices = [];
let readings = [];
let machines = {}; // Armazena estado das maquinas por machineId

// Inicializacao
document.addEventListener('DOMContentLoaded', () => {
    initNavigation();
    loadDashboard();
    startAutoRefresh();
});

// Navegacao
function initNavigation() {
    document.querySelectorAll('.nav-item').forEach(item => {
        item.addEventListener('click', () => {
            const page = item.dataset.page;
            navigateTo(page);
        });
    });
}

function navigateTo(page) {
    // Atualiza menu
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.toggle('active', item.dataset.page === page);
    });

    // Atualiza paginas
    document.querySelectorAll('.page').forEach(p => {
        p.classList.toggle('active', p.id === `page-${page}`);
    });

    currentPage = page;

    // Carrega dados da pagina
    switch (page) {
        case 'dashboard':
            loadDashboard();
            break;
        case 'machines':
            loadMachines();
            break;
        case 'devices':
            loadDevices();
            break;
        case 'readings':
            loadReadings();
            loadDeviceFilters();
            break;
        case 'export':
            loadDeviceFilters();
            break;
    }
}

// Auto refresh
function startAutoRefresh() {
    setInterval(() => {
        if (currentPage === 'dashboard') {
            loadDashboard();
        } else if (currentPage === 'machines') {
            loadMachines();
        }
    }, UPDATE_INTERVAL);
}

// API Calls
async function fetchAPI(endpoint) {
    try {
        const response = await fetch(API_BASE + endpoint);
        if (!response.ok) throw new Error('Erro na requisicao');
        return await response.json();
    } catch (error) {
        console.error('Erro API:', error);
        updateConnectionStatus(false);
        throw error;
    }
}

function updateConnectionStatus(connected) {
    const statusEl = document.querySelector('.status-dot');
    const textEl = document.querySelector('.connection-status span:last-child');

    if (connected) {
        statusEl.classList.remove('offline');
        textEl.textContent = 'Conectado';
    } else {
        statusEl.classList.add('offline');
        textEl.textContent = 'Desconectado';
    }
}

// Dashboard
async function loadDashboard() {
    try {
        const [stats, devicesData, readingsData] = await Promise.all([
            fetchAPI('/api/stats'),
            fetchAPI('/api/devices'),
            fetchAPI('/api/readings?limit=10')
        ]);

        updateConnectionStatus(true);

        // Atualiza cards de estatisticas
        document.getElementById('totalDevices').textContent = stats.total_devices || 0;
        document.getElementById('totalReadings').textContent = formatNumber(stats.total_readings || 0);
        document.getElementById('readings24h').textContent = formatNumber(stats.readings_last_24h || 0);

        // Calcula RSSI medio
        if (readingsData.readings && readingsData.readings.length > 0) {
            const avgRssi = readingsData.readings.reduce((sum, r) => sum + r.rssi, 0) / readingsData.readings.length;
            document.getElementById('avgRssi').textContent = avgRssi.toFixed(0) + ' dBm';
        }

        // Atualiza atividade recente
        updateRecentActivity(readingsData.readings || []);
        document.getElementById('activityCount').textContent = readingsData.count || 0;

        // Atualiza dispositivos ativos
        updateActiveDevices(devicesData.devices || []);

        devices = devicesData.devices || [];

    } catch (error) {
        console.error('Erro ao carregar dashboard:', error);
    }
}

function updateRecentActivity(readings) {
    const container = document.getElementById('recentActivity');

    if (readings.length === 0) {
        container.innerHTML = '<div class="empty-state">Nenhuma atividade recente</div>';
        return;
    }

    container.innerHTML = readings.map(reading => {
        const dataStr = formatSensorData(reading.data);
        const timeStr = formatDateTime(reading.received_at);

        return `
            <div class="activity-item">
                <div class="activity-icon">&#128225;</div>
                <div class="activity-content">
                    <div class="activity-title">${escapeHtml(reading.node_id)}</div>
                    <div class="activity-meta">
                        ${timeStr} | RSSI: ${reading.rssi} dBm
                    </div>
                    <div class="activity-data">${dataStr}</div>
                </div>
            </div>
        `;
    }).join('');
}

function updateActiveDevices(devices) {
    const container = document.getElementById('activeDevices');

    if (devices.length === 0) {
        container.innerHTML = '<div class="empty-state">Nenhum dispositivo registrado</div>';
        return;
    }

    container.innerHTML = devices.slice(0, 6).map(device => `
        <div class="device-card" onclick="showDeviceDetail('${escapeHtml(device.node_id)}')">
            <div class="device-card-header">
                <div class="device-card-icon">&#128268;</div>
                <div>
                    <div class="device-card-title">${escapeHtml(device.node_id)}</div>
                    <div class="device-card-type">${escapeHtml(device.node_type || 'sensor')}</div>
                </div>
            </div>
            <div class="device-card-stats">
                <span>${device.total_packets} pacotes</span>
                <span>${formatTimeAgo(device.last_seen)}</span>
            </div>
        </div>
    `).join('');
}

// =============================================
// Machines Page - Monitoramento de Maquinas
// =============================================

async function loadMachines() {
    try {
        // Busca leituras recentes para extrair dados das maquinas
        const data = await fetchAPI('/api/readings?limit=500');
        updateConnectionStatus(true);

        console.log('[Machines] Leituras recebidas:', data.readings?.length || 0);

        // Processa leituras para extrair estado mais recente de cada maquina
        const machineReadings = {};

        (data.readings || []).forEach(reading => {
            const machineData = reading.data || {};

            // Verifica se e um dado de maquina (tem machineId ou digitalInputs)
            if (machineData.machineId || machineData.digitalInputs) {
                const machineId = machineData.machineId || reading.node_id;

                // Guarda apenas a leitura mais recente de cada maquina
                if (!machineReadings[machineId]) {
                    machineReadings[machineId] = {
                        ...machineData,
                        node_id: reading.node_id,
                        rssi: reading.rssi,
                        snr: reading.snr,
                        received_at: reading.received_at
                    };
                    console.log('[Machines] Maquina encontrada:', machineId, machineReadings[machineId]);
                }
            }
        });

        machines = machineReadings;
        console.log('[Machines] Total de maquinas:', Object.keys(machines).length);
        renderMachines(machines);

    } catch (error) {
        console.error('Erro ao carregar maquinas:', error);
        document.getElementById('machinesGrid').innerHTML = `
            <div class="empty-state">
                <p>Erro ao carregar maquinas</p>
                <p style="font-size: 14px; margin-top: 8px;">${error.message}</p>
            </div>
        `;
    }
}

function renderMachines(machines) {
    const container = document.getElementById('machinesGrid');

    const machineList = Object.values(machines);

    if (machineList.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <p>Nenhuma maquina detectada</p>
                <p style="font-size: 14px; margin-top: 8px;">
                    Aguardando dados no formato de maquina...
                </p>
            </div>
        `;
        return;
    }

    container.innerHTML = machineList.map(machine => {
        const isOnline = isRecentReading(machine.received_at, 120); // 120 segundos (2 min)
        const statusClass = isOnline ? 'online' : 'offline';
        const statusText = isOnline ? 'Online' : 'Offline';

        // Digital Inputs
        const digitalInputs = machine.digitalInputs || {};
        const digitalHtml = Object.entries(digitalInputs).map(([key, value]) => `
            <div class="digital-input">
                <div class="digital-input-indicator ${value ? 'on' : 'off'}"></div>
                <span class="digital-input-label">${key.toUpperCase()}</span>
            </div>
        `).join('');

        // Analog Inputs
        const analogInputs = machine.analogInputs || {};
        const analogHtml = Object.entries(analogInputs).map(([key, value]) => {
            const percentage = Math.min((value / 4095) * 100, 100); // ADC 12-bit
            return `
                <div class="analog-input">
                    <div class="analog-input-header">
                        <span class="analog-input-label">${key.toUpperCase()}</span>
                        <span class="analog-input-value">${value}</span>
                    </div>
                    <div class="analog-input-bar">
                        <div class="analog-input-fill" style="width: ${percentage}%"></div>
                    </div>
                </div>
            `;
        }).join('');

        // Temperature
        const temp = machine.temperature;
        let tempClass = 'normal';
        if (temp > 50) tempClass = 'hot';
        else if (temp > 35) tempClass = 'warm';

        const tempHtml = temp !== undefined ? `
            <div class="io-section">
                <div class="io-section-title">Temperatura</div>
                <div class="temperature-display">
                    <span class="temperature-icon">🌡️</span>
                    <span class="temperature-value ${tempClass}">
                        ${temp.toFixed(1)}<span class="temperature-unit">°C</span>
                    </span>
                </div>
            </div>
        ` : '';

        // Trigger info
        const trigger = machine.trigger || 'periodic';
        const triggerClass = trigger === 'event' ? 'event' : 'periodic';

        return `
            <div class="machine-card ${isOnline ? '' : 'offline'}">
                <div class="machine-card-header">
                    <div class="machine-info">
                        <div class="machine-icon">⚙️</div>
                        <div>
                            <div class="machine-title">${escapeHtml(machine.machineId || machine.node_id)}</div>
                            <div class="machine-mac">${escapeHtml(machine.macAddress || '-')}</div>
                        </div>
                    </div>
                    <div class="machine-status ${statusClass}">
                        <span class="machine-status-dot"></span>
                        ${statusText}
                    </div>
                </div>

                <div class="machine-card-body">
                    ${Object.keys(digitalInputs).length > 0 ? `
                        <div class="io-section">
                            <div class="io-section-title">Entradas Digitais</div>
                            <div class="digital-inputs">${digitalHtml}</div>
                        </div>
                    ` : ''}

                    ${Object.keys(analogInputs).length > 0 ? `
                        <div class="io-section">
                            <div class="io-section-title">Entradas Analogicas</div>
                            <div class="analog-inputs">${analogHtml}</div>
                        </div>
                    ` : ''}

                    ${tempHtml}
                </div>

                <div class="machine-card-footer">
                    <span>Atualizado: ${formatTimeAgo(machine.received_at)}</span>
                    <span class="machine-trigger ${triggerClass}">
                        ${trigger === 'event' ? '⚡' : '🔄'} ${trigger}
                    </span>
                </div>
            </div>
        `;
    }).join('');
}

function isRecentReading(dateStr, maxSeconds = 120) {
    if (!dateStr) return false;
    try {
        const date = new Date(dateStr);
        const now = new Date();
        const diff = (now - date) / 1000;
        return diff <= maxSeconds;
    } catch (e) {
        return false;
    }
}

// Devices Page
async function loadDevices() {
    try {
        const data = await fetchAPI('/api/devices');
        devices = data.devices || [];
        renderDevicesTable(devices);
    } catch (error) {
        console.error('Erro ao carregar dispositivos:', error);
    }
}

function renderDevicesTable(devices) {
    const tbody = document.getElementById('devicesTableBody');

    if (devices.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="empty-state">Nenhum dispositivo encontrado</td></tr>';
        return;
    }

    tbody.innerHTML = devices.map(device => `
        <tr>
            <td><strong>${escapeHtml(device.node_id)}</strong></td>
            <td>${escapeHtml(device.node_type || 'sensor')}</td>
            <td>${escapeHtml(device.gateway_id || '-')}</td>
            <td>${device.total_packets}</td>
            <td>${formatDateTime(device.first_seen)}</td>
            <td>${formatDateTime(device.last_seen)}</td>
            <td>
                <button class="btn btn-sm btn-secondary" onclick="showDeviceDetail('${escapeHtml(device.node_id)}')">
                    Ver Detalhes
                </button>
            </td>
        </tr>
    `).join('');
}

function refreshDevices() {
    loadDevices();
    showToast('Lista atualizada', 'success');
}

// Readings Page
async function loadReadings() {
    const nodeId = document.getElementById('filterDevice')?.value || '';
    const limit = document.getElementById('filterLimit')?.value || 50;

    try {
        let endpoint = `/api/readings?limit=${limit}`;
        if (nodeId) endpoint += `&node_id=${encodeURIComponent(nodeId)}`;

        const data = await fetchAPI(endpoint);
        readings = data.readings || [];
        renderReadingsTable(readings);
        document.getElementById('readingsCount').textContent = data.count || 0;
    } catch (error) {
        console.error('Erro ao carregar leituras:', error);
    }
}

function renderReadingsTable(readings) {
    const tbody = document.getElementById('readingsTableBody');

    if (readings.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="empty-state">Nenhuma leitura encontrada</td></tr>';
        return;
    }

    tbody.innerHTML = readings.map(reading => {
        const rssiClass = getRssiClass(reading.rssi);
        const dataStr = formatSensorData(reading.data);

        return `
            <tr>
                <td>${formatDateTime(reading.received_at)}</td>
                <td><strong>${escapeHtml(reading.node_id)}</strong></td>
                <td>${escapeHtml(reading.node_type || 'sensor')}</td>
                <td class="data-cell" title="${escapeHtml(JSON.stringify(reading.data))}">${dataStr}</td>
                <td class="${rssiClass}">${reading.rssi} dBm</td>
                <td>${reading.snr.toFixed(1)} dB</td>
            </tr>
        `;
    }).join('');
}

function applyFilters() {
    loadReadings();
    showToast('Filtros aplicados', 'info');
}

async function loadDeviceFilters() {
    try {
        const data = await fetchAPI('/api/devices');
        const devices = data.devices || [];

        const selects = ['filterDevice', 'exportDevice'];
        selects.forEach(selectId => {
            const select = document.getElementById(selectId);
            if (!select) return;

            // Mantem a primeira opcao (Todos)
            const currentValue = select.value;
            select.innerHTML = '<option value="">Todos</option>';

            devices.forEach(device => {
                const option = document.createElement('option');
                option.value = device.node_id;
                option.textContent = device.node_id;
                select.appendChild(option);
            });

            select.value = currentValue;
        });
    } catch (error) {
        console.error('Erro ao carregar filtros:', error);
    }
}

// Device Detail Modal
async function showDeviceDetail(nodeId) {
    const modal = document.getElementById('deviceModal');
    const title = document.getElementById('modalDeviceTitle');
    const body = document.getElementById('modalDeviceBody');

    title.textContent = `Dispositivo: ${nodeId}`;
    body.innerHTML = '<div class="empty-state">Carregando...</div>';
    modal.classList.add('active');

    try {
        const data = await fetchAPI(`/api/node/${encodeURIComponent(nodeId)}/readings?limit=20`);

        if (data.readings.length === 0) {
            body.innerHTML = '<div class="empty-state">Nenhuma leitura encontrada</div>';
            return;
        }

        body.innerHTML = `
            <div style="margin-bottom: 20px;">
                <strong>Total de leituras:</strong> ${data.count}
            </div>
            <div class="table-container">
                <table class="data-table">
                    <thead>
                        <tr>
                            <th>Data/Hora</th>
                            <th>Dados</th>
                            <th>RSSI</th>
                            <th>SNR</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${data.readings.map(r => `
                            <tr>
                                <td>${formatDateTime(r.received_at)}</td>
                                <td class="data-cell">${formatSensorData(r.data)}</td>
                                <td class="${getRssiClass(r.rssi)}">${r.rssi} dBm</td>
                                <td>${r.snr.toFixed(1)} dB</td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;
    } catch (error) {
        body.innerHTML = '<div class="empty-state">Erro ao carregar dados</div>';
    }
}

function closeModal() {
    document.getElementById('deviceModal').classList.remove('active');
}

// Fechar modal com ESC ou clicando fora
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeModal();
});

document.getElementById('deviceModal')?.addEventListener('click', (e) => {
    if (e.target.classList.contains('modal')) closeModal();
});

// Export Functions
async function exportCSV() {
    const nodeId = document.getElementById('exportDevice')?.value || '';

    try {
        let endpoint = '/api/readings?limit=10000';
        if (nodeId) endpoint += `&node_id=${encodeURIComponent(nodeId)}`;

        const data = await fetchAPI(endpoint);

        if (data.readings.length === 0) {
            showToast('Nenhum dado para exportar', 'error');
            return;
        }

        // Gera CSV
        let csv = 'Data/Hora,Node ID,Tipo,Dados,RSSI,SNR\n';
        data.readings.forEach(r => {
            csv += `"${r.received_at}","${r.node_id}","${r.node_type}","${JSON.stringify(r.data).replace(/"/g, '""')}",${r.rssi},${r.snr}\n`;
        });

        downloadFile(csv, 'lora_readings.csv', 'text/csv');
        showToast(`${data.readings.length} registros exportados`, 'success');
    } catch (error) {
        showToast('Erro ao exportar', 'error');
    }
}

async function exportJSON() {
    const limit = document.getElementById('exportLimit')?.value || 100;

    try {
        const endpoint = `/api/readings?limit=${limit === '0' ? 10000 : limit}`;
        const data = await fetchAPI(endpoint);

        if (data.readings.length === 0) {
            showToast('Nenhum dado para exportar', 'error');
            return;
        }

        const json = JSON.stringify(data, null, 2);
        downloadFile(json, 'lora_readings.json', 'application/json');
        showToast(`${data.readings.length} registros exportados`, 'success');
    } catch (error) {
        showToast('Erro ao exportar', 'error');
    }
}

function downloadFile(content, filename, mimeType) {
    const blob = new Blob([content], { type: mimeType });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

// Utility Functions
function formatNumber(num) {
    if (num >= 1000000) return (num / 1000000).toFixed(1) + 'M';
    if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
    return num.toString();
}

function formatDateTime(dateStr) {
    if (!dateStr) return '-';
    const date = new Date(dateStr);
    return date.toLocaleString('pt-BR', {
        day: '2-digit',
        month: '2-digit',
        year: '2-digit',
        hour: '2-digit',
        minute: '2-digit'
    });
}

function formatTimeAgo(dateStr) {
    if (!dateStr) return '-';
    const date = new Date(dateStr);
    const now = new Date();
    const diff = Math.floor((now - date) / 1000);

    if (diff < 60) return 'agora';
    if (diff < 3600) return Math.floor(diff / 60) + 'm atras';
    if (diff < 86400) return Math.floor(diff / 3600) + 'h atras';
    return Math.floor(diff / 86400) + 'd atras';
}

function formatSensorData(data) {
    if (!data || Object.keys(data).length === 0) return '-';

    return Object.entries(data)
        .map(([key, value]) => {
            if (typeof value === 'number') {
                return `${key}: ${value.toFixed ? value.toFixed(1) : value}`;
            }
            return `${key}: ${value}`;
        })
        .join(' | ');
}

function getRssiClass(rssi) {
    if (rssi >= -70) return 'rssi-good';
    if (rssi >= -90) return 'rssi-medium';
    return 'rssi-poor';
}

function escapeHtml(text) {
    if (typeof text !== 'string') return text;
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Toast Notifications
function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `<span>${message}</span>`;
    document.body.appendChild(toast);

    setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}
