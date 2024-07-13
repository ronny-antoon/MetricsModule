#pragma once    

#include <esp_heap_trace.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unity.h>
#include <unity_fixture.h>

#ifndef BEGIN_MEMORY_LEAK_TEST
#define BEGIN_MEMORY_LEAK_TEST(trace_record)                                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        heap_trace_stop();                                                                                                         \
        ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, sizeof(trace_record) / sizeof(trace_record[0])));                 \
        ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));                                                                       \
    } while (0)
#endif

#ifndef END_MEMORY_LEAK_TEST
#define END_MEMORY_LEAK_TEST(trace_record)                                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        ESP_ERROR_CHECK(heap_trace_stop());                                                                                        \
        if (trace_record[0].size != 0)                                                                                             \
        {                                                                                                                          \
            heap_trace_dump();                                                                                                     \
            TEST_FAIL_MESSAGE("Memory leak detected!");                                                                            \
        }                                                                                                                          \
    } while (0)
#endif