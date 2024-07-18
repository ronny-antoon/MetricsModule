#include "MetricsModule.hpp"
#include "wifiHelper.hpp"

extern "C" void app_main()
{
    bool * wifiConnected = new bool(false);
    wifi_init_sta(wifiConnected);
    while (!*wifiConnected)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MetricsModule * metricsModule = new MetricsModule(nullptr, "TestLocation", nullptr);
    metricsModule->start();

    vTaskDelay(20000 / portTICK_PERIOD_MS);

    delete metricsModule;
}