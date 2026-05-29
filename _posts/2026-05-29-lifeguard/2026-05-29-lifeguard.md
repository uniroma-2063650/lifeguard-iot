---
title:       "Lifeguard"
description: "A distributed system for low-power, non-invasive patient health monitoring and intervention"
date:        2026-05-29
---

{{ page.date | date_to_string }}

---

# Lifeguard

## Introduction

Although medical and fitness devices are becoming ever more capable, two flaws are often obvious: they are either extremely expensive, or, most of all in hospitals, comfort and always-on functionalities aren't considered an important factor in their design. Wires running everywhere around a hospital patient aren't ideal; it's easy for them to get torn or constrict body parts, and in general they aren't comfortable at all.

As part of this class on Internet-of-Things algorithms and services, we chose to address the above points and focus on developing a health monitoring system for hospital patients that could harness IoT functionalities while providing good battery life, a potentially wireless design and a lower cost compared to existing solutions, all without compromising on accuracy and real-time data updates.

## Goals and Constraints

The project initially comprised a full end-to-end monitoring system ranging from straps monitoring patients' health, to hubs in each room providing nurses with real-time data on each patient, to a computer terminal that would have access to hospital-wide data. However, the project's scope had to be shrunk over time as the original workload was shown to be unfeasible, excluding the hospital-wide terminal in favor of focusing more on the individual straps and room hubs.

Within this resized scope, we set out to achieve a few goals:
- Having a simple monitoring strap design that could eventually translate to an inexpensive and non-invasive wearable product
- Collecting (at first) heart rate and SpO2 data with acceptable accuracy (eventually using other wearable devices as a ground truth for comparison)
- Transmitting real-time data for all patients in the room to the close-by hub devices and alerting nurses with visual and audible cues promptly enough to allow effective intervention

As a consequence of the stated goals, the following technical constraints were identified:
- **At least 1 week of strap battery life considering a standard 2500 mAh, 3.7 V LiPo rechargeable battery**
    - We didn't want patients to have to recharge their monitoring straps too often, or the benefits of the device being wireless would have been greatly diminished; 7 days were seen as an acceptable target, seeing as most patients don't stay for much longer
- **Being able to run on an embedded device running at 240 MHz at most**
    - This was caused materially by the nature of the ESP32-S3 used for the prototype, although in the end the processing power turned out to be more than enough for simple processing
- **Running all HR/SpO2 processing on-device**
    - This was indirectly caused by the impossibility to transmit the samples themselves, both because of duty cycle limitations and because of the occurrence of higher power consumption by passing data around rather than processing and aggregating it locally

Although it wasn't a hard constraint in principle, development was also carried out by trying to follow this general principle:
- **Collection of data every 5 seconds, with transmission every 30 seconds for stable data or earlier for anomalous data**
    - This was later changed to **an always-guaranteed 5-second interval for collection of initial or anomalous data, and a 30-second interval in consistently stable situations**
    - While the upper bound isn't formally derived, taking measurements every 30 seconds or even less often when hard real-time updates aren't needed seems to be a general standard upheld by wearable devices like the Apple Watch, saving power while still checking frequently enough to allow prompt intervention
    - On the other hand, 5 seconds of minimum delay were seen from the start as the lowest possible amount to process incoming sensor data without excessive fluctuations and inaccuracies
    - Importantly, constant collection and aggregation were scrapped in favor of making the monitoring strap adaptively adjust its own data collection habits; while this adds latency in case of a previously stable state degenerating, it's not usual for such a situation to develop from a stable state over the span of less than 30 seconds

## The Development Process

<img src="{{ '/assets/images/2026-05-29-lifeguard/overview.png' | relative_url }}" alt="General overview" width="400">

### Acquiring Hardware

The project was purposefully kept as minimal as possible to avoid incurring excessive costs both in the final product and more concretely for the people developing it. For this reason, the components used to build the prototype were:
- Two ESP32-S3 development boards, specifically one Heltec LoRa 32 v3 and one v4
- One MAX30102 sensor (with one backup, that ended up being used as the first one sadly failed) for heart rate and SpO2 measurements
- An RGB led
- An active buzzer

In addition, while unused in the actual devices, other tools were used for development
- An INA219 sensor, used for current monitoring
- An ATmega2560 development board to read data over I<sup>2</sup>C from the INA219, and send it back to a computer over USB-C (this setup was adopted out of necessity before a second ESP32-S3 was available)
- Two 4.7 k&Omega; resistors (used as I<sup>2</sup>C pull-ups for the ATmega2560 board)
- A breadboard and jumper wires to connect the components to each other
- Soldering tools for the various pin headers across components

### A First Prototype

To get a prototype, we first needed at least some pieces of code for:
- Heart sensor reading
- Sensor data processing into HR/SpO2 data
- Wireless data transmission to the room hub
- Patient data processing in the hub
- Warning/critical state signaling
- Monitor output for patient summaries

In general, instead of using the Arduino IDE or PlatformIO, we chose to use ESP-IDF because of its significantly higher closeness to hardware, enabling much less indirection and better optimization, especially for power saving, at the cost of making the resulting code less portable. As seen below, this unfortunately went on to create significant trouble with some libraries, but the higher ease of access to the underlying FreeRTOS environment provided immense benefits in idle power usage.

#### Heart Sensor Reading

The MAX30102 is an extremely small but relatively powerful device: with just two LEDs (one shining infrared light and one shining red light) and a photodiode[^max30102], it is able to detect changes in properties of the material in front of it.

This works for heart rate sensing by exploiting the way blood flows: as a heartbeat happens, the pressure inside blood vessels changes, ever so slightly affecting their thickness. That means that the volume of blood in front of the sensor changes, and in turn the light absorption profile. In this way, the photodiode ends up registering a periodic change in the amount of red and IR light reflected back to it, with the period coinciding exactly with the heart rate of the person using the sensor. As it turns out, these variations also encode SpO2 data (it helps to think about it in terms of the different color between deoxygenated and oxygenated blood). In general, the field of estimating cardiological data points from optical ones is called [photoplethysmography](https://en.wikipedia.org/wiki/Photoplethysmogram), and is at the heart of most modern wearable devices' health monitoring.

Several options are configurable: the sensor can choose whether to get red data, IR data or both; its has a configurable sample rate and pulse width and can perform sample averaging, which can be coupled with a higher sample rate to reduce fluctuations. In addition, the device has an internal 32-sample queue (with one sample containing both red and IR data), an interrupt pin and an option to assert it when the queue reaches a configurable size, freeing the main processor from the need to poll it.

For connectivity, the MAX30102 provides a standard I<sup>2</sup>C interface, which means that all[^its-one-i2c-device-what-could-it-take-an-hour] that was needed to connect to it was using the builtin `esp_driver_i2c` library; for the initial prototype, we ignored the power-saving features and queued and polled for samples for simplicity.

[^max30102]: Technically, it also has a temperature sensor, but it's not used by the device itself, and only provided as an optional additional measurement to be used by the sample processing software for calibration.
[^its-one-i2c-device-what-could-it-take-an-hour]: This is a lie and it took two days and a lot of debugging to figure out stupid bugs in the I<sup>2</sup>C-interfacing code manifesting as garbage data instead of error codes, I blame Espressif for constantly mixing bits and bytes.

#### Sensor Data Processing into HR/SpO2 Data

We now had red and IR samples but didn't know what to do with them!

Our first instinct was to run the simplest possible transformation on the data: convert it from numbers into something visual. For this, a small tool (in `utils/graph`) was written in Python to just graph samples into something that could be scanned visually.

After some changes[^cleanest-iot-sensor-data], we were able to see how the data looked:

![Red and IR samples]({{ '/assets/images/2026-05-29-lifeguard/red_ir_samples.png' | relative_url }})

Of course, however, while the heart rate was clearly visible in a graph, this had to be translated into code. Luckily, Maxim, the maker of the MAX30102, had already provided [a reference implementation](https://github.com/uniroma-2063650/lifeguard-iot/blob/main/source/components/max30102/src/maxim_hr_spo2.cc) for both heart rate and SpO2 calculations, so we used that for the first prototype, to later evaluate potentially better algorithms.

For the moment, this resulted in a constant loop of taking 100 samples at a rate of 25 samples/s (therefore with a window size of 4 seconds) and processing them through the Maxim code as a blackbox. To reduce fluctuations, we actually made the MAX30102 run at a sample rate of 400 samples/s, and then take averages of every 16 samples to reduce that to 25 samples/s.

We now had a mostly working set up:

```
1422685 171367(HR: 115, SpO2: 100)
1422645 171388(HR: 115, SpO2: 100)
1422623 171342(HR: 115, SpO2: 100)
...
```

[^cleanest-iot-sensor-data]: This was the moment when, after a lot of suffering, it was found that the data wasn't simply difficult to interpret, the original MAX30102 was broken and the "data" was literal noise.

### Patient Data Processing in the Hub

After getting the basic heart rate and SpO2 monitoring working, we decided to shift the focus to the room hub. Several ideas were considered like the hospital-wide control panel and using a small neural network to evaluate individual patient trends and give warning and critical state alerts. However, for now, we just calculated normal, warning and critical states for each patient by hardcoding limits in the source code (triggering a warning state for a HR of above 150 or below 30, or for a SpO2 below 90, and a critical one for  a HR above 190 or below 10 or a SpO2 below 60; in hindsight these limits didn't really make sense and were updated).

### Warning/Critical State Signaling

Of course, those warning and critical states that we calculated had to be reported to the nurses somehow, and noticeably so!

To avoid including (and having to buy) additional hardware, we chose to use an RGB LED and an active buzzer as a rudimentary alert system: they were connected to the ESP32-S3's pins and controlled by a new `alert` component, which would run on its own thread, listen to state updates over a queue and update the color of the LED and the buzzer's level accordingly. In this way:
- A normal state would keep both the LED and the buzzer inactive
- A warning state would light up the LED with a yellow color, and keep the buzzer inactive
- A critical state would light up the LED with a red color, and activate the buzzer

Of course, with these alerts not being patient-specific, the highest-criticality state across all patients was picked as the aggregated one (if even one patient was critical the buzzer would activate).

### Monitor Output for Patient Summaries

It wasn't enough to just show the aggregated state of patients, nurses also had to have access to the individual data for it to be of any use.

Conveniently, Heltec's LoRa 32 devboards all have the option to come with an OLED screen included, and ours did; despite it being an extremely small, 64x32-pixel monochrome screen, it would make do.

We put all the OLED-interfacing code in the `monitor` component, where we'd interface with the OLED over I<sup>2</sup>C with the help of both `esp_driver_i2c`, as before, and the `esp_lcd` ESP-IDF component.

Of course, however, this allowed us to put pixels on the screen and nothing more elaborate:

<img src="{{ '/assets/images/2026-05-29-lifeguard/monitor_checkerboard.png' | relative_url }}" alt="Monitor checkerboard" width="500">

A custom, very basic drawing library was made from the ground up to draw things to the screen; we also took the time to write a very very tiny font, just big enough to convey information while fitting as much data as possible in the display. Soon enough, we had actual (mock) data showing:

<img src="{{ '/assets/images/2026-05-29-lifeguard/monitor_patients.png' | relative_url }}" alt="Monitor with mock patient data" width="500">

### Wireless Data Transmission to the Room Hub

Now that the individual devices were working, it was time to actually make them talk to each other. It turned out that this wouldn't be possible by the time the first prototype had to be made available (and that we'd have to change approaches altogether and scrap all this code due to external circumstances), but an attempt was made anyway.

We chose to use LoRaWAN transmission, as it seemed to be the most energy efficient. However, all updated versions of LMIC (LoRaWAN MAC in C, the most widespread LoRaWAN library) were written for an Arduino environment, not for ESP-IDF. Thus began the effort to port MCCI-Catena's [arduino-lorawan](https://github.com/mcci-catena/arduino-lorawan) library to ESP-IDF, by completely rewriting its HAL implementation.

The process eventually resulted in 4 separate components all being added to the code:
- A `lmic` component with all the LMIC code, including the new HAL implementation
- A `comm_common` component with packet types and the code to pack/unpack them to minimize the size of the transmitted payload
- A `comm_strap` component that would ingest data sent by the HR/SpO2 calculation task over a FreeRTOS queue and transmit it over LoRaWAN
- A `comm_hub` component that would receive data and make it available over a FreeRTOS queue to the data processing task

While the implementation seemed to be functional, the closeness to the deadline by this point made it so that we had to postpone actually using it to a later deadline; the lines initiating actual data transmission in `comm_strap` were commented out, while the hub avoided starting the receiving process altogether and instead pushed mock data to the queue manually.

#### The Finished First Prototype

By this point, we had a very rough prototype, but it was still enough to use as a proof of concept.
In particular, these components were present in the source tree:
- `alert`: Used by the hub to signal alert states audibly and with a LED
- `monitor`: Used by the hub to show patient data on the builtin OLED monitor over I<sup>2</sup>C
- `max30102`: Used by the strap to interface with the MAX30102 controller and read samples out of it; also contained Maxim's HR and SpO2 processing code
- `lmic`: The adapted version of LMIC used by the `comm_*` components below
- `comm_common`: Common communication code shared between the strap and the hub, like some LMIC configuration options, utility macros and over-the-air packet format definitions
- `comm_hub`: The hub's communication code for receiving samples from the strap (unused and replaced with a mock)
- `comm_strap`: The strap's communication code for transmitting samples to the hub (with the actual `LMIC_setTxData2_strict` call commented out)
- (`ina219`: Currently unused and incomplete code for interfacing with the INA219)

&nbsp;

![First delivery dependency graph]({{ '/assets/images/2026-05-29-lifeguard/dependencies_first_delivery.png' | relative_url }})

{:style="text-align: center"}
_The dependency graph_

&nbsp;

![First delivery architectural graph]({{ '/assets/images/2026-05-29-lifeguard/architecture_first_delivery.png' | relative_url }})

{:style="text-align: center"}
_The general system architecture_

### Improving on the First Prototype

While the proof-of-concept was done, there were still at least a few missing pieces:
- The strap and hub weren't communicating at all yet
- Maxim's HR and SpO2 algorithm had turned out to be very unreliable in practice
- We hadn't measured the power usage at all, and generally hadn't optimized for it at all (or actively done things that would hurt it)

#### Making the Strap and Hub Communicate

The project was set up to use LoRaWAN, which, as it turned out, relied on a LoRaWAN gateway; as we were unable to test with the university-provided one, we had to get a gateway for our own testing. Hobbyist LoRaWAN gateways start at &euro;150 at the lowest, or &euro;100 for DIY ones.

LoRaWAN was now completely out of the question, so all the communication code was rewritten from the ground up to use BLE (Bluetooth Low Energy) by using ESP-IDF's `bt` component.

Compared to LoRaWAN, which works very similarly to the TCP/IP stack, BLE brings a complete paradigm shift: devices are either "peripheral" or "central", and all peripheral devices do is provide collections of value slots ("services" containing "characteristics") that the central device can read from or write to, or optionally subscribe to to listen for changes[^gatt-is-not-universal].

To save data, services and characteristics are identified not by names but by 16- to 128-bit UUIDs. In particular, the Bluetooth SIG defines 16-bit UUIDs for most basic functionalities (we ended up not using anything but these standard UUIDs).

With just value slots, it might look like the central device is forced into polling its peripherals for updates. However, as was briefly mentioned, peripherals can enable subscription to these values through "indications" and "notifications" (the only difference being that the former use reliable transport while the latter don't): once the central device reports that it wants to subscribe to changes to that value, the peripheral device starts sending indication/notification packets back to it whenever the value changes, containing a copy of the new value.

The connection process is also different from just connecting to a shared network: when not connected to a central device, an active peripheral device "advertises" its presence to its surroundings, with a packet containing its own address and other basic data; the central device, in turn, can scan the network for these advertisements and initiate a connection request.

In our case, logically, we made the hub a central device and the strap a peripheral one. Predicting the hospital to be organized in a hierarchy of rooms and beds, we put the room and bed identification data directly in the strap's advertisement packet, so that a room's hub could immediately ignore any straps not belonging to that room.

As for the strap's GATT services, the following ones were implemented:
- Service `0x180D` (Heart Rate Service):
    - Characteristic `0x2A37` (Heart Rate Measurement): readable and subscribable to via indications
- Service `0x1822` (Pulse Oximeter Service):
    - Characteristic `0x2AF9` (Generic Level): SpO2 percentage, readable and subscribable to via indications
- Service `0x180A` (Device Information Service):
    - Characteristic `0x2C34` (Installed Location): Room number, readable
    - Characteristic `0x2A9A` (User Index): Bed number, readable

Soon enough, the strap could be connected to even from a normal computer, and all these values could be seen in real time:

![Connecting to the BLE device from a computer]({{ '/assets/images/2026-05-29-lifeguard/computer_ble.png' | relative_url }})

With the strap working, all that was needed on the hub's end was scanning for peripheral devices, filtering them by room number, connecting to them and subscribing to the heart rate and SpO2 characteristics in order to receive their updates in real time.

[^gatt-is-not-universal]: The connection management part is called the GAP, or generic access profile, while the service/characteristic system is called the GATT profile, or the generic attribute profile, and is used by most BLE applications; the most notable exception to this is the BLE mesh profile, which is only based on the GAP, and is more similar to TCP/IP.

#### Improving the HR and SpO2 Algorithm

At this point, the strap and hub could communicate, but the data wasn't very high quality and kept fluctuating as soon as the sensor experienced the slightest movement. Of course, it was very desirable to improve this, so we went back to the `graph` utility and expanded it a bit with some attempts at making better sense of the data, some transformations and its frequency analysis:

![Red and IR samples and their FFT]({{ '/assets/images/2026-05-29-lifeguard/red_ir_samples_fft.png' | relative_url }})

It could be seen that the reflected IR and red light amounts followed a very specific curve which, while being periodic, wasn't just a sinusoid; this could have be canceled out by a 7-sample window, but that would have meant potentially losing information about higher heart rates (although not very realistic, as anything over 240 BPM is rare). Luckily enough, the FFT alone revealed peak frequencies very well, especially with an adjustment (as seen as the bottom) to favor common heart rates over noise and the frequencies corresponding to the heartbeat's shape.

After getting this confirmation, the `arduinoFFT` library was added to the project as a component (with very small changes to make it run in a non-Arduino environment), and a new `fft_hr_spo2.cc` file was added to the `max30102` component with the new analysis code.

While the FFT revealed the heart rate itself, the way to calculate SpO2 was a bit more complicated and required reverse-engineering Maxim's code; in the end, it consisted of:
- Identifying peaks and valleys for each heartbeat
- Calculating $$ \frac{peak - valley}{peak} $$ for both red and IR light
- Dividing the result for red light by the result for IR light
- Mapping the result through the function $$f(x) = -45.060 * x^2 + 30.354 * x + 94.845$$, with $$0 \le x < 1.825$$ (as from that point on $$f(x)$$ would have been negative), yielding results between 0 and 100 (interpreted as percentage points)

In the end, the resulting calculation was:

$$ x = \frac{peak_{red} - valley_{red}}{peak_{red}}\dot{}\frac{peak_{ir}}{peak_{ir} - valley_{ir}} $$

$$ f(x) = \max\{-45.060 * x^2 + 30.354 * x + 94.845,\; 0\} $$

Although, instead of being calculated on the fly, $$ f(x) $$ was replaced by a lookup table.

This approach, coupled with using a FFT to calculate the heart rate, yielded much better and more stable results than before, at the cost of needing a little more processing power; as arduinoFFT only deals with power-of-two window sizes, we adjusted the number of samples to 128 (which, at the unchanged sample rate of 25 samples/s, corresponded to a window size of 5.12 s).

### Optimizing Power Usage

<img src="{{ '/assets/images/2026-05-29-lifeguard/setup.png' | relative_url }}" alt="Professional Optimizer Setup" width="500">

With the lowest-hanging fruit taken care of, we now had to focus on the main unmet requirement: battery life. This first of all required writing some tool to record the device's power draw at all: usually, a multimeter would have been used for that, but we also had the option of interfacing with an already available INA219 sensor over I<sup>2</sup>C to measure small currents.[^ina219]

Since at that point there was only one ESP32-S3 available, and making it measure its own power usage via an INA219 would have skewed the results, the only remaining option was to find some other device to connect to the sensor. Luckily, an ATmega2560 development board was available, which did support I<sup>2</sup>C (albeit by calling it TWI).

A program was written to connect to the INA219, configure it and read measurements from the ATmega2560, available in `src/power_measurements`. In this way, we'd be able to evaluate improvements in power draw with different approaches.

[^ina219]: The sensor actually detects voltage drops, but because its Vin+ and Vin- pins are connected through an internal shunt resistor, they can instead be placed in series with a device (Vin+ wired to an external Vcc and Vin- used as the device's Vcc) to measure the shunt's voltage drop, dividing it by the shunt's resistance to obtain the current draw.

#### Harnessing Light Sleep and CPU Frequency Scaling

We now had to optimize the strap's power draw to achieve the 7-day battery life we were looking for, and first of all, having a lot of idle time wouldn't have mattered if the processor could never go to sleep during it!

To free programmers from the burden of having to insert manual power management calls all over the place, ESP-IDF offers the option to directly hook into FreeRTOS's scheduling to enter light sleep whenever nothing is scheduled for some time. This complicates things a bit with wakeups, as interrupt sources also have to be registered (and registrable) as wakeup sources, but it's otherwise the most immediate way to save a lot of power, especially when, like on the ESP32-S3, the processor's power draw in light sleep can go down to less than 240 &mu;A while still waking up in very little time.

The first step needed to enable this was importing the `esp_pm` component and adding the following lines to `app_main`:
```cpp
esp_pm_config_t pm_config = {
    .max_freq_mhz = 240, .min_freq_mhz = 240, .light_sleep_enable = true};
ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
```

At that point, however, another part of this call jumped to the eye: the frequency of the CPU itself could be changed as needed! 80 MHz as a max frequency (the lowest one allowed) and 10 MHz as a minimum one seemed to work pretty well:
```cpp
esp_pm_config_t pm_config = {
    .max_freq_mhz = 80, .min_freq_mhz = 10, .light_sleep_enable = true};
ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
```

The rest of the code had to be adjusted a bit to account for both automatic light sleep and CPU frequency scaling: heavy processing code was updated to ask the system to speed up to 80 MHz, and all I<sup>2</sup>C code had to prevent the system from going to sleep while performing transactions due to quirks in the driver.

To avoid dropping BLE connections whenever the processor entered light sleep, the `bt` component also required allowing the modem to sleep at the same time, as well as using an external 32 kHz clock when doing so, found on the devboard itself.

#### Optimizing Sensor Reading

As explained before, the ESP32 on the strap was polling the MAX30102 constantly for new samples, denying it the chance of ever going into light sleep as we had just implemented.

Luckily, we had also mentioned that the MAX30102 does have an internal 32-sample queue and an interrupt line that can be asserted when it's full enough. For a 128-sample window we could have asserted the interrupt line when the queue was full to minimize wakeups, but that could inadvertently have caused issues if the ESP32 didn't wake up fast enough and the MAX30102 had to drop new samples due to running out of space. For this reason, we decided to raise an interrupt for every 26 samples collected, waking up 5 times instead of 4, around every second, but having a safer margin.

After implementing this, the power draw had improved, but it was still very much over our target of 15 mA, especially when connected to via BLE.

#### Dimming the Lights

After optimizing everything else, the most glaring issue remaining wasn't in the code itself: did we really need to run the sensor's LEDs at 25.6 mA, and the BLE TX antenna at the default power of +9 dBm (around 8 mW)?

We ran several tests confirming that the sensor's accuracy could actually be kept even by lowering its sample rate from 400 samples/s to 100 samples/s, adjusting the aggregation window to 4 samples to keep the same output sample rate of 25 samples/s. The same was confirmed for LED currents down to 6.4 mA, making those our new settings.

The next target for power cuts was the BLE TX antenna: after some tests, it was seen that running it at -18 dBm (0.016 mW) wasn't significantly affecting connectivity, especially testing at the close distances seen in a hospital room.

These changes greatly lowered the current draw, but it still went a bit over the target whenever the BLE connection to the hub was active.

#### Less Frequent Checks

At this point, we remembered that from the start, we had predicted that the sensor wouldn't be usable all the time due to power concerns: in fact, that's what most wearable devices do, as after some point, constant updates become useless.

Of course, we couldn't neglect patients that did need that constant monitoring, so the best thing to do was to determine whether the patient wearing the strap did or didn't need frequent check-ins by using heuristics.

After a lot of thinking, we came up with the following checks:
- Check whether the current HR and SpO2 value is in a "safe" range (40-120 BPM for heart rate, with a low limit to avoid oversampling sleeping people, and 94%-100% for SpO2)
- Check whether a history has been built up (around one minute of history, or 12 HR/SpO2 values, of which at least 9 not inconclusive)
- Calculate the medians of the HR and SpO2 histories, and check them against the same safe range as before (if many values start going out of range, it's probably good to keep watch even if some go back into it)
- Calculate the median absolute deviations for HR and SpO2, and check that the new HR and SpO2 aren't deviating too much (over 2 times the MAD) and that the MADs themselves aren't too high (20 BPM and 5% for HR and SpO2; rapidly changing data should also cause worry)

If any of the above checks failed, the system would monitor the patient closely, taking constant samples until the situation changed into a safe one. If they all passed, the checks would be reduced to once a minute (meaning the sensor would only be active around 8.5% of the time), still monitoring the situation to switch to the more active mode as needed.

To simplify implementing this, the MAX30102 itself had a "sleep" function, meaning that instead of physically disconnecting the sensor, we could conveniently leave it connected and just put it to sleep to pause all LED usage and readings.

By now, we finally measured the power draw again, by letting the strap run undisturbed for 10 minutes:

![Current draw over 10 minutes after implementing MAX30102 sleeping]({{ '/assets/images/2026-05-29-lifeguard/current_max30102_sleep.png' | relative_url }})

We had finally met the target, and overdone it too, at 11.69 mA against the 15 mA we were initially aiming for!
However...

#### BLE Optimization, Part 2

After some thinking about it, we had forgotten one critical part of the BLE stack: in the last picture, there were visible, constant spikes up and down, caused by BLE waking the antenna up constantly. Had we forgotten to configure the connection parameters correctly?

```cpp
const ble_gap_upd_params params = {.itvl_min = conn_desc.conn_itvl,
                                   .itvl_max = conn_desc.conn_itvl,
                                   .latency = 3,
                                   .supervision_timeout =
                                       conn_desc.supervision_timeout,
                                   .min_ce_len = 0,
                                   .max_ce_len = 0};
```

They weren't just incorrect, the code was unchanged from the BLE examples! After realizing, it was updated to reflect more sensible parameters for the application:

```cpp
const ble_gap_upd_params params = {
          .itvl_min = BLE_GAP_CONN_ITVL_MS(50),
          .itvl_max = BLE_GAP_CONN_ITVL_MS(100),
          .latency = 10,
          .supervision_timeout =
              std::max((uint16_t)BLE_GAP_SUPERVISION_TIMEOUT_MS(2000),
                       conn_desc.supervision_timeout),
          .min_ce_len = 0,
```

We set the connection interval to anywhere between 50 ms and 100 ms (generally the time between data packet transmissions), and the peripheral latency to 10 connection intervals (500 ms-1 s).

In BLE, the packet latency is generally determined by the connection interval, while the "peripheral latency" indicates how long the peripheral can sleep for when it doesn't have any new data to send: this means that, with a connection interval of 100 ms and a peripheral latency of 1 s, the peripheral can dynamically change the rate at which it sends data updates anywhere within that range, and save a lot of power.

After this last optimization, we measured the power draw again, by letting the strap run undisturbed for 10 minutes:

![Current draw over 10 minutes]({{ '/assets/images/2026-05-29-lifeguard/current_final.png' | relative_url }})

Not only had we met our target, but we had actually overdone it! Even with the slight disturbance shown at one point, and including the initial power draw spike, the system was able to run at an average current of 8.14 mA, almost half of our initial target of 15 mA, giving us a battery life of almost 13 days with an average 2500 mAh LiPo cell.

Even then, we still tried loosening the checks' frequency a bit more, to every 10 seconds when anomalous and every 60 seconds when normal; while not corresponding to our original goals, these frequencies would also have provided an acceptable resolution at an even lower current draw of 7.30 mA:

![Current draw over 10 minutes, less frequent checks]({{ '/assets/images/2026-05-29-lifeguard/current_final_less_frequent.png' | relative_url }})

In general, disregarding these further optimization opportunities, it was extremely fulfilling to have overshot the original target by so much without having to make significant compromises.

## The End Result

![First delivery dependency graph]({{ '/assets/images/2026-05-29-lifeguard/dependencies_final.png' | relative_url }})

{:style="text-align: center"}
_The dependency graph_

&nbsp;

![First delivery architectural graph]({{ '/assets/images/2026-05-29-lifeguard/architecture_final.png' | relative_url }})

{:style="text-align: center"}
_The general system architecture_

&nbsp;

## Evaluating the Final Product

### How and Whether We Met Our Goals

We had initially set out to meet the following goals:
> - Having a simple monitoring strap design that could eventually translate to an inexpensive and non-invasive wearable product
> - Collecting (at first) heart rate and SpO2 data with acceptable accuracy (eventually using other wearable devices as a ground truth for comparison)
> - Transmitting real-time data for all patients in the room to the close-by hub devices and alerting nurses with visual and audible cues promptly enough to allow effective intervention

And these added constraints:
> - At least 1 week of strap battery life considering a standard 2500 mAh, 3.7 V LiPo rechargeable battery
> - Being able to run on an embedded device running at 240 MHz at most
> - Running all HR/SpO2 processing on-device

As things stand, we did meet the goals of having a simple design (although whether it can translate to a cheap wearable still has to be tested), collecting accurate HR and SpO2 data and transmitting it to a hub (after the switch from LoRaWAN to BLE) to process them into alerts and display output.

As for the constraints, we achieved on-device HR/SpO2 processing through Maxim's code, and later through our own FFT-based implementation, and were even able to do that while significantly lowering the CPU's clock frequency to 80 MHz to do that processing. As for battery life, we ended up vastly overshooting the target, getting it to almost 13 days, although in hindsight that might be counterbalanced by the fact that actual portable batteries are often much smaller in capacity, getting us back to a lower one.

### Comparison with Commercial Products

For a comparison, the closest product available to the device we built was a simple Apple Watch (even non-recent): an Apple Watch Series 6, released in 2020 for around &euro;379, carries a 303.8 mAh battery touting a 18-hour battery life, and supports automatic HR and SpO2 measurements through the wrist.

We proceeded to make some comparisons both for health data accuracy and power draw.

#### HR and SpO2 

In general, we were able to match the HR and SpO2 reported by the Apple Watch extremely closely:

<img src="{{ '/assets/images/2026-05-29-lifeguard/apple_watch_hr.png' | relative_url }}" alt="Apple Watch HR (97 BPM)" width="250"> <img src="{{ '/assets/images/2026-05-29-lifeguard/apple_watch_spo2.png' | relative_url }}" alt="Apple Watch SpO2 (100%)" width="250">

{:style="text-align: center"}
_Examples of heart rate and SpO2 values shown on an Apple Watch Series 6_

&nbsp;

<img src="{{ '/assets/images/2026-05-29-lifeguard/lifeguard_hr_spo2.png' | relative_url }}" alt="Lifeguard HR and SpO2 (96 BPM, 100%)" width="500">

{:style="text-align: center"}
_Examples of heart rate and SpO2 values shown on the room hub_

At any time, the strap's SpO2 followed the Apple Watch's closely (with at most a &plusmn;2% error); compared to our device, the Apple Watch tended to underestimate SpO2 and very frequently give inconclusive results. However, this could have been mainly because of the different placement: there are many more blood vessels in fingers than there are in the wrist where a watch is placed (in fact, we initially wanted to make a wrist strap, but had to reevaluate the decision as it kept resulting in nothing but noise).

As for heart rate, both devices always matched each other very closely, with deviations of at most &plusmn;5 BPM; however, the larger deviations, instead of errors, seemed to be more about the Apple Watch lagging behind the strap by a few seconds, eventually reflecting the same trends just seen on the other device.

#### Power Draw

![Current draw over 10 minutes]({{ '/assets/images/2026-05-29-lifeguard/current_final.png' | relative_url }})

{:style="text-align: center"}
_Strap current draw over 10 minutes in an average situation_

As just mentioned, the Apple Watch advertises a 18-hour battery life with a 303.8 mAh battery, implying an average current draw during that time around 16.88 mA. At the same time, based on the measurements we just ran before, the strap's average current draw in non-anomalous situations is around 8.14 mA.

Based on raw numbers alone, these results show that our device's energy impact is much lower, by 50%, which is impressive. However, this requires the added context of the Apple Watch being much more than a heart rate/SpO2 device, with other smartwatch functionality inevitably increasing its energy usage and even just the frequency of its BLE communications.

## The Parts We Lost Along the Way (and Future Developments)

As shown by the difference between the original concept and the final device, several potential aspects of the project were left out that would be extremely interesting to explore in the future:
- Adapting to individual patients for better health analysis, possibly by training small neural networks over time
- Communicating with a hospital-wide control panel with bidirectional communication with the hubs (to i.e. override warnings)
- Adding more health data points, like body temperature or blood pressure
- Looking more into whether our device could work as a wriststrap too (and, in general, going from the final prototype to an actual wearable device)

## Conclusion

Despite development being troubled in so many ways, we were glad to eventually be able to not only get this project running, but meeting or overshooting initial goals.
We hope that this will potentially set the record for future developments, or be as useful to others as it was for us to learn the inner workings of several embedded devices and the ESP-IDF toolchain and components.

{:style="padding: 1em 2em;"}
_&mdash; Sara_

{:style="text-align: center;"}
[lifeguard-iot on GitHub](https://github.com/uniroma-2063650/lifeguard-iot)

&nbsp;

---
