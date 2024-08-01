cmake_minimum_required(VERSION 3.16)

file(GLOB SRC_FILES "src/*.cpp")

idf_component_register(SRCS "${SRC_FILES}"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client
                       PRIV_REQUIRES esp_netif esp_wifi)

# idf_build_set_property(COMPILE_OPTIONS "-DCONFIG_FREERTOS_USE_TRACE_FACILITY=y" APPEND)