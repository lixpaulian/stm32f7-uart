/*
 * test-cdc-dev.h
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
 * Created on: 15 Jan 2018 (LNP)
 */

#ifndef TEST_TEST_CDC_DEV_H_
#define TEST_TEST_CDC_DEV_H_

#include <uart-cdc-dev.h>

#if defined (__cplusplus)

extern "C"
{
  int8_t
  cdc_init (USBD_HandleTypeDef* husbd);

  int8_t
  cdc_deinit (USBD_HandleTypeDef* husbd);

  int8_t
  cdc_control (USBD_HandleTypeDef* husbd, uint8_t cmd, uint8_t* pbuf, uint16_t length);

  int8_t
  cdc_receive (USBD_HandleTypeDef* husbd, uint8_t* buf, uint32_t *len);
}

void
test_uart_cdc (void);

#endif /* __cplusplus */

#endif /* TEST_TEST_CDC_DEV_H_ */
