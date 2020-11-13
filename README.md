# stm32f7-uart
µOS++ UART drivers for the STM32F7xx family of controllers.

This package includes "glue" code between µOS++ and the ST provided HAL and Middleware libraries. One of the goals of this project was to use code generated by the CubeMX code generator with a minimum of customization. Currently the package includes an UART driver and an USB CDC Device driver (Virtual Com Port).

The drivers are fully functional, but several features are still missing (and the list is probably incomplete):
* Further implementation of serial port control through `struct termios` (although the most useful flags have been implemented)
* DCD signal handling (and perhaps modem signals handling too)
* The `fcntl` call
* The USB DCD driver lacks control commands (Baudrate, Stop Bits, CTS/DTR control, etc.); this is however useful only in applications where a Virtual Com Port connects to an UART.

The driver's API is conceived in a way to be easily integrated in a POSIX environment. Since version 2.0 of the UART driver and 0.7 of the UART CDC driver, the API has been changed for a better integration with the POSIX layer of uOS++. Both drivers are implementations of uOS++ character devices.

The POSIX approach to configure a serial port is through the `struct termios` and its related API. Unfortunately, the standard newlib for embeded development does not include it. The good news is that starting with version 6.3.13 Liviu added initial support for `termios` and friends in µOS++ (see the test example).

The STM32Fxx family UARTs supports only 7 and 8 bits characters, with or without parity. On the other hand, `termios` defines 5, 6, 7 and 8 bit characters, i.e. as CS5, CS6, CS7 and CS8, thus the driver supports only CS7 and CS8 modes.

The current UART implementation supports CTS/RTS hardware handshaking and can be enabled/disabled over the `termios` structure. This functionality is implemented by the HAL and the STM32F7xx hardware. The VCP implementation does not yet support hardware CTS/RTS handshake, however, between two CDC devices this is not necessary as it is handled by the USB lower layers.

The `termios` VMIN and VTIM control characters are properly interpreted; in addition, because in embedded applications much shorter delays than 0.1 seconds are often required, we use a second control caracter (mapped onto "spare 2") to reach a finer grain timeout for VTIM. This control character can be refered as `c_cc[VTIM_MS]`, or `c_cc[VTIM + 2]` and may take values from 0 to 99 ms. The final timeout (in ms) will be computed as `c_cc[VTIM] * 100 + c_cc[VTIM_MS]`.

The UART driver supports RS-485 half-duplex operation (the VCP does not). There are several aspects to consider:

POSIX does not support explicitely RS-485 mode, therefore RS-485 operation must be activated when instantiating the driver. For RS-485 mode, the `uart`constructor must be provided with an additional parameter: `uint32_t rs485_params` which is a composite of several flags:

* b0: if true, RS-485/RS-422 mode, otherwise RS-232
* b1: if true, half_duplex mode (i.e. RS-485), otherwise RS-422
* b2: if true, Data Enable polarity pin is high
* b3 - b7: reserved
* b8 - b15: Data Enable pin Assertion Time (in UART sample intervals)
* b16 - b23: Data Enable pin Deassertion Time (in UART sample intervals)

```c
#define TX_BUFFER_SIZE 200
#define RX_BUFFER_SIZE 200
#define DEAT 10
#define DEDT 12

uart uart1
  { "uart1", &huart1, nullptr, nullptr, TX_BUFFER_SIZE, RX_BUFFER_SIZE,
      RS485_MASK | RS485_DE_POLARITY_MASK |
      (DEAT << RS485_DE_ASSERT_TIME_POS) |
      (DEDT << RS485_DE_DEASSERT_TIME_POS) };
```

DEAT and DEDT are expressed in a number of sample time units (1/8 or 1/16 bit time, depending on the oversampling rate); they can be between 0 and 31. The Driver Enable Polarity will be 1 if the RS485_DE_POLARITY_MASK is added to the `rs485_params` argument. For more details consult the STM32F7xx family Reference Manual.

The STM32F7xx hardware has its built-in method of handling the DE pin (driver enable - this function is mapped onto the RTS pin). The initialization of the DE pin must be done externally, and if you use CubeMX this will be done automatically for you if the correct UART options are selected (e.g. RS-485 mode).

If the DE pin used is not the one defined by the STM32F7xx hardware, you can derive your own uart class and replace the function `void uart::do_rs485_de (bool state)`. The same applies for sending breaks: you may want to replace the function `int uart::do_tcsendbreak (int duration)` with your own. An example of such an approach can be seen in the SDI-12 Data Recorder library that makes use of this driver (https://github.com/lixpaulian/dacq).

The hardware generated break by the STM32F7xx family of controllers is only one character long (consult the controller's Reference Manual), and for some applications it might be too short. Moreover, in the built-in function, the parameter `duration` of the `tcsendbreak ()` function is simply ignored, whereas a custom implementation may/should use it. Such a custom function would probably reconfigure the UART's TxD pin as output port, then switch it low, wait for the specified amount of time in a uOS++ delay function, switch the port high and finally reconfigure the pin as TxD.

Note that the current VCP implementation does not support the `tcsendbreak()` call.

## Version
* 2.1.7 (10 November 2020)

## License
* MIT

## Package
The driver is provided as an XPACK and can be installed in an Eclipse based project using the attached script (however, the include and source paths must be manually added to the project in Eclipse). For more details on XPACKs see https://github.com/xpacks. The installation script requires the helper scripts that can be found at https://github.com/xpacks/scripts.

## Dependencies
The driver depends on the following software package:
* uOS++ (https://github.com/micro-os-plus/micro-os-plus-iii), version 6.3.14 and up.

In addition, you need the ST HAL, CMSIS and (if you use the USB DCD Device driver) the ST "Middleware". The driver is compatible with STM32F7 HAL up to version 1.15.0.

Note that the hardware initialisations (uController clock, peripherals clocks, etc.) must be separately performed, normaly in, or called from the `initialize_hardware.c` file of a gnuarmeclipse project. Alternatively you can do this using the CubeMX generator from ST. You may find helpful to check the following projects as references:
* https://github.com/micro-os-plus/eclipse-demo-projects/tree/master/f746gdiscovery-blinky-micro-os-plus
* https://github.com/micro-os-plus/eclipse-demo-projects/tree/master/f746gdiscovery-blinky-micro-os-plus/cube-mx which details how to integrate the CubeMX generated code into a uOS++ based project.

## UART Driver specifics
If you use CubeMX to initialize the UART(s), you have two choices: either you let the CubeMX generated code to initialize the uart handle (as well as the UART itself) at startup, or you provide your own function to initialize only the uart handle, while the UART itself will be initialized by the driver. In the first case, you have to define the `UART_INITED_BY_CUBE_MX` symbol to `true` (by default it is set to `false`). The second solution is the preferred one. An example is given below, see the `USER CODE BEGIN 2` section  (from the CubeMX `main.c` generated file):

```c
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* Enable I-Cache-------------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache-------------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();

  /* USER CODE BEGIN 2 */

  /* init UART6 handle */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* 'while' removed. */
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  return 0;
  /* USER CODE END 3 */

}
```
### Short theory of operation
The UART driver can perform data transfers DMA based, or interrupt based. The selection is done automatically at run-time, depending on the presence of the `hdmarx` and `hdmatx` handles, which are set or not during the hardware initialization phase (e.g. by the CubeMX). Both systems have their merits and pitfalls, but in general one would use a DMA based transfer for baud rates over 115200. At slow speeds (e.g. 19200 bps) DMA transfers do not make much sense.

### Transmit
The driver expects a buffer and a count of bytes to be sent. In both DMA and interrupt modes, it initializes the UART and DMA (if in DMA mode) and waits for the transfer to finsh. During this time, a semaphore blocks the caller to try to send more data, until the ongoing transfer is finished. An internal buffer is used for temporary storage until the data is sent.

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

## VCP Driver specifics
As already mentioned, the whole USB code can be generated by the CubeMX. There are several issues that must be observed though.

* There is a much discussed issue with a `malloc()` call in an interrupt routine. There is no universal solution to this problem, unless a large part of the USB code is re-written. I choose to statically allocate two RAM areas and use one or the other depending on the USB port ID used (HS or FS). Unfortunately, the file where the change must be done has no user modifiable areas. A solution is to modify the file directly in the CubeMX repository, so that every time the code is re-generated, the user changes are not lost. When a new version comes from ST, if you have checked-in your project in a repository, you can re-do the changes in the (new) original file from ST. The changes must be done in the file `usbd_cdc.c`, located in Repository/STM32Cube_FW_F7_V1.8.0/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/ (the FW version may differ). First, in the function `USBD_CDC_Init()`:

```c
    /* change begin: replace malloc() with static allocations (8/01/2018 LNP) */
    //pdev->pClassData = USBD_malloc(sizeof (USBD_CDC_HandleTypeDef));
    static USBD_CDC_HandleTypeDef usbdcdc_fs, usbdcdc_hs;
    if (pdev->id == DEVICE_FS)
    {
        pdev->pClassData = &usbdcdc_fs;
    }
    if (pdev->id == DEVICE_HS)
    {
        pdev->pClassData = &usbdcdc_hs;
    }
    /* change end */
```
And then in the function `USBD_CDC_DeInit()`:
	
```c
  /* change begin (8/01/2018 LNP) */
  // USBD_free(pdev->pClassData);
  /* change end */
```

* If you use the USB HS peripheral in FS mode, that is, without an external PHY chip, and the RTOS is configured to drive the ARM core to sleep (WFI or WFE), you must disable the ULPI sleep mode in the USB peripheral. This is done in the `usbd_conf.c` file (generated by CubeMX), as shown below (`USER CODE BEGIN USB_OTG_HS_MspInit 1`):

```c
void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  if(pcdHandle->Instance==USB_OTG_HS)
  {
  /* USER CODE BEGIN USB_OTG_HS_MspInit 0 */

  /* USER CODE END USB_OTG_HS_MspInit 0 */
  
    /**USB_OTG_HS GPIO Configuration    
    PB13     ------> USB_OTG_HS_VBUS
    PB14     ------> USB_OTG_HS_DM
    PB15     ------> USB_OTG_HS_DP 
    */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

    /* Peripheral interrupt init */
    HAL_NVIC_SetPriority(OTG_HS_IRQn, 11, 0);
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
  /* USER CODE BEGIN USB_OTG_HS_MspInit 1 */
  // This is required only if you use the USB HS device without an external PHY
    __HAL_RCC_USB_OTG_HS_ULPI_CLK_SLEEP_DISABLE();

  /* USER CODE END USB_OTG_HS_MspInit 1 */
  }
}
```
* And finally, you have to add/modify specific code in the `usbd_cdc_if.c` and `usbd_cdc_if.h` files. To make things simple, you will find in the `cube-mx-custom-files` as example the modified files for operation with this driver. You only have to generate the files (as often as you want) with the USB peripheral(s) you want to use and then activate the USB peripheral(s) you need by commenting out one or both of the following defines:
```c
#define USBD_DCD_FS
#define USBD_DCD_HS
```
At this point you should be done.

## Buffers selection
Both receive and transmit sections need decent buffers to properly operate. The buffer's size depends on your application. You can either provide two static buffers, or null pointers. In the later case the driver will dynamically allocate the buffers.

In general, for slow typed input and output, buffers of several tens of bytes are sufficient; it is important to test the result of the `read()` and `write()` functions to be sure all characters have been sent, or if you received the expected frame in its entirety.

However, if you implement a serial protocol, then the buffers should be sized according to the typical frame length of the protocol. Small buffers will still do, but the efficiency will decrease and at high speeds the driver might even lose characters.

## Tests
A separate directory `test` is included that contains a short test program for the UART: it opens a serial port, reads the current parameters, writes a string and receives it 10 times in a loop, then closes the port. The open/write/read/close cycle is repeated 10 times before the program exits.

Obviously, in order to function, you must short the RxD and TxD signals of your UART.

For the VCP there is too a simple test program: this one opens the VCP and echoes back all the characters it receives. You can try it with a terminal program by typing characters that should be echoed back. More elaborate testing can be done by means of a script or a small program written in your preferred language for your computer, that sends blocks of data and checks them when (and if) it receives them back.
