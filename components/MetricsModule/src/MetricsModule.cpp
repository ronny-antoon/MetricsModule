#include "MetricsModule.hpp"
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char * TAG = "MetricsModule";
#define DEVICEID_SIZE 5

MetricsModule::MetricsModule(const char * databaseUrl, const char * deviceLocation, const char * token) :
    m_senderTaskHandle(nullptr), m_databaseUrl(databaseUrl), m_deviceLocation(deviceLocation), m_token(token)
{
    ESP_LOGI(TAG, "MetricsModule created");

    if (m_databaseUrl == nullptr)
    {
        m_databaseUrl = CONFIG_M_M_DEFAULT_DATABASE_URL;
        ESP_LOGW(TAG, "No database URL provided, using default: %s", m_databaseUrl);
    }
    if (m_deviceLocation == nullptr)
    {
        m_deviceLocation = CONFIG_M_M_DEFAULT_DEVICE_LOCATION;
        ESP_LOGW(TAG, "No device Location provided, using default: %s", m_deviceLocation);
    }
    if (m_token == nullptr)
    {
        m_token = CONFIG_M_M_DEFAULT_TOKEN;
        ESP_LOGW(TAG, "No token provided, using default: %s", m_token);
    }

    size_t urlSize = strlen(m_databaseUrl) + 2;
    char * fullURL = (char *) malloc(urlSize);
    if (fullURL == nullptr)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for full URL");
        return;
    }
    snprintf(fullURL, urlSize, "%s", m_databaseUrl);
    m_databaseUrl = fullURL;

    m_metricsBuffer = (char *) calloc(CONFIG_M_M_BUFFER_SIZE, sizeof(char));
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for metrics buffer");
        return;
    }

    if (generateRandomDeviceId() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to generate random device ID");
    }
}

MetricsModule::~MetricsModule()
{
    if (m_senderTaskHandle != nullptr)
    {
        vTaskDelete(m_senderTaskHandle);
    }
    free(m_metricsBuffer);
    free((void *) m_databaseUrl);
    free((void *) m_deviceId);

    ESP_LOGI(TAG, "MetricsModule destroyed");
}

esp_err_t MetricsModule::start()
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_NO_MEM;
    }
    if (m_senderTaskHandle != nullptr)
    {
        ESP_LOGE(TAG, "Sender task already running");
        return ESP_ERR_INVALID_STATE;
    }
    if (!CONFIG_M_M_ENABLED)
    {
        ESP_LOGW(TAG, "MetricsModule is disabled. Enable it by setting CONFIG_M_M_ENABLED to y in sdkconfig.");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Starting metrics sender task");
    if (xTaskCreate(&MetricsModule::senderTask, "metrics_sender_task", CONFIG_M_M_TASK_STACK_SIZE, this, CONFIG_M_M_TASK_PRIORITY,
                    &m_senderTaskHandle) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create metrics sender task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void MetricsModule::senderTask(void * pvParameters)
{
    MetricsModule * self = (MetricsModule *) pvParameters;
    while (true)
    {
        if (self->resetBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to reset buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addPrefixJsonToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add prefix JSON to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addTokenToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add token to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addDeviceIdToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add device ID to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addLocationToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add location to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addFreeHeapToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add free heap to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addTasksFreeStackToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add tasks free stack to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addWifiRssiToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add wifi RSSI to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addPostfixJsonToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add postfix JSON to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (CONFIG_M_M_PRINT_METRICS_BUFFER)
        {
            if (self->printMetricBuffer() != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to print metric buffer");
            }
        }
        if (!self->checkNetworkConnection())
        {
            ESP_LOGW(TAG, "No network connection or time not correct. Retrying in %d seconds", CONFIG_M_M_SEND_METRICS_PERIOD);
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->sendBufferedMetrics() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send buffered metrics");
        }
        vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 1000 / portTICK_PERIOD_MS);
    }
}

esp_err_t MetricsModule::resetBuffer()
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    memset(m_metricsBuffer, 0, CONFIG_M_M_BUFFER_SIZE);
    return ESP_OK;
}

esp_err_t MetricsModule::addPrefixJsonToBuffer()
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    snprintf(m_metricsBuffer, CONFIG_M_M_BUFFER_SIZE, "{");
    return ESP_OK;
}

esp_err_t MetricsModule::addPostfixJsonToBuffer()
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    strncat(m_metricsBuffer, "}", CONFIG_M_M_BUFFER_SIZE - strlen(m_metricsBuffer) - 1);
    return ESP_OK;
}

esp_err_t MetricsModule::addMetricToBuffer(const char * metricName, const char * metricValue)
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }

    if (strlen(m_metricsBuffer) + strlen(metricName) + strlen(metricValue) + 5 >= CONFIG_M_M_BUFFER_SIZE)
    {
        ESP_LOGE(TAG, "Not enough space in metrics buffer to add metric");
        return ESP_ERR_NO_MEM;
    }

    // Add a comma if this is not the first metric
    if (strlen(m_metricsBuffer) > 1)
    {
        strncat(m_metricsBuffer, ",", CONFIG_M_M_BUFFER_SIZE - strlen(m_metricsBuffer) - 1);
    }

    // Format and add the metric to the buffer
    snprintf(m_metricsBuffer + strlen(m_metricsBuffer), CONFIG_M_M_BUFFER_SIZE - strlen(m_metricsBuffer), "\"%s\":\"%s\"",
             metricName, metricValue);

    return ESP_OK;
}

esp_err_t MetricsModule::addMetricToBuffer(const char * metricName, const int metricValue)
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }

    if (strlen(m_metricsBuffer) + strlen(metricName) + 5 >= CONFIG_M_M_BUFFER_SIZE) // 20 characters for the integer value
    {
        ESP_LOGE(TAG, "Not enough space in metrics buffer to add metric");
        return ESP_ERR_NO_MEM;
    }

    // Add a comma if this is not the first metric
    if (strlen(m_metricsBuffer) > 1)
    {
        strncat(m_metricsBuffer, ",", CONFIG_M_M_BUFFER_SIZE - strlen(m_metricsBuffer) - 1);
    }

    // Format and add the metric to the buffer
    snprintf(m_metricsBuffer + strlen(m_metricsBuffer), CONFIG_M_M_BUFFER_SIZE - strlen(m_metricsBuffer), "\"%s\":%d", metricName,
             metricValue);

    return ESP_OK;
}

esp_err_t MetricsModule::addDeviceIdToBuffer()
{
    return addMetricToBuffer("deviceId", m_deviceId);
}

esp_err_t MetricsModule::addLocationToBuffer()
{
    return addMetricToBuffer("location", m_deviceLocation);
}

esp_err_t MetricsModule::addTokenToBuffer()
{
    return addMetricToBuffer("token", m_token);
}

esp_err_t MetricsModule::addWifiRssiToBuffer()
{
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get AP info: %s", esp_err_to_name(err));
        return err;
    }
    return addMetricToBuffer("wifiRssi", ap_info.rssi);
}

esp_err_t MetricsModule::addFreeHeapToBuffer()
{
    addMetricToBuffer("freeHeap", (int) heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    addMetricToBuffer("minFreeHeap", (int) heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
    addMetricToBuffer("largestFreeBlock", (int) heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    return ESP_OK;
}

esp_err_t MetricsModule::addTasksFreeStackToBuffer()
{
    TaskStatus_t * pxTaskStatusArray;
    volatile UBaseType_t uxArraySize;

    uxArraySize = uxTaskGetNumberOfTasks();
    if (uxArraySize == 0)
    {
        ESP_LOGW(TAG, "No tasks found");
        return ESP_FAIL;
    }

    pxTaskStatusArray = (TaskStatus_t *) malloc(uxArraySize * sizeof(TaskStatus_t));
    if (pxTaskStatusArray == nullptr)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for task status array");
        return ESP_ERR_NO_MEM;
    }

    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);
    for (UBaseType_t i = 0; i < uxArraySize; i++)
    {
        char metricName[40];
        if (strcmp(pxTaskStatusArray[i].pcTaskName, "IDLE") == 0)
        {
            snprintf(metricName, sizeof(metricName), "IDLE_%d", (int) pxTaskStatusArray[i].xTaskNumber);
        }
        else
        {
            snprintf(metricName, sizeof(metricName), "%s", pxTaskStatusArray[i].pcTaskName);
        }
        addMetricToBuffer(metricName, (int) pxTaskStatusArray[i].usStackHighWaterMark);
    }

    free(pxTaskStatusArray);
    return ESP_OK;
}

esp_err_t MetricsModule::sendBufferedMetrics()
{
    if (m_metricsBuffer == nullptr || strlen(m_metricsBuffer) == 0)
    {
        ESP_LOGW(TAG, "No metrics to send");
        return ESP_OK;
    }

    esp_http_client_config_t config = {
        .url        = m_databaseUrl,
        .method     = HTTP_METHOD_POST,
        .timeout_ms = CONFIG_M_M_HTTP_TIMEOUT_MS,
        .user_data  = this,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    esp_err_t err;
    err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set HTTP header: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    err = esp_http_client_set_post_field(client, m_metricsBuffer, strlen(m_metricsBuffer));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set HTTP post field: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to perform HTTP request: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return err;
}

bool MetricsModule::checkNetworkConnection()
{
    esp_netif_ip_info_t ip4_info;
    // Get the default network interface
    esp_netif_t * netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr || !esp_netif_is_netif_up(netif))
    {
        ESP_LOGW(TAG, "Failed to get network interface");
        return false;
    }

    if (esp_netif_get_ip_info(netif, &ip4_info) != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to get IPv4 address info");
        return false;
    }

    return true;
}

esp_err_t MetricsModule::printMetricBuffer()
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "URL: %s", m_databaseUrl);
    ESP_LOGI(TAG, "Metrics buffer: \n%s\n", m_metricsBuffer);
    return ESP_OK;
}

esp_err_t MetricsModule::generateRandomDeviceId()
{
    // Array to hold random bytes
    uint8_t randomBytes[DEVICEID_SIZE];
    esp_fill_random(randomBytes, DEVICEID_SIZE);

    // Array to hold the device ID string with quotes and null terminator
    char deviceId[DEVICEID_SIZE + 1];

    // Convert random bytes to English letters
    for (int i = 0; i < DEVICEID_SIZE; i++)
    {
        uint8_t randValue = randomBytes[i] % 52; // 52 letters in the English alphabet (26 uppercase + 26 lowercase)
        if (randValue < 26)
        {
            deviceId[i] = 'A' + randValue; // Map to uppercase letters
        }
        else
        {
            deviceId[i] = 'a' + (randValue - 26); // Map to lowercase letters
        }
    }

    // Add the closing quote and null terminator
    deviceId[DEVICEID_SIZE] = '\0';

    // Duplicate the string to m_deviceId
    m_deviceId = strdup(deviceId);
    if (m_deviceId == nullptr)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for device ID");
        return ESP_ERR_NO_MEM;
    }

    // Log the generated device ID
    ESP_LOGI(TAG, "Generated random device ID: %s", m_deviceId);

    return ESP_OK;
}

void MetricsModule::printStackTask()
{
    ESP_LOGW(TAG, "Free heap size: %d", (int) esp_get_free_heap_size());
    ESP_LOGW(TAG, "Minimum free heap size: %d", (int) esp_get_minimum_free_heap_size());

    TaskStatus_t * pxTaskStatusArray;
    volatile UBaseType_t uxArraySize;

    // Get the number of tasks
    uxArraySize = uxTaskGetNumberOfTasks();

    // Allocate memory for the task status array
    pxTaskStatusArray = (TaskStatus_t *) malloc(uxArraySize * sizeof(TaskStatus_t));

    // Get the system state
    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);

    // Print table header
    ESP_LOGW(TAG, "---------------------------------------------------------");
    ESP_LOGW(TAG, "|Task Name            |Status |Prio |HWM    |Task Number|");
    ESP_LOGW(TAG, "---------------------------------------------------------");

    // Print each task information
    for (UBaseType_t x = 0; x < uxArraySize; x++)
    {
        if ((int) pxTaskStatusArray[x].usStackHighWaterMark < 500)
        {
            ESP_LOGE(TAG, "|%-20s |%-6d |%-4d |%-6d |%-11d|", pxTaskStatusArray[x].pcTaskName,
                     (int) pxTaskStatusArray[x].eCurrentState, (int) pxTaskStatusArray[x].uxCurrentPriority,
                     (int) pxTaskStatusArray[x].usStackHighWaterMark, (int) pxTaskStatusArray[x].xTaskNumber);
        }
        else
        {
            ESP_LOGW(TAG, "|%-20s |%-6d |%-4d |%-6d |%-11d|", pxTaskStatusArray[x].pcTaskName,
                     (int) pxTaskStatusArray[x].eCurrentState, (int) pxTaskStatusArray[x].uxCurrentPriority,
                     (int) pxTaskStatusArray[x].usStackHighWaterMark, (int) pxTaskStatusArray[x].xTaskNumber);
        }
    }
    ESP_LOGW(TAG, "---------------------------------------------------------");

    // deallocate the array
    free(pxTaskStatusArray);
}