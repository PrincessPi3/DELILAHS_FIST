# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# set(COMPONENTS main esp_netif lwip startup esp_hw_support esp_system nvs_flash esp_wifi)
set(COMPONENTS main esp_netif startup esp_hw_support esp_system nvs_flash esp_wifi)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(DELILAHS_FIST)
