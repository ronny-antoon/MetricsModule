#pragma once

#include <esp_err.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class MetricsModule
{
public:
    MetricsModule(const char * databaseURL = nullptr, const char * deviceLocation = nullptr);

    ~MetricsModule();

    esp_err_t start();

private:
    uint8_t m_keyId;
    char * m_metricsBuffer;
    TaskHandle_t m_senderTaskHandle;
    const char * m_databaseURL;
    const char * m_deviceId;
    const char * m_deviceLocation;

    static void senderTask(void * pvParameters);

    esp_err_t resetBuffer();

    esp_err_t addPrefixJsonToBuffer();

    esp_err_t addPostfixJsonToBuffer();

    esp_err_t addMetricToBuffer(const char * metricName, const char * metricValue);

    esp_err_t addMetricToBuffer(const char * metricName, const char * metricValue, const char * metricType);

    esp_err_t addTimestampToBuffer();

    esp_err_t addDeviceIdToBuffer();

    esp_err_t addFreeHeapToBuffer();

    esp_err_t addTasksFreeStackToBuffer();

    esp_err_t sendBufferedMetrics();

    esp_err_t printMetricBuffer();

    bool checkNetworkConnection();

    esp_err_t generatRandomDeviceId();

    MetricsModule(const MetricsModule &)             = delete;
    MetricsModule & operator=(const MetricsModule &) = delete;
};
