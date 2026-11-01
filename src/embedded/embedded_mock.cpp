#define _CRT_SECURE_NO_WARNINGS
#include "embedded/embedded_mock.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace workbench::embedded {

static std::string get_current_iso_time() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm buf;
#if defined(_WIN32)
    gmtime_s(&buf, &in_time_t);
    ss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%SZ");
#else
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
#endif
    return ss.str();
}

/**
 * Microcontroller Mock Adapter (STM32 Cortex-M4)
 */
class McuMockAdapter : public DeviceAdapter {
public:
    McuMockAdapter() = default;

    std::string get_id() const override { return "MCU-STM32-01"; }
    std::string get_type() const override { return "Microcontroller (MCU)"; }
    std::string get_name() const override { return "STM32F401RE Nucleo-64"; }
    ConnectionState get_state() const override { return state_; }
    bool is_fault_injected() const override { return fault_injected_; }

    HardwareCapabilities get_capabilities() const override {
        HardwareCapabilities caps;
        caps.device_id = get_id();
        caps.device_type = "MCU";
        caps.model_name = get_name();
        caps.host_interface = "USB CDC-ACM Virtual COM Port (115200 8N1)";
        caps.logic_voltage = "3.3V LVCMOS (VDD = 3.3V)";
        caps.connector_type = "USB Micro-B & Morpho / Arduino Uno R3 Headers";
        caps.feature_list = {
            "ARM Cortex-M4 with FPU @ 84 MHz",
            "512 KB Flash, 96 KB SRAM",
            "12-bit ADC (16 channels, internal temp sensor)",
            "Hardware Timers & PWM generators",
            "SWD / JTAG Debug Tap Header"
        };
        caps.is_simulated = true;
        return caps;
    }

    core::Result<bool> connect() override {
        state_ = ConnectionState::Connected;
        fault_injected_ = false;
        return core::Result<bool>::Ok(true);
    }

    core::Result<bool> disconnect() override {
        state_ = ConnectionState::Disconnected;
        return core::Result<bool>::Ok(true);
    }

    void inject_fault(bool enable) override {
        fault_injected_ = enable;
        if (enable) {
            state_ = ConnectionState::CommunicationTimeout;
        } else if (state_ == ConnectionState::CommunicationTimeout) {
            state_ = ConnectionState::Connected;
        }
    }

    core::Result<DeviceTelemetry> read_telemetry() override {
        if (state_ == ConnectionState::Disconnected) {
            return core::Result<DeviceTelemetry>::Fail("Cannot read telemetry: MCU device is disconnected.");
        }
        if (fault_injected_) {
            state_ = ConnectionState::CommunicationTimeout;
            return core::Result<DeviceTelemetry>::Fail("ERR_COMM_TIMEOUT_500MS: Microcontroller UART RX buffer did not acknowledge heartbeat query. Signal line silent.");
        }

        packet_counter_++;
        DeviceTelemetry telem;
        telem.device_id = get_id();
        telem.timestamp = get_current_iso_time();
        telem.voltage_V = 3.30 + (0.02 * std::sin(packet_counter_ * 0.1));
        telem.temperature_C = 38.5 + (1.2 * std::cos(packet_counter_ * 0.05));
        telem.packet_sequence = packet_counter_;
        telem.state_summary = "SYS_OK | RTOS_RUNNING | ADC_CONV_ACTIVE";
        telem.is_simulated = true;

        telem.attributes["cpu_utilization_pct"] = "18.4";
        telem.attributes["free_heap_bytes"] = "48120";
        telem.attributes["firmware_version"] = "v2.4.1-sim";
        telem.attributes["clock_frequency_mhz"] = "84.0";

        return core::Result<DeviceTelemetry>::Ok(telem);
    }

private:
    ConnectionState state_{ConnectionState::Disconnected};
    bool fault_injected_{false};
    uint64_t packet_counter_{0};
};

/**
 * FPGA Mock Adapter (Xilinx Artix-7)
 */
class FpgaMockAdapter : public DeviceAdapter {
public:
    FpgaMockAdapter() = default;

    std::string get_id() const override { return "FPGA-ARTIX7-01"; }
    std::string get_type() const override { return "Field Programmable Gate Array (FPGA)"; }
    std::string get_name() const override { return "Xilinx Artix-7 XC7A35T (Basys 3)"; }
    ConnectionState get_state() const override { return state_; }
    bool is_fault_injected() const override { return fault_injected_; }

    HardwareCapabilities get_capabilities() const override {
        HardwareCapabilities caps;
        caps.device_id = get_id();
        caps.device_type = "FPGA";
        caps.model_name = get_name();
        caps.host_interface = "FTDI High-Speed SPI / UART Bridge (12 Mbaud)";
        caps.logic_voltage = "Dual Domain: 1.0V VCCINT Core, 3.3V/1.8V Bank VCCO";
        caps.connector_type = "USB-B micro & 4x Standard 12-pin PMOD headers";
        caps.feature_list = {
            "33,280 Logic Cells (5,200 slices)",
            "1,800 Kbits Block RAM",
            "90 DSP slices (25 x 18 multiplier)",
            "Dual 1 MSPS 12-bit On-Chip XADC",
            "Dedicated JTAG programming chain"
        };
        caps.is_simulated = true;
        return caps;
    }

    core::Result<bool> connect() override {
        state_ = ConnectionState::Connected;
        fault_injected_ = false;
        return core::Result<bool>::Ok(true);
    }

    core::Result<bool> disconnect() override {
        state_ = ConnectionState::Disconnected;
        return core::Result<bool>::Ok(true);
    }

    void inject_fault(bool enable) override {
        fault_injected_ = enable;
        if (enable) {
            state_ = ConnectionState::CommunicationTimeout;
        } else if (state_ == ConnectionState::CommunicationTimeout) {
            state_ = ConnectionState::Connected;
        }
    }

    core::Result<DeviceTelemetry> read_telemetry() override {
        if (state_ == ConnectionState::Disconnected) {
            return core::Result<DeviceTelemetry>::Fail("Cannot read telemetry: FPGA device is disconnected.");
        }
        if (fault_injected_) {
            state_ = ConnectionState::CommunicationTimeout;
            return core::Result<DeviceTelemetry>::Fail("ERR_SPI_CRC_MISMATCH: FPGA SPI burst failed frame validation checksum. Core packet dropped.");
        }

        packet_counter_++;
        DeviceTelemetry telem;
        telem.device_id = get_id();
        telem.timestamp = get_current_iso_time();
        telem.voltage_V = 1.00 + (0.01 * std::sin(packet_counter_ * 0.15));
        telem.temperature_C = 45.2 + (2.5 * std::sin(packet_counter_ * 0.08));
        telem.packet_sequence = packet_counter_;
        telem.state_summary = "CONFIG_DONE | PLL_LOCKED | BITSTREAM_VALID";
        telem.is_simulated = true;

        telem.attributes["lut_utilization_pct"] = "42.8";
        telem.attributes["dsp_utilization_pct"] = "26.7";
        telem.attributes["bitstream_crc32"] = "0xE482A109";
        telem.attributes["xadc_die_temp_C"] = std::to_string(telem.temperature_C);
        telem.attributes["pll_status"] = "LOCKED (sys_clk = 100 MHz)";

        return core::Result<DeviceTelemetry>::Ok(telem);
    }

private:
    ConnectionState state_{ConnectionState::Disconnected};
    bool fault_injected_{false};
    uint64_t packet_counter_{0};
};

/**
 * JTAG Debug Adapter Mock (FTDI FT2232H / CMSIS-DAP)
 */
class JtagMockAdapter : public DeviceAdapter {
public:
    JtagMockAdapter() = default;

    std::string get_id() const override { return "JTAG-DEBUG-01"; }
    std::string get_type() const override { return "JTAG Hardware Debug Adapter"; }
    std::string get_name() const override { return "FTDI FT2232H High-Speed USB-to-JTAG Debugger"; }
    ConnectionState get_state() const override { return state_; }
    bool is_fault_injected() const override { return fault_injected_; }

    HardwareCapabilities get_capabilities() const override {
        HardwareCapabilities caps;
        caps.device_id = get_id();
        caps.device_type = "JTAG_DEBUGGER";
        caps.model_name = get_name();
        caps.host_interface = "USB 2.0 High-Speed (480 Mbps) MPSSE Engine";
        caps.logic_voltage = "Target Voltage Sensing: 1.65V to 5.0V with Level Shifters";
        caps.connector_type = "Standard ARM 20-pin / 10-pin 0.05\" Pitch Ribbon Cable";
        caps.feature_list = {
            "IEEE 1149.1 Standard Boundary-Scan Controller",
            "Hardware TCK rate up to 30 MHz",
            "Signals: TCK, TMS, TDI, TDO, nTRST, nSRST, VTREF, GND",
            "Dedicated Debug Tap: NOT a generic compute coprocessor",
            "Hardware target reset pulse generator"
        };
        caps.is_simulated = true;
        return caps;
    }

    core::Result<bool> connect() override {
        state_ = ConnectionState::Connected;
        fault_injected_ = false;
        return core::Result<bool>::Ok(true);
    }

    core::Result<bool> disconnect() override {
        state_ = ConnectionState::Disconnected;
        return core::Result<bool>::Ok(true);
    }

    void inject_fault(bool enable) override {
        fault_injected_ = enable;
        if (enable) {
            state_ = ConnectionState::CommunicationTimeout;
        } else if (state_ == ConnectionState::CommunicationTimeout) {
            state_ = ConnectionState::Connected;
        }
    }

    core::Result<DeviceTelemetry> read_telemetry() override {
        if (state_ == ConnectionState::Disconnected) {
            return core::Result<DeviceTelemetry>::Fail("Cannot read telemetry: JTAG Debugger is disconnected.");
        }
        if (fault_injected_) {
            state_ = ConnectionState::CommunicationTimeout;
            return core::Result<DeviceTelemetry>::Fail("ERR_JTAG_TAP_LOST: TDO line stuck low during Shift-IR sequence. Target board powered down or cable disconnected.");
        }

        packet_counter_++;
        DeviceTelemetry telem;
        telem.device_id = get_id();
        telem.timestamp = get_current_iso_time();
        telem.voltage_V = 3.31 + (0.005 * std::cos(packet_counter_ * 0.2));
        telem.temperature_C = 32.1 + (0.4 * std::sin(packet_counter_ * 0.04));
        telem.packet_sequence = packet_counter_;
        telem.state_summary = "TAP_RUN_TEST_IDLE | CLK_15MHZ | VTREF_STABLE";
        telem.is_simulated = true;

        telem.attributes["tck_frequency_mhz"] = "15.0";
        telem.attributes["tap_state"] = "Run-Test/Idle";
        telem.attributes["target_vtref_v"] = "3.312";
        telem.attributes["detected_idcode"] = "0x0362D093 (Xilinx Artix-7 TAP)";
        telem.attributes["role_note"] = "Hardware Debug/Programming Tap (Non-Compute)";

        return core::Result<DeviceTelemetry>::Ok(telem);
    }

private:
    ConnectionState state_{ConnectionState::Disconnected};
    bool fault_injected_{false};
    uint64_t packet_counter_{0};
};

EmbeddedWorkbenchManager::EmbeddedWorkbenchManager() {
    auto mcu = std::make_unique<McuMockAdapter>();
    auto fpga = std::make_unique<FpgaMockAdapter>();
    auto jtag = std::make_unique<JtagMockAdapter>();

    adapters_[mcu->get_id()] = std::move(mcu);
    adapters_[fpga->get_id()] = std::move(fpga);
    adapters_[jtag->get_id()] = std::move(jtag);
}

std::vector<HardwareCapabilities> EmbeddedWorkbenchManager::discover_devices() const {
    std::vector<HardwareCapabilities> list;
    for (const auto& [id, adapter] : adapters_) {
        list.push_back(adapter->get_capabilities());
    }
    return list;
}

DeviceAdapter* EmbeddedWorkbenchManager::get_adapter(const std::string& device_id) {
    auto it = adapters_.find(device_id);
    if (it != adapters_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void EmbeddedWorkbenchManager::connect_all() {
    for (auto& [id, adapter] : adapters_) {
        adapter->connect();
    }
}

void EmbeddedWorkbenchManager::disconnect_all() {
    for (auto& [id, adapter] : adapters_) {
        adapter->disconnect();
    }
}

std::vector<core::Result<DeviceTelemetry>> EmbeddedWorkbenchManager::poll_all_telemetry() {
    std::vector<core::Result<DeviceTelemetry>> results;
    for (auto& [id, adapter] : adapters_) {
        results.push_back(adapter->read_telemetry());
    }
    return results;
}

} // namespace workbench::embedded
