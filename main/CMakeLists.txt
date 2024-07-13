cmake_minimum_required(VERSION 3.16)

idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS ""
                       REQUIRES 
                       PRIV_REQUIRES MetricsModule nvs_flash esp_wifi unity)