/*
 * uart-drv.h
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

#ifndef INCLUDE_UART_DRV_H_
#define INCLUDE_UART_DRV_H_

#include "cmsis_device.h"
#include "stm32f7xx_hal_usart.h"

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/posix-io/device-char.h>

#if defined (__cplusplus)

namespace os
{
  namespace driver
  {

    class uart : public os::posix::device_char
    {
    public:

      // --------------------------------------------------------------------

      uart (const char* name, UART_HandleTypeDef* huart, void* tx_buff,
            void* rx_buff, size_t tx_buff_size, size_t rx_buff_size);

      uart (const uart&) = delete;

      uart (uart&&) = delete;

      uart&
      operator= (const uart&) = delete;

      uart&
      operator= (uart&&) = delete;

      virtual
      ~uart () noexcept;

      // specific, not inherited functions
      void
      cb_tx_event (UART_HandleTypeDef* huart);

      void
      cb_rx_event (UART_HandleTypeDef* huart);

      // --------------------------------------------------------------------

    protected:

      int
      do_vopen (const char* path, int oflag, std::va_list args) override;

      int
      do_close (void) override;

      ssize_t
      do_read (void* buf, std::size_t nbyte) override;

      ssize_t
      do_write (const void* buf, std::size_t nbyte) override;

      bool
      do_is_opened (void) override;

      bool
      do_is_connected (void) override;

      // --------------------------------------------------------------------

    private:

      UART_HandleTypeDef* huart_;
      void* tx_buff_;
      void* rx_buff_;
      size_t tx_buff_size_;
      size_t rx_buff_size_;

      size_t tx_in_;
      size_t tx_out_;
      size_t rx_in_;
      size_t rx_out_;

      bool volatile is_connected_ = false;
      bool volatile is_opened_ = false;
      uint32_t error_code_ = HAL_UART_ERROR_NONE;

      os::rtos::semaphore_binary tx_sem_
        { "tx", 1 };
      os::rtos::semaphore_binary rx_sem_
        { "rx", 0 };

    };

  } /* namespace driver */
} /* namespace os */

#endif /* __cplusplus */

#endif /* INCLUDE_UART_DRV_H_ */
