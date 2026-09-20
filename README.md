# ADShare++ for PlayStation Portable
> [!NOTE]
> **ADShare++** is the modular, improved, and C++ rewrite of my original [PSP-ADShare](https://github.com/welabsdev/PSP-ADShare) project.
> [!IMPORTANT]
>ADShare and ADShare++ clients can still communicate with each other because they remain protocol-compatible. However, using the >same version on both PSP systems is recommended to ensure the best stability and compatibility.

It is a PSP homebrew application designed for direct file sharing between two PlayStation Portable systems using the console's native **Ad Hoc WLAN**, without requiring an internet connection, router, FTP server, or PC as an intermediary.

This version keeps the original ADShare concept and protocol while reorganizing the project into a cleaner C++ codebase split into dedicated modules under `src/`.

> Developed by **welabsdev**

---

## About this version

The original **PSP-ADShare** started as an experimental project written in C and gradually evolved into a functional PSP-to-PSP file sharing application.

**ADShare++** is the next step of that project.

The main goals of this version are:

- Rewrite the project in **C++**
- Replace the original single-file structure with a **modular architecture**
- Keep the working PSP Ad Hoc transfer system
- Improve code organization and maintainability
- Make future development easier
- Preserve compatibility with the PSPDEV / PSPSDK environment
- Keep the application lightweight enough for real PSP hardware

The networking concept remains the same: two PSP systems discover each other over Ad Hoc and transfer files directly between consoles.

---

## Inspiration

ADShare was originally inspired by **Adhoc File Transfer PSP**, released in 2010.

That application demonstrated that direct PSP-to-PSP file transfers were possible, but its interface was very simple and felt almost like a command-line utility.

I wanted to revisit that idea with a more modern interface and a workflow better suited for everyday use.

The project also came from a personal need: I often wanted to share music, screenshots, videos, homebrew, and other files between my PSP and my brother's PSP without having to connect both consoles to a computer.

The PSP WLAN hardware is limited, so transfer speeds are not comparable to modern devices. However, for smaller files ADShare works very well, and larger files can also be transferred if you are willing to wait.

---

## Features

- Direct PSP-to-PSP file sharing
- Native PSP Ad Hoc networking
- No internet connection required
- No router required
- No FTP server required
- No PC required during transfers
- Automatic discovery of nearby ADShare++ consoles
- Accept or reject incoming transfers
- File browser for selecting files
- Destination folder browser on the receiving PSP
- Exact save-location selection before a transfer starts
- Support for `ms0:/`
- Support for `ef0:/` on PSP Go
- Transfer progress display
- Transfer speed information
- Transfer cancellation
- CRC32 integrity validation
- Packet acknowledgements
- Sequence numbers
- Automatic retransmission of lost packets
- Duplicate packet detection
- Partial `.part` file handling
- Protection against overwriting existing files
- Ad Hoc channel selection
- Brazilian Portuguese interface
- English interface
- Runtime language switching
- PSP hardware information
- WLAN MAC address display
- Firmware information
- PSP generation detection
- Region / SKU information through IDStorage
- Tachyon, Baryon, and Pommel information
- Motherboard family/revision detection when enough hardware information is available
- Modular C++ source code

---

## Receiver-selected destination

One of the main improvements in recent versions is the ability for the receiving PSP to choose exactly where an incoming file should be saved.

The transfer flow is now:

```text
PSP A                                  PSP B
  |                                       |
  |---------- Transfer Request ---------->|
  |                                       |
  |<------------- Accept -----------------|
  |                                       |
  |<------------ PREPARE -----------------|
  |                                       |
  |       Receiver chooses a folder       |
  |                                       |
  |------------- HEADER ----------------->|
  |<----------- HEADER_ACK ---------------|
  |                                       |
  |------- CHUNK #0 + CRC32 ------------->|
  |<------------- ACK #0 -----------------|
  |                                       |
  |------- CHUNK #1 + CRC32 ------------->|
  |<------------- ACK #1 -----------------|
  |                    ...                |
  |--------------- FIN ------------------>|
  |<------------- FIN_ACK ----------------|
```

The receiving user can browse the Memory Stick or internal storage before the transfer begins.

Files are no longer forced into automatically created `ADShare` subfolders.

For example, the receiver can choose:

```text
ms0:/PICTURE/
ms0:/MUSIC/
ms0:/ISO/
ms0:/PSP/GAME/
ms0:/DOWNLOAD/
```

or another existing folder.

On PSP Go, `ef0:/` can also be selected.

If a file with the same name already exists, ADShare++ generates a unique filename instead of overwriting the existing file.

---

## File Transfer Protocol

ADShare++ uses a custom reliable protocol built on top of the PSP's **PDP Ad Hoc networking API**.

Discovery and control communication use one PDP channel, while file data transfer uses a separate PDP channel.

The data-transfer protocol uses:

- `HEADER`
- `HEADER_ACK`
- `CHUNK`
- `ACK`
- `FIN`
- `FIN_ACK`
- `CANCEL`

Each data chunk includes information used to validate and track the transfer.

Reliability features include:

- Packet sequence numbers
- CRC32 checksums
- ACK-based confirmation
- Automatic retries
- Duplicate packet detection
- Transfer tokens
- Timeout handling
- Partial file protection
- Clean cancellation

These mechanisms were added because packet loss can occur on real PSP Ad Hoc WLAN connections.

---

## Ad Hoc Channel

Both PSP systems must use the same Ad Hoc channel.

ADShare++ supports the channels exposed by the PSP system:

```text
Automatic
Channel 1
Channel 6
Channel 11
```

The channel can be changed directly inside the application using `SELECT`.

If the channel is changed while Ad Hoc is active, the application can restart the connection using the newly selected channel.

---

## Languages

ADShare++ currently supports:

- **Portuguese (Brazil)** — my native language
- **English**

The application attempts to follow the PSP system language when it starts.

The language can also be changed at runtime using:

```text
SQUARE
```

---

## Hardware Information

ADShare++ includes a hardware-information screen using PSP kernel-related resources.

The application uses:

- KUBridge
- LibPspExploit
- IDStorage
- Tachyon
- Baryon
- Pommel

Information displayed may include:

```text
Model
Generation
Firmware
Region / SKU
System language
WLAN MAC
Ad Hoc channel
IDStorage region
Tachyon
Baryon
Pommel
Motherboard family / revision
```

The PSP model is not guessed from available RAM.

When enough hardware information is available, ADShare++ can identify PSP generations and commercial model variants more accurately.

Motherboard information is only displayed when the available hardware identifiers are sufficient for a reasonable identification.

---

## Project Architecture

Unlike the original single-file version of PSP-ADShare, ADShare++ is organized into multiple C++ source files.

```text
src/
├── adshare.hpp
├── app.hpp
├── app.cpp
├── main.cpp
├── state.cpp
├── utils.cpp
├── hardware.cpp
├── ui.cpp
├── adhoc.cpp
├── data_protocol.cpp
├── transfer.cpp
├── browser.cpp
└── screens.cpp
```

### Module overview

| File | Responsibility |
|---|---|
| `main.cpp` | PSP entry point |
| `app.cpp` | Application lifecycle and main loop |
| `state.cpp` | Shared application state |
| `utils.cpp` | Common utilities and path helpers |
| `hardware.cpp` | PSP model, region and hardware information |
| `ui.cpp` | Shared UI drawing helpers |
| `adhoc.cpp` | Ad Hoc initialization, discovery and control packets |
| `data_protocol.cpp` | Reliable PDP data protocol |
| `transfer.cpp` | Send/receive transfer logic |
| `browser.cpp` | File and destination folder browsers |
| `screens.cpp` | Main application screens and menus |
| `adshare.hpp` | Shared declarations, protocol structures and constants |
| `app.hpp` | Application class interface |

This structure makes the project easier to understand, debug, maintain, and extend.

---

## C++ Rewrite

ADShare++ is written in **C++17**.

The migration from the original C code introduced a cleaner application structure while keeping compatibility with the PSP toolchain.

Some of the C++ improvements include:

- Namespaced project code
- Dedicated `Application` class
- Better separation between UI, networking, transfer logic, hardware information, and file browsing
- `constexpr` configuration values
- Stronger compiler type checking
- RAII-style cleanup for transfer state
- Reduced global implementation details between modules
- Cleaner public interfaces between source files

The project intentionally avoids heavy desktop-style C++ features that would not make sense on PSP hardware.

The build currently disables exceptions and RTTI:

```makefile
-fno-exceptions
-fno-rtti
-fno-threadsafe-statics
```

---

## Technologies

ADShare++ is built using the PSP homebrew development ecosystem.

Main technologies and libraries:

- C++17
- PSPSDK
- PSPDEV
- OSLib
- intraFont
- KUBridge
- LibPspExploit
- PSP Ad Hoc networking APIs
- PDP networking
- zlib / CRC32-related support

---

## Requirements

You need a PSP capable of running homebrew.

To compile the project, you need a working **PSPDEV / PSPSDK** environment and the required libraries.

Example:

```bash
psp-pacman -Sy psp-cfw-sdk libintrafont
```

You can verify some required files with:

```bash
ls /usr/local/pspdev/psp/include/kubridge.h
ls /usr/local/pspdev/psp/include/libpspexploit.h

ls /usr/local/pspdev/psp/lib/libpspkubridge.a
ls /usr/local/pspdev/psp/lib/libpspexploit.a
ls /usr/local/pspdev/psp/lib/libintrafont.a
```

---

## Building

Clone or download the repository and run:

```bash
make clean
make
```

A successful build should generate:

```text
ADShare.elf
PARAM.SFO
EBOOT.PBP
```

The individual C++ source files are compiled into object files inside `src/`.

For example:

```text
src/main.o
src/app.o
src/state.o
src/utils.o
src/hardware.o
src/ui.o
src/adhoc.o
src/data_protocol.o
src/transfer.o
src/browser.o
src/screens.o
```

---

## XMB Icon

If an `ICON0.PNG` file is present in the project root and enabled by the Makefile, it can be embedded directly into the generated `EBOOT.PBP`.

Recommended layout:

```text
PSP-ADShare/
├── ICON0.PNG
├── Makefile
└── src/
```

A common PSP `ICON0.PNG` size is:

```text
144 x 80
```

---

## Installing on PSP

Copy the generated `EBOOT.PBP` to a folder under:

```text
ms0:/PSP/GAME/
```

For example:

```text
ms0:/PSP/GAME/ADShare/EBOOT.PBP
```

The internal build target may still use the name `ADShare` for compatibility and simplicity, while the application itself is presented as **ADShare++**.

---

## Testing With Two PSPs

Install the **same ADShare++ build** on both consoles.

Then:

1. Enable WLAN on both PSP systems.
2. Open ADShare++ on both consoles.
3. Make sure both use the same Ad Hoc channel.
4. Start Ad Hoc on both systems.
5. Wait for console discovery.
6. Select the other PSP.
7. Choose a file to send.
8. Accept the request on the receiving PSP.
9. Choose the destination folder.
10. Start the transfer.
11. Wait for completion.

Using the same protocol version on both PSP systems is strongly recommended.

---

## Original PSP-ADShare

ADShare++ is based on my official **PSP-ADShare** code.

The original project is important because it contains the first implementation and the development history of the application.

ADShare++ should be considered the **modular, improved C++ evolution** of that codebase rather than an unrelated rewrite.

The main concepts remain the same:

- PSP-to-PSP communication
- Native Ad Hoc networking
- Direct file transfer
- PDP-based protocol
- Bilingual interface
- Hardware information
- Real PSP compatibility

The difference is mainly in the internal architecture, maintainability, and future development possibilities.

---

## Project Status

ADShare++ is under active development and is tested on real PSP hardware.

The original experimental codebase has now been reorganized into a modular C++ project, making future development significantly easier.

Possible areas for future work include:

- Further protocol optimization
- Transfer-speed improvements
- Additional UI improvements
- Better error reporting
- More hardware identification data
- Additional file-management features
- Further reduction of shared global state

Suggestions, bug reports, pull requests, code improvements, and testing results are welcome.

---

## Credits

### Development

**welabsdev**

### Original project

**PSP-ADShare**

ADShare++ is the modular C++ evolution of my original PSP-ADShare implementation.

### Inspiration

**Adhoc File Transfer PSP**

Original PSP Ad Hoc file-transfer homebrew released in 2010.

### Tools and libraries

Thanks to the developers and contributors of:

- PSPSDK
- PSPDEV
- OSLib
- intraFont
- KUBridge
- LibPspExploit
- The PSP homebrew development community

---

## Bug Reports

If you test ADShare++ on real PSP hardware and encounter a problem, useful information includes:

```text
PSP model:
Firmware / CFW:
ADShare++ version:
Ad Hoc channel:
Sender model:
Receiver model:
File type:
File size:
Save destination:
Error code:
```

Testing on different PSP models and revisions is especially useful for improving compatibility.

---

## License

Check the repository license for the terms that apply to this project and to any third-party libraries used by ADShare++.

---

**ADShare++**  
Direct PSP-to-PSP Ad Hoc File Sharing

Modular C++ evolution of **PSP-ADShare**

Developed by **welabsdev**
