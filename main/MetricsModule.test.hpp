#pragma once

#include "testHelper.hpp"
#include "wifiHelper.hpp"

#include "MetricsModule.hpp"

TEST_CASE("MetricsModule", "[MetricsModule] [Start] [wifi]")
{
    // bool * wifiConnected = new bool(false);
    // wifi_init_sta(wifiConnected);
    // while (!*wifiConnected)
    // {
    //     vTaskDelay(100 / portTICK_PERIOD_MS);
    // }
    // vTaskDelay(100 / portTICK_PERIOD_MS);

    MetricsModule * metricsModule = new MetricsModule();
    metricsModule->start();
    vTaskDelay(5 * 1000 / portTICK_PERIOD_MS);

    uint32_t freeHeapSize = esp_get_free_heap_size();

    heap_trace_record_t trace_record[30];
    BEGIN_MEMORY_LEAK_TEST(trace_record);
    do
    {
        // wait for 2 minutes equal to 120000 milliseconds
        vTaskDelay(3 * 60 * 1000 / portTICK_PERIOD_MS);
    } while (0);
    END_MEMORY_LEAK_TEST(trace_record);

    uint32_t freeHeapSizeAfter = esp_get_free_heap_size();

    TEST_ASSERT_EQUAL(freeHeapSize, freeHeapSizeAfter);

    delete metricsModule;
}
//  freeaddrinfo().