# stm32f7-usart
This is a USART driver for the STM32F7xx family of controllers.

## Version
* 0.1 (11 June 2017)

## License
* MIT

## Package
The driver is provided as an XPACK and can be installed in an Eclipse based project using the attached script (however, the include and source paths must be manually added to the project in Eclipse). For more details on XPACKs see https://github.com/xpacks. The installation script requires the helper scripts that can be found at https://github.com/xpacks/scripts.

## Dependencies
The driver depends on the following software packages:
* STM32F7 CMSIS (https://github.com/xpacks/stm32f7-cmsis)
* STM32F7xx HAL Library (https://github.com/xpacks/stm32f7-hal)
* uOS++ (https://github.com/micro-os-plus/micro-os-plus-iii)

Note that the hardware initialisations (uController clock, peripherals clocks, etc.) must be separately performed, normaly in, or called from the `initialize_hardware.c` file of a gnuarmeclipse project. You can do this using the CubeMX generator from ST. You may find helpful to check the following projects as references:
* https://github.com/micro-os-plus/eclipse-demo-projects/tree/master/f746gdiscovery-blinky-micro-os-plus
* https://github.com/micro-os-plus/eclipse-demo-projects/tree/master/f746gdiscovery-blinky-micro-os-plus/cube-mx which details how to integrate the CubeMX generated code into a uOS++ based project.

The driver was designed for the µOS++ ecosystem.

## Short theory of operation
    TBD

## Tests
    TBD

