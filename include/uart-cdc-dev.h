/*
 * uart-cdc-dev.h
 *
 * Copyright (c) 2018-2020 Lix N. Paulian (lix@paulian.net)
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
 * Created on: 13 Jan 2018 (LNP)
 */

#ifndef INCLUDE_UART_CDC_DEV_H_
#define INCLUDE_UART_CDC_DEV_H_

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/posix-io/tty.h>
#include <cmsis-plus/posix/termios.h>
#include <fcntl.h>

#include "cmsis_device.h"
#include "usbd_cdc_if.h"

#if defined (__cplusplus)

namespace os
{
  namespace driver
  {
    namespace stm32f7
    {
      class uart_cdc_dev : public os::posix::tty_impl
      {
      public:

        uart_cdc_dev (uint8_t usb_id, uint8_t* tx_buff, uint8_t* rx_buff,
                      size_t tx_buff_size, size_t rx_buff_size);

        uart_cdc_dev (const uart_cdc_dev&) = delete;

        uart_cdc_dev (uart_cdc_dev&&) = delete;

        uart_cdc_dev&
        operator= (const uart_cdc_dev&) = delete;

        uart_cdc_dev&
        operator= (uart_cdc_dev&&) = delete;

        virtual
        ~uart_cdc_dev () noexcept;

        void
        config (uint8_t usb_id, uint8_t* tx_buff, uint8_t* rx_buff,
                size_t tx_buff_size, size_t rx_buff_size);

        // driver specific, not inherited functions

        int8_t
        cb_init_event (void);

        int8_t
        cb_deinit_event (void);

        int8_t
        cb_control_event (uint8_t cmd, uint8_t* pbuf, uint16_t len);

        int8_t
        cb_receive_event (uint8_t* pbuf, uint32_t* len);

// --------------------------------------------------------------------

      protected:

        virtual int
        do_tcsendbreak (int duration) override;

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
        do_tcgetattr (struct termios* ptio) override;

        virtual int
        do_tcsetattr (int options, const struct termios* ptio) override;

        virtual int
        do_tcflush (int queue_selector) override;

        virtual int
        do_vioctl (int request, std::va_list args) override;

        virtual int
        do_tcdrain (void) override;

        static constexpr os::rtos::clock::duration_t open_timeout = 5000;

        uint8_t usb_id_;
        uint8_t* cdc_buff_;
        int packet_size_;
        bool volatile last_packet_ = false;
        USBD_HandleTypeDef* husbd_;

        uint8_t* tx_buff_;
        uint8_t* rx_buff_;
        size_t tx_buff_size_;
        size_t rx_buff_size_;
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

        os::rtos::semaphore_binary init_sem_
          { "init", 0 };
        os::rtos::semaphore_binary rx_sem_
          { "rx", 0 };

      };

    } /* namespace stm32f7 */
  } /* namespace driver */
} /* namespace os */

#endif /* __cplusplus */

#endif /* INCLUDE_UART_CDC_DEV_H_ */
