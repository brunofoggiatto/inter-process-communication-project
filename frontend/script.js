// Módulo principal da aplicação IPC - gerencia a interface e comunicação
class IPCDashboard {
    constructor() {
        this.methods = {
            pipe: { name: 'Anonymous Pipes', active: false },
            socket: { name: 'Local Sockets', active: false },
            shmem: { name: 'Shared Memory', active: false }
        };
        
        this.messages = [];
        // Inicializa tudo quando a classe é criada
        this.setup();
    }

    // Inicializa o dashboard
    setup() {
        this.bindEvents();
        this.logMessage('System initialized. Ready to start IPC testing.', 'info');
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

    // Lida com o clique no botão Start de qualquer método IPC  
    startMethod(event) {
        const card = event.target.closest('.ipc-card');
        const method = this.getMethod(card);
        
        if (method && !this.methods[method].active) {
            this.methods[method].active = true;
            this.updateCard(card, true);
            this.logMessage(`${this.methods[method].name} started successfully`, 'info');
            
            // Simula conexão - em produção seria uma chamada real para o backend
            this.connectMethod(method);
        }
    }

    // Lida com o clique no botão Stop de qualquer método IPC
    stopMethod(event) {
        const card = event.target.closest('.ipc-card');
        const method = this.getMethod(card);
        
        if (method && this.methods[method].active) {
            this.methods[method].active = false;
            this.updateCard(card, false);
            this.logMessage(`${this.methods[method].name} stopped`, 'info');
        }
    }

    // Processa o envio de mensagens quando usuário clica Send ou aperta Enter
    sendMessage() {
        const input = document.querySelector('.message-controls input[type="text"]');
        const select = document.querySelector('.message-controls select');
        
        const text = input.value.trim();
        const method = select.value;
        
        // Validações antes de enviar
        if (!text) {
            this.logMessage('Please enter a message to send', 'error');
            return;
        }
        
        if (!method) {
            this.logMessage('Please select an IPC method', 'error');
            return;
        }
        
        if (!this.methods[method].active) {
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
        
        // Simula resposta do servidor - em produção viria do backend real
        setTimeout(() => {
            const response = this.createResponse(method, message);
            this.logMessage(`[${time}] Received via ${this.methods[method].name}: ${response}`, 'received');
        }, 300 + Math.random() * 800);
    }

    // Gera uma resposta simulada para demonstração - em produção viria do servidor
    createResponse(method, original) {
        const responses = [
            `Echo: ${original}`,
            `Processed: ${original.toUpperCase()}`,
            `Response from ${method}: Message received`,
            `Acknowledgment: ${original.length} characters processed`
        ];
        return responses[Math.floor(Math.random() * responses.length)];
    }

    // Simula o processo de conexão com delay realista
    connectMethod(method) {
        setTimeout(() => {
            this.logMessage(`${this.methods[method].name} connection established`, 'received');
        }, 800);
    }

    // Descobre qual método IPC baseado no cartão que foi clicado
    getMethod(card) {
        const title = card.querySelector('h3').textContent;
        if (title.includes('Pipes')) return 'pipe';
        if (title.includes('Sockets')) return 'socket';  
        if (title.includes('Memory')) return 'shmem';
        return null;
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