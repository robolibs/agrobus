# agrobus

Modern C++20 ISO 11783 (ISOBUS) + J1939 + NMEA2000 networking stack with a fluent, application-first API.

Immense thanks to [AgIsoStack++](https://github.com/Open-Agriculture/AgIsoStack-plus-plus) for literally everything, mostly reference implementation and amazing documentation (way better than Agricultural Industry Electronics Foundation). Without that library, this would not have happened. A big part of this library is best described as a nicer API on top of AgIsoStack - the hard protocol truths, edge cases, and real-world behavior were learned from them first. See [ACKNOWLEDGMENTS](./ACKNOWLEDGMENTS.md). Please if you want to go to the original version, links are in acknowledgment :)

## Overview

`agrobus` is a header-first CAN protocol stack focused on ISO 11783 (ISOBUS) and related ecosystems used on agricultural and marine equipment.
It provides a high level message/transport/network abstraction on top of a raw CAN endpoint, while keeping the API close to the underlying
standards (PGNs, source/destination addressing, address-claiming, TP/ETP, etc.).

The core idea is: application code should read like intent ("send this PGN", "subscribe to that PGN", "start address claiming") while the
library handles the repetitive protocol machinery: frame encoding, multi-packet reassembly, session tracking, and common timing rules.

The project is organized into four sub-namespaces:
- **`agrobus::net`** - CAN frame layer, network management (IsoNet), address claiming, transport protocols (TP/ETP/FastPacket), utilities
- **`agrobus::j1939`** - J1939/SAE protocol services: diagnostics, heartbeat, acknowledgment, speed/distance, time/date, engine, transmission
- **`agrobus::isobus`** - ISO 11783 application layer: Virtual Terminal, Task Controller, Sequence Control, implement messages, file server
- **`agrobus::nmea`** - NMEA2000: definitions, interface parsing/generation, GNSS helpers

### Architecture Diagrams

High level data flow (receive path):

```
+-------------------------------------------------------------------------+
|                                 Application                               |
|  - register_pgn_callback(PGN, fn)                                         |
|  - protocol modules (VT/TC/TIM/FS/Diagnostics/...)                        |
+-------------------------------+-------------------------------------------+
                                | on_message / callbacks
                                v
+-------------------------------------------------------------------------+
|                          agrobus::net::IsoNet                              |
|  - endpoint polling / send_frame                                          |
|  - address claiming + CF tracking                                         |
|  - routes TP/ETP/FastPacket frames                                        |
|  - dispatches Message{pgn,data,src,dst,prio,ts}                           |
+---------------+-----------------------+-----------------------+-----------+
                |                       |                       |
                v                       v                       v
      +----------------+      +-------------------+     +-------------------+
      | AddressClaimer |      | TransportProtocol |     | FastPacketProtocol|
      | (ISO11783-5)   |      | + ETP             |     | (NMEA2000)        |
      +-------+--------+      +-------+-----------+     +----------+--------+
              |                       |                            |
              +-----------+-----------+-------------+--------------+
                          v                         v
               +------------------+    +-------------------------+
               |   Frame / ID     |    | wirebit::CanEndpoint    |
               | (29-bit CAN ID)  |    | (driver abstraction)    |
               +------------------+    +-------------------------+
```

Library surface (include layout):

```
include/agrobus.hpp
+-- include/agrobus/
    |-- net/           (types, Frame, Message, IsoNet, address claiming,
    |                   transport TP/ETP/FastPacket, utilities, NIU)
    |-- j1939/         (engine, transmission, diagnostics, heartbeat,
    |                   acknowledgment, speed/distance, time/date, ...)
    |-- isobus/        (top-level: TIM, functionalities, auxiliary, guidance, ...)
    |   |-- vt/        (Virtual Terminal client/server + object pool)
    |   |-- tc/        (Task Controller client/server, DDOP, geo, peer control)
    |   |-- sc/        (Sequence Control master/client)
    |   |-- implement/ (tractor/implement messages and helpers)
    |   +-- fs/        (file server connection/properties)
    +-- nmea/          (NMEA2000 definitions, interface, GNSS, serial)
```

## Installation

Repository: https://github.com/robolibs/agrobus

### Quick Start (CMake FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
  agrobus
  GIT_REPOSITORY https://github.com/robolibs/agrobus
  GIT_TAG main
)
FetchContent_MakeAvailable(agrobus)

target_link_libraries(your_target PRIVATE agrobus::agrobus)
```

Notes:
- The project is C++20.
- Dependencies are pulled via `FetchContent` as declared in `PROJECT`.

### Complete Development Environment (Nix + Direnv + Devbox)

This repository includes `devbox.json` and is compatible with direnv.

1) Install Nix:

```bash
curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix | sh -s -- install
```

2) Install direnv:

```bash
sudo apt install direnv
eval "$(direnv hook bash)"
```

3) Install Devbox:

```bash
curl -fsSL https://get.jetpack.io/devbox | bash
```

4) Enter the environment:

```bash
direnv allow
```

## Usage

The central entrypoint is `agrobus::net::IsoNet`. You create an instance, attach a wirebit CAN endpoint (either default vcan0 or a custom one),
create one or more internal control functions, start address claiming, then send/receive PGNs.

### Basic Usage

```cpp
#include <agrobus.hpp>

using namespace agrobus::net;

int main() {
    IsoNet net;

    // Option 1: default vcan0 endpoint
    net.set_default_endpoint();

    // Option 2: custom wirebit endpoint
    // auto link = std::make_shared<wirebit::SocketCanLink>(...);
    // wirebit::CanEndpoint ep(link, wirebit::CanConfig{.bitrate = 250000}, 0);
    // net.set_endpoint(0, &ep);

    // Create an internal control function (ECU) and claim an address.
    auto icf = net.create_internal(Name{}, 0).value();
    net.start_address_claiming();

    // Subscribe to a PGN.
    net.register_pgn_callback(PGN_ADDRESS_CLAIMED, [](const Message &msg) {
        (void)msg;
    });

    // Send a message (single frame / TP / ETP chosen automatically).
    dp::Vector<u8> payload;
    payload.push_back(0xFF);
    net.send(0x00EA00 /* Request PGN */, payload, icf);

    while (true) {
        net.update(10);
    }
}
```

### Advanced Usage

Transport selection rules are built-in:
- <= 8 bytes: single CAN frame
- 9..1785 bytes: ISO 11783 / J1939 TP
- > 1785 bytes: ISO 11783 / J1939 ETP (connection-mode only)
- Optional: NMEA2000 fast packet for registered PGNs

You can register NMEA2000 fast-packet PGNs:

```cpp
net.register_fast_packet_pgn(129029); // Example N2K GNSS position PGN
```

And you can access the protocol engines directly for custom integrations:

```cpp
auto &tp = net.transport_protocol();
auto &etp = net.extended_transport_protocol();
auto &fp = net.fast_packet_protocol();
```

## Features

- **Network manager** - Port-based CAN endpoint integration, address claiming, CF tracking, and PGN-based dispatch.
- **Transport (TP/ETP)** - Automatic segmentation/reassembly for multi-packet ISO 11783 / J1939 messages.
- **NMEA2000 fast packet** - Optional fast-packet support for registered PGNs.
- **J1939 protocol modules** - Diagnostics (DM1-DM13), heartbeat, acknowledgment, PGN request, speed/distance, time/date, engine/transmission.
- **Virtual Terminal (VT)** - Client/server support with object pool utilities.
- **Task Controller (TC)** - Client/server, DDOP helpers, DDI database, geo helpers, and peer control.
- **Sequence Control (SC)** - Master/client components and types.
- **Implement messages** - Tractor/implement speed, lighting, guidance, aux valve, machine speed commands, facilities.
- **NMEA2000** - Definitions, interface parsing/generation, serial GNSS helpers.
- **Integration tests + examples** - Large set of real usage examples under `examples/` and protocol tests under `test/`.

## Building and Testing

This repo ships a `Makefile` wrapper that selects a build system (CMake by default).

Common targets:

```bash
make config
make build
make test
```

Notes:
- `make build` runs `clang-format` over `./include` and `./src` before compiling.
- CMake options are driven by `PROJECT` and exposed as `AGROBUS_BUILD_EXAMPLES`, `AGROBUS_ENABLE_TESTS`, and `AGROBUS_BIG_TRANSFER`.

## Dependency Graph

`agrobus` composes a small set of external libraries.
The `PROJECT` file is the source of truth for versions.

External dependencies (library):
- `echo` - logging and categories used throughout the stack
- `datapod` - containers and utilities (`dp::Vector`, `dp::Map`, `dp::Array`, `dp::Optional`)
- `optinum` - optional helpers and small utilities (varies by module)
- `wirebit` - CAN driver abstraction (`wirebit::CanEndpoint`) and CAN primitives
- `concord` - coordinate transforms and geo utilities

Test dependency:
- `doctest` - unit tests in `test/`

Internal dependency map (conceptual):

```
net (core types, network, transport, utilities)
  |-- j1939 (diagnostics, engine, heartbeat, ...)
  |-- isobus
  |     |-- vt (Virtual Terminal)
  |     |-- tc (Task Controller)
  |     |-- sc (Sequence Control)
  |     |-- implement (tractor/implement messages)
  |     +-- fs (file server)
  +-- nmea (NMEA2000)
```

## Core Concepts

This section gives names to the key building blocks, so the rest of the codebase is easier to navigate.

### CAN Frame vs Message

- `Frame` is a single CAN frame: a 29-bit identifier and up to 8 data bytes.
- `Message` is an arbitrary length payload tagged with routing metadata (PGN, src, dst, priority).
- `IsoNet` turns incoming frames into messages either directly (single frame) or after reassembly (TP/ETP/FastPacket).

### PGN / Priority / Addressing

- A PGN is the high level message type identifier in J1939 / ISO 11783.
- Messages are either broadcast or destination-specific.
- For destination-specific messages the destination address is encoded in the 29-bit identifier.

### Control Functions (CF)

`agrobus` models the bus participants as Control Functions:
- `InternalCF` represents an ECU owned by your application.
- `PartnerCF` represents a remote ECU that you care about and optionally filter by NAME.

Control functions have a NAME (64-bit) and an address (source address, SA).

### Address Claiming

On an ISOBUS/J1939 network, devices claim an address using their NAME and a priority comparison.
This repo includes an address claimer that participates in that process and updates control function state.

From the app perspective:
- create one or more `InternalCF`
- attach a CAN endpoint to a port
- call `start_address_claiming()`
- poll `update()`

### Ports and Endpoints

The library supports multiple ports so you can represent:
- multi-channel gateways
- multi-bus simulation
- NIU style bridging

Each port is attached to a `wirebit::CanEndpoint` which provides:
- `send_can(can_frame)`
- `recv_can(can_frame)`

### Transport Protocols

J1939 / ISO 11783 defines multi-packet transport on top of CAN:
- TP (up to 1785 bytes)
- ETP (extended, for larger payloads; connection-mode only)

`IsoNet::send()` automatically picks:
- single frame for <= 8 bytes
- TP for 9..1785
- ETP for > 1785 when destination-specific

The receive side reassembles the payload and emits a single `Message`.

### NMEA2000 Fast Packet

NMEA2000 uses a different segmentation scheme called fast packet.
`agrobus` includes a fast packet engine that can be enabled and tied to a set of PGNs you register.

### Events and Callbacks

There are two complementary ways to consume messages:
- `IsoNet::on_message` - stream of all decoded messages
- `IsoNet::register_pgn_callback(pgn, fn)` - PGN specific callbacks

Both are synchronous callbacks fired in `IsoNet::update()`.

## Protocol Coverage

This codebase intentionally spans multiple parts of ISO 11783 plus a subset of J1939 and NMEA2000.

### Implemented Areas (Highlights)

- **Core J1939/ISOBUS routing**: identifier encode/decode, frame/message model
- **Network layer**: address claiming, CF tracking, PGN dispatch, bus load tracking
- **Transport**: TP + ETP session handling, plus optional NMEA2000 fast packet
- **Virtual Terminal (VT)**: object pool modeling, client/server utilities, state tracking
- **Task Controller (TC)**: client/server, DDOP helpers, DDI database, geo helpers, peer control
- **Diagnostics**: DM1/DM2/DM5/DM13 handling, DTC management, suspend/resume
- **File Server**: file transfer protocol helpers and FS connection/properties types
- **Sequence Control (SC)**: master/client types and state machine scaffolding
- **NMEA2000**: definitions, interface parsing/generation, GNSS helpers

## Module Tour

If you are reading the code, these are the most useful entrypoints.

### `include/agrobus.hpp`

The umbrella include that exposes the full stack in one header.

### `include/agrobus/net/`

- `types.hpp` - fixed width integer aliases and common scalar types
- `pgn.hpp` and `pgn_defs.hpp` - PGN types and common definitions
- `name.hpp` - J1939 NAME packing/unpacking and helpers
- `identifier.hpp` - 29-bit identifier encode/decode (priority, PGN, src, dst)
- `frame.hpp` - CAN frame wrapper
- `message.hpp` - decoded message container for arbitrary-length payloads
- `error.hpp` - error codes and `Result<T>` wrapper
- `network_manager.hpp` - IsoNet: the central orchestrator, owns transport engines, claimers, callbacks
- `address_claimer.hpp` - address claiming state machine and timing
- `control_function.hpp` - common CF types and state
- `internal_cf.hpp` - internal ECU representation
- `partner_cf.hpp` - partner discovery by NAME filtering
- `working_set.hpp` - ISOBUS working set modeling with 100ms member message timing
- `tp.hpp` / `etp.hpp` - transport protocol connection management
- `fast_packet.hpp` - NMEA2000 fast packet segmentation/reassembly
- `eth_can.hpp` - Ethernet-CAN bridge integration point

### `include/agrobus/j1939/`

- `engine.hpp` / `transmission.hpp` - engine and transmission parameter messages
- `diagnostic.hpp` / `dm_memory.hpp` - DM1/DM2/DM5/DM13, DTC management, suspend/resume
- `heartbeat.hpp` - periodic heartbeat with timeout detection
- `acknowledgment.hpp` - ACK/NACK handling
- `pgn_request.hpp` / `request2.hpp` - PGN request protocol
- `speed_distance.hpp` - wheel/ground speed and distance
- `time_date.hpp` / `language.hpp` - time/date and localization messages

### `include/agrobus/isobus/`

- `vt/` - Virtual Terminal: object definitions, pool management, client/server, state tracking
- `tc/` - Task Controller: client/server, DDOP modeling, DDI database, geo helpers, peer control
- `sc/` - Sequence Control: master/client components and types
- `implement/` - Tractor/implement messages: lighting, guidance, speed/distance, facilities, aux valves
- `fs/` - File server: connection and properties helpers
- `tim.hpp` / `functionalities.hpp` / `auxiliary.hpp` / `guidance.hpp` - top-level protocol helpers

### `include/agrobus/nmea/`

- `definitions.hpp` - NMEA2000 PGN definitions
- `interface.hpp` - parser/generator utilities
- `n2k_management.hpp` - N2K network management
- `position.hpp` - GNSS position types
- `serial_gnss.hpp` - serial GNSS helpers

## Safety and Correctness

Industrial networks are harsh: you will see missing packets, reboots, and bad actors.
The library contains a mix of defensive decoding and explicit state tracking.

Key behaviors:
- Default values for missing fields in message decoders (0xFF/0xFFFF patterns where appropriate)
- Strict size bounds for TP/ETP payloads
- Address violation detection in IsoNet (re-assert address claim when another device uses our SA)
- RTxD delay on address re-claim after contention loss
- DM13 suspend duration tracking with auto-resume

## Performance Notes

This project is focused on low overhead message routing.
Some choices you will see:
- stack-friendly fixed arrays for CAN frame payloads
- vector-based payloads only after multi-packet reassembly
- minimal allocations on the single-frame path

SIMD flags can be enabled/disabled via `AGROBUS_ENABLE_SIMD` when building with CMake.

## Testing

Tests live under `test/` and use doctest.
They cover:
- core encoding/decoding (PGN/ID/frame/message)
- TP/ETP session behavior and timers
- VT pool validation and end-to-end flows
- TC DDOP correctness and end-to-end flows
- NMEA2000 parsing and batch utilities
- diagnostics DM1/DM13 suspend/resume
- heartbeat timeout detection
- address claim contention and RTxD delay

To run everything:

```bash
make config
make test
```

## Project Layout

Top-level highlights:
- `include/` - public headers (`agrobus.hpp` umbrella + `agrobus/` tree)
- `examples/` - example executables (picked up automatically by CMake)
- `test/` - doctest test executables (picked up automatically by CMake)
- `PROJECT` - name/version/dependency manifest consumed by CMake
- `Makefile` - build system wrapper

## Contributing

Contributions are welcome, especially:
- protocol conformance fixes vs ISO 11783 / J1939 specs
- interoperability testing reports with real ECUs
- additional tests covering edge cases and timing

Practical guidance:
- keep public APIs in `include/`
- prefer small, testable message encoders/decoders
- add a focused example when adding a large feature

## Troubleshooting

### Build fails fetching dependencies

Dependencies are pulled via CMake FetchContent.
If you are behind a proxy or have restricted outbound access:
- set `CMAKE_TLS_VERIFY=ON` and configure proxy env vars
- prefer a local dependency mirror

### No messages received

Check these common integration issues:
- your `wirebit::CanEndpoint` returns frames in extended format (29-bit IDs)
- you are calling `IsoNet::update()` frequently enough
- your internal CF has successfully claimed an address (not NULL)

### TP/ETP sessions time out

Multi-packet transfers require periodic updates to progress timers.
Ensure `update(elapsed_ms)` is called with realistic timing, and that your endpoint send/recv is non-blocking.

## Versioning

Project version is defined in `PROJECT`.

## License

MIT License - see `LICENSE` for details.

## Acknowledgments

Made possible thanks to the projects and references listed in `ACKNOWLEDGMENTS.md`.
