/*
 * usart-drv.cpp
 *
 * Copyright (c) 2017-2019 Lix N. Paulian (lix@paulian.net)
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

// Set this switch to true if your UART(s) is (are) fully initialized by
// the start-up routines. Note: the UART handle MUST have been initialized!
#ifndef UART_INITED_BY_CUBE_MX
#define UART_INITED_BY_CUBE_MX false
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

// Explicit template instantiation.
template class os::posix::tty_implementable<os::driver::stm32f7::uart_impl>;

namespace os
{
  namespace driver
  {
    namespace stm32f7
    {
      uart_impl::uart_impl (UART_HandleTypeDef* huart, uint8_t* tx_buff,
                            uint8_t* rx_buff, size_t tx_buff_size,
                            size_t rx_buff_size) :
          uart_impl
            { huart, tx_buff, rx_buff, tx_buff_size, rx_buff_size, 0 } //
      {
        ;
      }

      uart_impl::uart_impl (UART_HandleTypeDef* huart, uint8_t* tx_buff,
                            uint8_t* rx_buff, size_t tx_buff_size,
                            size_t rx_buff_size, uint32_t rs485_params) : //
          huart_
            { huart }, //
          tx_buff_
            { tx_buff }, //
          rx_buff_
            { rx_buff }, //
          tx_buff_size_
            { tx_buff_size }, //
          rx_buff_size_
            { rx_buff_size }, //
          rs485_params_
            { rs485_params }
      {
        trace::printf ("%s() %p\n", __func__, this);

#if UART_INITED_BY_CUBE_MX == true
        // de-initialize the UART, as we assume it was automatically initialized
        // by the CubeMX generated code in the CubeMX's main () function.
        if (huart->Instance != nullptr)
          {
            HAL_UART_DeInit (huart);
          }
#endif

        // if not using DMA, the rx_buff_size must be even
        rx_buff_size_ % 2 ? rx_buff_size_-- : rx_buff_size_;
      }

      uart_impl::~uart_impl ()
      {
        trace::printf ("%s() %p\n", __func__, this);

        huart_ = nullptr;
        is_opened_ = false;
      }

      int
      uart_impl::do_vopen (const char* path, int oflag, std::va_list args)
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

            if (huart_->Instance == nullptr)
              {
                errno = EIO;      // no UART defined
                break;
              }

            // initialize the UART
            if (rs485_params_ & RS485_MASK)
              {
                if ((hal_result = HAL_RS485Ex_Init (
                    huart_, //
                    rs485_params_ & RS485_DE_POLARITY_MASK ? //
                    UART_DE_POLARITY_HIGH :
                    UART_DE_POLARITY_LOW,
                    (rs485_params_ & RS485_DE_ASSERT_TIME_MASK)
                        >> RS485_DE_ASSERT_TIME_POS,
                    (rs485_params_ & RS485_DE_DEASSERT_TIME_MASK)
                        >> RS485_DE_DEASSERT_TIME_POS)) != HAL_OK)
                  {
                    break;
                  }
              }
            else
              {
                if ((hal_result = HAL_UART_Init (huart_)) != HAL_OK)
                  {
                    break;
                  }
              }

            // clear receiver idle flag, then enable interrupt on receiver idle
            __HAL_UART_CLEAR_IDLEFLAG(huart_);
            __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);

            // if no rx/tx static buffers supplied, create them dynamically
            if (tx_buff_ == nullptr)
              {
                if ((tx_buff_ = new uint8_t[tx_buff_size_]) == nullptr)
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
                if ((rx_buff_ = new uint8_t[rx_buff_size_]) == nullptr)
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

            // set initial timeout depending on the O_NONBLOCK flag
            if (oflag & O_NONBLOCK)
              {
                rx_timeout_ = 0x0;
                o_nonblock_ = true;
              }
            else
              {
                rx_timeout_ = 0xFFFFFFFF;
                o_nonblock_ = false;
              }

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

            // clean-up dynamic allocations, if any
            if (tx_buff_dyn_ == true)
              {
                delete[] tx_buff_;
                tx_buff_ = nullptr;
              }

            if (rx_buff_dyn_ == true)
              {
                delete[] rx_buff_;
                rx_buff_ = nullptr;
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
      uart_impl::do_close (void)
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
            delete[] tx_buff_;
            tx_buff_ = nullptr;
          }

        if (rx_buff_dyn_ == true)
          {
            delete[] rx_buff_;
            rx_buff_ = nullptr;
          }

        is_opened_ = false;

        return 0;
      }

      ssize_t
      uart_impl::do_read (void* buf, std::size_t nbyte)
      {
        uint8_t* lbuf = (uint8_t *) buf;
        ssize_t count = 0;

        rtos::clock::duration_t timeout =
            o_nonblock_ ? 0 : (cc_vmin_ > 0) ? 0xFFFFFFFF : rx_timeout_;

        uint32_t last_count =
            huart_->hdmarx == nullptr ?
                huart_->RxXferCount : huart_->hdmarx->Instance->NDTR;

        // compute mask for possible parity bit masking
        UART_MASK_COMPUTATION(huart_);

        do
          {
            while (rx_out_ == rx_in_)
              {
                if (is_error_ == true)
                  {
                    is_error_ = false;
                    errno = EIO;
                    return -1;  // an error was reported, exit
                  }

                if (rx_sem_.timed_wait (timeout) != rtos::result::ok)
                  {
                    if (last_count
                        == (huart_->hdmarx == nullptr ?
                            huart_->RxXferCount : huart_->hdmarx->Instance->NDTR))
                      {
                        // no more chars received: that means inter-char timeout,
                        // return number of chars collected, if any
                        break;
                      }
                    last_count =
                        huart_->hdmarx == nullptr ?
                            huart_->RxXferCount :
                            huart_->hdmarx->Instance->NDTR;
                  }
              }

            // retrieve accumulated chars, if any
              {
                while (rx_out_ != rx_in_ && count < (ssize_t) nbyte)
                  {
                    rtos::interrupts::critical_section ics;  // critical section

                    // we mask potential parity bit as HAL doesn't do
                    // it on DMA transfers
                    *lbuf++ = rx_buff_[rx_out_++] & huart_->Mask;
                    if (++count == 1)
                      {
                        // VMIN > 0, apply timeout (can be infinitum too)
                        timeout = rx_timeout_;
                      }
                    if (rx_out_ >= rx_buff_size_)
                      {
                        rx_out_ = 0;
                      }
                  }
              }
            if (count >= (ssize_t) nbyte)
              {
                break;
              }
          }
        while (count < cc_vmin_);

        return count;
      }

      ssize_t
      uart_impl::do_write (const void* buf, std::size_t nbyte)
      {
        HAL_StatusTypeDef result;
        ssize_t count = 0;

        tx_sem_.wait ();
        memcpy (tx_buff_, buf, count = std::min (tx_buff_size_, nbyte));

        // enable the rs-485 driver to send
        do_rs485_de (true);

        // send the buffer, as much as we can
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
            count = -1;
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
      uart_impl::do_is_opened (void)
      {
        return is_opened_;
      }

      bool
      uart_impl::do_is_connected (void)
      {
        return true;      // TODO: check DCD, but where's the DCD line?
      }

      int
      uart_impl::do_tcgetattr (struct termios *ptio)
      {
        // clear the termios structure
        bzero ((void *) ptio, sizeof(struct termios));

        // termios.h: CSIZE: CS5, CS6, CS7, CS8; ST can CS7 and CS8 only
        // note: ST uses a normal bit for parity, must be subtracted from total
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
        ptio->c_ispeed = huart_->Init.BaudRate;
        ptio->c_ospeed = huart_->Init.BaudRate;

        // termios.h: CRTSCTS: we support only CTS/RTS flow control
        ptio->c_cflag |=
            huart_->Init.HwFlowCtl == UART_HWCONTROL_RTS_CTS ? CRTSCTS :
            huart_->Init.HwFlowCtl == UART_HWCONTROL_RTS ? CRTS_IFLOW :
            huart_->Init.HwFlowCtl == UART_HWCONTROL_CTS ? CCTS_OFLOW : 0;

        // termios.h: retrieve supported control characters (c_cc[])
        // we use the "spare 2" character for a fine grained delay (1 ms)
        ptio->c_cc[VMIN] = cc_vmin_;
        ptio->c_cc[VTIME] = cc_vtime_;
        ptio->c_cc[VTIME_MS] = cc_vtime_milli_;

        return 0;
      }

      int
      uart_impl::do_tcsetattr (int options, const struct termios *ptio)
      {
        bool reinit = false;

        // ST UARTs support only CS7 and CS8
        if ((ptio->c_cflag & CSIZE) < CS7 || options > TCIOFLUSH)
          {
            errno = EINVAL;
            return -1;
          }

        // set parity
        uint32_t temp32 =
            ptio->c_cflag & PARENB ?
                (ptio->c_cflag & PARODD ? UART_PARITY_ODD : UART_PARITY_EVEN) :
                UART_PARITY_NONE;
        if (temp32 != huart_->Init.Parity)
          {
            huart_->Init.Parity = temp32;
            reinit = true;
          }

        // set character size
        if (huart_->Init.Parity == UART_PARITY_NONE)
          {
            // ST UARTs can't do 6 and 5 bit characters, only 7, 8 and 9
            temp32 = (ptio->c_cflag & CSIZE) == CS8 ? //
                UART_WORDLENGTH_8B : UART_WORDLENGTH_7B;
          }
        else
          {
            temp32 = (ptio->c_cflag & CSIZE) == CS8 ? //
                UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
          }
        if (temp32 != huart_->Init.WordLength)
          {
            huart_->Init.WordLength = temp32;
            reinit = true;
          }

        // set number of stop bits
        temp32 = ptio->c_cflag & CSTOPB ? UART_STOPBITS_2 : UART_STOPBITS_1;
        if (temp32 != huart_->Init.StopBits)
          {
            huart_->Init.StopBits = temp32;
            reinit = true;
          }

        // set hardware flow control
        temp32 = (ptio->c_cflag & CRTSCTS) == CRTSCTS ? UART_HWCONTROL_RTS_CTS :
                 (ptio->c_cflag & CRTSCTS) == CRTS_IFLOW ? UART_HWCONTROL_RTS :
                 (ptio->c_cflag & CRTSCTS) == CCTS_OFLOW ? UART_HWCONTROL_CTS :
                 UART_HWCONTROL_NONE;
        if (temp32 != huart_->Init.HwFlowCtl)
          {
            huart_->Init.HwFlowCtl = temp32;
            reinit = true;
          }

        // set baud rate TODO: should we really close the port if baud rate is 0?
        temp32 = ptio->c_ispeed ? ptio->c_ispeed : ptio->c_ospeed;
        if (temp32 != huart_->Init.BaudRate)
          {
            huart_->Init.BaudRate = temp32;
            reinit = true;
          }

        cc_vmin_ = ptio->c_cc[VMIN];
        cc_vtime_ = ptio->c_cc[VTIME];
        // we expect in the "spare 2" character the fine grained delay (1 ms)
        cc_vtime_milli_ =
            (ptio->c_cc[VTIME_MS] > 99) ? 99 : ptio->c_cc[VTIME_MS];

        // compute rx timeout
        if (o_nonblock_)
          {
            rx_timeout_ = 0;
          }
        else
          {
            if (cc_vtime_ == 0 && cc_vtime_milli_ == 0)
              {
                rx_timeout_ = 0xFFFFFFFF;
              }
            else
              {
                // VTIME is expressed in 0.1 seconds
                rx_timeout_ = cc_vtime_ * 100 + cc_vtime_milli_;
              }
          }

        // evaluate the options
        switch (options)
          {
          case TCSAFLUSH:
            // flush input
            do_tcflush (TCIFLUSH);
            // no break, falls through

          case TCSADRAIN:
            // wait for output to be drained
            while (huart_->gState == HAL_UART_STATE_BUSY_TX)
              {
                tx_sem_.wait ();
              }
          }

        if (reinit)
          {
            // before sending the new configuration, stop the UART
            __HAL_UART_DISABLE(huart_);

            // send configuration and restart UART
            HAL_StatusTypeDef result = UART_SetConfig (huart_);
            if (result == HAL_OK)
              {
                if (huart_->hdmarx == nullptr)
                  {
                    // enable receive through UART interrupt transfers
                    result = HAL_UART_Receive_IT (huart_, rx_buff_,
                                                  rx_buff_size_ / 2);
                  }
                else
                  {
                    // enable receive through DMA transfers
                    result = HAL_UART_Receive_DMA (huart_, rx_buff_,
                                                   rx_buff_size_);
                  }
              }
            __HAL_UART_ENABLE(huart_);

            if (result != HAL_OK)
              {
                switch (result)
                  {
                  case HAL_BUSY:
                    errno = EBUSY;
                    break;

                  default:
                    errno = EINVAL;
                    break;
                  }
                return -1;
              }
          }

        return 0;
      }

      int
      uart_impl::do_tcflush (int queue_selector)
      {
        HAL_StatusTypeDef hal_result;
        int result = 0;

        if (queue_selector > TCIOFLUSH)
          {
            errno = EINVAL;
            result = -1;
          }
        else
          {
            if (huart_->hdmarx != nullptr || huart_->hdmatx != nullptr)
              {
                HAL_UART_DMAStop (huart_);
              }

            if (queue_selector & TCIFLUSH)
              {
                huart_->RxState = HAL_UART_STATE_READY;
                rx_sem_.reset ();
                rx_in_ = rx_out_ = 0;
              }

            if (queue_selector & TCOFLUSH)
              {
                tx_sem_.reset ();
                tx_in_ = tx_out_ = 0;
                do_rs485_de (false);
              }

            // restart receive
            if (huart_->hdmarx == nullptr)
              {
                hal_result = HAL_UART_Receive_IT (huart_, rx_buff_,
                                                  rx_buff_size_ / 2);
              }
            else
              {
                hal_result = HAL_UART_Receive_DMA (huart_, rx_buff_,
                                                   rx_buff_size_);
              }
            if (hal_result != HAL_OK)
              {
                errno = EIO;
                result = -1;
              }
          }
        return result;
      }

      int
      uart_impl::do_tcsendbreak (int duration)
      {
        __HAL_UART_SEND_REQ(huart_, UART_SENDBREAK_REQUEST);
        while (__HAL_UART_GET_FLAG(huart_, UART_FLAG_SBKF))
          ;
        return 0;
      }

      int
      uart_impl::do_vioctl (int request, std::va_list args)
      {
        return -1;
      }

      int
      uart_impl::do_tcdrain (void)
      {
        return -1;      // TODO: implement
      }

      void
      uart_impl::termination (bool new_state)
      {

      }

      void
      uart_impl::do_rs485_de (bool state)
      {
        // do nothing, as the rs485 driver is normally enabled by the hardware.
      }

      /**
       * @brief  Transmit event call-back.
       */
      void
      uart_impl::cb_tx_event (void)
      {
        tx_sem_.post ();

        // switch off the rs-485 driver enable signal
        do_rs485_de (false);
      }

      /**
       * @brief  Receive event call-back. Here are reported receive errors too.
       */
      void
      uart_impl::cb_rx_event (bool half)
      {
        size_t xfered;
        size_t half_buffer_size = rx_buff_size_ / 2;

        // handle errors (PE, FE, etc.), if any
        if (huart_->ErrorCode != HAL_UART_ERROR_NONE)
          {
            is_error_ = true;
          }

        // compute the number of chars received during the last transfer
        if (huart_->hdmarx == nullptr)
          {
            // non-DMA transfer
            xfered = rx_in_
                - (rx_in_ >= half_buffer_size ? half_buffer_size : 0);
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

        // re-initialize system for receive
        if (huart_->hdmarx == nullptr)
          {
            // for non-DMA transfer
            if (huart_->RxXferCount == 0 //
            || huart_->ErrorCode != HAL_UART_ERROR_NONE)
              {
                HAL_UART_Receive_IT (
                    huart_,
                    rx_in_ == 0 ? rx_buff_ : rx_buff_ + half_buffer_size,
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
            if ((half == false && rx_in_ == 0)
                || huart_->ErrorCode != HAL_UART_ERROR_NONE)
              {
                HAL_UART_Receive_DMA (huart_, rx_buff_, rx_buff_size_);
              }
          }

        rx_sem_.post ();
      }

    } /* namespace stm32f7 */
  } /* namespace driver */
} /* namespace os */

#pragma GCC diagnostic pop
