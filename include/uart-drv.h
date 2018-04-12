/*
 * uart-drv.h
 *
 * Copyright (c) 2017, 2018 Lix N. Paulian (lix@paulian.net)
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

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/posix-io/tty.h>
#include <cmsis-plus/posix/termios.h>
#include <fcntl.h>

#if defined (__cplusplus)

namespace os
{
  namespace driver
  {
    namespace stm32f7
    {
      class uart_impl;
      using uart = posix::tty_implementable<uart_impl>;

      class uart_impl : public posix::tty_impl
      {
      public:

        // --------------------------------------------------------------------

        // description of the rs485_flags:
        //
        // b0: if true, RS-485/RS-422 mode, otherwise RS-232
        // b1: if true, half_duplex mode (i.e. RS-485), otherwise RS-422
        // b2: if true, Data Enable polarity pin is high
        // b3 - b7: reserved
        // b8 - b15: Data Enable pin Assertion Time (in UART sample intervals)
        // b16 - b23: Data Enable pin Deassertion Time (in UART sample intervals)

        static constexpr uint32_t RS485_HALF_DUPLEX_POS = 1;
        static constexpr uint32_t RS485_DE_POLARITY_POS = 2;
        static constexpr uint32_t RS485_DE_ASSERT_TIME_POS = 7;
        static constexpr uint32_t RS485_DE_DEASSERT_TIME_POS = 15;

        static constexpr uint32_t RS485_MASK = (1 << 0);
        static constexpr uint32_t RS485_HALF_DUPLEX_MASK = (1
            << RS485_HALF_DUPLEX_POS);
        static constexpr uint32_t RS485_DE_POLARITY_MASK = (1
            << RS485_DE_POLARITY_POS);
        static constexpr uint32_t RS485_DE_ASSERT_TIME_MASK = (0x1F
            << RS485_DE_ASSERT_TIME_POS);
        static constexpr uint32_t RS485_DE_DEASSERT_TIME_MASK = (0x1F
            << RS485_DE_DEASSERT_TIME_POS);

        uart_impl (UART_HandleTypeDef* huart,
                   uint8_t* tx_buff, uint8_t* rx_buff, size_t tx_buff_size,
                   size_t rx_buff_size);

        uart_impl (UART_HandleTypeDef* huart,
                   uint8_t* tx_buff, uint8_t* rx_buff, size_t tx_buff_size,
                   size_t rx_buff_size, uint32_t rs485_params);

        uart_impl (const uart_impl&) = delete;

        uart_impl (uart_impl&&) = delete;

        uart_impl&
        operator= (const uart_impl&) = delete;

        uart_impl&
        operator= (uart_impl&&) = delete;

        virtual
        ~uart_impl () noexcept;

        void
        get_version (uint8_t& version_major, uint8_t& version_minor);

        // driver specific, not inherited functions
        void
        cb_tx_event (void);

        void
        cb_rx_event (bool half);

        // --------------------------------------------------------------------

      protected:

        virtual int
        do_tcsendbreak (int duration) override;

        virtual void
        do_rs485_de (bool state);

        // --------------------------------------------------------------------

      private:

        virtual int
        do_vopen (const char* path, int oflag, std::va_list args) override;

        virtual int
        do_close (void) override;

        virtual ssize_t
        do_read (void* buf, std::size_t nbyte) override;

        virtual ssize_t
        do_write (const void* buf, std::size_t nbyte) override;

        virtual bool
        do_is_opened (void) override;

        virtual bool
        do_is_connected (void) override;

        virtual int
        do_tcgetattr (struct termios *ptio) override;

        virtual int
        do_tcsetattr (int options, const struct termios *ptio) override;

        virtual int
        do_tcflush (int queue_selector) override;

        virtual int
        do_vioctl (int request, std::va_list args) override;

        virtual int
        do_tcdrain (void) override;

        static constexpr uint8_t UART_DRV_VERSION_MAJOR = 2;
        static constexpr uint8_t UART_DRV_VERSION_MINOR = 10;

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

        rtos::semaphore_binary tx_sem_
          { "tx", 1 };
        rtos::semaphore_binary rx_sem_
          { "rx", 0 };

      protected:

        uint32_t rs485_params_;

      };

      /**
       * @brief  Return the version of the driver.
       * @param  version_major: major version.
       * @param  version_minor: minor version.
       */
      inline void
      uart_impl::get_version (uint8_t& version_major, uint8_t& version_minor)
      {
        version_major = UART_DRV_VERSION_MAJOR;
        version_minor = UART_DRV_VERSION_MINOR;
      }

    } /* namespace stm32f7 */
  } /* namespace driver */
} /* namespace os */

#endif /* __cplusplus */

#endif /* INCLUDE_UART_DRV_H_ */
