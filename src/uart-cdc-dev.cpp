/*
 * uart-cdc-dev.cpp
 *
 * Copyright (c) 2018 Lix N. Paulian (lix@paulian.net)
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

#include <cmsis-plus/rtos/os.h>
#include <cmsis-plus/diag/trace.h>

#include <uart-cdc-dev.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

namespace os
{
  namespace driver
  {
    namespace stm32f7
    {

      uart_cdc_dev::uart_cdc_dev (const char* name, uint8_t usb_id,
                                  uint8_t* tx_buff, uint8_t* rx_buff,
                                  size_t tx_buff_size, size_t rx_buff_size) : //
          tty
            { name }, //
          usb_id_
            { usb_id }, //
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
      }

      uart_cdc_dev::~uart_cdc_dev ()
      {
        trace::printf ("%s() %p\n", __func__, this);

        is_opened_ = false;
      }

      int
      uart_cdc_dev::do_vopen (const char* path, int oflag, std::va_list args)
      {
        int result = -1;

        do
          {
            if (is_opened_)
              {
                errno = EEXIST; // already opened
                break;
              }

            // initialize FIFO
            rx_in_ = rx_out_ = 0;

            // reset semaphores
            init_sem_.reset ();
            rx_sem_.reset ();

            // initialize the USB peripheral
            if ((husbd_ = USB_DEVICE_Init (usb_id_)) == nullptr)
              {
                errno = EIO;
                break;
              }

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
                    if (tx_buff_dyn_ == true)
                      {
                        delete[] tx_buff_;
                        tx_buff_ = nullptr;
                      }
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

            // wait for the usb device initialization to complete
            init_sem_.wait ();

            if ((cdc_buff_ = new uint8_t[packet_size_]) == nullptr)
              {
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
                errno = ENOMEM;
                break;
              }

            is_opened_ = true;
            result = 0;

            // start receiving, basically wait for input characters
            USBD_CDC_SetRxBuffer (husbd_, cdc_buff_);
            USBD_CDC_ReceivePacket (husbd_);
          }
        while (false);

        return result;
      }

      int
      uart_cdc_dev::do_close (void)
      {
        // wait for potential ongoing write operation to finish
        if (husbd_ != nullptr)
          {
            USBD_CDC_HandleTypeDef* pcd =
                (USBD_CDC_HandleTypeDef*) husbd_->pClassData;
            int count = 100;      // 100 ms timeout
            while (pcd->TxState != 0 && count--)
              {
                // busy, wait
                rtos::sysclock.sleep_for (1);
              }
          }
        // shut down the USB peripheral
        USBD_DeInit (husbd_);

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

        delete[] cdc_buff_;

        is_opened_ = false;

        return 0;
      }

      ssize_t
      uart_cdc_dev::do_read (void* buf, std::size_t nbyte)
      {
        uint8_t* lbuf = (uint8_t *) buf;
        ssize_t count = 0;

        os::rtos::clock::duration_t timeout =
            o_nonblock_ ? 0 : (cc_vmin_ > 0) ? 0xFFFFFFFF : rx_timeout_;

        uint32_t last_count = rx_in_;

        do
          {
            while (rx_out_ == rx_in_)
              {
                if (rx_sem_.timed_wait (timeout) != os::rtos::result::ok)
                  {
                    if (last_count == rx_in_)
                      {
                        // no more chars received: that means inter-char timeout
                        // return number of chars collected, if any
                        break;
                      }
                    last_count = rx_in_;
                  }
                if (is_error_ == true)
                  {
                    is_error_ = false;
                    errno = EIO;
                    return -1;  // an error was reported, exit
                  }
              }

            // retrieve accumulated chars, if any
            while (rx_out_ != rx_in_ && count < (ssize_t) nbyte)
              {
                *lbuf++ = rx_buff_[rx_out_++];
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
            if (count >= (ssize_t) nbyte)
              {
                break;
              }
          }
        while (last_packet_ == false || count < cc_vmin_);

        last_packet_ = false;

        return count;
      }

      ssize_t
      uart_cdc_dev::do_write (const void* buf, std::size_t nbyte)
      {
        USBD_StatusTypeDef result;
        uint8_t* p = (uint8_t*) buf;
        ssize_t count = 0;
        size_t total = 0;

        USBD_CDC_HandleTypeDef* pcd =
            (USBD_CDC_HandleTypeDef*) husbd_->pClassData;

        do
          {
            if (is_error_ == true)
              {
                is_error_ = false;
                errno = EIO;
                return -1;  // an error was reported, exit
              }

            // wait for possible previous ongoing write operation to finish
            while (pcd->TxState != 0)
              {
                // busy, wait one tick
                os::rtos::sysclock.sleep_for (1);
              }

            memcpy (tx_buff_, p + total,
                    count = std::min (tx_buff_size_, nbyte - total));

            // send the buffer, as much as we can
            USBD_CDC_SetTxBuffer (husbd_, tx_buff_, count);
            result = (USBD_StatusTypeDef) USBD_CDC_TransmitPacket (husbd_);

            if (result != USBD_OK)
              {
                count = -1;
                switch (result)
                  {
                  case USBD_BUSY:
                    errno = EBUSY;
                    break;

                  default:
                    errno = EIO;
                    break;
                  }
                break;
              }

            total += count;
          }
        while (total < nbyte);

        if (total > 0 && nbyte % packet_size_ == 0)
          {
            // send a zero length packet
            while (pcd->TxState != 0)
              {
                // busy, wait one tick
                os::rtos::sysclock.sleep_for (1);
              }
            USBD_CDC_SetTxBuffer (husbd_, tx_buff_, 0);
            USBD_CDC_TransmitPacket (husbd_);
          }

        return total;
      }

      bool
      uart_cdc_dev::do_is_opened (void)
      {
        return is_opened_;
      }

      bool
      uart_cdc_dev::do_is_connected (void)
      {
        return is_connected_;
      }

      int
      uart_cdc_dev::do_tcgetattr (struct termios *ptio)
      {
        // clear the termios structure
        bzero ((void *) ptio, sizeof(struct termios));

        // TODO: get the following parameters from the USB driver (CDC control)
        // termios.h: CSIZE: CS5, CS6, CS7, CS8; ST can CS7 and CS8 only
        ptio->c_cflag = CS8;

        // termios.h: CSTOPB: if true, two stop bits, otherwise only one
        ptio->c_cflag |= 0;

        // termios.h: flags PARENB = parity enabled, PARODD = parity odd
        ptio->c_cflag |= 0;

        // get baud rate
        ptio->c_ispeed = 115200L;
        ptio->c_ospeed = 115200L;

        // termios.h: CRTSCTS: no flow control
        ptio->c_cflag |= 0;

        // termios.h: retrieve supported control characters (c_cc[])
        // we use the "spare 2" character for a fine grained delay (1 ms)
        ptio->c_cc[VMIN] = cc_vmin_;
        ptio->c_cc[VTIME] = cc_vtime_;
        ptio->c_cc[VTIME_MS] = cc_vtime_milli_;

        return 0;
      }

      int
      uart_cdc_dev::do_tcsetattr (int options, const struct termios *ptio)
      {
        USBD_StatusTypeDef result = USBD_OK;

        // TODO: set c_cflag accordingly through the USB driver (CDC control)
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
            if (husbd_ != nullptr)
              {
                USBD_CDC_HandleTypeDef* pcd =
                    (USBD_CDC_HandleTypeDef*) husbd_->pClassData;
                while (pcd->TxState != 0)
                  {
                    // busy, wait one tick
                    rtos::sysclock.sleep_for (1);
                  }
              }
          }

        // TODO: send the new configuration

        if (result != USBD_OK)
          {
            switch (result)
              {
              case USBD_BUSY:
                errno = EBUSY;
                break;

              default:
                errno = EINVAL;
                break;
              }
            return -1;
          }

        return 0;
      }

      int
      uart_cdc_dev::do_tcflush (int queue_selector)
      {
        USBD_StatusTypeDef hal_result = USBD_OK;
        int result = 0;

        if (queue_selector > TCIOFLUSH)
          {
            errno = EINVAL;
            result = -1;
          }
        else
          {
            if (queue_selector & TCIFLUSH)
              {
                rx_sem_.reset ();
                rx_in_ = rx_out_ = 0;
                last_packet_ = false;
              }

            if (queue_selector & TCOFLUSH)
              {
                ; // nothing to do
              }

            if (hal_result != USBD_OK)
              {
                errno = EIO;
                result = -1;
              }
          }
        return result;
      }

      int
      uart_cdc_dev::do_tcsendbreak (int duration)
      {
        return 0;
      }

// --------------------------------------------------------------------

// The following call-backs are executed on an interrupt context

      int8_t
      uart_cdc_dev::cb_init_event (void)
      {
        // get packet size; at this point the host/device negotiation is done
        packet_size_ = (husbd_->dev_speed == USBD_SPEED_HIGH) ? //
            USB_HS_MAX_PACKET_SIZE :
            USB_FS_MAX_PACKET_SIZE;
        is_connected_ = true;

        init_sem_.post ();

        return USBD_OK;
      }

      int8_t
      uart_cdc_dev::cb_deinit_event (void)
      {
        // USB disconnected
        is_error_ = true;
        is_connected_ = false;
        rx_sem_.post ();

        return USBD_OK;
      }

      int8_t
      uart_cdc_dev::cb_control_event (uint8_t cmd, uint8_t* pbuf, uint16_t len)
      {
        // TODO: implement control commands
        return USBD_OK;
      }

      /**
       * @brief  Receive event call-back.
       */
      int8_t
      uart_cdc_dev::cb_receive_event (uint8_t* pbuf, uint32_t* len)
      {
        size_t xfered = *len;

        while (xfered--)
          {
            rx_buff_[rx_in_++] = *pbuf++;
            if (rx_in_ >= rx_buff_size_)
              {
                rx_in_ = 0;
              }
          }

        // restart receive
        USBD_CDC_SetRxBuffer (husbd_, cdc_buff_);
        USBD_CDC_ReceivePacket (husbd_);

        // last packet?
        xfered = *len;
        if (xfered == 0 || xfered % packet_size_ > 0)
          {
            last_packet_ = true; // yes
          }

        // inform background we have something
        rx_sem_.post ();

        return USBD_OK;
      }
    } /* namespace stm32f7 */
  } /* namespace driver */
} /* namespace os */

#pragma GCC diagnostic pop

