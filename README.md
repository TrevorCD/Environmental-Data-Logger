# Environmental-Data-Logger

## Description
This project implements a real-time environmental data logger using an STM32 Nucleo-F446RE microcontroller, a BME680 environmental sensor, and a microSD card for storage. The system runs on FreeRTOS, featuring a custom I²C driver for the BME680. The result is a robust and power efficient environmental data logger that can reliably record temperature, humidity, pressure, and gas levels over time.

### Key Features
- **Interrupt-Driven Design:** The BME680’s data-ready interrupt triggers a FreeRTOS task to read sensor values over I²C.
- **Data Logging:** Sensor readings are passed via FreeRTOS queues to a dedicated task that writes timestamped CSV entries to the microSD card (SPI + FAT32).
- **Separation of Concerns:** Device driver, SD card interface, and RTOS tasks are cleanly separated.
- **UART Monitoring:** Optional task to send sensor readings over USB for real-time analysis.

### This project provides practical experience with:
- Writing device drivers for I²C peripherals.
- Using SPI to interface with a microSD card.
- Designing FreeRTOS multitasking systems.
- Handling interrupts safely in a real-time environment.
- Managing embedded data storage in CSV format.

## Goals
- Gain fundamental embedded SWE skills: 
  - STM32 microcontroller development and configuration
  - Communication protocols: I2C, SPI
  - RTOS development: scheduling, tasks, and queues
- Hone existing skills:
  - Device driver development
  - Datasheet comprehension
  - Hardware/software debugging
  - Embedded C programming
  - Interrupt handling
  - UART communication protocol
  - Technical documentation

## Hardware Components
### NUCLEO-64 STM32F446RE EVAL BRD
**Description:**
STM32 Nucleo-64 development board with STM32F446RE MCU  
**Manufacturer:**
STMicroelectronics  
**Product Number:**
NUCLEO-F446RE  
**Documentation:**
[Product Page](https://www.digikey.com/en/products/detail/stmicroelectronics/NUCLEO-F446RE/5347712)    [Datasheet](https://www.st.com/content/ccc/resource/technical/document/datasheet/65/cb/75/50/53/d6/48/24/DM00141306.pdf/files/DM00141306.pdf/jcr:content/translations/en.DM00141306.pdf)    [Databrief](https://www.st.com/content/ccc/resource/technical/document/data_brief/c8/3c/30/f7/d6/08/4a/26/DM00105918.pdf/files/DM00105918.pdf/jcr:content/translations/en.DM00105918.pdf)    [Website](https://www.st.com/en/evaluation-tools/nucleo-f446re.html?ecmp=tt9470_gl_link_feb2019&rt=db&id=DB2196#overview)  
**Voltage:**
VDD 1.7V - 3.6V, VDDIO 1.7V-3.6V  
**Interface:**
GPIO, I2C, SPI, UART  

### BME680-BREAKOUT BOARD
**Description:**
Humidity, pressure, temperature and air quality sensor  
**Manufacturer:**
Watterott Electronic GmbH  
**Product Number:**
201878  
**Documentation:**
[Product Page](https://www.digikey.com/en/products/detail/watterott-electronic-gmbh/201878/10071156)    [Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme680-ds001.pdf)    [Databrief](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/567/201878_Web.pdf)  
**Voltage:**
VDD 1.71V - 3.6V, VDDIO 1.2V - 3.6V  
**Interface:**
I2C, SPI  

### MICROSD SPI/SPIO BREAKOUT BOARD
**Description:**
Breakout board connects microSD slot to pins for SPI/SPIO  
**Manufacturer:**
Adafruit Industries LLC  
**Product Number:**
4682  
**Documentation:**
[Product Page](https://www.digikey.com/en/products/detail/adafruit-industries-llc/4682/12822319)    [Datasheet](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-microsd-spi-sdio.pdf)  
**Voltage:**
VDD 3.3V (delicate)  
**Interface:**
SPI, SPIO  

## Main Challenges:
- Writing an I²C device driver for BME680 -> Understanding datasheet, I2C Setup and Protocol
- Using SPI for SD card communication -> SPI Hardware Setup and Protocol
- Interrupts: need to configure EXTI + ISR + proper task signaling
- Synchronize with queues and deal with blocking file I/O
- Designing clean task flow architecture
- Power management: Set CPU sleep modes between interrupts
- Handling timing constraints and RTOS behavior

##  Dependencies
**FreeRTOS** v202406.01-LTS  
Real-time operating system for tasks, queues, and synchronization

**FatFS** R0.16  
File system library providing FAT32 support for SD card storage

**STM32CubeF4** 1.28.3  
STM32 configuration tool for initializing peripherals and middleware

**STM32 HAL drivers** v1.8.5  
Vendor-provided abstraction layer for STM32 Boards

##  FreeRTOS Task Layout:
- **Environmental Sensor Task**
  - Waits on a binary semaphore that is given by the ISR when new data is ready
  - Reads sensor via I²C when semaphore is given
  - Push data onto queue
- **Data Logger Task**
  - Waits on a queue of sensor readings from the sensor task
  - Pops data off queue
  - Write data to the SD card
- **Interrupt Handler**
  - Triggered by sensor’s “data ready” pin
  - Gives the semaphore to the sensor task
- **Serial Comm Task**
  - Print readings to serial console for debugging
  - Triggered by Data Logger Task when data popped off queue

Functions not yet assigned to tasks:
- Changing sleep mode
- 
