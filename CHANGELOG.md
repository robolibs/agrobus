# Changelog

## [0.0.20] - 2026-02-10

### <!-- 0 -->⛰️  Features

- Add comprehensive demos for new diagnostic and VT features
- Add comprehensive tests for new DM codes and fix bugs (ISO 11783-12 Phase 4)
- Implement VT Version 6 advanced features
- Add dynamic VT object pool swapping
- Implement missing diagnostic messages (DM4, DM6-8, DM12, DM21, DM23)
- Complete ISO 11783-8 powertrain implementation

## [0.0.19] - 2026-02-10

### <!-- 0 -->⛰️  Features

- Add persistent NVM storage to VT server
- Add comprehensive NIU demonstration example (ISO 11783-4 Phase 5)
- Implement specialized NIU types with address translation (ISO 11783-4 Phase 5)
- Enhance NIU Filter Database with NAME filtering, rate limiting, and persistence (ISO 11783-4 Phase 5)
- Add diagnostic extended features demo (ISO 11783-12 Phase 4)
- Implement DM20 Monitor Performance Ratios (ISO 11783-12 Phase 4)
- Implement DM25 Freeze Frame capture and retrieval (ISO 11783-12 Phase 4)
- Add VT advanced features demo (ISO 11783-6 Phase 3)
- Add language negotiation to VT client (ISO 11783-6/7 Phase 3)
- Add VT advanced object types and alarm priority stack (ISO 11783-6 Phase 3)
- Complete Phase 2 - File Server implementation (ISO 11783-13)
- Implement FileClient class (ISO 11783-13)
- Implement Volume Status state machine (ISO 11783-13 Section 7.7)
- Add directory operations to file server (ISO 11783-13)
- Implement enhanced FileServer with full TAN support (ISO 11783-13)
- Add comprehensive file server error codes and types (ISO 11783-13)
- Implement TractorECU class (ISO 11783-9 Phase 1)
- Add three new complex TC client examples
- Add RTxD delay for address re-claiming and DM13 suspend logic
- Reinit
- Integrate PHTG data for enhanced boom section control
- Feat: Implement GNSS authentication and add new ISOBUS examples
- Implement a comprehensive C++ serial communication library
- A very detailed example 'main.cpp'
- Add NMEA 0183 and PHTG to CAN and serial bridges
- Add SocketCAN ISOBUS example and fix library link order
- Add AgIsoStack as a dependency
- Init

### <!-- 1 -->🐛 Bug Fixes

- Fix test failures in file_server, vt_advanced, and tractor_ecu tests
- Use correct parameter counts for MacroCommand tests
- Maintain power during minimum hold period in shutdown
- Change loop variable to u16 to prevent overflow in vt_advanced_test
- Use vcan0 with dual-network setup for file_server_test
- Fix bitfield_test template issues and test assertions
- Fix bus_load and scheduler test failures
- Fix API mismatches and test errors
- Fix test compilation errors and disable outdated tests
- Resolve compilation errors across codebase

### <!-- 2 -->🚜 Refactor

- Rename library to agrobus
- Introduce event-driven serial communication API
- Rename project from isobus to tractor
- Cherry picking
- Refactor build system for clearer linker control

### <!-- 3 -->📚 Documentation

- Add initial documentation and acknowledgments

### <!-- 6 -->🧪 Testing

- Add comprehensive ISOBUS guidance message tests
- Add J1939 message protocol tests (Phase 6)
- Add net utilities tests (Phase 6)
- Add maintain_power and shortcut_button tests
- Add comprehensive J1939 language/units message tests (Phase 6.2)
- Add comprehensive J1939 heartbeat state machine tests (Phase 6.2)
- Add comprehensive J1939 transmission message tests (Phase 6.2)
- Add comprehensive J1939 time/date message tests (Phase 6.2)
- Add comprehensive J1939 engine message tests (Phase 6.2)
- Add comprehensive data_span utility tests (Phase 6.1)
- Add comprehensive bitfield utility tests (Phase 6.1)
- Add comprehensive VT Phase 3 advanced features tests
- Add comprehensive NIU tests for Phase 5 features
- Add comprehensive diagnostic extended tests (ISO 11783-12 Phase 4)
- Add comprehensive test suites for TractorECU and FileServer

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Refactor and clarify README introduction

### Build

- Update build environment variables and aliases

