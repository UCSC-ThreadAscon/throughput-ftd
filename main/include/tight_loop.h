#pragma once

#include "workload.h"

#define THROUGHPUT_CONFIRMABLE_URI "throughput-confirmable"
#define THROUGHPUT_NONCONFIRMABLE_URI "throughput-nonconfirmable"
#define PACKET_LOSS_CONFIRMABLE_URI "packet-loss-confirmable"
#define PACKET_LOSS_NONCONFIRMABLE_URI "packet-loss-nonconfirmable"

#define TIGHT_LOOP_PAYLOAD_BYTES 4

#define InitSocket(socketPtr)                            \
  EmptyMemory(socketPtr, sizeof(otSockAddr));            \
  *socketPtr = createSocket(CONFIG_SERVER_IP_ADDRESS);   \

void createRandomPayload(uint8_t *buffer);
void tpConfirmableMain(void);
void tpNonConfirmableMain(void);

void plConfirmableMain(void);
void plNonConfirmableMain(void);

void tpConfirmableResponseCallback(void *aContext,
                                   otMessage *aMessage,
                                   const otMessageInfo *aMessageInfo,
                                   otError aResult);
void tpConfirmableSend(otSockAddr *socket);

void plConfirmableResponseCallback(void *aContext,
                                   otMessage *aMessage,
                                   const otMessageInfo *aMessageInfo,
                                   otError aResult);
void plConfirmableSend(otSockAddr *socket);