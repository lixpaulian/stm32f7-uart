/*
 * test-uart.cpp
 *
 * Copyright (c) 2017 Lix N. Paulian (lix@paulian.net)
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
 * Created on: 16 Jun 2017 (LNP)
 */

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include <cmsis-plus/posix-io/file-descriptors-manager.h>

#include "uart-drv.h"
#include "io.h"

extern "C"
{
  UART_HandleTypeDef huart6;
}

using namespace os;
using namespace os::rtos;

// Static manager
os::posix::file_descriptors_manager descriptors_manager
  { 8 };

#define TX_BUFFER_SIZE 200
#define RX_BUFFER_SIZE 200

uint8_t tx_buffer[TX_BUFFER_SIZE];
uint8_t rx_buffer[RX_BUFFER_SIZE];

driver::uart uart6
  { "uart6", &huart6, tx_buffer, rx_buffer, sizeof(tx_buffer), sizeof(rx_buffer) };

void
HAL_UART_TxCpltCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == huart6.Instance)
    {
      uart6.cb_tx_event (huart);
    }
}

void
HAL_UART_RxHalfCpltCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == huart6.Instance)
    {
      uart6.cb_rx_event (huart);
    }
}

void
HAL_UART_ErrorCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == huart6.Instance)
    {
      uart6.cb_rx_event (huart);
    }
}

/**
 * @brief  This is a test function that exercises the UART driver.
 */
void
test_uart (void)
{
  int fd, count;
  char text[] =
    { "The quick brown fox jumps over the lazy dog 1234567890\r\n" };

  // configure the MPI interface
  mpi_ctrl mpi
    { };

  mpi.init_pins ();
  mpi.rs485 (false);
  mpi.shutdown (false);
  mpi.half_duplex (false);

  // open the serial device
  if ((fd = open ("/dev/uart6", 0)) < 0)
    {
      trace::printf ("Error at open\n");
    }

  // send text
  for (int i = 0; i < 1000; i++)
    {
      if ((count = write (fd, text, strlen (text))) < 0)
        {
          trace::printf ("Error at write (%d)\n", i);
        }
    }

}
