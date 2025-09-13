class BMCU370WebInterface {
    constructor() {
        this.ws = null;
        this.wsReconnectTimer = null;
        this.isConnected = false;
        this.currentTab = 'dashboard';
        this.statusData = null;
        this.configData = null;
        this.initialConfig = {};
        this.configValidation = {};
        
        this.init();
    }

    init() {
        this.setupEventListeners();
        this.connectWebSocket();
        this.loadInitialData();
        this.startPeriodicUpdates();
    }

    setupEventListeners() {
        // Tab navigation
        document.querySelectorAll('.tab-button').forEach(button => {
            button.addEventListener('click', (e) => {
                this.switchTab(e.target.dataset.tab);
            });
        });

        // Configuration controls
        this.setupConfigurationControls();
        
        // Diagnostic controls
        this.setupDiagnosticControls();
        
        // Network controls
        this.setupNetworkControls();

        // Firmware controls
        this.setupFirmwareControls();

        // Toast close
        document.getElementById('toastClose').addEventListener('click', () => {
            this.hideToast();
        });
    }

    setupConfigurationControls() {
        this.configInputs = [
            document.getElementById('mainLedBrightness'),
            document.getElementById('channelLedBrightness'),
            document.getElementById('voltageHigh'),
            document.getElementById('voltageLow'),
            document.getElementById('motionSendTime'),
            document.getElementById('motionFilterK'),
        ];

        this.configInputs.forEach(input => {
            const isSlider = input.type === 'range';
            const eventType = isSlider ? 'input' : 'change';

            input.addEventListener(eventType, (e) => {
                if (isSlider) {
                    document.getElementById(`${e.target.id}Value`).textContent = e.target.value;
                }
                this.validateAndCheckChanges();
            });
        });

        document.getElementById('saveConfig').addEventListener('click', () => {
            this.saveConfiguration();
        });

        document.getElementById('resetConfig').addEventListener('click', () => {
            this.confirmAction('Are you sure you want to reset all configuration to defaults?', () => {
                this.resetConfiguration();
                this.validateAndCheckChanges();
            });
        });
    }

    validateField(input) {
        const value = parseFloat(input.value);
        const min = parseFloat(input.min);
        const max = parseFloat(input.max);
        let isValid = true;

        if (isNaN(value)) {
            isValid = false;
        } else if (value < min || value > max) {
            isValid = false;
        }

        if (isValid) {
            input.classList.remove('input-error');
        } else {
            input.classList.add('input-error');
        }

        this.configValidation[input.id] = isValid;
        return isValid;
    }

    hasConfigChanged() {
        return this.configInputs.some(input => {
            return String(this.initialConfig[input.id]) !== String(input.value);
        });
    }

    validateAndCheckChanges() {
        let allFieldsValid = true;
        this.configInputs.forEach(input => {
            if (!this.validateField(input)) {
                allFieldsValid = false;
            }
        });

        const hasChanged = this.hasConfigChanged();
        document.getElementById('saveConfig').disabled = !allFieldsValid || !hasChanged;
    }

    setupDiagnosticControls() {
        document.getElementById('resetBmcu').addEventListener('click', () => {
            this.confirmAction('Reset BMCU370?', () => this.systemControl('reset_bmcu370'));
        });

        document.getElementById('enterDfu').addEventListener('click', () => {
            this.confirmAction('Enter DFU Mode? Device will disconnect.', () => this.systemControl('dfu_mode'));
        });

        document.getElementById('resetEsp32').addEventListener('click', () => {
            this.confirmAction('Reset ESP32? This will restart the interface.', () => this.systemControl('reset_esp32'));
        });

        document.getElementById('refreshLogs').addEventListener('click', () => {
            this.refreshLogs();
        });
    }

    setupNetworkControls() {
        document.getElementById('scanNetworks').addEventListener('click', () => {
            this.scanWiFiNetworks();
        });

        document.getElementById('wifiForm').addEventListener('submit', (e) => {
            e.preventDefault();
            this.connectToWiFi();
        });
    }

    setupFirmwareControls() {
        const uploadArea = document.getElementById('uploadArea');
        const fileInput = document.getElementById('firmwareFile');
        const uploadButton = document.getElementById('uploadFirmware');
        const abortButton = document.getElementById('abortUpload');
        const clearButton = document.getElementById('clearFile');

        // File input and drag-and-drop
        uploadArea.addEventListener('click', () => fileInput.click());
        uploadArea.addEventListener('dragover', (e) => {
            e.preventDefault();
            uploadArea.classList.add('dragover');
        });
        uploadArea.addEventListener('dragleave', () => {
            uploadArea.classList.remove('dragover');
        });
        uploadArea.addEventListener('drop', (e) => {
            e.preventDefault();
            uploadArea.classList.remove('dragover');
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                this.handleFileSelection(files[0]);
            }
        });

        fileInput.addEventListener('change', (e) => {
            if (e.target.files.length > 0) {
                this.handleFileSelection(e.target.files[0]);
            }
        });

        clearButton.addEventListener('click', () => {
            this.clearSelectedFile();
        });

        uploadButton.addEventListener('click', () => {
            this.uploadFirmware();
        });

        abortButton.addEventListener('click', () => {
            this.abortUpload();
        });
    }

    switchTab(tabName) {
        // Update tab buttons
        document.querySelectorAll('.tab-button').forEach(button => {
            button.classList.remove('active');
        });
        document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');

        // Update tab content
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.remove('active');
        });
        document.getElementById(tabName).classList.add('active');

        this.currentTab = tabName;

        // Load tab-specific data
        if (tabName === 'diagnostics') {
            this.refreshLogs();
        } else if (tabName === 'network') {
            this.updateNetworkStatus();
        } else if (tabName === 'firmware') {
            this.updateFirmwareInfo();
        }
    }

    connectWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/ws`;
        
        this.ws = new WebSocket(wsUrl);
        
        this.ws.onopen = () => {
            console.log('WebSocket connected');
            this.updateConnectionStatus(true);
            this.hideLoading();
            
            // Clear reconnect timer
            if (this.wsReconnectTimer) {
                clearTimeout(this.wsReconnectTimer);
                this.wsReconnectTimer = null;
            }
        };
        
        this.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                this.handleWebSocketMessage(data);
            } catch (error) {
                console.error('Error parsing WebSocket message:', error);
            }
        };
        
        this.ws.onclose = () => {
            console.log('WebSocket disconnected');
            this.updateConnectionStatus(false);
            this.scheduleReconnect();
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.updateConnectionStatus(false);
        };
    }

    scheduleReconnect() {
        if (this.wsReconnectTimer) return;
        
        this.wsReconnectTimer = setTimeout(() => {
            console.log('Attempting to reconnect WebSocket...');
            this.connectWebSocket();
        }, 3000);
    }

    handleWebSocketMessage(data) {
        if (data.error) {
            this.showToast(data.error, 'error');
            return;
        }
        
        // Handle different message types
        switch (data.type) {
            case 'status':
                this.statusData = data;
                this.updateDashboard();
                break;
            case 'wifi_scan_result':
                this.hideLoading();
                this.displayNetworks(data.networks || []);
                this.showToast(`Found ${data.networks.length || 0} networks`, 'success');
                break;
            case 'ota_progress':
                if (this.currentTab === 'firmware') {
                    this.updateUploadProgress(data.progress, `Uploading... ${data.progress}%`);
                }
                break;
            default:
                if (data.system) { // For backward compatibility with general status pushes
                    this.statusData = data;
                    this.updateDashboard();
                }
                break;
        }
    }

    updateConnectionStatus(connected) {
        this.isConnected = connected;
        const statusDot = document.getElementById('statusDot');
        const statusText = document.getElementById('statusText');
        
        if (connected) {
            statusDot.className = 'status-dot connected';
            statusText.textContent = 'Connected';
        } else {
            statusDot.className = 'status-dot';
            statusText.textContent = 'Disconnected';
        }
    }

    async loadInitialData() {
        this.showLoading();
        
        try {
            // Load status
            const statusResponse = await this.apiCall('/api/status');
            if (statusResponse) {
                this.statusData = statusResponse;
                this.updateDashboard();
            }
            
            // Load configuration
            const configResponse = await this.apiCall('/api/config');
            if (configResponse) {
                this.configData = configResponse;
                this.updateConfigurationUI();
            }
        } catch (error) {
            console.error('Error loading initial data:', error);
            this.showToast('Failed to load initial data', 'error');
        } finally {
            this.hideLoading();
        }
    }

    async apiCall(endpoint, options = {}) {
        try {
            const response = await fetch(endpoint, {
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                },
                ...options
            });
            
            if (!response.ok) {
                if (response.status === 429) {
                    throw new Error('HTTP 429: Too Many Requests');
                }
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            // Handle empty responses
            const text = await response.text();
            return text ? JSON.parse(text) : {};

        } catch (error) {
            console.error('API call failed:', error);
            if (error.message.includes('429')) {
                this.showToast('Too many requests. Please wait a moment.', 'warning');
            } else {
                this.showToast(`API Error: ${error.message}`, 'error');
            }
            return null;
        }
    }

    updateDashboard() {
        if (!this.statusData) return;

        const { system, channels } = this.statusData;
        const isBmcuConnected = system && (system.bambubus_status === 'online' || system.bambubus_status === 'unreachable');
        
        if (isBmcuConnected && system.bambubus_status !== 'unreachable') {
            document.getElementById('systemVersion').textContent = system.version || '--';
            document.getElementById('systemUptime').textContent = this.formatUptime(system.uptime || 0);
            document.getElementById('deviceType').textContent = system.device_type || '--';
        } else {
            document.getElementById('systemVersion').textContent = 'N/A';
            document.getElementById('systemUptime').textContent = 'N/A';
            document.getElementById('deviceType').textContent = 'N/A';
        }

        const bambuBusStatus = document.getElementById('bambuBusStatus');
        if (system.bambubus_status) {
            bambuBusStatus.textContent = system.bambubus_status;
            bambuBusStatus.className = `status-badge ${system.bambubus_status}`;
        } else {
            bambuBusStatus.textContent = 'offline';
            bambuBusStatus.className = 'status-badge offline';
        }

        this.updateChannelsDisplay(channels || []);
    }

    updateChannelsDisplay(channels) {
        const channelsGrid = document.getElementById('channelsGrid');
        const channelsDetail = document.getElementById('channelsDetail');
        
        // Clear existing content
        channelsGrid.innerHTML = '';
        channelsDetail.innerHTML = '';
        
        channels.forEach((channel, index) => {
            // Create channel card
            const card = this.createChannelCard(channel, index);
            channelsGrid.appendChild(card);
            
            // Create channel detail
            const detail = this.createChannelDetail(channel, index);
            channelsDetail.appendChild(detail);
        });
    }

    createChannelCard(channel, index) {
        const div = document.createElement('div');
        div.className = 'channel-card';
        div.onclick = () => this.selectChannel(index);
        
        const isOnline = channel.filament && channel.filament.status === 'online';
        
        div.innerHTML = `
            <div class="channel-header">
                <span class="channel-id">Channel ${channel.id}</span>
                <span class="channel-status ${isOnline ? 'online' : ''}"></span>
            </div>
            <div class="channel-info">
                <div>${channel.filament ? channel.filament.name || 'Unknown' : 'No Filament'}</div>
                <div style="font-size: 0.9em; color: #666;">
                    ${channel.motion ? channel.motion.state || 'Unknown' : 'No Motion Data'}
                </div>
            </div>
        `;
        
        return div;
    }

    createChannelDetail(channel, index) {
        const div = document.createElement('div');
        div.className = 'channel-detail';
        div.id = `channel-${index}-detail`;
        div.style.display = index === 0 ? 'block' : 'none';
        
        const filament = channel.filament || {};
        const motion = channel.motion || {};
        const rgb = channel.rgb || {};
        const sensors = channel.sensors || {};
        
        div.innerHTML = `
            <h4><i class="fas fa-info-circle"></i> Channel ${channel.id} Details</h4>
            <div class="detail-grid">
                <div class="detail-section">
                    <h4><i class="fas fa-spool"></i> Filament</h4>
                    <div class="info-item">
                        <label>Status:</label>
                        <span class="status-badge ${filament.status === 'online' ? 'online' : 'offline'}">
                            ${filament.status || 'Unknown'}
                        </span>
                    </div>
                    <div class="info-item">
                        <label>Name:</label>
                        <span>${filament.name || 'N/A'}</span>
                    </div>
                    <div class="info-item">
                        <label>Remaining:</label>
                        <span>${filament.meters_remaining || 0} meters</span>
                    </div>
                    <div class="info-item">
                        <label>Temperature:</label>
                        <span>${filament.temperature ? `${filament.temperature.min}-${filament.temperature.max}°C` : 'N/A'}</span>
                    </div>
                </div>
                
                <div class="detail-section">
                    <h4><i class="fas fa-cogs"></i> Motion</h4>
                    <div class="info-item">
                        <label>State:</label>
                        <span>${motion.state || 'Unknown'}</span>
                    </div>
                    <div class="info-item">
                        <label>Position:</label>
                        <span>${motion.position || 0}</span>
                    </div>
                    <div class="info-item">
                        <label>Speed:</label>
                        <span>${motion.speed || 0} units/s</span>
                    </div>
                    <div class="info-item">
                        <label>Pressure:</label>
                        <span>${motion.pressure || 0}</span>
                    </div>
                </div>
                
                <div class="detail-section">
                    <h4><i class="fas fa-lightbulb"></i> RGB LED</h4>
                    <div class="info-item">
                        <label>Brightness:</label>
                        <span>${rgb.brightness || 0}</span>
                    </div>
                    <div class="info-item">
                        <label>Color:</label>
                        <span style="display: inline-block; width: 20px; height: 20px; background: rgb(${rgb.current_color ? `${rgb.current_color.r}, ${rgb.current_color.g}, ${rgb.current_color.b}` : '128, 128, 128'}); border-radius: 3px; vertical-align: middle; margin-left: 10px;"></span>
                    </div>
                </div>
                
                <div class="detail-section">
                    <h4><i class="fas fa-sensor"></i> Sensors</h4>
                    <div class="info-item">
                        <label>Hall Position:</label>
                        <span>${sensors.hall_position || 0}</span>
                    </div>
                    <div class="info-item">
                        <label>Filament Present:</label>
                        <span class="status-badge ${sensors.filament_present ? 'online' : 'offline'}">
                            ${sensors.filament_present ? 'Yes' : 'No'}
                        </span>
                    </div>
                </div>
            </div>
        `;
        
        return div;
    }

    selectChannel(index) {
        // Update channel card selection
        document.querySelectorAll('.channel-card').forEach((card, i) => {
            card.classList.toggle('active', i === index);
        });
        
        // Show corresponding detail
        document.querySelectorAll('.channel-detail').forEach((detail, i) => {
            detail.style.display = i === index ? 'block' : 'none';
        });
    }

    updateConfigurationUI() {
        if (!this.configData || !this.configData.config) return;
        
        const config = this.configData.config;
        
        // Update LED brightness controls
        if (config.led_brightness) {
            if (config.led_brightness.main !== undefined) {
                document.getElementById('mainLedBrightness').value = config.led_brightness.main;
                document.getElementById('mainLedBrightnessValue').textContent = config.led_brightness.main;
            }
            if (config.led_brightness.channels !== undefined) {
                document.getElementById('channelLedBrightness').value = config.led_brightness.channels;
                document.getElementById('channelLedBrightnessValue').textContent = config.led_brightness.channels;
            }
        }
        
        // Update voltage thresholds
        if (config.voltage_thresholds) {
            if (config.voltage_thresholds.high !== undefined) {
                document.getElementById('voltageHigh').value = config.voltage_thresholds.high;
            }
            if (config.voltage_thresholds.low !== undefined) {
                document.getElementById('voltageLow').value = config.voltage_thresholds.low;
            }
        }
        
        // Update motion parameters
        if (config.motion_params) {
            if (config.motion_params.send_time !== undefined) {
                document.getElementById('motionSendTime').value = config.motion_params.send_time;
            }
            if (config.motion_params.filter_k !== undefined) {
                document.getElementById('motionFilterK').value = config.motion_params.filter_k;
            }
        }

        // Store initial config and set save button state
        this.configInputs.forEach(input => {
            this.initialConfig[input.id] = input.value;
            input.classList.remove('input-error');
        });

        this.validateAndCheckChanges();
    }

    async saveConfiguration() {
        this.showLoading();
        
        const config = {
            led_brightness_main: document.getElementById('mainLedBrightness').value,
            led_brightness_channels: document.getElementById('channelLedBrightness').value,
            voltage_high: document.getElementById('voltageHigh').value,
            voltage_low: document.getElementById('voltageLow').value,
            motion_send_time: document.getElementById('motionSendTime').value,
            motion_filter_k: document.getElementById('motionFilterK').value
        };
        
        let allSuccess = true;
        
        // Send each parameter individually
        for (const [key, value] of Object.entries(config)) {
            const formData = new FormData();
            formData.append('key', key);
            formData.append('value', value);
            
            const result = await this.apiCall('/api/config', {
                method: 'POST',
                body: formData
            });
            
            if (!result || !result.success) {
                allSuccess = false;
                break;
            }
        }
        
        this.hideLoading();
        
        if (allSuccess) {
            this.showToast('Configuration saved successfully', 'success');
            // Update initial config to the new values
            this.configInputs.forEach(input => {
                this.initialConfig[input.id] = input.value;
            });
            this.validateAndCheckChanges(); // This will disable the save button
        } else {
            this.showToast('Failed to save some configuration parameters', 'error');
        }
    }

    resetConfiguration() {
        // Reset to default values
        document.getElementById('mainLedBrightness').value = 35;
        document.getElementById('mainLedBrightnessValue').textContent = 35;
        document.getElementById('channelLedBrightness').value = 15;
        document.getElementById('channelLedBrightnessValue').textContent = 15;
        document.getElementById('voltageHigh').value = 1.85;
        document.getElementById('voltageLow').value = 1.45;
        document.getElementById('motionSendTime').value = 1200;
        document.getElementById('motionFilterK').value = 100;
        
        this.showToast('Configuration reset to defaults', 'warning');
    }

    async systemControl(action) {
        this.showLoading();

        if (action === 'reset_esp32') {
            this.showToast('Rebooting ESP32...', 'info');
            // Don't wait for the response, as the server will restart
            this.apiCall('/api/system', {
                method: 'POST',
                body: new URLSearchParams({ action })
            }).catch(err => {
                // Ignore network errors which are expected during a restart
                console.log('Ignoring expected error from ESP32 restart:', err);
            });

            // Hide loading indicator after a short delay
            setTimeout(() => this.hideLoading(), 1500);
            return;
        }

        const formData = new FormData();
        formData.append('action', action);

        const result = await this.apiCall('/api/system', {
            method: 'POST',
            body: formData
        });

        this.hideLoading();

        if (result && result.success) {
            this.showToast(result.message || 'Command executed successfully', 'success');
        } else {
            this.showToast('Failed to execute command', 'error');
        }
    }

    async refreshLogs() {
        const result = await this.apiCall('/api/logs');
        
        if (result) {
            this.updateDiagnosticsDisplay(result);
        }
    }

    updateDiagnosticsDisplay(logs) {
        // Update statistics
        if (logs.bmcu370_interface) {
            document.getElementById('totalCommands').textContent = logs.bmcu370_interface.command_count || 0;
            document.getElementById('totalErrors').textContent = logs.bmcu370_interface.error_count || 0;
        }
        
        if (logs.web_server) {
            document.getElementById('webRequests').textContent = logs.web_server.request_count || 0;
            document.getElementById('connectedClients').textContent = logs.web_server.connected_clients || 0;
        }
        
        // Update log entries
        const logContainer = document.getElementById('logContainer');
        logContainer.innerHTML = '';
        
        if (logs.esp32_logs && logs.esp32_logs.length > 0) {
            logs.esp32_logs.forEach(entry => {
                const div = document.createElement('div');
                div.className = 'log-entry';
                div.textContent = entry;
                logContainer.appendChild(div);
            });
        } else {
            logContainer.innerHTML = '<div class="log-entry">No recent logs available</div>';
        }
    }

    scanWiFiNetworks() {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            this.showToast('WebSocket not connected. Cannot start scan.', 'error');
            return;
        }

        this.showLoading();
        const networksList = document.getElementById('networksList');
        networksList.innerHTML = '<div class="no-networks">Scanning for networks...</div>';
        
        this.ws.send(JSON.stringify({ command: 'start_wifi_scan' }));
    }

    displayNetworks(networks) {
        const networksList = document.getElementById('networksList');
        
        if (networks.length === 0) {
            networksList.innerHTML = '<div class="no-networks">No networks found</div>';
            return;
        }
        
        networksList.innerHTML = networks.map(network => {
            const signalBars = this.getSignalBars(network.rssi);
            return `
                <div class="network-item" onclick="window.bmcuInterface.selectNetwork('${network.ssid}')">
                    <div class="network-info">
                        <div class="network-name">${network.ssid}</div>
                        <div class="network-details">
                            <span class="network-encryption">${network.encryption}</span>
                            <span class="network-signal">${signalBars} ${network.rssi} dBm</span>
                        </div>
                    </div>
                </div>
            `;
        }).join('');
    }

    getSignalBars(rssi) {
        if (rssi > -50) return '📶📶📶📶';
        if (rssi > -60) return '📶📶📶';
        if (rssi > -70) return '📶📶';
        return '📶';
    }

    selectNetwork(ssid) {
        document.getElementById('wifiSSIDInput').value = ssid;
        this.showToast(`Selected network: ${ssid}`, 'info');
    }

    async connectToWiFi() {
        const ssid = document.getElementById('wifiSSIDInput').value;
        const password = document.getElementById('wifiPasswordInput').value;
        
        if (!ssid) {
            this.showToast('Please enter a network name', 'warning');
            return;
        }
        
        this.showLoading();
        
        const formData = new FormData();
        formData.append('ssid', ssid);
        formData.append('password', password);
        
        const result = await this.apiCall('/api/wifi/connect', {
            method: 'POST',
            body: formData
        });
        
        this.hideLoading();
        
        if (result && result.success) {
            this.showToast('WiFi connection successful! Checking status...', 'success');
            document.getElementById('wifiForm').reset();
            
            // Update network status after a short delay
            setTimeout(() => {
                this.updateNetworkStatus();
            }, 3000);
        } else {
            this.showToast(result?.error || 'WiFi connection failed', 'error');
        }
    }

    async updateNetworkStatus() {
        const result = await this.apiCall('/api/wifi/status');
        
        if (result) {
            // Update WiFi status
            const statusElement = document.getElementById('wifiStatus');
            const ssidElement = document.getElementById('wifiSSID');
            const ipElement = document.getElementById('wifiIP');
            const signalElement = document.getElementById('wifiSignal');
            const networkHelpElement = document.getElementById('networkHelp');
            
            if (result.connected) {
                statusElement.textContent = 'Connected';
                statusElement.className = 'status-badge online';
                ssidElement.textContent = result.ssid || '--';
                ipElement.textContent = result.ip || '--';
                signalElement.textContent = result.signal ? `${result.signal} dBm` : '--';
                networkHelpElement.style.display = 'none';
            } else {
                if (result.ap_active) {
                    statusElement.textContent = 'Config Mode (AP)';
                    statusElement.className = 'status-badge warning';
                    ssidElement.textContent = 'BMCU370-Config';
                    ipElement.textContent = result.ap_ip || '--';
                    signalElement.textContent = '--';
                    networkHelpElement.style.display = 'block';
                } else {
                    statusElement.textContent = 'Disconnected';
                    statusElement.className = 'status-badge offline';
                    ssidElement.textContent = '--';
                    ipElement.textContent = '--';
                    signalElement.textContent = '--';
                    networkHelpElement.style.display = 'none';
                }
            }
        }
    }

    // Firmware management methods
    async updateFirmwareInfo() {
        try {
            // Update OTA status
            const otaResponse = await fetch('/api/ota/status');
            if (otaResponse.ok) {
                const otaData = await otaResponse.json();
                this.updateOTAStatus(otaData);
            }
            
            // Update system info for firmware tab
            const statusResponse = await fetch('/api/status');
            if (statusResponse.ok) {
                const statusData = await statusResponse.json();
                this.updateSystemInfo(statusData);
            }
        } catch (error) {
            console.error('Failed to update firmware info:', error);
        }
    }

    updateOTAStatus(otaData) {
        const statusElement = document.getElementById('otaStatus');
        let statusText = 'Idle';
        let statusClass = 'idle';

        switch (parseInt(otaData.state)) {
            case 0: // IDLE
                statusText = 'Idle';
                statusClass = 'idle';
                break;
            case 1: // STARTING
                statusText = 'Starting';
                statusClass = 'warning';
                break;
            case 2: // IN_PROGRESS
                statusText = `Updating (${otaData.progress}%)`;
                statusClass = 'info';
                break;
            case 3: // SUCCESS
                statusText = 'Success';
                statusClass = 'success';
                break;
            case 4: // ERROR
                statusText = 'Error';
                statusClass = 'error';
                break;
        }

        statusElement.textContent = statusText;
        statusElement.className = `status-badge ${statusClass}`;

        if (otaData.error && otaData.error !== '') {
            this.showToast(`OTA Error: ${otaData.error}`, 'error');
        }
    }

    updateSystemInfo(statusData) {
        if (statusData && statusData.system) {
            const system = statusData.system;
            
            // Update ESP32 firmware info
            document.getElementById('firmwareVersion').textContent = system.esp32_version || '1.0.0';
            document.getElementById('buildDate').textContent = system.esp32_build_date || '--';

            if (system.esp32_flash_size) {
                const flashSizeMB = system.esp32_flash_size / (1024 * 1024);
                document.getElementById('flashUsage').textContent = `${flashSizeMB} MB`;
            } else {
                document.getElementById('flashUsage').textContent = '--';
            }

            if (system.esp32_free_heap) {
                document.getElementById('freeHeap').textContent = `${(system.esp32_free_heap / 1024).toFixed(1)} KB`;
            } else {
                document.getElementById('freeHeap').textContent = '--';
            }
        }
    }

    handleFileSelection(file) {
        if (!file.name.endsWith('.bin')) {
            this.showToast('Please select a .bin firmware file', 'error');
            return;
        }

        if (file.size > 2 * 1024 * 1024) { // 2MB limit
            this.showToast('File too large. Maximum size is 2MB', 'error');
            return;
        }

        this.selectedFile = file;
        
        // Show file info
        document.getElementById('fileName').textContent = file.name;
        document.getElementById('fileSize').textContent = `${(file.size / 1024).toFixed(1)} KB`;
        document.getElementById('fileInfo').style.display = 'flex';
        document.getElementById('uploadFirmware').disabled = false;
        
        this.showToast('Firmware file selected', 'success');
    }

    clearSelectedFile() {
        this.selectedFile = null;
        document.getElementById('fileInfo').style.display = 'none';
        document.getElementById('uploadFirmware').disabled = true;
        document.getElementById('firmwareFile').value = '';
    }

    async uploadFirmware() {
        if (!this.selectedFile) {
            this.showToast('Please select a firmware file first', 'error');
            return;
        }

        const formData = new FormData();
        formData.append('firmware', this.selectedFile, this.selectedFile.name);

        document.getElementById('uploadProgress').style.display = 'block';
        document.getElementById('uploadFirmware').style.display = 'none';
        document.getElementById('abortUpload').style.display = 'inline-flex';
        this.updateUploadProgress(0, 'Starting upload...');

        try {
            const response = await fetch('/api/ota/upload', {
                method: 'POST',
                body: formData,
            });

            if (response.ok) {
                this.updateUploadProgress(100, 'Upload complete! Device restarting...');
                this.showToast('Firmware uploaded successfully! Device will restart.', 'success');
                setTimeout(() => window.location.reload(), 10000);
            } else {
                const error = await response.json();
                throw new Error(error.error || 'Upload failed');
            }
        } catch (error) {
            this.updateUploadProgress(0, 'Upload failed');
            this.showToast(`Upload failed: ${error.message}`, 'error');
            this.resetUploadUI();
        }
    }

    updateUploadProgress(percent, status) {
        document.getElementById('progressFill').style.width = `${percent}%`;
        document.getElementById('progressPercent').textContent = `${percent}%`;
        document.getElementById('progressStatus').textContent = status;
    }

    async abortUpload() {
        try {
            await fetch('/api/ota/abort', { method: 'POST' });
            this.showToast('Upload aborted', 'warning');
        } catch (error) {
            console.error('Failed to abort upload:', error);
        }
        this.resetUploadUI();
    }

    resetUploadUI() {
        document.getElementById('uploadProgress').style.display = 'none';
        document.getElementById('uploadFirmware').style.display = 'inline-flex';
        document.getElementById('abortUpload').style.display = 'none';
        this.updateUploadProgress(0, 'Preparing...');
    }

    confirmAction(message, callback) {
        if (confirm(message)) {
            callback();
        }
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        
        if (days > 0) {
            return `${days}d ${hours}h ${minutes}m`;
        } else if (hours > 0) {
            return `${hours}h ${minutes}m`;
        } else {
            return `${minutes}m`;
        }
    }

    showLoading() {
        document.getElementById('loadingOverlay').classList.add('show');
    }

    hideLoading() {
        document.getElementById('loadingOverlay').classList.remove('show');
    }

    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        const toastMessage = document.getElementById('toastMessage');
        
        toastMessage.textContent = message;
        toast.className = `toast ${type} show`;
        
        // Auto-hide after 5 seconds
        setTimeout(() => {
            this.hideToast();
        }, 5000);
    }

    hideToast() {
        document.getElementById('toast').classList.remove('show');
    }

    startPeriodicUpdates() {
        // Update diagnostics every 30 seconds when on diagnostics tab
        setInterval(() => {
            if (this.currentTab === 'diagnostics') {
                this.refreshLogs();
            }
        }, 30000);
    }
}

// Initialize the application when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.bmcuInterface = new BMCU370WebInterface();
});