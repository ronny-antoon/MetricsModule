# MetricsModule

MetricsModule is a C++ class designed for collecting and sending metrics data from an ESP32-based device. The module supports buffering metrics in JSON format and sending them to a specified database URL. It integrates with FreeRTOS for task management and leverages ESP-IDF for network and logging capabilities.

## Features

- Collects various system metrics (e.g., free heap, task stack sizes).
- Buffers metrics in JSON format.
- Sends buffered metrics to a remote server.
- Handles network connectivity checks.
- Generates a unique device ID.
- Configurable through `sdkconfig`.

## Getting Started

### Prerequisites

- ESP-IDF setup
- FreeRTOS

### Installation

1. Clone the repository:

    ```sh
    git clone <repository-url>
    cd <repository-directory>
    ```

2. Set up ESP-IDF:

    Follow the [official ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up the ESP-IDF environment.

3. Configure the project:

    ```sh
    idf.py menuconfig
    ```

    Ensure that you set the following configurations:
    - `CONFIG_M_M_DEFAULT_DATABASE_URL` to your metrics database URL.
    - `CONFIG_M_M_DEFAULT_DEVICE_LOCATION` to the location of your device.
    - Enable `CONFIG_M_M_ENABLED` to start the metrics collection.

### Building and Flashing

Build and flash the project to your ESP32 device:

```sh
idf.py build
idf.py flash
idf.py monitor
```

### Usage

After flashing the firmware, the MetricsModule will start collecting and sending metrics data. You can customize and extend the metrics collection by modifying the MetricsModule class methods.

### Example

To use the MetricsModule:

```cpp
#include "MetricsModule.hpp"

extern "C" void app_main()
{
    MetricsModule metrics("http://your-database-url", "device-location");
    metrics.start();
}
```
