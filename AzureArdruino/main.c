#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include "epoll_timerfd_utilities.h"
#include <applibs/uart.h>
#include <applibs/gpio.h>
#include <applibs/log.h>

#include "mt3620_rdb.h"

// This C application for the MT3620 Reference Development Board (Azure Sphere)
// outputs a string every second to Visual Studio's Device Output window
//
// It uses the API for the following Azure Sphere application libraries:
// - log (messages shown in Visual Studio's Device Output window during debugging)
static int uartFd = -1;
static int incomingDataLedGpioFd = -1;
static int epollFd = -1;
static size_t totalBytesReceived = 0;

static volatile sig_atomic_t terminationRequired = false;

/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
    terminationRequired = true;
}

static void charArrayToNumbers(uint8_t *receiveBuffer, ssize_t bufferLen, float *num)
{
    uint8_t i=0;
    uint8_t delim[] = ",";
    char *tmp = strtok(receiveBuffer, delim);
    while (tmp != NULL)
    {
        sscanf(tmp, "%f", &num[i++]);
        tmp = strtok(NULL, delim);
    }
}

static void UartEventHandler(EventData *eventData)
{
    const size_t receiveBufferSize = 256;
    uint8_t receiveBuffer[receiveBufferSize + 1]; // allow extra byte for string termination
    float num[2];
    ssize_t bytesRead;

    // Read UART message
    bytesRead = read(uartFd, receiveBuffer, receiveBufferSize);
    if (bytesRead < 0) {
        Log_Debug("ERROR: Could not read UART: %s (%d).\n", strerror(errno), errno);
        terminationRequired = true;
        return;
    }

    if (bytesRead > 0) {
        // Null terminate the buffer to make it a valid string, and print it
        receiveBuffer[bytesRead] = 0;
        charArrayToNumbers(receiveBuffer, bytesRead, num);
        Log_Debug("Humidiy %.2f, Temperature %.2f\n", num[0], num[1]);
        // If the total received bytes is odd, turn the LED on, otherwise turn it off
        totalBytesReceived += (size_t)bytesRead;
        int result =
            GPIO_SetValue(incomingDataLedGpioFd,
            (totalBytesReceived % 2) == 1 ? GPIO_Value_Low : GPIO_Value_High);
        if (result != 0) {
            Log_Debug("ERROR: Could not set LED output value: %s (%d).\n", strerror(errno), errno);
            terminationRequired = true;
        }
    }
}

static EventData uartEventData = { .eventHandler = &UartEventHandler };

static int InitPeripheralsAndHandlers(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);

    epollFd = CreateEpollFd();
    if (epollFd < 0) {
        return -1;
    }

    // Create a UART_Config object, open the UART and set up UART event handler
    UART_Config uartConfig;
    UART_InitConfig(&uartConfig);
    uartConfig.baudRate = 115200;
    uartConfig.flowControl = UART_FlowControl_None;
    uartFd = UART_Open(MT3620_RDB_HEADER2_ISU0_UART, &uartConfig);
    if (uartFd < 0) {
        Log_Debug("ERROR: Could not open UART: %s (%d).\n", strerror(errno), errno);
        return -1;
    }
    if (RegisterEventHandlerToEpoll(epollFd, uartFd, &uartEventData, EPOLLIN) != 0) {
        return -1;
    }

    // Open LED GPIO and set as output with value GPIO_Value_High (off)
    Log_Debug("Opening MT3620_RDB_LED2_RED.\n");
    incomingDataLedGpioFd =
        GPIO_OpenAsOutput(MT3620_RDB_LED2_RED, GPIO_OutputMode_PushPull, GPIO_Value_High);
    if (incomingDataLedGpioFd < 0) {
        Log_Debug("ERROR: Could not open LED GPIO: %s (%d).\n", strerror(errno), errno);
        return -1;
    }

    return 0;
}

static void ClosePeripheralsAndHandlers(void)
{
    // Leave the LED off
    if (incomingDataLedGpioFd >= 0) {
        GPIO_SetValue(incomingDataLedGpioFd, GPIO_Value_High);
    }

    Log_Debug("Closing file descriptors.\n");
    CloseFdAndPrintError(incomingDataLedGpioFd, "IncomingDataLedGpio");
    CloseFdAndPrintError(uartFd, "Uart");
    CloseFdAndPrintError(epollFd, "Epoll");
}

/// <summary>
///     Main entry point for this sample.
/// </summary>
int main(int argc, char *argv[])
{
    Log_Debug("UART application starting.\n");
    if (InitPeripheralsAndHandlers() != 0) {
        terminationRequired = true;
    }

    // Use epoll to wait for events and trigger handlers, until an error or SIGTERM happens
    while (!terminationRequired) {
        if (WaitForEventAndCallHandler(epollFd) != 0) {
            terminationRequired = true;
        }
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return 0;
}
