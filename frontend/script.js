// Módulo principal da aplicação IPC - gerencia a interface e comunicação
class IPCDashboard {
    constructor() {
        this.baseURL = window.location.origin;
        this.methods = {
            pipes: { name: 'Anonymous Pipes', active: false },
            sockets: { name: 'Local Sockets', active: false },
            shared_memory: { name: 'Shared Memory', active: false }
        };
        
        this.messages = [];
        // Inicializa tudo quando a classe é criada
        this.setup();
    }

    // Inicializa o dashboard
    setup() {
        this.bindEvents();
        this.logMessage('System initialized. Ready to start IPC testing.', 'info');
        // Atualizações periódicas
        this.refreshAll();
        this.poller = setInterval(() => this.refreshAll(), 1000);
    }

    // Conecta todos os eventos da interface com suas funções
    bindEvents() {
        // Delegação de eventos para os botões dos cartões IPC
        document.addEventListener('click', (e) => {
            if (e.target.matches('.ipc-card .btn-primary')) {
                this.startMethod(e);
            } else if (e.target.matches('.ipc-card .btn-danger')) {
                this.stopMethod(e);
            } else if (e.target.matches('.message-controls .btn-success')) {
                this.sendMessage();
            } else if (e.target.matches('.log-header .btn-danger')) {
                this.clearLog();
            }
        });

        // Escutador de evento para Enter no campo de mensagem
        const input = document.querySelector('.message-controls input[type="text"]');
        if (input) {
            input.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    this.sendMessage();
                }
            });
        }

        // Sistema de responsividade avançada para diferentes dispositivos
        this.updateLayout();
        window.addEventListener('resize', () => this.updateLayout());
    }

    // Obtém mecanismo a partir do card
    getMethod(card) {
        return card?.dataset?.mech || null;
    }

    // Lida com o clique no botão Start de qualquer método IPC  
    startMethod(event) {
        const card = event.target.closest('.ipc-card');
        const method = this.getMethod(card);
        if (!method) return;
        fetch(`${this.baseURL}/ipc/start/${method}`, { method: 'POST' })
            .then(r => r.json().catch(() => ({})))
            .then(() => {
                this.logMessage(`${this.methods[method].name} started`, 'started');
                // Atualiza UI imediatamente
                const card = document.querySelector(`.ipc-card[data-mech="${method}"]`);
                if (card) this.updateCard(card, true);
                this.methods[method].active = true;
                // Atualiza detalhes após start
                this.refreshDetails(method);
            })
            .catch(() => this.logMessage(`Failed to start ${method}`, 'error'));
    }

    // Lida com o clique no botão Stop de qualquer método IPC
    stopMethod(event) {
        const card = event.target.closest('.ipc-card');
        const method = this.getMethod(card);
        
        if (!method) return;
        fetch(`${this.baseURL}/ipc/stop/${method}`, { method: 'POST' })
            .then(r => r.json().catch(() => ({})))
            .then(() => {
                this.logMessage(`${this.methods[method].name} stopped`, 'stopped');
                // Atualiza UI imediatamente
                const card = document.querySelector(`.ipc-card[data-mech="${method}"]`);
                if (card) this.updateCard(card, false);
                this.methods[method].active = false;
            })
            .catch(() => this.logMessage(`Failed to stop ${method}`, 'error'));
    }

    // Processa o envio de mensagens quando usuário clica Send ou aperta Enter
    sendMessage() {
        const input = document.querySelector('.message-controls input[type="text"]');
        const select = document.querySelector('.message-controls select');
        
        const text = input.value.trim();
        const mapSelect = { pipe: 'pipes', socket: 'sockets', shmem: 'shared_memory' };
        const method = mapSelect[select.value] || '';
        
        // Validações antes de enviar
        if (!text) {
            this.logMessage('Please enter a message to send', 'error');
            return;
        }
        
        if (!method) {
            this.logMessage('Please select an IPC method', 'error');
            return;
        }
        
        if (!this.methods[method]?.active) {
            this.logMessage(`${this.methods[method].name} is not active`, 'error');
            return;
        }
        
        // Processa e envia a mensagem via método IPC selecionado
        this.sendIPCMessage(method, text);
        input.value = '';
    }

    // Envia a mensagem usando o método IPC selecionado e simula resposta
    sendIPCMessage(method, message) {
        const time = new Date().toLocaleTimeString();
        this.logMessage(`[${time}] Sent via ${this.methods[method].name}: ${message}`, 'sent');
        fetch(`${this.baseURL}/ipc/send`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mechanism: method, message })
        })
           .then(r => r.json().catch(() => ({})))
           .then(() => {
               this.logMessage(`[${time}] Backend acknowledged ${this.methods[method].name}`, 'received');
               this.refreshDetails(method);
           })
           .catch(() => this.logMessage(`Send failed on ${method}`, 'error'));
    }

    // Atualiza status e detalhes do backend
    refreshAll() {
        this.fetchStatus().then(() => {
            Object.keys(this.methods).forEach(m => this.refreshDetails(m));
        });
    }

    fetchStatus() {
        return fetch(`${this.baseURL}/ipc/status`, { cache: 'no-store' })
            .then(r => r.json())
            .then(status => {
                if (!status || !Array.isArray(status.mechanisms)) return;
                status.mechanisms.forEach(mech => {
                    const method = mech.name; // 'pipes' | 'sockets' | 'shared_memory'
                    if (this.methods[method] !== undefined) {
                        this.methods[method].active = !!mech.is_active;
                        const card = document.querySelector(`.ipc-card[data-mech="${method}"]`);
                        if (card) this.updateCard(card, !!mech.is_active);
                    }
                });
            })
            .catch(() => {});
    }

    refreshDetails(method) {
        fetch(`${this.baseURL}/ipc/detail/${method}`, { cache: 'no-store' })
            .then(r => r.json())
            .then(detail => this.updateDetailsUI(method, detail))
            .catch(() => {});
    }

    updateDetailsUI(method, detail) {
        const card = document.querySelector(`.ipc-card[data-mech="${method}"]`);
        if (!card || !detail) return;
        const op = detail.last_operation || {};

        const msgEl = card.querySelector('.detail-message');
        const bytesEl = card.querySelector('.detail-bytes');
        const timeEl = card.querySelector('.detail-time');
        const pidsEl = card.querySelector('.detail-pids');
        const syncEl = card.querySelector('.detail-sync');
        const lastmodEl = card.querySelector('.detail-lastmod');

        if (method === 'shared_memory') {
            if (msgEl) msgEl.textContent = (op?.data?.content ?? op?.content ?? '—');
            if (bytesEl) bytesEl.textContent = (op?.data?.size ?? op?.size ?? 0);
            if (timeEl) {
                const t = op?.time_ms ?? 0;
                timeEl.textContent = typeof t === 'number' ? t.toFixed(3) : t;
            }
            if (syncEl) syncEl.textContent = (op?.data?.sync_state ?? op?.sync_state ?? '—');
            if (lastmodEl) lastmodEl.textContent = (op?.data?.last_modified ?? op?.last_modified ?? '—');
        } else {
            if (msgEl) msgEl.textContent = op?.message ?? '—';
            if (bytesEl) bytesEl.textContent = op?.bytes ?? 0;
            if (timeEl) {
                const t = op?.time_ms ?? 0;
                timeEl.textContent = typeof t === 'number' ? t.toFixed(3) : t;
            }
            if (pidsEl) pidsEl.textContent = `${op?.sender_pid ?? '—'} → ${op?.receiver_pid ?? '—'}`;
        }
    }

    // Atualiza visualmente o status do cartão (ativo/inativo)
    updateCard(card, active) {
        const status = card.querySelector('.status');
        const startBtn = card.querySelector('.btn-primary');
        const stopBtn = card.querySelector('.btn-danger');
        
        if (active) {
            status.textContent = 'Online';
            status.className = 'status active';
            card.classList.add('active');
            startBtn.disabled = true;
            stopBtn.disabled = false;
        } else {
            status.textContent = 'Offline';
            status.className = 'status inactive';
            card.classList.remove('active');
            startBtn.disabled = false;
            stopBtn.disabled = true;
        }
    }

    // Adiciona uma nova mensagem ao log de comunicação
    logMessage(message, type = 'info') {
        const log = document.querySelector('.log-content');
        if (!log) return;

        const entry = document.createElement('div');
        entry.className = `log-entry ${type}`;
        entry.textContent = message;
        
        log.appendChild(entry);
        log.scrollTop = log.scrollHeight;
        
        this.messages.push({ message, type, time: Date.now() });
        
        // Limita número de entradas do log para manter performance
        if (this.messages.length > 500) {
            this.messages = this.messages.slice(-250);
            this.rebuildLog();
        }
    }

    // Limpa todo o log de mensagens
    clearLog() {
        const log = document.querySelector('.log-content');
        if (log) {
            log.innerHTML = '';
            this.messages = [];
            this.logMessage('Log cleared', 'info');
        }
    }

    // Reconstrói o log quando há muitas entradas (otimização)
    rebuildLog() {
        const log = document.querySelector('.log-content');
        if (!log) return;
        
        log.innerHTML = '';
        this.messages.forEach(entry => {
            const div = document.createElement('div');
            div.className = `log-entry ${entry.type}`;
            div.textContent = entry.message;
            log.appendChild(div);
        });
        log.scrollTop = log.scrollHeight;
    }

    // Detecta o tipo de dispositivo e aplica otimizações correspondentes
    updateLayout() {
        const width = window.innerWidth;
        
        if (width <= 480) {
            this.mobileMode();
        } else if (width <= 768) {
            this.tabletMode();
        } else {
            this.desktopMode();
        }
    }

    // Ajustes específicos para telas pequenas de celular
    mobileMode() {
        const log = document.querySelector('.log-content');
        if (log) log.style.height = '200px';
        document.body.style.touchAction = 'manipulation';
    }

    // Ajustes específicos para tablets
    tabletMode() {
        const log = document.querySelector('.log-content');
        if (log) log.style.height = '280px';
    }

    // Ajustes específicos para desktops e telas grandes
    desktopMode() {
        const log = document.querySelector('.log-content');
        if (log) log.style.height = '320px';
        document.body.style.touchAction = 'auto';
    }
}

// Módulo de utilidades para detecção e adaptação a diferentes dispositivos
// Classe com funções úteis para detectar tipo de dispositivo
class DeviceUtils {
    // Verifica se é um dispositivo móvel
    static isMobile() {
        return window.innerWidth <= 480 || /Android|iPhone|iPad|iPod|BlackBerry/i.test(navigator.userAgent);
    }

    // Verifica se é um tablet
    static isTablet() {
        return window.innerWidth <= 768 && window.innerWidth > 480;
    }

    // Verifica se o dispositivo tem tela sensível ao toque
    static hasTouch() {
        return 'ontouchstart' in window || navigator.maxTouchPoints > 0;
    }

    // Descobre se a tela está em retrato ou paisagem
    static getOrientation() {
        return window.innerHeight > window.innerWidth ? 'portrait' : 'landscape';
    }
}

// Gerencia temas claro e escuro automaticamente
class ThemeManager {
    constructor() {
        this.current = this.detectTheme();
        this.apply();
    }

    // Detecta se o usuário prefere tema escuro
    detectTheme() {
        if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
            return 'dark';
        }
        return 'light';
    }

    // Aplica o tema selecionado na interface
    apply() {
        document.body.classList.toggle('dark-theme', this.current === 'dark');
        document.body.classList.toggle('light-theme', this.current === 'light');
    }

    // Alterna entre tema claro e escuro
    toggle() {
        this.current = this.current === 'dark' ? 'light' : 'dark';
        this.apply();
    }
}

// Inicializa a aplicação quando o DOM estiver completamente carregado
document.addEventListener('DOMContentLoaded', () => {
    // Verifica se todos os elementos necessários estão presentes no DOM
    const required = ['.container', '.ipc-section', '.message-controls', '.log-content'];
    const hasAll = required.every(sel => document.querySelector(sel));
    
    if (hasAll) {
        window.dashboard = new IPCDashboard();
        window.themes = new ThemeManager();
        
        // Informações de debug apenas em ambiente de desenvolvimento
        if (window.location.hostname === 'localhost') {
            console.log('IPC Dashboard initialized');
            console.log('Device info:', {
                mobile: DeviceUtils.isMobile(),
                tablet: DeviceUtils.isTablet(),
                touch: DeviceUtils.hasTouch(),
                orientation: DeviceUtils.getOrientation(),
                size: `${window.innerWidth}x${window.innerHeight}`
            });
        }
    } else {
        console.error('Required DOM elements not found');
    }
});
