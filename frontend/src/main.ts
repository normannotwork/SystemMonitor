interface SystemMetrics {
    cpu: number;
    ram_total: number;
    ram_used: number;
    load: number;
    net_rx: number;
    net_tx: number;
}

class Dashboard {
    private el(id: string): HTMLElement {
        return document.getElementById(id) as HTMLElement;
    }

    public update(data: SystemMetrics) {
        this.el('cpu-val').textContent = data.cpu.toFixed(1) + '%';
        this.el('cpu-bar').style.width = data.cpu + '%';
        this.el('cpu-bar').style.background = data.cpu > 80 ? 'var(--danger)' : 'var(--accent)';
        
        const ramPct = (data.ram_used / data.ram_total) * 100;
        this.el('ram-val').textContent = `${data.ram_used} / ${data.ram_total} MB`;
        this.el('ram-bar').style.width = ramPct + '%';
        this.el('ram-bar').style.background = ramPct > 85 ? 'var(--danger)' : 'var(--accent)';
        
        this.el('load-val').textContent = data.load.toFixed(2);
        this.el('rx-val').textContent = data.net_rx.toFixed(1);
        this.el('tx-val').textContent = data.net_tx.toFixed(1);
    }

    public setStatus(connected: boolean) {
        const statusEl = this.el('status');
        statusEl.textContent = connected ? 'Connected' : 'Disconnected';
        statusEl.className = connected ? 'status ok' : 'status err';
    }
}

function initSSE() {
    const dashboard = new Dashboard();
    const es = new EventSource('/metrics');

    es.onopen = () => dashboard.setStatus(true);
    
    es.onmessage = (e: MessageEvent) => {
        try {
            const data: SystemMetrics = JSON.parse(e.data);
            dashboard.update(data);
        } catch (err) {
            console.error("Parse error:", err);
        }
    };

    es.onerror = () => {
        dashboard.setStatus(false);
        es.close();
        setTimeout(initSSE, 2000);
    };
}

document.addEventListener('DOMContentLoaded', initSSE);
