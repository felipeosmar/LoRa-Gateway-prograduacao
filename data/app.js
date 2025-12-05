// Gateway LoRa Dashboard - JavaScript
// Atualiza o dashboard via API REST

const API_BASE = '';
const UPDATE_INTERVAL = 2000; // 2 segundos
const MAX_LOG_ENTRIES = 20;

// Estado da aplicacao
let isConnected = false;
let lastPackets = [];
let timeSynced = false;

// Elementos DOM
const elements = {
    connectionStatus: null,
    statusText: null,
    gatewayId: null,
    uptime: null,
    gatewayTime: null,
    wifiRssi: null,
    freeHeap: null,
    packetsReceived: null,
    packetsForwarded: null,
    packetsError: null,
    successRate: null,
    loraFreq: null,
    loraSF: null,
    loraBW: null,
    loraCR: null,
    loraTXPower: null,
    loraSyncWord: null,
    devicesTableBody: null,
    packetsLog: null,
    lastUpdate: null
};

// Inicializacao
document.addEventListener('DOMContentLoaded', function() {
    initElements();
    syncTime();  // Sincroniza tempo na primeira conexao
    startUpdates();
});

function initElements() {
    for (const key in elements) {
        elements[key] = document.getElementById(key);
    }
}

// Sincroniza o relogio do ESP32 com o navegador
async function syncTime() {
    try {
        // Primeiro verifica se ja esta sincronizado
        const checkResponse = await fetch(API_BASE + '/api/time');
        if (checkResponse.ok) {
            const timeData = await checkResponse.json();
            if (timeData.synced) {
                console.log('Tempo ja sincronizado no ESP32');
                timeSynced = true;
                return;
            }
        }

        // Envia timestamp atual do navegador
        const timestamp = Math.floor(Date.now() / 1000);
        const formData = new FormData();
        formData.append('timestamp', timestamp.toString());

        const response = await fetch(API_BASE + '/api/time', {
            method: 'POST',
            body: formData
        });

        if (response.ok) {
            const data = await response.json();
            if (data.success) {
                console.log('Tempo sincronizado com sucesso!', new Date(data.synced_time * 1000));
                timeSynced = true;
            }
        }
    } catch (error) {
        console.error('Erro ao sincronizar tempo:', error);
    }
}

function startUpdates() {
    // Primeira atualizacao imediata
    fetchStats();
    fetchDevices();

    // Atualizacoes periodicas
    setInterval(fetchStats, UPDATE_INTERVAL);
    setInterval(fetchDevices, UPDATE_INTERVAL * 2);
}

// Busca estatisticas do gateway
async function fetchStats() {
    try {
        const response = await fetch(API_BASE + '/api/stats');
        if (!response.ok) throw new Error('Erro HTTP: ' + response.status);

        const data = await response.json();
        updateStats(data);
        setConnectionStatus(true);
    } catch (error) {
        console.error('Erro ao buscar stats:', error);
        setConnectionStatus(false);
    }
}

// Busca lista de dispositivos
async function fetchDevices() {
    try {
        const response = await fetch(API_BASE + '/api/devices');
        if (!response.ok) throw new Error('Erro HTTP: ' + response.status);

        const data = await response.json();
        updateDevicesTable(data.devices || [], data.uptime_ms || 0);
        updatePacketsLog(data.lastPackets || [], data.uptime_ms || 0, data.time_synced, data.boot_time || 0);
    } catch (error) {
        console.error('Erro ao buscar devices:', error);
    }
}

// Atualiza status de conexao
function setConnectionStatus(connected) {
    isConnected = connected;
    const statusEl = elements.connectionStatus;
    const textEl = elements.statusText;

    if (statusEl && textEl) {
        statusEl.className = 'status-indicator ' + (connected ? 'connected' : 'disconnected');
        textEl.textContent = connected ? 'Conectado' : 'Desconectado';
    }
}

// Atualiza estatisticas na interface
function updateStats(data) {
    if (elements.gatewayId) {
        elements.gatewayId.textContent = data.gateway_id || '--';
    }

    if (elements.uptime) {
        elements.uptime.textContent = formatUptime(data.uptime_s || 0);
    }

    if (elements.gatewayTime) {
        if (data.time_synced && data.current_time) {
            const date = new Date(data.current_time * 1000);
            elements.gatewayTime.textContent = date.toLocaleTimeString('pt-BR');
            elements.gatewayTime.title = date.toLocaleDateString('pt-BR');
        } else {
            elements.gatewayTime.textContent = 'Nao sincronizado';
            elements.gatewayTime.title = 'Aguardando sincronizacao';
        }
    }

    if (elements.wifiRssi) {
        const rssi = data.wifi_rssi || 0;
        elements.wifiRssi.textContent = rssi + ' dBm';
        elements.wifiRssi.className = getRssiClass(rssi);
    }

    if (elements.freeHeap) {
        const heapKB = Math.round((data.free_heap || 0) / 1024);
        elements.freeHeap.textContent = heapKB + ' KB';
    }

    // Estatisticas de pacotes
    const rx = data.packets_rx || 0;
    const fwd = data.packets_fwd || 0;
    const err = data.packets_err || 0;

    if (elements.packetsReceived) {
        elements.packetsReceived.textContent = rx;
    }
    if (elements.packetsForwarded) {
        elements.packetsForwarded.textContent = fwd;
    }
    if (elements.packetsError) {
        elements.packetsError.textContent = err;
    }
    if (elements.successRate) {
        const rate = rx > 0 ? Math.round((fwd / rx) * 100) : 0;
        elements.successRate.textContent = rate + '%';
    }

    // Configuracao LoRa
    if (data.lora) {
        if (elements.loraFreq) {
            elements.loraFreq.textContent = (data.lora.freq / 1000000).toFixed(1) + ' MHz';
        }
        if (elements.loraSF) {
            elements.loraSF.textContent = 'SF' + data.lora.sf;
        }
        if (elements.loraBW) {
            elements.loraBW.textContent = (data.lora.bw / 1000) + ' kHz';
        }
        if (elements.loraCR) {
            elements.loraCR.textContent = '4/' + data.lora.cr;
        }
        if (elements.loraTXPower) {
            elements.loraTXPower.textContent = data.lora.tx_power + ' dBm';
        }
        if (elements.loraSyncWord) {
            elements.loraSyncWord.textContent = '0x' + data.lora.sync_word.toString(16).toUpperCase();
        }
    }

    // Atualiza timestamp
    if (elements.lastUpdate) {
        elements.lastUpdate.textContent = new Date().toLocaleTimeString('pt-BR');
    }
}

// Atualiza tabela de dispositivos
function updateDevicesTable(devices, uptimeMs) {
    if (!elements.devicesTableBody) return;

    if (devices.length === 0) {
        elements.devicesTableBody.innerHTML =
            '<tr><td colspan="6" class="no-data">Nenhum dispositivo detectado</td></tr>';
        return;
    }

    let html = '';
    devices.forEach(device => {
        const rssiClass = getRssiClass(device.rssi);
        // Calcula diferenca em segundos entre uptime atual e ultimo contato
        const diffSeconds = Math.floor((uptimeMs - device.last_seen_ms) / 1000);
        const lastSeen = formatTimeDiff(diffSeconds);

        html += `
            <tr>
                <td><strong>${escapeHtml(device.id)}</strong></td>
                <td>${escapeHtml(device.type || 'sensor')}</td>
                <td class="${rssiClass}">${device.rssi} dBm</td>
                <td>${device.snr.toFixed(1)} dB</td>
                <td>${device.packets}</td>
                <td>${lastSeen}</td>
            </tr>
        `;
    });

    elements.devicesTableBody.innerHTML = html;
}

// Atualiza log de pacotes
function updatePacketsLog(packets, uptimeMs, timeSynced, bootTime) {
    if (!elements.packetsLog) return;

    if (packets.length === 0) {
        elements.packetsLog.innerHTML = '<div class="log-empty">Aguardando pacotes...</div>';
        return;
    }

    let html = '';
    packets.slice(0, MAX_LOG_ENTRIES).forEach(pkt => {
        let timeStr = '--:--:--';

        if (timeSynced && bootTime > 0) {
            // Calcula timestamp Unix do pacote: boot_time + (timestamp_ms / 1000)
            const packetUnixTime = bootTime + Math.floor(pkt.timestamp_ms / 1000);
            const date = new Date(packetUnixTime * 1000);
            timeStr = date.toLocaleTimeString('pt-BR');
        } else {
            // Tempo nao sincronizado - mostra tempo desde boot
            const secondsSinceBoot = Math.floor(pkt.timestamp_ms / 1000);
            timeStr = formatUptime(secondsSinceBoot);
        }

        const rssiClass = getRssiClass(pkt.rssi);

        html += `
            <div class="log-entry">
                <span class="log-time">${timeStr}</span>
                <span class="log-node">${escapeHtml(pkt.node_id)}</span>
                <span class="log-data">${escapeHtml(JSON.stringify(pkt.data || {}))}</span>
                <span class="log-rf ${rssiClass}">RSSI: ${pkt.rssi} | SNR: ${pkt.snr.toFixed(1)}</span>
            </div>
        `;
    });

    elements.packetsLog.innerHTML = html;
}

// Utilitarios
function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (days > 0) {
        return `${days}d ${hours}h ${mins}m`;
    } else if (hours > 0) {
        return `${hours}h ${mins}m ${secs}s`;
    } else if (mins > 0) {
        return `${mins}m ${secs}s`;
    } else {
        return `${secs}s`;
    }
}

function formatTimeSince(timestamp) {
    if (!timestamp) return '--';

    const now = Date.now() / 1000;
    const diff = Math.floor(now - timestamp);

    if (diff < 60) return diff + 's atras';
    if (diff < 3600) return Math.floor(diff / 60) + 'm atras';
    if (diff < 86400) return Math.floor(diff / 3600) + 'h atras';
    return Math.floor(diff / 86400) + 'd atras';
}

// Formata diferenca de tempo em segundos (para ultimo contato)
function formatTimeDiff(diffSeconds) {
    if (diffSeconds < 0) return '--';

    if (diffSeconds < 60) return diffSeconds + 's atras';
    if (diffSeconds < 3600) return Math.floor(diffSeconds / 60) + 'm atras';
    if (diffSeconds < 86400) return Math.floor(diffSeconds / 3600) + 'h atras';
    return Math.floor(diffSeconds / 86400) + 'd atras';
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
