#pragma once

#include <esp_err.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @class MetricsModule
 * @brief A class for collecting and sending metrics data.
 */
class MetricsModule
{
public:
    /**
     * @brief Constructs a new MetricsModule object.
     * @param databaseUrl URL of the database to send metrics to.
     * @param deviceLocation Location of the device.
     */
    MetricsModule(const char * databaseUrl = nullptr, const char * deviceLocation = nullptr);

    /**
     * @brief Destroys the MetricsModule object.
     */
    ~MetricsModule();

    /**
     * @brief Starts the metrics collection and sending task.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t start();

private:
    uint8_t m_keyId;                 ///< Key ID for metrics buffer.
    char * m_metricsBuffer;          ///< Buffer for storing metrics data.
    TaskHandle_t m_senderTaskHandle; ///< Handle for the sender task.
    const char * m_databaseUrl;      ///< URL of the metrics database.
    const char * m_deviceId;         ///< Device ID for metrics.
    const char * m_deviceLocation;   ///< Location of the device.

    /**
     * @brief Task function for sending metrics data.
     * @param pvParameters Parameters for the task.
     */
    static void senderTask(void * pvParameters);

    /**
     * @brief Resets the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t resetBuffer();

    /**
     * @brief Adds JSON prefix to the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addPrefixJsonToBuffer();

    /**
     * @brief Adds JSON postfix to the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addPostfixJsonToBuffer();

    /**
     * @brief Adds a metric to the metrics buffer.
     * @param metricName Name of the metric.
     * @param metricValue Value of the metric.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addMetricToBuffer(const char * metricName, const char * metricValue);

    /**
     * @brief Adds a metric with a specified type to the metrics buffer.
     * @param metricName Name of the metric.
     * @param metricValue Value of the metric.
     * @param metricType Type of the metric.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addMetricToBuffer(const char * metricName, const char * metricValue, const char * metricType);

    /**
     * @brief Adds a timestamp to the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addTimestampToBuffer();

    /**
     * @brief Adds the device ID to the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addDeviceIdToBuffer();

    /**
     * @brief Adds the free heap size to the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addFreeHeapToBuffer();

    /**
     * @brief Adds the free stack size of tasks to the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t addTasksFreeStackToBuffer();

    /**
     * @brief Sends the buffered metrics to the server.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t sendBufferedMetrics();

    /**
     * @brief Prints the metrics buffer.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t printMetricBuffer();

    /**
     * @brief Checks the network connection.
     * @return True if connected, false otherwise.
     */
    bool checkNetworkConnection();

    /**
     * @brief Generates a random device ID.
     * @return ESP_OK on success, error code otherwise.
     */
    esp_err_t generateRandomDeviceId();

    MetricsModule(const MetricsModule &)             = delete;
    MetricsModule & operator=(const MetricsModule &) = delete;
};
