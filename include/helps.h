#pragma once
#include "Arduino.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

class NullStream : public Stream
{
  public:
    int available() override
    {
        return 0;
    }

    void flush() override
    {
    }

    int peek() override
    {
        return -1;
    }

    int read() override
    {
        return -1;
    }

    size_t write(uint8_t u_Data) override
    {
        UNUSED(u_Data);
        return 0x01;
    }

    size_t write(const uint8_t *buffer, size_t size) override
    {
        UNUSED(buffer);
        return size;
    }
};

#define UART_BAUD_RATE (115200)
// #define UART_BUF_SIZE 1024
// #define UART_CONTROLLER UART_NUM_1
