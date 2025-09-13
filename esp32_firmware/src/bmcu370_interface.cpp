#include "bmcu370_interface.h"
#include <esp_log.h>
#include <usb/usb_host.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

static const char* TAG = "BMCU370_Interface";

// USB Class definitions (only define if not already defined)
#ifndef USB_CLASS_CDC
#define USB_CLASS_CDC               0x02
#endif
#ifndef USB_CDC_SUBCLASS_ACM  
#define USB_CDC_SUBCLASS_ACM        0x02
#endif
// USB_CLASS_CDC_DATA is already defined in ESP-IDF, so don't redefine it

// USB Host client handle
static usb_host_client_handle_t usb_host_client_handle = NULL;

// USB Host stack variables
static SemaphoreHandle_t usb_host_semaphore = NULL;
static TaskHandle_t usb_host_task_handle = NULL;
static bool usb_host_initialized = false;

// USB device variables
static usb_device_handle_t bmcu_device_handle = NULL;
static usb_transfer_t* bulk_in_transfer = NULL;
static usb_transfer_t* bulk_out_transfer = NULL;
static uint8_t bulk_in_ep_addr = 0;
static uint8_t bulk_out_ep_addr = 0;

// Communication synchronization
static SemaphoreHandle_t tx_semaphore = NULL;
static SemaphoreHandle_t rx_semaphore = NULL;
static QueueHandle_t rx_data_queue = NULL;

// Transfer callback data
typedef struct {
    bool transfer_complete;
    int actual_length;
    usb_transfer_status_t status;
} transfer_result_t;

static transfer_result_t tx_result = {false, 0, USB_TRANSFER_STATUS_COMPLETED};
static transfer_result_t rx_result = {false, 0, USB_TRANSFER_STATUS_COMPLETED};

// USB Host event handling
static void usb_host_event_handler(const usb_host_client_event_msg_t* event_msg, void* arg) {
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "New USB device connected");
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGW(TAG, "USB device disconnected");
            if (bmcu_device_handle) {
                usb_host_device_close(usb_host_client_handle, bmcu_device_handle);
                bmcu_device_handle = NULL;
            }
            break;
        default:
            break;
    }
}

// USB transfer completion callbacks
static void bulk_out_transfer_callback(usb_transfer_t* transfer) {
    tx_result.transfer_complete = true;
    tx_result.actual_length = transfer->actual_num_bytes;
    tx_result.status = transfer->status;
    
    if (tx_semaphore) {
        xSemaphoreGive(tx_semaphore);
    }
}

static void bulk_in_transfer_callback(usb_transfer_t* transfer) {
    rx_result.transfer_complete = true;
    rx_result.actual_length = transfer->actual_num_bytes;
    rx_result.status = transfer->status;
    
    // Queue received data if valid
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED && transfer->actual_num_bytes > 0) {
        if (rx_data_queue) {
            // Copy data to queue for processing
            char* rx_data = (char*)malloc(transfer->actual_num_bytes + 1);
            if (rx_data) {
                memcpy(rx_data, transfer->data_buffer, transfer->actual_num_bytes);
                rx_data[transfer->actual_num_bytes] = '\0';
                
                if (xQueueSend(rx_data_queue, &rx_data, 0) != pdTRUE) {
                    free(rx_data); // Queue full, free memory
                }
            }
        }
    }
    
    if (rx_semaphore) {
        xSemaphoreGive(rx_semaphore);
    }
}

// USB Host task
static void usb_host_task(void* arg) {
    ESP_LOGI(TAG, "USB Host task started");
    
    while (usb_host_initialized) {
        uint32_t event_flags;
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "USB host lib handle events error: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "No more USB clients");
            break;
        }
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "All USB devices freed");
        }
    }
    
    ESP_LOGI(TAG, "USB Host task exiting");
    vTaskDelete(NULL);
}
// BMCU370_USB_Host Implementation
BMCU370_USB_Host::BMCU370_USB_Host() 
    : initialized(false), device_connected(false), last_connect_attempt(0),
      vid(BMCU370_VID), pid(BMCU370_PID), 
      interface_class(USB_INTERFACE_CLASS), interface_subclass(USB_INTERFACE_SUBCLASS) {
    memset(command_buffer, 0, sizeof(command_buffer));
    memset(response_buffer, 0, sizeof(response_buffer));
}

BMCU370_USB_Host::~BMCU370_USB_Host() {
    disconnect();
    
    // Cleanup synchronization objects
    if (usb_host_initialized) {
        usb_host_initialized = false;
        
        if (usb_host_task_handle) {
            vTaskDelete(usb_host_task_handle);
            usb_host_task_handle = NULL;
        }
        
        if (usb_host_client_handle) {
            usb_host_client_deregister(usb_host_client_handle);
            usb_host_client_handle = NULL;
        }
        
        usb_host_uninstall();
    }
    
    if (usb_host_semaphore) {
        vSemaphoreDelete(usb_host_semaphore);
        usb_host_semaphore = NULL;
    }
    
    if (tx_semaphore) {
        vSemaphoreDelete(tx_semaphore);
        tx_semaphore = NULL;
    }
    
    if (rx_semaphore) {
        vSemaphoreDelete(rx_semaphore);
        rx_semaphore = NULL;
    }
    
    if (rx_data_queue) {
        // Free any remaining data in queue
        char* data;
        while (xQueueReceive(rx_data_queue, &data, 0) == pdTRUE) {
            free(data);
        }
        vQueueDelete(rx_data_queue);
        rx_data_queue = NULL;
    }
}

bool BMCU370_USB_Host::init() {
    if (initialized) {
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing USB host interface for ESP32-S3");
    
    // Create synchronization objects
    usb_host_semaphore = xSemaphoreCreateBinary();
    tx_semaphore = xSemaphoreCreateBinary();
    rx_semaphore = xSemaphoreCreateBinary();
    rx_data_queue = xQueueCreate(10, sizeof(char*)); // Queue for received data
    
    if (!usb_host_semaphore || !tx_semaphore || !rx_semaphore || !rx_data_queue) {
        ESP_LOGE(TAG, "Failed to create synchronization objects");
        return false;
    }
    
    // Install USB Host library
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    
    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB host: %s", esp_err_to_name(err));
        return false;
    }
    
    // Register USB Host client
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = usb_host_event_handler,
            .callback_arg = this
        }
    };
    
    err = usb_host_client_register(&client_config, &usb_host_client_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register USB client: %s", esp_err_to_name(err));
        usb_host_uninstall();
        return false;
    }
    
    // Start USB Host task
    usb_host_initialized = true;
    BaseType_t task_result = xTaskCreate(
        usb_host_task,
        "usb_host_task",
        4096,
        NULL,
        5,
        &usb_host_task_handle
    );
    
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB host task");
        usb_host_client_deregister(usb_host_client_handle);
        usb_host_uninstall();
        usb_host_initialized = false;
        return false;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "USB host interface initialized successfully");
    
    return true;
}

bool BMCU370_USB_Host::connect() {
    if (!initialized) {
        ESP_LOGE(TAG, "USB host not initialized");
        return false;
    }
    
    unsigned long current_time = millis();
    if (current_time - last_connect_attempt < USB_RETRY_DELAY_MS) {
        // Check if existing connection is still valid
        if (device_connected && checkConnection()) {
            return device_connected;
        }
    }
    
    last_connect_attempt = current_time;
    
    // Only log connection attempts every 30 seconds to reduce spam
    static unsigned long last_log_time = 0;
    static int connection_attempts = 0;
    bool should_log = (current_time - last_log_time) > 30000;
    
    if (should_log) {
        connection_attempts++;
        ESP_LOGI(TAG, "Attempting to connect to BMCU370 (attempt %d)...", connection_attempts);
        last_log_time = current_time;
    }
    
    // Check existing connection first
    bool connection_result = checkConnection();
    if (!connection_result) {
        // Try to enumerate and connect to new device
        connection_result = enumerateDevice();
    }
    
    if (connection_result && !device_connected) {
        // New connection established
        device_connected = true;
        connection_attempts = 0; // Reset counter on successful connection
        ESP_LOGI(TAG, "Successfully connected to BMCU370");
        printDeviceInfo();
    } else if (!connection_result && device_connected) {
        // Connection lost
        device_connected = false;
        ESP_LOGW(TAG, "Lost connection to BMCU370 device");
    } else if (!connection_result && should_log) {
        ESP_LOGD(TAG, "BMCU370 device not found (will retry every %dms)", USB_RETRY_DELAY_MS);
        
        // After many failed attempts, suggest troubleshooting
        if (connection_attempts > 10) {
            ESP_LOGW(TAG, "Failed to connect after %d attempts. Check USB cable and BMCU370 power.", connection_attempts);
        }
    }
    
    return device_connected;
}

void BMCU370_USB_Host::disconnect() {
    if (device_connected) {
        ESP_LOGI(TAG, "Disconnecting from BMCU370");
        closeCDCInterface();
        device_connected = false;
    }
}

bool BMCU370_USB_Host::sendCommand(const String& cmd) {
    if (!device_connected) {
        ESP_LOGE(TAG, "Device not connected");
        return false;
    }
    
    if (cmd.length() >= USB_COMMAND_BUFFER_SIZE - 2) { // -2 for \n and \0
        ESP_LOGE(TAG, "Command too long");
        return false;
    }
    
    snprintf(command_buffer, sizeof(command_buffer), "%s\n", cmd.c_str());
    
#if DEBUG_USB_COMMUNICATION
    ESP_LOGI(TAG, "Sending command: %s", cmd.c_str());
#endif
    
    return writeCommand(command_buffer);
}

String BMCU370_USB_Host::readResponse(uint32_t timeout_ms) {
    if (!device_connected) {
        ESP_LOGE(TAG, "Device not connected");
        return "";
    }
    
    int bytes_read = readResponse(response_buffer, sizeof(response_buffer), timeout_ms);
    if (bytes_read <= 0) {
        ESP_LOGW(TAG, "No response received");
        return "";
    }
    
    response_buffer[bytes_read] = '\0';
    String result(response_buffer);
    
#if DEBUG_USB_COMMUNICATION
    ESP_LOGI(TAG, "Received response (%d bytes): %s", bytes_read, response_buffer);
#endif
    
    return result;
}

bool BMCU370_USB_Host::sendCommandAndGetResponse(const String& cmd, String& response, uint32_t timeout_ms) {
    if (!sendCommand(cmd)) {
        return false;
    }
    
    response = readResponse(timeout_ms);
    return !response.isEmpty();
}

void BMCU370_USB_Host::printDeviceInfo() {
    ESP_LOGI(TAG, "BMCU370 Device Info:");
    ESP_LOGI(TAG, "  VID: 0x%04X", vid);
    ESP_LOGI(TAG, "  PID: 0x%04X", pid);
    ESP_LOGI(TAG, "  Interface Class: 0x%02X", interface_class);
    ESP_LOGI(TAG, "  Interface Subclass: 0x%02X", interface_subclass);
}

bool BMCU370_USB_Host::enumerateDevice() {
    if (!initialized || !usb_host_client_handle) {
        return false;
    }
    
    // Get list of connected devices using correct API
    int num_devices;
    esp_err_t err = usb_host_device_addr_list_fill(0, NULL, &num_devices);
    if (err != ESP_OK || num_devices == 0) {
        return false; // No devices connected
    }
    
    uint8_t* device_addr_list = (uint8_t*)malloc(num_devices);
    if (!device_addr_list) {
        return false;
    }
    
    err = usb_host_device_addr_list_fill(num_devices, device_addr_list, &num_devices);
    if (err != ESP_OK) {
        free(device_addr_list);
        return false;
    }
    
    // Check each device for BMCU370 VID/PID
    bool found_bmcu = false;
    for (int i = 0; i < num_devices; i++) {
        usb_device_handle_t device_handle;
        err = usb_host_device_open(usb_host_client_handle, device_addr_list[i], &device_handle);
        if (err != ESP_OK) {
            continue;
        }
        
        // Get device descriptor
        const usb_device_desc_t* device_desc;
        err = usb_host_get_device_descriptor(device_handle, &device_desc);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Found USB device: VID=0x%04X, PID=0x%04X", 
                     device_desc->idVendor, device_desc->idProduct);
            
            // For now, accept any CDC-ACM device (since BMCU370 may appear as generic CDC)
            // In production, you'd check specific VID/PID
            if (device_desc->bDeviceClass == USB_CLASS_CDC || device_desc->bDeviceClass == 0) {
                ESP_LOGI(TAG, "Found potential BMCU370 CDC device!");
                bmcu_device_handle = device_handle;
                found_bmcu = true;
                
                // Get configuration descriptor to find CDC interface
                const usb_config_desc_t* config_desc;
                err = usb_host_get_active_config_descriptor(device_handle, &config_desc);
                if (err == ESP_OK) {
                    // Parse interfaces to find CDC-ACM
                    found_bmcu = openCDCInterface();
                }
                break;
            }
        }
        
        if (!found_bmcu) {
            usb_host_device_close(usb_host_client_handle, device_handle);
        }
    }
    
    free(device_addr_list);
    return found_bmcu;
}

bool BMCU370_USB_Host::openCDCInterface() {
    if (!bmcu_device_handle) {
        return false;
    }
    
    ESP_LOGI(TAG, "Attempting to open CDC interface");
    
    // For simplicity, assume the device has a standard CDC-ACM interface layout
    // Interface 0: Communication Class (control)  
    // Interface 1: Data Class (bulk endpoints)
    
    // Try to claim interface 0 (communication class)
    esp_err_t err = usb_host_interface_claim(usb_host_client_handle, bmcu_device_handle, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to claim interface 0: %s", esp_err_to_name(err));
    }
    
    // Try to claim interface 1 (data class) 
    err = usb_host_interface_claim(usb_host_client_handle, bmcu_device_handle, 1, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to claim data interface 1: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Successfully claimed CDC interfaces");
    
    // For standard CDC-ACM devices, data interface usually has:
    // Endpoint 0x81: Bulk IN (device to host)
    // Endpoint 0x02: Bulk OUT (host to device)
    // These are common default addresses for CDC devices
    
    bulk_in_ep_addr = 0x81;   // Standard bulk IN endpoint
    bulk_out_ep_addr = 0x02;  // Standard bulk OUT endpoint
    
    ESP_LOGI(TAG, "Using standard CDC endpoints: IN=0x%02X, OUT=0x%02X", 
             bulk_in_ep_addr, bulk_out_ep_addr);
    
    // Create USB transfers for communication
    err = usb_host_transfer_alloc(USB_RESPONSE_BUFFER_SIZE, 0, &bulk_in_transfer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate IN transfer: %s", esp_err_to_name(err));
        return false;
    }
    
    bulk_in_transfer->device_handle = bmcu_device_handle;
    bulk_in_transfer->bEndpointAddress = bulk_in_ep_addr;
    bulk_in_transfer->callback = bulk_in_transfer_callback;
    bulk_in_transfer->context = this;
    
    err = usb_host_transfer_alloc(USB_COMMAND_BUFFER_SIZE, 0, &bulk_out_transfer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate OUT transfer: %s", esp_err_to_name(err));
        usb_host_transfer_free(bulk_in_transfer);
        bulk_in_transfer = NULL;
        return false;
    }
    
    bulk_out_transfer->device_handle = bmcu_device_handle;
    bulk_out_transfer->bEndpointAddress = bulk_out_ep_addr;
    bulk_out_transfer->callback = bulk_out_transfer_callback;
    bulk_out_transfer->context = this;
    
    ESP_LOGI(TAG, "CDC interface successfully opened with transfers allocated");
    return true;
}

void BMCU370_USB_Host::closeCDCInterface() {
    if (bulk_in_transfer) {
        usb_host_transfer_free(bulk_in_transfer);
        bulk_in_transfer = NULL;
    }
    
    if (bulk_out_transfer) {
        usb_host_transfer_free(bulk_out_transfer);
        bulk_out_transfer = NULL;
    }
    
    bulk_in_ep_addr = 0;
    bulk_out_ep_addr = 0;
    
    ESP_LOGI(TAG, "CDC interface closed");
}

bool BMCU370_USB_Host::writeCommand(const char* command) {
    if (!bulk_out_transfer || !command) {
        return false;
    }
    
    int length = strlen(command);
    if (length > USB_COMMAND_BUFFER_SIZE - 1) {
        ESP_LOGE(TAG, "Command too long: %d bytes", length);
        return false;
    }
    
    // Copy command to transfer buffer
    memcpy(bulk_out_transfer->data_buffer, command, length);
    bulk_out_transfer->num_bytes = length;
    
    // Reset result
    tx_result.transfer_complete = false;
    tx_result.actual_length = 0;
    tx_result.status = USB_TRANSFER_STATUS_COMPLETED;
    
    // Submit transfer
    esp_err_t err = usb_host_transfer_submit(bulk_out_transfer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to submit OUT transfer: %s", esp_err_to_name(err));
        return false;
    }
    
    // Wait for completion
    if (xSemaphoreTake(tx_semaphore, pdMS_TO_TICKS(USB_TIMEOUT_MS)) == pdTRUE) {
        if (tx_result.status == USB_TRANSFER_STATUS_COMPLETED) {
            ESP_LOGD(TAG, "Command sent successfully: %d bytes", tx_result.actual_length);
            return true;
        } else {
            ESP_LOGE(TAG, "Transfer failed with status: %d", tx_result.status);
        }
    } else {
        ESP_LOGE(TAG, "Transfer timeout");
        // Cancel the transfer
        usb_host_transfer_submit_control(usb_host_client_handle, bulk_out_transfer);
    }
    
    return false;
}

int BMCU370_USB_Host::readResponse(char* buffer, size_t buffer_size, uint32_t timeout_ms) {
    if (!bulk_in_transfer || !buffer || buffer_size == 0) {
        return 0;
    }
    
    // First, check if we have data in the queue from previous reads
    char* queued_data = NULL;
    if (xQueueReceive(rx_data_queue, &queued_data, 0) == pdTRUE) {
        int data_len = strlen(queued_data);
        if (data_len < buffer_size) {
            strcpy(buffer, queued_data);
            free(queued_data);
            return data_len;
        } else {
            // Data too large for buffer
            free(queued_data);
            return -1;
        }
    }
    
    // No queued data, submit a new read transfer
    bulk_in_transfer->num_bytes = USB_RESPONSE_BUFFER_SIZE;
    
    // Reset result
    rx_result.transfer_complete = false;
    rx_result.actual_length = 0;
    rx_result.status = USB_TRANSFER_STATUS_COMPLETED;
    
    // Submit transfer
    esp_err_t err = usb_host_transfer_submit(bulk_in_transfer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to submit IN transfer: %s", esp_err_to_name(err));
        return 0;
    }
    
    // Wait for completion
    if (xSemaphoreTake(rx_semaphore, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        if (rx_result.status == USB_TRANSFER_STATUS_COMPLETED && rx_result.actual_length > 0) {
            int copy_len = min((int)(buffer_size - 1), rx_result.actual_length);
            memcpy(buffer, bulk_in_transfer->data_buffer, copy_len);
            buffer[copy_len] = '\0';
            
            ESP_LOGD(TAG, "Response received: %d bytes", copy_len);
            return copy_len;
        } else {
            ESP_LOGE(TAG, "IN transfer failed with status: %d", rx_result.status);
        }
    } else {
        ESP_LOGD(TAG, "Read timeout after %d ms", timeout_ms);
        // Cancel the transfer
        usb_host_transfer_submit_control(usb_host_client_handle, bulk_in_transfer);
    }
    
    return 0;
}

String BMCU370_USB_Host::getLastError() const {
    // Return last USB error based on transfer results
    if (tx_result.status != USB_TRANSFER_STATUS_COMPLETED) {
        return "TX Transfer error: " + String(tx_result.status);
    }
    if (rx_result.status != USB_TRANSFER_STATUS_COMPLETED) {
        return "RX Transfer error: " + String(rx_result.status);
    }
    return "No error";
}

bool BMCU370_USB_Host::checkConnection() {
    // Check if device handle is still valid and device is still connected
    if (!bmcu_device_handle) {
        return false;
    }
    
    // Quick check by trying to get device descriptor (non-intrusive)
    const usb_device_desc_t* device_desc;
    esp_err_t err = usb_host_get_device_descriptor(bmcu_device_handle, &device_desc);
    
    return (err == ESP_OK);
}

// BMCU370_Interface Implementation
BMCU370_Interface::BMCU370_Interface() 
    : last_status_update(0), last_config_update(0), 
      connection_state_changed(false), was_connected_last_update(false),
      device_online(false), command_count(0), error_count(0) {
}

BMCU370_Interface::~BMCU370_Interface() {
}

bool BMCU370_Interface::init() {
    ESP_LOGI(TAG, "Initializing BMCU370 interface");
    
    if (!usb_host.init()) {
        ESP_LOGE(TAG, "Failed to initialize USB host");
        return false;
    }
    
    // Initialize JSON documents
    status_cache.clear();
    config_cache.clear();
    
    ESP_LOGI(TAG, "BMCU370 interface initialized successfully");
    return true;
}

bool BMCU370_Interface::updateStatus() {
    bool connected = usb_host.connect();
    updateConnectionState(connected);
    
    if (!connected) {
        status_cache.clear();
        JsonObject system = status_cache["system"].to<JsonObject>();
        system["bambubus_status"] = "offline";
        status_cache["channels"].to<JsonArray>();
        return true; // Return true to indicate status is "known" (disconnected)
    }
    
    String response;
    if (!usb_host.sendCommandAndGetResponse("GET_STATUS", response)) {
        error_count++;
        last_error = "Failed to get status from BMCU370";
        ESP_LOGW(TAG, "%s", last_error.c_str());

        // Create a default "unreachable" status
        status_cache.clear();
        JsonObject system = status_cache["system"].to<JsonObject>();
        system["bambubus_status"] = "unreachable";
        system["version"] = "N/A";
        system["uptime"] = 0;
        system["device_type"] = "N/A";
        status_cache["channels"].to<JsonArray>();

        return true; // Return true but with unreachable status
    }
    
    if (!parseJsonResponse(response, status_cache)) {
        error_count++;
        last_error = "Failed to parse status JSON";
        ESP_LOGW(TAG, "%s", last_error.c_str());
        return false;
    }
    
    if (!validateStatusResponse(status_cache)) {
        error_count++;
        last_error = "Invalid status response format";
        ESP_LOGW(TAG, "%s", last_error.c_str());
        return false;
    }
    
    command_count++;
    last_status_update = millis();
    
#if DEBUG_USB_COMMUNICATION
    ESP_LOGI(TAG, "Status updated successfully");
#endif
    
    return true;
}

bool BMCU370_Interface::updateConfig() {
    if (!device_online) {
        return false;
    }
    
    String response;
    if (!usb_host.sendCommandAndGetResponse("GET_CONFIG", response)) {
        error_count++;
        last_error = "Failed to get config from BMCU370";
        return false;
    }
    
    if (!parseJsonResponse(response, config_cache)) {
        error_count++;
        last_error = "Failed to parse config JSON";
        return false;
    }
    
    if (!validateConfigResponse(config_cache)) {
        error_count++;
        last_error = "Invalid config response format";
        return false;
    }
    
    command_count++;
    last_config_update = millis();
    return true;
}

bool BMCU370_Interface::setParameter(const String& key, const String& value) {
    if (!device_online) {
        last_error = "Device not connected";
        return false;
    }
    
    String command = "SET_PARAM " + key + "=" + value;
    String response;
    
    if (!usb_host.sendCommandAndGetResponse(command, response)) {
        error_count++;
        last_error = "Failed to set parameter: " + key;
        return false;
    }
    
    // Check if response indicates success
    if (response.indexOf("OK") == -1) {
        error_count++;
        last_error = "Parameter set failed: " + response;
        return false;
    }
    
    command_count++;
    ESP_LOGI(TAG, "Parameter set successfully: %s=%s", key.c_str(), value.c_str());
    
    // Update config cache
    updateConfig();
    
    return true;
}

bool BMCU370_Interface::setLEDBrightness(int channel, int brightness) {
    if (channel < 0 || channel >= MAX_FILAMENT_CHANNELS) {
        last_error = "Invalid channel number";
        return false;
    }
    
    if (brightness < 0 || brightness > 255) {
        last_error = "Invalid brightness value (0-255)";
        return false;
    }
    
    String key = "led_brightness_ch" + String(channel);
    return setParameter(key, String(brightness));
}

bool BMCU370_Interface::setMotionParameter(const String& param, float value) {
    String key = "motion_" + param;
    return setParameter(key, String(value, 2)); // 2 decimal places
}

bool BMCU370_Interface::resetDevice() {
    if (!device_online) {
        return false;
    }
    
    String response;
    bool result = usb_host.sendCommandAndGetResponse("RESET", response);
    
    if (result) {
        command_count++;
        ESP_LOGI(TAG, "Device reset command sent");
        // Device will disconnect after reset
        device_online = false;
    } else {
        error_count++;
        last_error = "Failed to send reset command";
    }
    
    return result;
}

bool BMCU370_Interface::enterDFUMode() {
    if (!device_online) {
        return false;
    }
    
    String response;
    bool result = usb_host.sendCommandAndGetResponse("DFU", response);
    
    if (result) {
        command_count++;
        ESP_LOGI(TAG, "DFU mode command sent");
        // Device will disconnect and enter DFU mode
        device_online = false;
    } else {
        error_count++;
        last_error = "Failed to send DFU command";
    }
    
    return result;
}

String BMCU370_Interface::getVersion() {
    if (!device_online) {
        return "";
    }
    
    String response;
    if (usb_host.sendCommandAndGetResponse("GET_VERSION", response)) {
        command_count++;
        return response;
    } else {
        error_count++;
        last_error = "Failed to get version";
        return "";
    }
}

void BMCU370_Interface::updateConnectionState(bool connected) {
    if (connected != device_online) {
        connection_state_changed = true;
        was_connected_last_update = device_online;
        device_online = connected;
        
        if (connected) {
            ESP_LOGI(TAG, "BMCU370 connected");
        } else {
            ESP_LOGI(TAG, "BMCU370 disconnected");
        }
    } else {
        connection_state_changed = false;
        was_connected_last_update = device_online;
    }
}

bool BMCU370_Interface::parseJsonResponse(const String& response, JsonDocument& doc) {
    doc.clear();
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        ESP_LOGE(TAG, "JSON parsing failed: %s", error.c_str());
        return false;
    }
    
    return true;
}

bool BMCU370_Interface::validateStatusResponse(const JsonDocument& doc) {
    // Check for required top-level objects
    if (!doc["system"].is<JsonObject>() || !doc["channels"].is<JsonArray>()) {
        ESP_LOGE(TAG, "Status response missing required fields");
        return false;
    }
    
    // Validate system object
    JsonObjectConst system = doc["system"].as<JsonObjectConst>();
    if (!system["uptime"].is<int>() || !system["version"].is<const char*>()) {
        ESP_LOGE(TAG, "System object missing required fields");
        return false;
    }
    
    // Validate channels array
    JsonArrayConst channels = doc["channels"].as<JsonArrayConst>();
    if (channels.size() == 0 || channels.size() > MAX_FILAMENT_CHANNELS) {
        ESP_LOGE(TAG, "Invalid number of channels: %d", channels.size());
        return false;
    }
    
    return true;
}

bool BMCU370_Interface::validateConfigResponse(const JsonDocument& doc) {
    // Check for config object
    if (!doc["config"].is<JsonObject>()) {
        ESP_LOGE(TAG, "Config response missing config object");
        return false;
    }
    
    return true;
}

String BMCU370_Interface::getConnectionStatus() const {
    if (device_online) {
        return "Connected";
    } else {
        return "Disconnected";
    }
}

void BMCU370_Interface::printDebugInfo() {
    ESP_LOGI(TAG, "=== BMCU370 Interface Debug Info ===");
    ESP_LOGI(TAG, "Connection Status: %s", getConnectionStatus().c_str());
    ESP_LOGI(TAG, "Commands Sent: %lu", command_count);
    ESP_LOGI(TAG, "Errors: %lu", error_count);
    ESP_LOGI(TAG, "Last Update: %lu ms ago", millis() - last_status_update);
    if (!last_error.isEmpty()) {
        ESP_LOGI(TAG, "Last Error: %s", last_error.c_str());
    }
    ESP_LOGI(TAG, "=======================================");
}