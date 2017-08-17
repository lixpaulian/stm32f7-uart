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

#include <termios.h>
#include <fcntl.h>
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
      tcgetattr (struct termios *ptio);

      int
      tcsetattr (int options, const struct termios *ptio);

      int
      tcsendbreak (int duration);

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

      int
      do_tcgetattr (struct termios *ptio);

      int
      do_tcsetattr (int options, const struct termios *ptio);

      virtual int
      do_tcsendbreak (int duration);

      virtual void
      do_rs485_de (bool state);

      // --------------------------------------------------------------------

    private:

      static constexpr uint8_t UART_DRV_VERSION_MAJOR = 1;
      static constexpr uint8_t UART_DRV_VERSION_MINOR = 0;

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
      bool volatile is_error_ = false;

      bool volatile o_nonblock_ = false;

      uint8_t volatile cc_vmin_ = 1; // at least one character should be received
      uint8_t volatile cc_vtime_ = 0; // timeout indefinitely
      uint8_t volatile cc_vtime_milli_ = 0; // extension to VTIME: timeout in ms

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

    inline int
    uart::tcsendbreak (int duration)
    {
      return do_tcsendbreak (duration);
    }

    inline int
    uart::tcgetattr (struct termios *ptio)
    {
      return do_tcgetattr (ptio);
    }

    inline int
    uart::tcsetattr (int options, const struct termios *ptio)
    {
      return do_tcsetattr (options, ptio);
    }

    inline int
    uart::do_tcsendbreak (int duration __attribute__ ((unused)))
    {
      __HAL_UART_SEND_REQ(huart_, UART_SENDBREAK_REQUEST);
      while (__HAL_UART_GET_FLAG(huart_, UART_FLAG_SBKF))
        ;
      return 0;
    }

    inline void
    uart::do_rs485_de (bool state __attribute__ ((unused)))
    {
      // do nothing, as the rs485 driver is enabled by the hardware.
    }

  } /* namespace driver */
} /* namespace os */

#endif /* __cplusplus */

#endif /* INCLUDE_UART_DRV_H_ */
