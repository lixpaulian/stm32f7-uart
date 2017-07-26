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

#define TEST_ROUNDS 10
#define WRITE_READ_ROUNDS 10

uint8_t tx_buffer[TX_BUFFER_SIZE];
uint8_t rx_buffer[RX_BUFFER_SIZE];

static ssize_t
targeted_read (int filedes, char *buffer, size_t expected_size);

driver::uart uart6
  { "uart6", &huart6, nullptr, nullptr, TX_BUFFER_SIZE, RX_BUFFER_SIZE };

void
HAL_UART_TxCpltCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == huart6.Instance)
    {
      uart6.cb_tx_event ();
    }
}

void
HAL_UART_RxCpltCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == huart6.Instance)
    {
      uart6.cb_rx_event (false);
    }
}

void
HAL_UART_RxHalfCpltCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == huart6.Instance)
    {
      uart6.cb_rx_event (true);
    }
}

void
HAL_UART_ErrorCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == huart6.Instance)
    {
      uart6.cb_rx_event (false);
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
  char text_end[] =
    { "---------\r\n" };
  char buffer[100];

#ifdef M717
  // configure the MPI interface
  mpi_ctrl mpi
    { };

  /* set to RS232 */
  mpi.init_pins ();
  mpi.rs485 (false);
  mpi.shutdown (false);
  mpi.half_duplex (false);
#endif

  for (int i = 0; i < TEST_ROUNDS; i++)
    {
      // open the serial device
      if ((fd = open ("/dev/uart6", 0)) < 0)
        {
          trace::printf ("Error at open\n");
        }
      else
        {
          // get serial port parameters
          struct termios tios;
          if (uart6.do_tcgetattr (&tios) < 0)
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

          for (int j = 0; j < WRITE_READ_ROUNDS; j++)
            {
              // send text
              if ((count = write (fd, text, strlen (text))) < 0)
                {
                  trace::printf ("Error at write (%d)\n", j);
                  break;
                }

              // read text
              count = targeted_read (fd, buffer, strlen (text));
              if (count > 0)
                {
                  buffer[count] = '\0';
                  trace::printf ("%s", buffer);
                }
              else
                {
                  trace::printf ("Error reading data\n");
                }
            }

          // send separating dashes
          if ((count = write (fd, text_end, strlen (text_end))) < 0)
            {
              trace::printf ("Error at write end text\n");
              break;
            }

          // read separating dashes
          count = targeted_read (fd, buffer, strlen (text_end));
          if (count > 0)
            {
              buffer[count] = '\0';
              trace::printf ("%s", buffer);
            }
          else
            {
              trace::printf ("Error reading separator\n");
            }

          // close the serial device
          if ((close (fd)) < 0)
            {
              trace::printf ("Error at close\n");
              break;
            }
        }
    }
}

/**
 * @brief This function waits to read a known amount of bytes before returning.
 * @param fd: file descriptor.
 * @param buffer: buffer to return data into.
 * @param expected_size: the expected number of characters to wait for.
 * @return The number of characters read or an error if negative.
 */
static ssize_t
targeted_read (int fd, char *buffer, size_t expected_size)
{
  int count, total = 0;

  do
    {
      if ((count = read (fd, buffer + total, expected_size - total)) < 0)
        {
          break;
        }
      total += count;
    }
  while (total < (int) expected_size);

  return total;
}
