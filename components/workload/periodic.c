/**
 * This file contains the worker thread that will do periodic sending
 * of CoAP requests.
*/
#include "workload.h"

void periodicWorker(void* context) {
  otIp6Address *server = (otIp6Address *) context;

  while (true) {
    if (otCoapSecureIsConnected(OT_INSTANCE)) {
      sendRequest(Periodic, server);

        otLogNotePlat(
          "Will wait %" PRIu32 " ms before sending next the periodic CoAP request.",
          PERIODIC_WAIT_TIME
        );

        TickType_t lastWakeupTime = xTaskGetTickCount();
        vTaskDelayUntil(&lastWakeupTime, PERIODIC_WAIT_TIME);
    } else {
      vTaskDelay(MAIN_WAIT_TIME);
    }
  }
  return;
}