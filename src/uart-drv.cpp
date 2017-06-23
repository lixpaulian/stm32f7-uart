/*
 * usart-drv.cpp
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
 * Created on: 11 Jun 2017 (LNP)
 */

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>
#include "uart-drv.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace os
{
  namespace driver
  {

    uart::uart (const char* name, UART_HandleTypeDef* huart, void* tx_buff,
                void* rx_buff, size_t tx_buff_size, size_t rx_buff_size) : //
        device_char
          { name }, //
        huart_
          { huart }, //
        tx_buff_
          { tx_buff }, //
        rx_buff_
          { rx_buff }, //
        tx_buff_size_
          { tx_buff_size }, //
        rx_buff_size_
          { rx_buff_size }
    {
      trace::printf ("%s() %p\n", __func__, this);

      if (tx_buff == nullptr)
        {
          tx_buff = malloc (tx_buff_size);
          assert(tx_buff != nullptr);
        }

      if (rx_buff == nullptr)
        {
          rx_buff = malloc (rx_buff_size_);
          assert(rx_buff != nullptr);
        }
    }

    uart::~uart ()
    {
      huart_ = nullptr;
      trace::printf ("%s() %p\n", __func__, this);
    }

    int
    uart::do_vopen (const char* path, int oflag, std::va_list args)
    {
      if (is_opened_)
        {
          errno = EEXIST; // Already opened
          return -1;
        }

      // initialize fifos and semaphores
      tx_in_ = tx_out_ = 0;
      rx_in_ = rx_out_ = 0;

      // reset semaphores
      tx_sem_.reset ();
      rx_sem_.reset ();

      is_opened_ = true;

      return 0;
    }

    int
    uart::do_close (void)
    {
      is_opened_ = false;
      return 0;
    }

    ssize_t
    uart::do_read (void* buf, std::size_t nbyte)
    {
      HAL_StatusTypeDef result;

      return 0;
    }

    ssize_t
    uart::do_write (const void* buf, std::size_t nbyte)
    {
      HAL_StatusTypeDef result;
      ssize_t count = 0;

      tx_sem_.wait();
      memcpy(tx_buff_, buf, count = std::min (tx_buff_size_, nbyte));

      // try to send the buffer
      result = HAL_UART_Transmit_DMA (huart_, (uint8_t *) tx_buff_, count);
      if (result != HAL_OK)
        {
          count = 0;
          switch (result)
          {
            case HAL_BUSY:
              errno = EBUSY;
              break;

            default:
              errno = EIO;
              break;
          }
        }
      return count;
    }

    bool
    uart::do_is_opened (void)
    {
      return true;
    }

    bool
    uart::do_is_connected (void)
    {
      return true;
    }

    /**
     * @brief  Transmit event call-back.
     */
    void
    uart::cb_tx_event (UART_HandleTypeDef* huart)
    {
      tx_sem_.post ();
    }

    /**
     * @brief  Receive event call-back. Here are reported receive errors too.
     */
    void
    uart::cb_rx_event (UART_HandleTypeDef* huart)
    {
      error_code_ = huart->ErrorCode;
      rx_sem_.post ();
    }

  } /* namespace driver */
} /* namespace os */

#pragma GCC diagnostic pop
