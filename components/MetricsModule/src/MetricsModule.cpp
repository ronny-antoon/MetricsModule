#include "MetricsModule.hpp"
#include <arch/sys_arch.h>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char * TAG = "MetricsModule";
#define DEVICEID_SIZE 5

MetricsModule::MetricsModule(const char * databaseUrl, const char * deviceLocation) :
    m_keyId(0), m_senderTaskHandle(nullptr), m_databaseUrl(databaseUrl), m_deviceLocation(deviceLocation)
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
        ESP_LOGW(TAG, "No device ID provided, using default: %s", m_deviceLocation);
    }

    size_t urlSize = strlen(m_databaseUrl) + strlen(m_deviceLocation) + 2;
    char * fullURL = (char *) malloc(urlSize);
    if (fullURL == nullptr)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for full URL");
        return;
    }
    snprintf(fullURL, urlSize, "%s/%s", m_databaseUrl, m_deviceLocation);
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
    free((void *) m_metricsBuffer);
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
    bool isTimeCorrect   = false;
    while (true)
    {
        if (self->resetBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to reset buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 60 * 1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (self->addPrefixJsonToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add prefix JSON to buffer");
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 60 * 1000 / portTICK_PERIOD_MS);
            continue;
        }

        isTimeCorrect = self->addTimestampToBuffer() == ESP_OK;
        if (self->addDeviceIdToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add device ID to buffer");
        }
        if (self->addFreeHeapToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add free heap to buffer");
        }
        if (self->addTasksFreeStackToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add tasks free stack to buffer");
        }
        if (self->addPostfixJsonToBuffer() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add postfix JSON to buffer");
        }

        if (CONFIG_M_M_PRINT_METRICS_BUFFER)
        {
            if (self->printMetricBuffer() != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to print metric buffer");
            }
        }

        if (!self->checkNetworkConnection() && !isTimeCorrect)
        {
            ESP_LOGW(TAG, "No network connection or time not correct. Retrying in %d minutes", CONFIG_M_M_SEND_METRICS_PERIOD);
            vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 60 * 1000 / portTICK_PERIOD_MS);
            continue;
        }

        if (self->sendBufferedMetrics() != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send buffered metrics");
        }

        vTaskDelay(CONFIG_M_M_SEND_METRICS_PERIOD * 60 * 1000 / portTICK_PERIOD_MS);
    }
}

esp_err_t MetricsModule::resetBuffer()
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    m_keyId = 0;
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
    strcat(m_metricsBuffer, "{\"fields\":{");
    return ESP_OK;
}

esp_err_t MetricsModule::addPostfixJsonToBuffer()
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    strcat(m_metricsBuffer, "}}");
    return ESP_OK;
}

esp_err_t MetricsModule::addMetricToBuffer(const char * metricName, const char * metricValue)
{
    return addMetricToBuffer(metricName, metricValue, "integerValue");
}

esp_err_t MetricsModule::addMetricToBuffer(const char * metricName, const char * metricValue, const char * metricType)
{
    if (m_metricsBuffer == nullptr)
    {
        ESP_LOGE(TAG, "Metrics buffer is not allocated");
        return ESP_ERR_INVALID_STATE;
    }
    if (strlen(m_metricsBuffer) + strlen(metricName) + strlen(metricValue) + strlen(metricType) + 32 > CONFIG_M_M_BUFFER_SIZE)
    {
        ESP_LOGE(TAG, "Buffer is full. Failed to add metric");
        return ESP_ERR_NO_MEM;
    }

    strcat(m_metricsBuffer, "\"");
    strcat(m_metricsBuffer, metricName);
    strcat(m_metricsBuffer, "\":{\"");
    strcat(m_metricsBuffer, metricType);
    strcat(m_metricsBuffer, "\":");
    strcat(m_metricsBuffer, metricValue);
    strcat(m_metricsBuffer, "},");
    return ESP_OK;
}

esp_err_t MetricsModule::addTimestampToBuffer()
{
    char timestamp[30];
    time_t now;
    struct tm timeinfo;
    time(&now);

    // Set timezone to UTC
    setenv("TZ", "UTC", 1);

    tzset();
    localtime_r(&now, &timeinfo);
    if ((timeinfo.tm_year + 1900) < 2020)
    {
        ESP_LOGW(TAG, "Time is not correct (%d year). Skipping timestamp metric", (timeinfo.tm_year + 1900));
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_netif_sntp_init(&config);
        return ESP_FAIL;
    }
    strftime(timestamp, sizeof(timestamp), "\"%Y-%m-%dT%H:%M:%SZ\"", &timeinfo);
    return addMetricToBuffer("timestamp", timestamp, "timestampValue");
}

esp_err_t MetricsModule::addDeviceIdToBuffer()
{
    return addMetricToBuffer("deviceId", m_deviceId, "stringValue");
}

esp_err_t MetricsModule::addFreeHeapToBuffer()
{
    const char * freeHeapKey    = "freeHeap";
    const char * minFreeHeapKey = "minFreeHeap";

    char freeHeapValue[40];
    snprintf(freeHeapValue, sizeof(freeHeapValue), "%d", (int) esp_get_free_heap_size());

    char minFreeHeapValue[40];
    snprintf(minFreeHeapValue, sizeof(minFreeHeapValue), "%d", (int) esp_get_minimum_free_heap_size());

    addMetricToBuffer(freeHeapKey, freeHeapValue);
    addMetricToBuffer(minFreeHeapKey, minFreeHeapValue);

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
        char metricValue[40];
        if (strcmp(pxTaskStatusArray[i].pcTaskName, "IDLE") == 0)
        {
            snprintf(metricName, sizeof(metricName), "IDLE_%d", (int) pxTaskStatusArray[i].xTaskNumber);
        }
        else
        {
            snprintf(metricName, sizeof(metricName), "%s", pxTaskStatusArray[i].pcTaskName);
        }
        snprintf(metricValue, sizeof(metricValue), "%d", (int) pxTaskStatusArray[i].usStackHighWaterMark);
        addMetricToBuffer(metricName, metricValue);
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
    char deviceId[DEVICEID_SIZE + 2 + 1];

    // Add the opening quote
    deviceId[0] = '"';

    // Convert random bytes to English letters
    for (int i = 0; i < DEVICEID_SIZE; i++)
    {
        uint8_t randValue = randomBytes[i] % 52; // 52 letters in the English alphabet (26 uppercase + 26 lowercase)
        if (randValue < 26)
        {
            deviceId[i + 1] = 'A' + randValue; // Map to uppercase letters
        }
        else
        {
            deviceId[i + 1] = 'a' + (randValue - 26); // Map to lowercase letters
        }
    }

    // Add the closing quote and null terminator
    deviceId[DEVICEID_SIZE + 1] = '"';
    deviceId[DEVICEID_SIZE + 2] = '\0';

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
