#pragma once

#include "workload.h"

#define DELAY_URI "delay-confirmable"

otError createResource(otCoapResource *resource,
                       const char *resourceName,
                       otCoapRequestHandler requestHandler);

void resourceDestructor(otCoapResource *resource);
void sendCoapResponse(otMessage *aRequest, const otMessageInfo *aRequestInfo);
void printRequest(otMessage *aMessage, const otMessageInfo *aMessageInfo);

void defaultRequestHandler(void* aContext,
                           otMessage *aMessage,
                           const otMessageInfo *aMessageInfo);

void delayRequestHandler(void *aContext,
                         otMessage *aMessage,
                         const otMessageInfo *aMessageInfo);