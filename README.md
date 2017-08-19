# stm32f7-uart
This is a µOS++ UART driver for the STM32F7xx family of controllers.

The driver is functional, but several features are still missing (and the list is probably incomplete):
* Further implementation of serial port control through `struct termios` (although the most useful flags have been implemented)
* DCD signal handling (and perhaps modem signals handling too?)
* The `fcntl` call

The POSIX approach to configure a serial port is through the `struct termios` and its related API. Unfortunately, the standard newlib for embeded development does not include it. The good news is that Liviu plans to support `termios` and friends in an upcoming version of the µOS++. Until then the `termios.h` and `fcntl.h` header files have been included with the driver. The lack of newlib support means that you cannot call `tcgetattr` and `tcsetattr` library functions. However, you can still access these functions directly from the driver (see the test example).

The STM32Fxx family UARTs support only 7 and 8 bits characters, with or without parity. On the other hand, `termios` defines 5, 6, 7 and 8 bit characters, i.e. as CS5, CS6, CS7 and CS8, thus the driver supports only CS7 and CS8 modes.

The current implementation supports CTS/RTS hardware handshaking and can be enabled/disabled over the `termios` structure. This functionality is implemented by the HAL and the STM32F7xx hardware.

The `termios` VMIN and VTIM control characters are properly interpreted; in addition, because in embedded applications much shorter delays than 0.1 seconds are often required, we use a second control caracter (mapped onto "spare 2") to reach a finer grain timeout for VTIM. This control character can be refered as `c_cc[VTIM_MS]`, or `c_cc[VTIM + 2]` and can take values from 0 to 99 ms. The final timeout will be computed as `c_cc[VTIM] * 100 + c_cc[VTIM_MS]`. 

The driver supports RS-485 half-duplex operation. There are two aspects to consider:

* POSIX does not support explicitely RS-485 mode, therefore as an initial solution a new flag has been defined (O_RS485) that must be used when opening a port in RS-485 mode as shown below:

```c
	#define DEAT 10
	#define DEDT 12
	uint32_t mode = RS485_POLARITY | (DEDT << 8) | DEAT;
	
	if ((fd = open ("/dev/uart6", O_RS485, mode)) < 0)
	 {
	 	// handle error
	 } 
```

* The STM32F7xx hardware has its built-in method of handling of the DE pin (driver enable); the pin is mapped onto the RTS pin. The initialization of the DE pin must be done externally, and if you use CubeMX this will be done automatically for you if the correct UART options are selected (e.g. RS-485 mode).

In the example above, the `mode` parameter is a composite of the following variables:

* Driver Enable Assertion Time: the least significant 8 bits (bits 0-7)
* Driver Enable Deassertion Time: the next 8 bits (bits 8-15)
* Driver Enable Polarity: the most significant bit (bit 31)

The first two values are expressed in number of sample time units (1/8 or 1/16 bit time, depending on the oversampling rate); they can be between 0 and 31. The Driver Enable Polarity will be 1 if the RS485_POLARITY is added to the `mode` argument. For more details consult the STM32F7xx family Reference Manual.

If the DE pin used is not the one defined by the STM32F7xx hardware, you can derive your own uart class and replace the function `void uart::do_rs485_de (bool state)`. The same applies for sending breaks: you may want to replace the function `int uart::do_tcsendbreak (int duration)` with your own. The hardware generated break by the STM32F7xx family of controllers is only one character long (consult the controller's Reference Manual), and for some applications it might be too short. Moreover, in the built-in function, the parameter `duration` of the `tcsendbreak ()` function is simply ignored, whereas a custom implementation may/should use it. Such a custom function would probably reconfigure the UART's TxD pin as output port, then switch it low, wait for the specified amount of time in a uOS++ delay function, switch the port high and finally reconfigure the pin as TxD.

## Version
* 1.10 (20 August 2017)

## License
* MIT

## Package
The driver is provided as an XPACK and can be installed in an Eclipse based project using the attached script (however, the include and source paths must be manually added to the project in Eclipse). For more details on XPACKs see https://github.com/xpacks. The installation script requires the helper scripts that can be found at https://github.com/xpacks/scripts.

## Dependencies
The driver depends on the following software packages:
* STM32F7 CMSIS (https://github.com/xpacks/stm32f7-cmsis)
* STM32F7xx HAL Library (https://github.com/xpacks/stm32f7-hal)
* uOS++ (https://github.com/micro-os-plus/micro-os-plus-iii)

Note that the hardware initialisations (uController clock, peripherals clocks, etc.) must be separately performed, normaly in, or called from the `initialize_hardware.c` file of a gnuarmeclipse project. Alternatively you can do this using the CubeMX generator from ST. You may find helpful to check the following projects as references:
* https://github.com/micro-os-plus/eclipse-demo-projects/tree/master/f746gdiscovery-blinky-micro-os-plus
* https://github.com/micro-os-plus/eclipse-demo-projects/tree/master/f746gdiscovery-blinky-micro-os-plus/cube-mx which details how to integrate the CubeMX generated code into a uOS++ based project.

The driver was designed for the µOS++ ecosystem.

## Short theory of operation
The driver can perform data transfers DMA based, or interrupt based. The selection is done automatically at run-time, depending on the presence of the `hdmarx` and `hdmatx` handles, which are set or not during the hardware initialization phase (e.g. by the CubeMX). Both systems have their merits and pitfalls, but in general one would use a DMA based transfer for baud rates over 115200. At slow speeds (e.g. 19200 bps) DMA transfers do not make much sense.

The driver's API is conceived in a way to be easily integrated in a POSIX environment. It is derived from the `device_char` class found in µOS++.

### Transmit
The driver expects a buffer and a count of bytes to be sent. In both DMA and interrupt modes, it initializes the UART and DMA (if in DMA mode) and waits for the transfer to be finshed. During this time, a semaphore blocks the caller to try to send more data, until the ongoing transfer is finished. An internal buffer is used for temporary storage until the data is sent.

### Receive
Using DMA to receive is a bit tricky, because generally you don't know how much data you are going to get so that you know how to program the DMA's counter. The solution is to use the "interrupt on idle" property (most UARTs "know" this). What is an "idle character"? This is defined as the period of time equal to a character at the given baud rate, during which time the line is in spacing state (that is, at the stop bit's level).

If data comes in bursts, each idle character determines an interupt and the data is copied from the internal buffer to the caller's buffer. The ST DMAs have a neat feature called "half-complete" transfer, which comes in handy. Thus we can "free" the internal buffer and transfer the incoming data to the caller's buffer, even while the burst of incoming data is still ongoing.

A similar approach is used for the interrupt based receive, with a simulated "half-complete" transfer implemented in software by dividing the internal buffer in two equal parts.

Because the UART HAL library does not handle interrupt on idle, this must be added manually if you generate your files with CubeMX, as shown below (in the generated file `stm32f7xx_it.c`):

```c
/**
* @brief This function handles USART6 global interrupt.
*/
void USART6_IRQHandler(void)
{
  /* USER CODE BEGIN USART6_IRQn 0 */

  /* USER CODE END USART6_IRQn 0 */
  HAL_UART_IRQHandler(&huart6);
  /* USER CODE BEGIN USART6_IRQn 1 */
  if (__HAL_UART_GET_FLAG (&huart6, UART_FLAG_IDLE))
    {
      __HAL_UART_CLEAR_IDLEFLAG (&huart6);
      HAL_UART_RxCpltCallback (&huart6);
    }
  /* USER CODE END USART6_IRQn 1 */
}
```

### Buffers selection
Both receive and transmit sections need decent buffers to properly operate. The buffer's size depends on your application. You can either provide two static buffers, or null pointers and in this case the driver will dynamically allocate the buffers.

In general, for slow typed input and output, buffers of several tens of bytes are sufficient; it is important to test the result of the `read()` and `write()` functions to be sure all characters have been sent, or if you received the expected frame in its entirety.

However, if you implement a serial protocol, then the buffers should be sized according to the typical frame length of the protocol. Small buffers will still do, but the efficiency will decrease and at high speeds the driver might even lose characters.

## Tests
A separate directory `test` is included that contains a short test program: it opens a serial port, reads the current parameters, writes a string and receives it 10 times in a loop, then closes the port. The open/write/read/close cycle is repeated 10 times before the program exits.

Obviously, in order to function, you must short the RxD and TxD signals of your UART.
