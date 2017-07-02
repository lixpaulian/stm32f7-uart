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
#include <termios.h>

#if defined (__cplusplus)

namespace os
{
  namespace driver
  {

    class uart : public os::posix::device_char
    {
    public:

      // --------------------------------------------------------------------

      uart (const char* name, UART_HandleTypeDef* huart, uint8_t* tx_buff,
            uint8_t* rx_buff, size_t tx_buff_size, size_t rx_buff_size);

      uart (const uart&) = delete;

      uart (uart&&) = delete;

      uart&
      operator= (const uart&) = delete;

      uart&
      operator= (uart&&) = delete;

      virtual
      ~uart () noexcept;

      // driver specific, not inherited functions
      void
      cb_tx_event (void);

      void
      cb_rx_event (bool half);

      void
      get_version (uint8_t& version_major, uint8_t& version_minor);

      int
      do_tcgetattr (struct termios *ptio);

      int
      do_tcsetattr (int options, const struct termios *ptio);

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

      static constexpr uint8_t UART_DRV_VERSION_MAJOR = 0;
      static constexpr uint8_t UART_DRV_VERSION_MINOR = 4;

      UART_HandleTypeDef* huart_;
      uint8_t* tx_buff_;
      uint8_t* rx_buff_;
      size_t tx_buff_size_;
      size_t rx_buff_size_;
      size_t volatile tx_in_;
      size_t volatile tx_out_;
      size_t volatile rx_in_;
      size_t volatile rx_out_;
      bool tx_buff_dyn_;
      bool rx_buff_dyn_;

      rtos::clock_systick::duration_t rx_timeout_;

      bool volatile is_connected_ = false;
      bool volatile is_opened_ = false;

      os::rtos::semaphore_binary tx_sem_
        { "tx", 1 };
      os::rtos::semaphore_binary rx_sem_
        { "rx", 0 };
    };

    /**
     * @brief  Return the version of the driver.
     * @param  version_major: major version.
     * @param  version_minor: minor version.
     */
    inline void
    uart::get_version (uint8_t& version_major, uint8_t& version_minor)
    {
      version_major = UART_DRV_VERSION_MAJOR;
      version_minor = UART_DRV_VERSION_MINOR;
    }

  } /* namespace driver */
} /* namespace os */

#endif /* __cplusplus */

#endif /* INCLUDE_UART_DRV_H_ */
