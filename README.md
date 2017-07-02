# stm32f7-uart
This is a USART driver for the STM32F7xx family of controllers.

## Version
* 0.4 (2 July 2017)

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
The driver can be used DMA based, or interrupt based. The selection is done automatically at run-time, depending on the presence of the `hdmarx` and `hdmatx` handles, which are set or not during the hardware initialization phase (e.g. by the CubeMX). Both systems have their merits, but in general one would use a DMA based transfer for baud rates over 115200. At slow speeds it does not make much sense to use DMA based transfers.

The driver's API is conceived in a way to be easily integrated in a POSIX environment. It is derived from the `device_char` class found in µOS++.

### Transmit
The driver expects a buffer and a count to be sent. In both DMA and interrupt modes, it initializes the UART and DMA (if in DMA mode) and waits for the transfer to be finshed. During this time, a semaphore blocks the caller to try to send more data, until the transfer going on is finished. An internal buffer is used for temporary storage until the data is sent.

### Receive
Using a DMA is a bit tricky for receiving, because generally you don't know how much data you are going to receive so that you can program the DMA's counter. The solution is to use the "interrupt on idle" property (most UARTs "know" this). What is an "idle character"? This is defined as the period of time equal to a character at the given baud rate, during which time the line is in spacing state (that is, the level of the stop bit).

If data comes in bursts, each idle character determines an interupt and the data is copied from the internal buffer to the caller's buffer. The ST DMAs have a neat feature called "half-complete" transfer, which comes in handy because it is used to "free" the internal buffer and transfer the incoming data to the caller's buffer, even while the burst of data is still ongoing.

A similar approach is used for the interrupt based receive, with a simulated "half-complete" transfer implemented in software by dividing the internal buffer in two equal parts.

Because the UART HAL libraries does not handle interrupt on idle, this must be done manually in case of CubeMX generated files:

``
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
``

## Tests
A separate directory `test` is included that opens a serial port, reads the main serial parameters, then writes a string and receives it 10 times in a loop, then closes the port. The open/write/read/close cycle is repeated 10 times before the progarm exits.

Obviously, in order to function, you must close the RxD and TxD of your UART in a loop.


