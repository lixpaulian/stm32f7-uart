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

static const speed_t br_tab[] =
  { 0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600,
      19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000, 921600 };

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

      // de-initialize the UART, as we assume it was automatically initialized
      // by the CubeMX generated code in the CubeMX's main () function.
      if (huart->Instance != nullptr)
        {
          HAL_UART_DeInit (huart);
        }

      // if not using DMA, the rx_buff_size must be even
      rx_buff_size_ % 2 ? rx_buff_size_-- : rx_buff_size_;
    }

    uart::~uart ()
    {
      trace::printf ("%s() %p\n", __func__, this);

      huart_ = nullptr;
      is_opened_ = false;
    }

    int
    uart::do_vopen (const char* path, int oflag, std::va_list args)
    {
      HAL_StatusTypeDef hal_result = HAL_OK;
      int result = -1;

      do
        {
          if (is_opened_)
            {
              errno = EEXIST; // already opened
              break;
            }

          // initialize the UART
          if ((hal_result = HAL_UART_Init (huart_)) != HAL_OK)
            {
              break;
            }

          // clear receiver idle flag, then enable interrupt on receiver idle
          __HAL_UART_CLEAR_IDLEFLAG(huart_);
          __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);

          // if no rx/tx static buffers supplied, create them dynamically
          if (tx_buff_ == nullptr)
            {
              if ((tx_buff_ = (uint8_t *) malloc (tx_buff_size_)) == nullptr)
                {
                  errno = ENOMEM;
                  break;
                }
              else
                {
                  tx_buff_dyn_ = true;
                }
            }
          else
            {
              tx_buff_dyn_ = false;
            }

          if (rx_buff_ == nullptr)
            {
              if ((rx_buff_ = (uint8_t *) malloc (rx_buff_size_)) == nullptr)
                {
                  errno = ENOMEM;
                  break;
                }
              else
                {
                  rx_buff_dyn_ = true;
                }
            }
          else
            {
              rx_buff_dyn_ = false;
            }

          // set initial timeout to infinitum (i.e. blocking)
          rx_timeout_ = 0xFFFFFFFF;

          // initialize FIFOs and semaphores
          tx_in_ = tx_out_ = 0;
          rx_in_ = rx_out_ = 0;

          // reset semaphores
          tx_sem_.reset ();
          rx_sem_.reset ();

          // start receiving, basically wait for input characters
          // check if we have DMA enabled for receive
          if (huart_->hdmarx == nullptr)
            {
              // enable receive through UART interrupt transfers
              hal_result = HAL_UART_Receive_IT (huart_, rx_buff_,
                                                rx_buff_size_ / 2);
            }
          else
            {
              // enable receive through DMA transfers
              hal_result = HAL_UART_Receive_DMA (huart_, rx_buff_,
                                                 rx_buff_size_);
            }
        }
      while (false);

      if (hal_result != HAL_OK)
        {
          switch (hal_result)
            {
            case HAL_BUSY:
              errno = EBUSY;
              break;

            default:
              errno = EIO;
              break;
            }
        }
      else
        {
          is_opened_ = true;
          result = 0;
        }

      return result;
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

      // clean-up dynamic allocations, if any
      if (tx_buff_dyn_ == true)
        {
          free (tx_buff_);
          tx_buff_ = nullptr;
        }

      if (rx_buff_dyn_ == true)
        {
          free (rx_buff_);
          rx_buff_ = nullptr;
        }

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
          // non-DMA transfer
          result = HAL_UART_Transmit_IT (huart_, tx_buff_, count);
        }
      else
        {
          // DMA transfer
          // clean the data cache to mitigate incoherence before DMA transfers
          // (all RAM except DTCM RAM is cached)
          if ((tx_buff_ + tx_buff_size_) >= (uint8_t *) SRAM1_BASE)
            {
              uint32_t *aligned_buff = (uint32_t *) (((uint32_t) (tx_buff_))
                  & 0xFFFFFFE0);
              uint32_t aligned_count = (uint32_t) (tx_buff_size_ & 0xFFFFFFE0)
                  + 32;
              SCB_CleanDCache_by_Addr (aligned_buff, aligned_count);
            }
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

    int
    uart::do_tcgetattr (struct termios *ptio)
    {
      // clear termios structure
      bzero ((void *) ptio, sizeof(struct termios));

      // termios.h: CSIZE: CS5, CS6, CS7, CS8
      // note: ST uses a standard bit for parity, must be subtracted from total
      if (huart_->Init.Parity == UART_PARITY_NONE)
        {
          ptio->c_cflag =
              huart_->Init.WordLength == UART_WORDLENGTH_9B ? 0 :
              huart_->Init.WordLength == UART_WORDLENGTH_8B ? CS8 : CS7;
        }
      else
        {
          ptio->c_cflag =
              huart_->Init.WordLength == UART_WORDLENGTH_9B ? CS8 :
              huart_->Init.WordLength == UART_WORDLENGTH_8B ? CS7 : CS6;
        }

      // termios.h: CSTOPB: if true, two stop bits, otherwise only one
      ptio->c_cflag |= huart_->Init.StopBits == UART_STOPBITS_2 ? CSTOPB : 0;

      // termios.h: flags PARENB = parity enabled, PARODD = parity odd
      ptio->c_cflag |= huart_->Init.Parity == UART_PARITY_NONE ? 0 : PARENB;
      ptio->c_cflag |= huart_->Init.Parity == UART_PARITY_ODD ? PARODD : 0;

      // get baud rate
      int i;
      for (i = 0; i < (int) sizeof(br_tab); i++)
        {
          if (br_tab[i] == huart_->Init.BaudRate)
            break;
        }
      if (i < (int) sizeof(br_tab))
        {
          ptio->c_cflag |= i;
          ptio->c_ispeed = huart_->Init.BaudRate;
          ptio->c_ospeed = huart_->Init.BaudRate;
        }

      return 0;
    }

    int
    uart::do_tcsetattr (int options, const struct termios *ptio)
    {
      HAL_StatusTypeDef result;

      // set parity
      huart_->Init.Parity =
          ptio->c_cflag & PARENB ?
              (ptio->c_cflag & PARODD ? UART_PARITY_ODD : UART_PARITY_EVEN) :
              UART_PARITY_NONE;

      // set character size
      if (huart_->Init.Parity == UART_PARITY_NONE)
        {
          // ST UARTs can't do 6 and 5 bit characters, they do however 9 bit
          huart_->Init.WordLength =
              ptio->c_cflag & CS8 ? UART_WORDLENGTH_8B :
              ptio->c_cflag & CS7 ? UART_WORDLENGTH_7B : UART_WORDLENGTH_9B;
        }
      else
        {
          huart_->Init.WordLength =
              ptio->c_cflag & CS8 ? UART_WORDLENGTH_9B :
              ptio->c_cflag & CS7 ? UART_WORDLENGTH_8B : UART_WORDLENGTH_7B;
        }

      // set number of stop bits
      huart_->Init.StopBits =
          ptio->c_cflag & CSTOPB ? UART_STOPBITS_2 : UART_STOPBITS_1;

      // set baud rate; if baud rates in c_cflag are not specified, we use
      // the values in c_ispeed or c_ospeed
      huart_->Init.BaudRate =
          ptio->c_cflag & 037 /* see note */? br_tab[ptio->c_cflag & 037] :
          ptio->c_ispeed ? ptio->c_ispeed : ptio->c_ospeed;
      // note: the 037 mask (octal!) is defined in termios.h as CBAUD, but
      // its availability is conditional in the header file currently used;
      // hopefully this issue will be clarified in a future version of µOS++.

      // TODO: handle options

      // before sending the new configuration, stop UART
      __HAL_UART_DISABLE(huart_);

      // send config and restart UART
      result = UART_SetConfig (huart_);
      __HAL_UART_ENABLE(huart_);

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

      return 0;
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

      // TODO: handle errors (PE, FE, etc.) Caution: the errors should be
      // read directly from the UART instance, as the interrupt on idle
      // does not pass through the HAL interrupt handler!

      // compute the number of chars received during the last transfer
      if (huart_->hdmarx == nullptr)
        {
          // non-DMA transfer
          xfered = rx_in_ - (rx_in_ >= half_buffer_size ? half_buffer_size : 0);
          xfered = half_buffer_size - xfered - huart_->RxXferCount;
        }
      else
        {
          // DMA transfer
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
          // for non-DMA transfer
          if (huart_->RxXferCount == 0)
            {
              HAL_UART_Receive_IT (
                  huart_, rx_in_ == 0 ? rx_buff_ : rx_buff_ + half_buffer_size,
                  half_buffer_size);
            }
        }
      else
        {
          // for DMA transfer
          // flush and clean the data cache to mitigate incoherence after
          // DMA transfers (all but the DTCM RAM is cached)
          if ((rx_buff_ + rx_buff_size_) >= (uint8_t *) SRAM1_BASE)
            {
              uint32_t *aligned_buff = (uint32_t *) (((uint32_t) (rx_buff_))
                  & 0xFFFFFFE0);
              uint32_t aligned_count = (uint32_t) (rx_buff_size_ & 0xFFFFFFE0)
                  + 32;
              SCB_CleanInvalidateDCache_by_Addr (aligned_buff, aligned_count);
            }

          // reload DMA receive
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
