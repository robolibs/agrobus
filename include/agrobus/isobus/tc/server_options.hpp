#pragma once

#include <agrobus/net/types.hpp>
#include <datapod/datapod.hpp>

namespace agrobus::isobus::tc {
    using namespace agrobus::net;

    // ─── TC Server enumerations ──────────────────────────────────────────────────

    enum class ObjectPoolActivationError : u8 {
        NoErrors = 0x00,
        ThereAreErrorsInTheDDOP = 0x01,
        TaskControllerRanOutOfMemoryDuringActivation = 0x02,
        AnyOtherError = 0x04,
        DifferentDDOPExistsWithSameStructureLabel = 0x08
    };

    enum class ObjectPoolDeletionErrors : u8 {
        ObjectPoolIsReferencedByTaskData = 0,
        ServerCannotCheckForObjectPoolReferences = 1,
        ErrorDetailsNotAvailable = 0xFF
    };

    enum class ObjectPoolErrorCodes : u8 {
        NoErrors = 0x00,
        MethodOrAttributeNotSupported = 0x01,
        UnknownObjectReference = 0x02,
        AnyOtherError = 0x04,
        DDOPWasDeletedFromVolatileMemory = 0x08
    };

    enum class ProcessDataCommands : u8 {
        TechnicalCapabilities = 0x00,
        DeviceDescriptor = 0x01,
        RequestValue = 0x02,
        Value = 0x03,
        MeasurementTimeInterval = 0x04,
        MeasurementDistanceInterval = 0x05,
        MeasurementMinimumWithinThreshold = 0x06,
        MeasurementMaximumWithinThreshold = 0x07,
        MeasurementChangeThreshold = 0x08,
        PeerControlAssignment = 0x09,
        SetValueAndAcknowledge = 0x0A,
        Acknowledge = 0x0D,
        Status = 0x0E,
        ClientTask = 0x0F
    };

    enum class ServerOptions : u8 {
        SupportsDocumentation = 0x01,
        SupportsTCGEOWithoutPositionBasedControl = 0x02,
        SupportsTCGEOWithPositionBasedControl = 0x04,
        SupportsPeerControlAssignment = 0x08,
        SupportsImplementSectionControl = 0x10
    };

    enum class ProcessDataAcknowledgeErrorCodes : u8 {
        NoError = 0x00,
        ElementNotSupportedByThisDevice = 0x01,
        ValueIsOutsideValidRange = 0x02,
        NoProcessingResourcesAvailable = 0x03,
        DDEXValueNotSupported = 0x04
    };

    enum class TCServerState : u8 { Disconnected, WaitForClients, Active };

    // Bitwise operators for ServerOptions
    inline u8 operator|(ServerOptions a, ServerOptions b) { return static_cast<u8>(a) | static_cast<u8>(b); }

} // namespace agrobus::isobus::tc
