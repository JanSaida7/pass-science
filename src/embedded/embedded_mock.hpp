#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

// Prevent Win32 API macro collision
#if defined(DeviceCapabilities)
#undef DeviceCapabilities
#endif

namespace workbench::embedded {

/**
 * Device operational status.
 */
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    CommunicationTimeout,
    Error
};

/**
 * Telemetry snapshot emitted by an embedded device.
 */
struct DeviceTelemetry {
    std::string device_id;
    std::string timestamp;
    double temperature_C{0.0};
    double voltage_V{0.0};
    uint64_t packet_sequence{0};
    std::string state_summary;
    std::map<std::string, std::string> attributes;
    bool is_simulated{true};
};

/**
 * Hardware capability reporting.
 */
struct HardwareCapabilities {
    std::string device_id;
    std::string device_type; // "MCU", "FPGA", "JTAG_DEBUGGER"
    std::string model_name;
    std::string host_interface; // e.g., "USB CDC-ACM (115200 8N1)"
    std::string logic_voltage;   // e.g., "3.3V LVCMOS", "1.8V/1.2V Dual"
    std::string connector_type;  // e.g., "USB-C", "10-pin 0.05\" Cortex Debug"
    std::vector<std::string> feature_list;
    bool is_simulated{true};
};

/**
 * Base polymorphic adapter interface for embedded devices.
 */
class DeviceAdapter {
public:
    virtual ~DeviceAdapter() = default;
    virtual std::string get_id() const = 0;
    virtual std::string get_type() const = 0;
    virtual std::string get_name() const = 0;
    virtual ConnectionState get_state() const = 0;
    virtual HardwareCapabilities get_capabilities() const = 0;
    virtual core::Result<bool> connect() = 0;
    virtual core::Result<bool> disconnect() = 0;
    virtual core::Result<DeviceTelemetry> read_telemetry() = 0;
    virtual void inject_fault(bool enable) = 0;
    virtual bool is_fault_injected() const = 0;
};

/**
 * Host Embedded Workbench Manager coordinating all connected hardware devices.
 */
class EmbeddedWorkbenchManager {
public:
    EmbeddedWorkbenchManager();

    /**
     * Discovers all available devices on the host bus.
     */
    std::vector<HardwareCapabilities> discover_devices() const;

    /**
     * Retrieves pointer to a specific adapter.
     */
    DeviceAdapter* get_adapter(const std::string& device_id);

    /**
     * Connects all discovered devices.
     */
    void connect_all();

    /**
     * Disconnects all devices safely.
     */
    void disconnect_all();

    /**
     * Polls telemetry from all connected devices.
     */
    std::vector<core::Result<DeviceTelemetry>> poll_all_telemetry();

private:
    std::map<std::string, std::unique_ptr<DeviceAdapter>> adapters_;
};

} // namespace workbench::embedded
