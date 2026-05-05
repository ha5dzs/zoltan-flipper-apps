# POCSAG pager

This is mostly based on the [original code](https://github.com/xMasterX/flipper-pager) that was in turn mostly based on the stock Sub-GHz code with whack-on features to support FM and [POCSAG](https://en.wikipedia.org/wiki/POCSAG) reception. The menu structure and modus operandi are pretty much the same as the original app, with a few extra menus whacked on here and there.

I used this app many times, but I got increasingly annoyed with the memory leaks and some basic settings that were hard-coded. Every time I started the app, I had to go through a set of key sequences to get it to start, and the more pages I collected the slower it worked until the Flipper eventually locked up.

Note that not everything is working perfectly, it's a development tool and I really did this to learn about these myself and not to make some fancy production-quality stuff. But, it's open-source, the code is available free of charge you are welcome to change something if you don't like it - as long as you share the source code too, like I did.

This code is reasonably heavily modified (mostly by my local, offline GenAI pipeline, much less so by me personally) so that:

* Most of the main settings are in a config file `/ext/pocsag/pocsag_settings.txt`
If the file and directory doesn't exist, the code will create it automatically with default settings. It uses the standard FlipperFormat. Very straightforward.
* Every received page is automatically logged to a csv file `/ext/pocsag/pocsag_received_pages.csv`
It automatically creates this file, with the header in it. The fields are:
  * `UnixTime`, which is th number of MILLISECONDS elapsed since the 1st of January, 1970
  * `Address`, which is the Radio Identification Code 'RIC' the message is being sent to
  * `Message` is the message inside the page. Shocking, right?

  This is just for testing purposes, so additional information (frequency, bitrate, etc.) are not logged.

In addition, it is now possible to:

* **Set the local RIC of the device, so your Flipper now can behave like a REAL pager!**

This is done with a compromise. GUI is difficult. There is a VariableItemList to select +/-10 RICs from the base address as specified in `pocsag_settings.txt`. Additionally, if the RIC is set to 0, then the alarm feature is disabled.

* **There are additional modulation presets, but mostly they don't work.**
I couldn't figure out how the CC1101 hardware is interacting via the Flipper HAL. Either there is a bug or something is not implementing properly, or I missed something. The 'FM95' preset deviates too high, +/- 9.5 kHz on my devices. The 'FM45' is supposed to address this to reduce the deviation to +/- 4.5 kHz (should be +/- 4.8 actually), but it doesn't. The 'FM45_B0H' inverts the bit assignment. This may be particularly useful when you only can find the sync bits and nothing else decodes, say because you are receiving some weird intermodulational product instead of the real signal.
* **You can transmit pages too!**
I made this a bit more difficult to use this. The bitrate is limited in the code to 512 baud (you need to understand the code and modify it to change this), and the frequencies are also limited. These can be bypassed if you know how to compile and debug code. Note that most POCSAG transmitters are long-range and not intended for bi-directional communication, and unless you are really close to a POCSAG-compliant pager, you probably won't achieve much with this device. But, if you find a pager in the e-waste bin or in a job lot and you want to test it, the low-power CC1101 hardware is enough to send a few test pages.
* **You can now leave this running for days, it will quietly log everything and won't lock up.**
At least, so far I didn't manage to lock it up - users are talented in breaking stuff, so probably I'll get a ton of issues in soon. :) Needless to say, the RTC of the Flipper Zero must be set properly - either via the mobile phone app or via qFlipper. These things tend to drift a little from time-to-time, so it won't be super accurate, but better than nothing. The millisecond precision timestamping is admittedly a bit of a gimmick - it's not ACTUAL time, the fractional milliseconds are derived from system ticks, and not from the RTC. The purpose is to avoid logging two pages with the same timestamp.

## Compromises and issues

This may be the Understatment of the Millenium, but while this code is functional, by any means, it is not perfect. This is what happens when you modify other people's code instead of writing from scratch. If I were to start making something like this, I would have used a completely different approach. There are some compromises made that are either due to missing features or cutting corners. These are:

* **No direct CC1101 configuration.** You probably need to use [TI's tool](https://www.ti.com/tool/SMARTRFTM-STUDIO).
...wishfully assuming that you are licensed to use these bands and know what you are doing to avoid interference. I also haven't quite figured out how to account for frequency offsets in the 26 MHz reference oscillator. By the time its reference gets multiplied up to 433 MHz, on the geekzero clone at least, this manifests as an about 50 kHz offset ( ((433/433.050)-1) / (433/26) ... about 10 ppm, cheapskates! ). It may be stored in the firmware, but I haven't found it yet.
* **It doesn't work with the Momentum firmware.**
I haven't really debugged this properly, but this may have something to do with how it handles the region-based TX lock. It is especially more prominent with devices that have no OTP flashed.
* **GUI is glitchy.**
The page detail screen is off by 1 pixel. 'Send message' only works if you fill the RIC and the message. If you don't keep this sequence, some flag is not being set, and you'll get back to the send menu without transmitting. There is some confirmation dialog as well, but right now it is bypassed.
* **Flipper HAL handling is glitchy.**
It was very difficult to make the Sub-GHz hardware stop transmitting with this circular DMA buffer business: even after it finished transmitting the POCSAG page, the transmitting didn't stop. So, I literally had to have a watchdog timer set up that would kill the transmission, because it just ended up transmitting continuous '1's, which manifested as a blank carrier hanging. Now, if this happens after 6 seconds, the hardware is reset irrespective of its state. Not pretty, but we don't want spectral pollution either.
* **Bit timing is off.**
According to the source code, the Flipper HAL only handles timing in microseconds. The POCSAG bit times are not exactly round in microseconds. So, eventually, you'll end up transmitting garbage due to cumulative error in the bit timings. This is least likely to happen at 512 baud. If you know how to crank the bitrate up, you'll probably find the code that changes the bit timing slightly every X bits, but it's not working very well. You have been warned.
* **I am not even sure if the transmission is properly POCSAG-compliant.**
It works with its own detector, but putting [gqrx](https://www.gqrx.dk/) on with [multimon-ng](https://github.com/EliasOenal/multimon-ng) with `nc -ul 127.0.0.1 7355 |sox -t raw -esigned-integer -b16 -r 48000 - -esigned-integer -b16 -r 22050 -t raw - | multimon-ng -t raw -v 3 -a POCSAG512 -a POCSAG1200 -a POCSAG2400 --timestamp -` doesn't seem to decode what the Flipper sent at all. My MMDVM-based POCSAG trasnmitter spewing pages from DAPNET works fine though.
* **The entire POCSAG protocol is bitbanged in software.**
This makes it slow. If your POCSAG transmitter is a bit hasty and has the tendency to transmit pages back-to-back, you will lose some of them. Can't be helped without dedicated hardware or at least more RAM. Same with transmitting, especially for the alphanumeric messages. 7 bit ASCII, MSB first, filler (idle) codewords, in all its glory. The CC1101's buffer is filled via a bunch of software interrupts (and additional nested callback functions), which probably contributes to the timing issues, especially considering that the Flipper's CPU is also busy other things too.
* **No support for numeric (`0b00`) and other exotic (`0b10` - `0b01`) formats.**
Right now, the encoder is hard-coded to have the function bits to (`0b11`), to send alphanumeric-only pages. Numeric-only pages would need special characters to be made on the display, and I don't even know what the other alternating function bits would do - probably nothing, it's not mentioned in the [IRU-R M.584.2](https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.584-2-199711-I!!PDF-E.pdf) document from 1997, 16 years after the initial standard came out. So probably nobody used them anyway.
* **Synchronisation could be better.**
Right now, as far as I understand, the bit timings are derived from the preamble. FSCs are not directly used to recover information, so if you lose the preamble, you'll lose the entire page. Probably not a big issue, and you really shouldn't be using this device when the signal conditions are dire.

## Code structure

This application follows the more-or-less standard Flipper Zero application structure with scenes, views, and helper modules:

### Main Application Files

* `pocsag_pager_app.c` / `pocsag_pager_app_i.c` - Main application entry point, initialization, and cleanup
* `pocsag_pager_app_i.h` - Internal header with application state structures
* `pocsag_pager_history.c` / `pocsag_pager_history.h` - Message history storage (circular buffer)

### Protocol Handling

* `protocols/pocsag.c` - POCSAG decoder implementation with auto-baud detection (512/1200/2400 baud)
* `protocols/pocsag_encoder.c` - POCSAG encoder with BCH(31,21) error correction
* `protocols/pcsg_generic.c` - Generic protocol helpers for serialization/deserialization
* `protocols/protocol_items.c` - Protocol registry definition

### Scene System (GUI navigation) (i.e. the difficult bit)

* `scenes/pocsag_pager_scene.c` - Scene handler configuration
* `scenes/pocsag_pager_scene.h` - Scene type definitions
* `scenes/pocsag_pager_scene_config.h` - Scene registration macros
* `scenes/pocsag_pager_scene_start.c` - Main menu scene
* `scenes/pocsag_pager_scene_receiver.c` - Message reception scene
* `scenes/pocsag_pager_scene_receiver_config.c` - Configuration scene (frequency, hopping, address)
* `scenes/pocsag_pager_scene_receiver_info.c` - Message details display scene
* `scenes/pocsag_pager_scene_about.c` - About/info scene
* `scenes/pocsag_pager_scene_send.c` - Send menu scene
* `scenes/pocsag_pager_scene_send_ric.c` - RIC input scene
* `scenes/pocsag_pager_scene_send_message.c` - Message input scene
* `scenes/pocsag_pager_scene_send_transmit.c` - Transmission control scene

### View System (UI rendering)

* `views/pocsag_pager_receiver.c` / `.h` - Receiver menu view with message list
* `views/pocsag_pager_receiver_info.c` / `.h` - Message details view

### Helper Modules

* `helpers/pocsag_config.c` / `.h` - Settings management (config file, CSV logging)
* `helpers/pocsag_pager_types.h` - Type definitions (states, enums)
* `helpers/radio_device_loader.c` / `.h` - Radio device selection (internal/external CC1101)

### Unused/Deprecated Code

The following code is present but NOT used in the current implementation:

* `pocsag_tx_yield_callback` - Legacy async TX callback (replaced by direct TX management via pcsg_tx_start)
* `pocsag_tx_*` static state variables - Legacy TX data tracking (not used, pcsg_tx_start uses local state)
* `pocsag_tx_yield_callback` - Legacy async TX callback (replaced by direct TX management)
* `pocsag_tx_*` static state variables - Legacy TX data tracking (not used)
* `pcsg_history_get_type_protocol()` - Protocol type getter (not called)
* `pcsg_history_get_protocol_name()` - Protocol name getter (not called)
* `pcsg_history_get_raw_data()` - Raw data getter (not called)
* `pcsg_history_get_text_item_menu()` - Menu text getter (not called)
* `pocsag_pager_scene_*_on_event()` functions in receiver/about scenes - Event handlers that return false (scene doesn't process events)

## The code

Yes, there are redundant bits in it that don't get used in this iteration. This is because I went off on weird tangents, and sometimes I made negative progress. I decided to leave those in as a memento. This will undoubtedly confuse others, but sometimes I was taken for a ride via a weird tangent and I didn't realise it until it was pretty late in the process. Not everything is restored to vanilla, and that's OK - it's a ham radio-spirited learning tool, not it is not intended to be or falsely pretend to be production-quality software.

## Installation, compiling, running

The flipper-ready compiled binary is in `dist/pocsag_pager.fap`. If you feel adventurous, or the Flipper creators introduces some breaking changes in the firmware AGAIN, your best bet is to recompile the code with `ufbt`: check for compilation errors with `ufbt`. Deploy and launch on device with `ufbt launch`. If you are curious or just ~~borderline insane~~ want to see the internals working, get to the command-line interface with `ufbt cli`. Launch the application from the menu, or via `loader` and then type `log debug` in the command prompt. There will be some debug statements remaining, but I had to remove them because they were degrading device performance.
