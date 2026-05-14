// Типы данных от сервера (можно расширять)
interface SystemMetrics {
    cpu: number;
    ram_total: number;
    ram_used: number;
    load: number;
    net_rx: number;
    net_tx: number;
    load5?: number;
    load15?: number;
    cpu_cores?: number;
    kernel_version?: string;
}

class Dashboard {
    // Кэш DOM‑элементов для быстрого доступа
    private els: Record<string, HTMLElement | null> = {};

    constructor() {
        const ids = [
            'cpu-val', 'cpu-bar', 'cpu-cores',
            'ram-perc', 'ram-bar', 'ram-val',
            'load-val', 'load-5', 'load-15',
            'rx-val', 'tx-val',
            'kernel-ver', 'status', 'net-pulse'
        ];
        ids.forEach(id => { this.els[id] = document.getElementById(id); });
    }

    private safeSetText(id: string, text: string): void {
        const el = this.els[id];
        if (el) el.textContent = text;
    }

    private safeSetStyle(id: string, prop: string, value: string): void {
        const el = this.els[id] as HTMLElement;
        if (el) (el.style as any)[prop] = value;
    }

    /** Обновляет все виджеты новыми данными */
    update(data: SystemMetrics): void {
        // --- CPU ---
        const cpu = data.cpu || 0;
        this.safeSetText('cpu-val', cpu.toFixed(1) + '%');
        this.safeSetStyle('cpu-bar', 'width', cpu + '%');
        if (this.els['cpu-bar']) {
            const bar = this.els['cpu-bar'] as HTMLElement;
            bar.style.background = cpu > 80 ? 'var(--danger)' : 'var(--accent)';
        }

        // --- RAM ---
        const ramTotal = data.ram_total || 1;
        const ramUsed = data.ram_used || 0;
        const ramPct = (ramUsed / ramTotal) * 100;
        this.safeSetText('ram-perc', ramPct.toFixed(1) + '%');
        this.safeSetText('ram-val', ramUsed + ' / ' + ramTotal + ' MB');
        this.safeSetStyle('ram-bar', 'width', ramPct + '%');
        if (this.els['ram-bar']) {
            const bar = this.els['ram-bar'] as HTMLElement;
            bar.style.background = ramPct > 85 ? 'var(--danger)' : 'var(--accent)';
        }

        // --- Load Averages ---
        this.safeSetText('load-val', (data.load || 0).toFixed(2));
        

        // --- Network ---
        this.safeSetText('rx-val', (data.net_rx || 0).toFixed(1));
        this.safeSetText('tx-val', (data.net_tx || 0).toFixed(1));

        // Анимация пульса сети (ширина мини‑бара)
        const pulse = this.els['net-pulse'];
        if (pulse) {
            const maxVal = Math.max(data.net_rx || 0, data.net_tx || 0, 1);
            // Масштаб: считаем, что 100 KB/s = 100% ширины
            const scale = Math.min((maxVal / 100) * 100, 100);
            pulse.style.width = scale + '%';
        }
    }

    /** Устанавливает статус подключения (зелёный / красный) */
    setStatus(connected: boolean): void {
        const statusEl = this.els['status'];
        if (!statusEl) return;
        statusEl.textContent = connected ? 'Connected' : 'Disconnected';
        statusEl.className = connected ? 'status ok' : 'status err';
    }
}

// ---------- Логика подключения ----------
function initMonitoring(): void {
    const dashboard = new Dashboard();
    const endpoint = '/metrics';

    let eventSource: EventSource | null = null;
    let pollTimer: number | null = null;

    // === Попытка SSE ===
    function connectSSE(): void {
        // Если браузер вообще не знает EventSource – сразу переходим на polling
        if (typeof EventSource === 'undefined') {
            startPolling();
            return;
        }

        eventSource = new EventSource(endpoint);

        eventSource.onopen = function () {
            dashboard.setStatus(true);
        };

        eventSource.onmessage = function (e: MessageEvent) {
            try {
                const data: SystemMetrics = JSON.parse(e.data);
                dashboard.update(data);
            } catch (err) {
                console.error('Parse error:', err);
            }
        };

        eventSource.onerror = function () {
            dashboard.setStatus(false);
            if (eventSource) {
                eventSource.close();
                eventSource = null;
            }
            // Переключаемся на polling при любой ошибке SSE
            startPolling();
        };
    }

    // === Резервный вариант: fetch‑polling каждые 2 секунды ===
    function startPolling(): void {
        if (pollTimer !== null) return; // уже работает

        dashboard.setStatus(false);

        const poll = function () {
            fetch(endpoint)
                .then(function (response) {
                    if (!response.ok) throw new Error('Network response was not ok');
                    return response.json();
                })
                .then(function (data: SystemMetrics) {
                    dashboard.update(data);
                    // Если SSE не активно, показываем, что данные всё же поступают
                    if (!eventSource) {
                        dashboard.setStatus(true);
                    }
                })
                .catch(function (err) {
                    console.error('Polling error:', err);
                    dashboard.setStatus(false);
                });
        };

        // Первый запрос сразу, затем по интервалу
        poll();
        pollTimer = window.setInterval(poll, 2000);
    }

    // Запускаем мониторинг
    connectSSE();
}

// Старт после полной загрузки DOM
document.addEventListener('DOMContentLoaded', initMonitoring);
