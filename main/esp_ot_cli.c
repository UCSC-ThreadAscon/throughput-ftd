/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Command Line Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
*/
#include "txpower.h"
#include "workload.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_ot_config.h"
#include "esp_vfs_eventfd.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/uart_types.h"
#include "nvs_flash.h"
#include "openthread/cli.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
#include "ot_led_strip.h"
#endif

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

#define TAG "ot_cli"
#define COAP_SECURE_SERVER_PORT CONFIG_COAP_SECURE_SERVER_PORT

static esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&cfg);
    assert(netif != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

    return netif;
}

static void ot_task_worker(void *aContext)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    // Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
    ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
#endif

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
    // The OpenThread log level directly matches ESP log level
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif
    // Initialize the OpenThread cli
#if CONFIG_OPENTHREAD_CLI
    esp_openthread_cli_init();
#endif

    esp_netif_t *openthread_netif;
    // Initialize the esp_netif bindings
    openthread_netif = init_openthread_netif(&config);
    esp_netif_set_default_netif(openthread_netif);

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
    esp_cli_custom_command_init();
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

  // TX power must be set before starting the OpenThread CLI.
  setTxPower();

    // Run the main loop
#if CONFIG_OPENTHREAD_CLI
    esp_openthread_cli_create_task();
#endif
#if CONFIG_OPENTHREAD_AUTO_START
    otOperationalDatasetTlvs dataset;
    otError error = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
    ESP_ERROR_CHECK(esp_openthread_auto_start((error == OT_ERROR_NONE) ? &dataset : NULL));
#endif
    esp_openthread_launch_mainloop();

    // Clean up
    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(openthread_netif);

    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Used eventfds:
    // * netif
    // * ot task queue
    // * radio driver
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    xTaskCreate(ot_task_worker, "ot_cli_main", 10240,
                xTaskGetCurrentTaskHandle(), 5, NULL);

    /** ---- Set up the CoAP Server ---- */
    checkConnection(OT_INSTANCE);
    x509Init();

    otError error =
      otCoapSecureStartWithMaxConnAttempts(OT_INSTANCE, COAP_SECURE_SERVER_PORT,
                                           0, NULL, NULL);

    if (error != OT_ERROR_NONE) {
      otLogCritPlat("Failed to start COAPS server.");
    } else {
      otLogNotePlat("Started CoAPS server at port %d.",
                    COAP_SECURE_SERVER_PORT);
    }

    // CoAP server handling aperiodic packets.
    otCoapResource aPeriodicResource;
    createAPeriodicResource(&aPeriodicResource);
    otCoapSecureAddResource(OT_INSTANCE, &aPeriodicResource);
    otLogNotePlat("Set up resource URI: '%s'.", aPeriodicResource.mUriPath);

    // CoAP server for handling periodic packets.
    otCoapResource periodicResource;
    createPeriodicResource(&periodicResource);
    otCoapSecureAddResource(OT_INSTANCE, &periodicResource);
    otLogNotePlat("Set up resource URI: '%s'.", periodicResource.mUriPath);

    /** ---- CoAP Client Code ---- */
    otSockAddr socket;
    otIp6Address server;

    EmptyMemory(&socket, sizeof(otSockAddr));
    EmptyMemory(&server, sizeof(otIp6Address));

    otIp6AddressFromString(CONFIG_SERVER_IP_ADDRESS, &server);
    socket.mAddress = server;
    socket.mPort = COAP_SECURE_SERVER_PORT;

    // Sending of periodic packets will be handled by a worker thread.
    xTaskCreate(periodicWorker, "periodic_client", 5120,
                xTaskGetCurrentTaskHandle(), 5, NULL);

    while (true) {
      if (otCoapSecureIsConnected(OT_INSTANCE)) {
        sendRequest(APeriodic, &server);

        uint32_t nextWaitTime = aperiodicWaitTimeMs();
        otLogNotePlat(
          "Will wait %" PRIu32 " ms before sending next aperiodic CoAP request.",
          nextWaitTime
        );

        TickType_t lastWakeupTime = xTaskGetTickCount();
        vTaskDelayUntil(&lastWakeupTime, MS_TO_TICKS(nextWaitTime));
      }
      else {
        clientConnect(&socket);
        vTaskDelay(MAIN_WAIT_TIME);
      }
     }
    return;
}
