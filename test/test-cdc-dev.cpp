/*
 * test-cdc-dev.cpp
 *
 * Copyright (c) 2018 Lix N. Paulian (lix@paulian.net)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Created on: 15 Jan 2018 (LNP)
 */

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include <cmsis-plus/posix-io/file-descriptors-manager.h>

#include "sysconfig.h"
#include "test-cdc-dev.h"
#include "usbd_cdc_if.h"

#if (UART_CDC_DEV_TEST == true)

using namespace os;
using namespace os::rtos;
using namespace os::driver::stm32f7;

// Static manager
os::posix::file_descriptors_manager descriptors_manager
  { 8 };

#define TX_BUFFER_SIZE 400
#define RX_BUFFER_SIZE 400

#define CTRL_C 3

// Note: both USB peripherals are instantiated to show how two DCD devices can
// be implemented. However, in the example below only one peripheral is used.

uart_cdc_dev cdc0
  { "cdc0", DEVICE_FS, nullptr, nullptr, TX_BUFFER_SIZE, RX_BUFFER_SIZE };

uart_cdc_dev cdc1
  { "cdc1", DEVICE_HS, nullptr, nullptr, TX_BUFFER_SIZE, RX_BUFFER_SIZE };

int8_t
cdc_init (USBD_HandleTypeDef* husbd)
{
  if (husbd->id == DEVICE_FS)
    {
      return cdc0.cb_init_event ();
    }
  if (husbd->id == DEVICE_HS)
    {
      return cdc1.cb_init_event ();
    }
  return USBD_OK;
}

int8_t
cdc_deinit (USBD_HandleTypeDef* husbd)
{
  if (husbd->id == DEVICE_FS)
    {
      return cdc0.cb_deinit_event ();
    }
  if (husbd->id == DEVICE_HS)
    {
      return cdc1.cb_deinit_event ();
    }
  return USBD_OK;
}

int8_t
cdc_control (USBD_HandleTypeDef* husbd, uint8_t cmd, uint8_t* pbuf,
             uint16_t length)
{
  if (husbd->id == DEVICE_FS)
    {
      return cdc0.cb_control_event (cmd, pbuf, length);
    }
  if (husbd->id == DEVICE_HS)
    {
      return cdc1.cb_control_event (cmd, pbuf, length);
    }
  return USBD_OK;
}

int8_t
cdc_receive (USBD_HandleTypeDef* husbd, uint8_t* buf, uint32_t *len)
{
  if (husbd->id == DEVICE_FS)
    {
      return cdc0.cb_receive_event (buf, len);
    }
  if (husbd->id == DEVICE_HS)
    {
      return cdc1.cb_receive_event (buf, len);
    }
  return USBD_OK;
}

/**
 * @brief  This is a test function that exercises the UART driver.
 */
void
test_uart_cdc (void)
{
  int leave = false;

  char buffer[520];

  os::posix::tty* tty;

  while (1)
    {
      // open the serial device
      tty = static_cast<os::posix::tty*> (os::posix::open ("/dev/cdc1", 0));
      if (tty == nullptr)
        {
          trace::printf ("Error at open\n");
        }
      else
        {
          // get serial port parameters
          struct termios tios;
          if (tty->tcgetattr (&tios) < 0)
            {
              trace::printf ("Error getting serial port parameters\n");
            }
          else
            {
              trace::printf (
                  "Serial port parameters: "
                  "%d baud, %d bits, %d stop bit(s), %s parity, flow control %s\r\n",
                  tios.c_ispeed,
                  (tios.c_cflag & CSIZE) == CS7 ? 7 :
                  (tios.c_cflag & CSIZE) == CS8 ? 8 : 9,
                  tios.c_cflag & CSTOPB ? 2 : 1,
                  tios.c_cflag & PARENB ?
                      tios.c_cflag & PARODD ? "odd" : "even" : "no",
                  (tios.c_cflag & CRTSCTS) == CRTSCTS ? "RTS/CTS" :
                  (tios.c_cflag & CRTSCTS) == CCTS_OFLOW ? "CTS" :
                  (tios.c_cflag & CRTSCTS) == CRTS_IFLOW ? "RTS" : "none");
            }

          for (;;)
            {
              int count;

              // read text
              count = tty->read (buffer, sizeof(buffer));
              trace::printf ("got %d\n", count);
              if (buffer[0] == CTRL_C)
                {
                  leave = true;
                  break;
                }
              if (count > 0)
                {
                  count = tty->write (buffer, count);
                  trace::printf ("sent %d\n", count);
                  if (count < 0)
                    {
                      trace::printf ("Error at write\n");
                      break;
                    }
                }
              else if (count < 0)
                {
                  trace::printf ("Error reading data\n");
                  break;
                }
            }

          // close the serial device
          if (tty->close () < 0)
            {
              trace::printf ("Error at close\n");
            }
        }
      if (leave == true)
        {
          break;        // exit
        }
    }
}

#endif
