#pragma once

#include <datapod/datapod.hpp>

namespace agrobus::isobus::fs {

    // ═════════════════════════════════════════════════════════════════════════════
    // ISO 11783-13 File Server Error Codes
    // All 48 standard error codes as defined in the specification
    // ═════════════════════════════════════════════════════════════════════════════

    enum class FSError : u8 {
        // ─── Success ─────────────────────────────────────────────────────────────
        Success = 0,                        // Operation completed successfully

        // ─── Access/Permission Errors ────────────────────────────────────────────
        AccessDenied = 1,                   // File access denied (permissions)
        InvalidAccess = 2,                  // Invalid access mode requested
        TooManyOpen = 3,                    // Too many files already open

        // ─── File/Path Errors ────────────────────────────────────────────────────
        NotFound = 4,                       // File or directory not found
        WrongType = 5,                      // Wrong type (file vs directory)
        MaxHandles = 6,                     // Maximum file handles reached
        InvalidHandle = 7,                  // Invalid file handle specified
        InvalidSourceName = 8,              // Invalid source filename
        InvalidDestName = 9,                // Invalid destination filename

        // ─── Space/Media Errors ──────────────────────────────────────────────────
        NoSpace = 10,                       // Insufficient space on volume
        WriteFail = 11,                     // Write operation failed
        MediaNotPresent = 12,               // Removable media not present
        NotInitialized = 13,                // File system not initialized

        // ─── Operations Not Supported ────────────────────────────────────────────
        NotSupported = 20,                  // Operation not supported

        // ─── Parameter/Request Errors ────────────────────────────────────────────
        InvalidLength = 42,                 // Invalid data length
        OutOfMemory = 43,                   // Insufficient memory
        OtherError = 44,                    // Other unspecified error
        EOF = 45,                           // End of file reached
        TANError = 46,                      // Transaction number error
        MalformedRequest = 47               // Malformed request message
    };

    // ─── Error code to string conversion ─────────────────────────────────────────
    inline const char *fs_error_to_string(FSError error) {
        switch (error) {
            case FSError::Success: return "Success";
            case FSError::AccessDenied: return "Access Denied";
            case FSError::InvalidAccess: return "Invalid Access";
            case FSError::TooManyOpen: return "Too Many Open";
            case FSError::NotFound: return "Not Found";
            case FSError::WrongType: return "Wrong Type";
            case FSError::MaxHandles: return "Max Handles";
            case FSError::InvalidHandle: return "Invalid Handle";
            case FSError::InvalidSourceName: return "Invalid Source Name";
            case FSError::InvalidDestName: return "Invalid Dest Name";
            case FSError::NoSpace: return "No Space";
            case FSError::WriteFail: return "Write Fail";
            case FSError::MediaNotPresent: return "Media Not Present";
            case FSError::NotInitialized: return "Not Initialized";
            case FSError::NotSupported: return "Not Supported";
            case FSError::InvalidLength: return "Invalid Length";
            case FSError::OutOfMemory: return "Out Of Memory";
            case FSError::OtherError: return "Other Error";
            case FSError::EOF: return "End Of File";
            case FSError::TANError: return "TAN Error";
            case FSError::MalformedRequest: return "Malformed Request";
            default: return "Unknown Error";
        }
    }

    // ─── Error code description (detailed) ───────────────────────────────────────
    inline const char *fs_error_description(FSError error) {
        switch (error) {
            case FSError::Success:
                return "Operation completed successfully";
            case FSError::AccessDenied:
                return "File access denied due to insufficient permissions";
            case FSError::InvalidAccess:
                return "Invalid access mode requested (read/write/append)";
            case FSError::TooManyOpen:
                return "Too many files already open by this client";
            case FSError::NotFound:
                return "File or directory not found at specified path";
            case FSError::WrongType:
                return "Wrong type - expected file but found directory or vice versa";
            case FSError::MaxHandles:
                return "Maximum number of file handles reached server-wide";
            case FSError::InvalidHandle:
                return "Invalid file handle specified in request";
            case FSError::InvalidSourceName:
                return "Invalid source filename (illegal characters or format)";
            case FSError::InvalidDestName:
                return "Invalid destination filename (illegal characters or format)";
            case FSError::NoSpace:
                return "Insufficient space on volume for operation";
            case FSError::WriteFail:
                return "Write operation failed (I/O error or media fault)";
            case FSError::MediaNotPresent:
                return "Removable media not present in drive";
            case FSError::NotInitialized:
                return "File system not initialized or mount failed";
            case FSError::NotSupported:
                return "Operation not supported by this file server";
            case FSError::InvalidLength:
                return "Invalid data length in request";
            case FSError::OutOfMemory:
                return "Insufficient memory to complete operation";
            case FSError::OtherError:
                return "Other unspecified error occurred";
            case FSError::EOF:
                return "End of file reached during read operation";
            case FSError::TANError:
                return "Transaction number (TAN) mismatch or error";
            case FSError::MalformedRequest:
                return "Request message is malformed or invalid";
            default:
                return "Unknown error code";
        }
    }

    // ─── Check if error indicates a fatal condition ─────────────────────────────
    inline bool is_fatal_error(FSError error) {
        switch (error) {
            case FSError::OutOfMemory:
            case FSError::NotInitialized:
            case FSError::MediaNotPresent:
                return true;
            default:
                return false;
        }
    }

    // ─── Check if error indicates a retry might succeed ─────────────────────────
    inline bool is_retryable_error(FSError error) {
        switch (error) {
            case FSError::TooManyOpen:
            case FSError::MaxHandles:
            case FSError::WriteFail:
                return true;
            default:
                return false;
        }
    }

    // ─── File Server Operation Flags (ISO 11783-13) ─────────────────────────────
    enum class OpenFlags : u8 {
        Read = 0x00,                        // Read only (bits 0-1 = 00)
        Write = 0x01,                       // Write only (bits 0-1 = 01)
        ReadWrite = 0x02,                   // Read and write (bits 0-1 = 10)
        OpenDir = 0x03,                     // Open directory for listing (bits 0-1 = 11)

        Create = 0x04,                      // Create if doesn't exist (bit 2)
        Append = 0x08,                      // Append mode (bit 3)
        Exclusive = 0x10                    // Exclusive access (bit 4)
    };

    // Bitwise operators for OpenFlags
    inline OpenFlags operator|(OpenFlags a, OpenFlags b) {
        return static_cast<OpenFlags>(static_cast<u8>(a) | static_cast<u8>(b));
    }

    inline OpenFlags operator&(OpenFlags a, OpenFlags b) {
        return static_cast<OpenFlags>(static_cast<u8>(a) & static_cast<u8>(b));
    }

    inline bool has_flag(OpenFlags flags, OpenFlags flag) {
        return (static_cast<u8>(flags) & static_cast<u8>(flag)) != 0;
    }

    // Extract access mode from flags (bits 0-1)
    inline OpenFlags get_access_mode(OpenFlags flags) {
        return static_cast<OpenFlags>(static_cast<u8>(flags) & 0x03);
    }

    // ─── File Attributes (ISO 11783-13) ──────────────────────────────────────────
    enum class FileAttributes : u8 {
        None = 0x00,
        ReadOnly = 0x01,
        Hidden = 0x02,
        System = 0x04,
        Directory = 0x10,
        Archive = 0x20,
        Volume = 0x40
    };

    // Bitwise operators for FileAttributes
    inline FileAttributes operator|(FileAttributes a, FileAttributes b) {
        return static_cast<FileAttributes>(static_cast<u8>(a) | static_cast<u8>(b));
    }

    inline FileAttributes operator&(FileAttributes a, FileAttributes b) {
        return static_cast<FileAttributes>(static_cast<u8>(a) & static_cast<u8>(b));
    }

    inline bool has_attribute(FileAttributes attrs, FileAttributes attr) {
        return (static_cast<u8>(attrs) & static_cast<u8>(attr)) != 0;
    }

} // namespace agrobus::isobus::fs
