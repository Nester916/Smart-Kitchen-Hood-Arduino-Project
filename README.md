# Smart Kitchen Hood Automation

An Arduino-based smart ventilation system that actively monitors kitchen air quality, temperature, and potential gas leaks to autonomously manage an exhaust fan. The system features dynamic speed mapping, emergency overrides, and manual controls.

## Key Features

* **Autonomous Air Quality Control:** Utilizes a PM2.5 sensor to detect smoke and dynamically maps the exhaust fan's PWM speed (106 to 255) based on real-time particle density.
* **Emergency Gas Leak Override:** Immediately maxes out the exhaust fan (PWM 255), triggers a buzzer alarm, and flashes a warning LED upon detecting combustible gases.
* **Thermal Purge Mode:** Automatically forces the fan to maximum speed if the ambient temperature exceeds the critical threshold (45°C).
* **Debounce & Glitch Protection:** Implements dual-timer logic (8s detection, 15s clear) to prevent the fan from stuttering due to momentary sensor glitches or brief puffs of smoke.
* **Smart Lighting:** Integrates a BH1750 light sensor to automatically illuminate a green status LED when the kitchen goes dark.
* **Manual Override:** Includes physical push-buttons to force the system into Manual ON, Manual OFF, or return to AUTO mode.

## Hardware Stack

* **Microcontroller:** Arduino (Uno/Nano/Mega)
* **Air Quality:** Adafruit PM2.5 AQI Sensor (UART)
* **Temperature:** Dallas DS18B20 Temperature Sensor (OneWire)
* **Light:** BH1750 Light Sensor (I2C)
* **Gas/Smoke:** Digital Gas Sensor (e.g., MQ-2/MQ-135)
* **Actuators:** PWM-controlled Exhaust Fan, 5V Buzzer, Indicator LEDs (Green, Red/Gas)
* **Controls:** 3x Push Buttons (ON, OFF, AUTO)

## Pin Configuration

| Component               | Arduino Pin | Protocol / Type |
| ----------------------- | ----------- | --------------- |
| PM2.5 Sensor (RX/TX)    | D2 / D3     | SoftwareSerial  |
| Dallas Temp Sensor      | D4          | OneWire         |
| Exhaust Fan             | D5          | PWM Output      |
| Button: Manual ON       | D6          | Input Pullup    |
| Button: Manual OFF      | D7          | Input Pullup    |
| Button: Auto Mode       | D8          | Input Pullup    |
| LED: Gas Warning (Red)  | D9          | Digital Output  |
| LED: Status (Green)     | D10         | Digital Output  |
| Buzzer                  | D11         | Digital Output  |
| Digital Gas Sensor      | D12         | Digital Input   |
| BH1750 Light Sensor     | SDA / SCL   | I2C (Wire)      |

## Dependencies & Libraries

To compile and run this code, ensure you have the following libraries installed in your Arduino IDE:
* `SoftwareSerial.h` (Built-in)
* `Wire.h` (Built-in)
* `OneWire.h`
* `DallasTemperature.h`
* `BH1750.h`
* `Adafruit_PM25AQI.h`

## System Logic Details

1.  **Gas Leak Priority:** The gas sensor operates on a `LOW` active state. If triggered, it bypasses all other logic, maxes the fan, and sounds the alarm. It does not exit the loop, allowing serial data to continue streaming for debugging.
2.  **Temperature Priority:** If the temperature hits `45.0°C`, the system enters "THERMAL PURGE" and overrides the PM2.5 fan speeds.
3.  **Speed Memory:** The system saves the `lastFanSpeed`. If the PM2.5 sensor drops to 0 momentarily, the fan maintains its current speed to prevent annoying on/off cycling while cooking.
