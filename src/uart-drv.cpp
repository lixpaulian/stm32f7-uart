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

    uart::uart (const char* name, UART_HandleTypeDef* huart, uint8_t* tx_buff,
                uint8_t* rx_buff, size_t tx_buff_size, size_t rx_buff_size) : //
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

      // de-initialize the UART, as we assume this was automatically done by
      // the CubeMX generated code.
      if (huart->Instance != nullptr)
        {
          HAL_UART_DeInit (huart);
        }

      // if not using DMA, the rx_buff_size must be even
      rx_buff_size_ % 2 ? rx_buff_size_-- : rx_buff_size_;

      // if no rx/tx buffers supplied, create them dynamically
      if (tx_buff == nullptr)
        {
          tx_buff = (uint8_t *) malloc (tx_buff_size_);
          assert(tx_buff != nullptr);
        }

      if (rx_buff == nullptr)
        {
          rx_buff = (uint8_t *) malloc (rx_buff_size_);
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
      HAL_StatusTypeDef result;

      if (is_opened_)
        {
          errno = EEXIST; // Already opened
          return -1;
        }

      // initialize the UART
      HAL_UART_Init (huart_);

      // set initial timeout to infinitum (blocking)
      rx_timeout_ = 0xFFFFFFFF;

      // initialize FIFOs and semaphores
      tx_in_ = tx_out_ = 0;
      rx_in_ = rx_out_ = 0;

      // reset semaphores
      tx_sem_.reset ();
      rx_sem_.reset ();

      // clear receiver idle flag, then enable interrupt on receiver idle
      __HAL_UART_CLEAR_IDLEFLAG(huart_);
      __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);

      // start receiving, basically wait for input characters
      // check if we have DMA enabled for receive
      if (huart_->hdmarx == nullptr)
        {
          // this is receive via the non-DMA variant
          result = HAL_UART_Receive_IT (huart_, rx_buff_, rx_buff_size_ / 2);
        }
      else
        {
          // this is receive via the DMA variant
          result = HAL_UART_Receive_DMA (huart_, rx_buff_, rx_buff_size_);
        }

      if (result != HAL_OK)
        {
          switch (result)
            {
            case HAL_BUSY:
              errno = EBUSY;
              break;

            default:
              errno = EIO;
              break;
            }
          return -1;
        }
      else
        {
          is_opened_ = true;
          return 0;
        }
    }

    int
    uart::do_close (void)
    {
      // wait for possible ongoing write operation to finish
      while (huart_->gState == HAL_UART_STATE_BUSY_TX)
        {
          tx_sem_.wait ();
        }

      if (huart_->hdmarx != nullptr || huart_->hdmatx != nullptr)
        {
          // stop and disable the DMA
          HAL_UART_DMAStop (huart_);
        }

      // disable interrupt on receive idle and switch off the UART
      __HAL_UART_DISABLE_IT(huart_, UART_IT_IDLE);
      HAL_UART_DeInit (huart_);

      is_opened_ = false;

      return 0;
    }

    ssize_t
    uart::do_read (void* buf, std::size_t nbyte)
    {
      uint8_t* lbuf = (uint8_t *) buf;
      ssize_t count = 0;
      int c;

      while (rx_out_ == rx_in_)
        {
          if (rx_sem_.timed_wait (rx_timeout_) != os::rtos::result::ok)
            {
              return count;     // timeout, return 0 chars
            }
        }

      while (rx_out_ != rx_in_ && count < (ssize_t) nbyte)
        {
          c = rx_buff_[rx_out_++];

          // TODO: handle UART errors here (PE, FE, etc.)
          if (c >= 0)
            {
              *lbuf++ = c;
              count++;
              if (rx_out_ >= rx_buff_size_)
                {
                  rx_out_ = 0;
                }
            }
          else
            {
              count = c;
              break;
            }
        }

      return count;
    }

    ssize_t
    uart::do_write (const void* buf, std::size_t nbyte)
    {
      HAL_StatusTypeDef result;
      ssize_t count = 0;

      tx_sem_.wait ();
      memcpy (tx_buff_, buf, count = std::min (tx_buff_size_, nbyte));

      // send the buffer, as much as it can
      if (huart_->hdmatx == nullptr)
        {
          // the non-DMA variant
          result = HAL_UART_Transmit_IT (huart_, tx_buff_, count);
        }
      else
        {
          // the DMA variant
          result = HAL_UART_Transmit_DMA (huart_, tx_buff_, count);
        }

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
      return is_opened_;
    }

    bool
    uart::do_is_connected (void)
    {
      return true;      // TODO: check DCD, but where's the DCD line?
    }

    /**
     * @brief  Transmit event call-back.
     */
    void
    uart::cb_tx_event (void)
    {
      tx_sem_.post ();
    }

    /**
     * @brief  Receive event call-back. Here are reported receive errors too.
     */
    void
    uart::cb_rx_event (bool half)
    {
      size_t xfered;
      size_t half_buffer_size = rx_buff_size_ / 2;

      // TODO: handle errors (PE, FE, etc.) returned in huart->ErrorCode

      // compute the number of chars received during the last transfer
      if (huart_->hdmarx == nullptr)
        {
          // non DMA xfer
          xfered = rx_in_ - (rx_in_ >= half_buffer_size ? half_buffer_size : 0);
          xfered = half_buffer_size - xfered - huart_->RxXferCount;
        }
      else
        {
          // DMA xfer
          xfered = rx_buff_size_ - rx_in_ - huart_->hdmarx->Instance->NDTR;
        }

      // update the "in" pointer on buffer
      if ((rx_in_ += xfered) >= rx_buff_size_)
        {
          // if overflow, reset the "in" pointer, i.e. transfer was complete
          rx_in_ = 0;
        }

      // re-initialize system for receiving
      if (huart_->hdmarx == nullptr)
        {
          if (huart_->RxXferCount == 0)
            {
              HAL_UART_Receive_IT (
                  huart_, rx_in_ == 0 ? rx_buff_ : rx_buff_ + half_buffer_size,
                  half_buffer_size);
            }
        }
      else
        {
          if (half == false && rx_in_ == 0)
            {
              HAL_UART_Receive_DMA (huart_, rx_buff_, rx_buff_size_);
            }
        }

      rx_sem_.post ();
    }

  } /* namespace driver */
} /* namespace os */

#pragma GCC diagnostic pop
