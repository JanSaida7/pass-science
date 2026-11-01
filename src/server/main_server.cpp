#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>

#if defined(DeviceCapabilities)
#undef DeviceCapabilities
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <iomanip>

#include "parser/stl_parser.hpp"
#include "parser/ansys_parser.hpp"
#include "fea/fea_solver.hpp"
#include "multicore/batch_solver.hpp"
#include "embedded/embedded_mock.hpp"
#include "backprop/backpropagation.hpp"

using namespace workbench;

// Global embedded hardware manager
static embedded::EmbeddedWorkbenchManager g_embedded_mgr;
static std::chrono::steady_clock::time_point g_launch_time;

// Helper to escape JSON strings
static std::string json_escape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if ('\x00' <= c && c <= '\x1f') {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        } else {
            o << c;
        }
    }
    return o.str();
}

// Read whole file from disk
static std::string read_file_content(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// URL query parameter parser
static std::map<std::string, std::string> parse_query_params(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream ss(query);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            params[pair.substr(0, eq)] = pair.substr(eq + 1);
        } else if (!pair.empty()) {
            params[pair] = "";
        }
    }
    return params;
}

// Serialize STL Mesh to JSON
static std::string stl_to_json(const core::StlMesh& mesh) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\"success\":true,\"solid_name\":\"" << json_escape(mesh.solid_name) << "\","
       << "\"facet_count\":" << mesh.facets.size() << ","
       << "\"parse_duration_ms\":" << mesh.parse_duration_ms << ","
       << "\"bounds\":{"
       << "\"min\":[" << mesh.min_bounds.x << "," << mesh.min_bounds.y << "," << mesh.min_bounds.z << "],"
       << "\"max\":[" << mesh.max_bounds.x << "," << mesh.max_bounds.y << "," << mesh.max_bounds.z << "]"
       << "},\"facets\":[";

    for (size_t i = 0; i < mesh.facets.size(); ++i) {
        const auto& f = mesh.facets[i];
        if (i > 0) ss << ",";
        ss << "{"
           << "\"n\":[" << f.normal.x << "," << f.normal.y << "," << f.normal.z << "],"
           << "\"v1\":[" << f.v1.x << "," << f.v1.y << "," << f.v1.z << "],"
           << "\"v2\":[" << f.v2.x << "," << f.v2.y << "," << f.v2.z << "],"
           << "\"v3\":[" << f.v3.x << "," << f.v3.y << "," << f.v3.z << "]"
           << "}";
    }
    ss << "]}";
    return ss.str();
}

// Serialize ANSYS Dataset to JSON
static std::string ansys_to_json(const core::AnsysDataset& ds) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\"success\":true,\"node_count\":" << ds.nodes.size() << ","
       << "\"parse_duration_ms\":" << ds.parse_duration_ms << ","
       << "\"min_temp_C\":" << ds.min_temp_C << ",\"max_temp_C\":" << ds.max_temp_C << ","
       << "\"bounds\":{"
       << "\"min\":[" << ds.min_bounds.x << "," << ds.min_bounds.y << "," << ds.min_bounds.z << "],"
       << "\"max\":[" << ds.max_bounds.x << "," << ds.max_bounds.y << "," << ds.max_bounds.z << "]"
       << "},\"nodes\":[";

    for (size_t i = 0; i < ds.nodes.size(); ++i) {
        const auto& n = ds.nodes[i];
        if (i > 0) ss << ",";
        ss << "[" << n.node_id << "," << n.x_m << "," << n.y_m << "," << n.z_m << "," << n.temperature_C << "]";
    }
    ss << "]}";
    return ss.str();
}

// Serialize FEA Solution to JSON
static std::string fea_to_json(const fea::FeaSolution& sol) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\"success\":true,\"duration_us\":" << sol.solve_duration_us << ","
       << "\"max_midpoint_error_C\":" << sol.max_absolute_midpoint_error_C << ","
       << "\"config\":{"
       << "\"L\":" << sol.config.length_m << ",\"A\":" << sol.config.area_m2 << ","
       << "\"k\":" << sol.config.conductivity_k << ",\"q\":" << sol.config.heat_gen_q << ","
       << "\"T0\":" << sol.config.temp_left_C << ",\"TL\":" << sol.config.temp_right_C << ","
       << "\"N\":" << sol.config.element_count
       << "},\"nodes\":[";

    for (size_t i = 0; i < sol.x_coords.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{\"x\":" << sol.x_coords[i] << ",\"T_fe\":" << sol.temperatures_fe[i] 
           << ",\"T_exact\":" << sol.temperatures_exact[i] << "}";
    }
    ss << "],\"midpoints\":[";

    for (size_t i = 0; i < sol.midpoint_errors.size(); ++i) {
        const auto& mp = sol.midpoint_errors[i];
        if (i > 0) ss << ",";
        ss << "{\"element\":" << mp.element_index << ",\"x_mid\":" << mp.x_mid_m
           << ",\"T_fe\":" << mp.temp_fe_C << ",\"T_exact\":" << mp.temp_exact_C
           << ",\"error\":" << mp.absolute_error_C << "}";
    }
    ss << "]}";
    return ss.str();
}

// Handle HTTP client request
static void handle_client(SOCKET client_socket) {
    char buffer[65536];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';

    std::string request(buffer, bytes_received);
    std::istringstream req_stream(request);
    std::string method, full_url, http_version;
    req_stream >> method >> full_url >> http_version;

    std::string path = full_url;
    std::string query_string = "";
    size_t q_pos = full_url.find('?');
    if (q_pos != std::string::npos) {
        path = full_url.substr(0, q_pos);
        query_string = full_url.substr(q_pos + 1);
    }
    auto params = parse_query_params(query_string);

    std::string response_content_type = "application/json";
    std::string response_body = "";
    int status_code = 200;
    std::string status_text = "OK";

    if (method == "OPTIONS") {
        // CORS preflight
        std::string res = "HTTP/1.1 204 No Content\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
        send(client_socket, res.c_str(), static_cast<int>(res.size()), 0);
        closesocket(client_socket);
        return;
    }

    if (path == "/" || path == "/index.html") {
        response_content_type = "text/html; charset=utf-8";
        response_body = read_file_content("src/ui/index.html");
        if (response_body.empty()) {
            response_body = "<h1>Unified Engineering Workbench</h1><p>UI file not found.</p>";
            status_code = 404;
        }
    }
    else if (path.rfind("/assets/", 0) == 0 || path.rfind("/ui/", 0) == 0) {
        std::string file_path = "src" + path;
        if (path.find(".js") != std::string::npos) response_content_type = "application/javascript";
        else if (path.find(".css") != std::string::npos) response_content_type = "text/css";
        response_body = read_file_content(file_path);
        if (response_body.empty()) status_code = 404;
    }
    else if (path == "/api/status") {
        auto now = std::chrono::steady_clock::now();
        double ready_ms = std::chrono::duration<double, std::milli>(now - g_launch_time).count();
        auto hw = multicore::MulticoreBatchSolver::detect_hardware();

        std::ostringstream ss;
        ss << "{\"status\":\"RUNNING\",\"launch_to_ready_ms\":" << ready_ms << ","
           << "\"cpu_model\":\"" << json_escape(hw.cpu_model) << "\","
           << "\"physical_cores\":" << hw.physical_cores << ",\"logical_cores\":" << hw.logical_cores << ","
           << "\"ram_mb\":" << hw.total_ram_mb << ",\"compiler\":\"" << json_escape(hw.compiler_info) << "\","
           << "\"optimization_flags\":\"" << json_escape(hw.optimization_flags) << "\"}";
        response_body = ss.str();
    }
    else if (path == "/api/stl/default") {
        auto res = parser::StlParser::parse_file("data/bracket.stl");
        if (res.success && res.data) {
            response_body = stl_to_json(*res.data);
        } else {
            status_code = 400;
            response_body = "{\"success\":false,\"error\":\"" + json_escape(res.error_message) + "\"}";
        }
    }
    else if (path == "/api/ansys/default") {
        auto res = parser::AnsysParser::parse_file("data/synthetic_ansys_results.csv");
        if (res.success && res.data) {
            response_body = ansys_to_json(*res.data);
        } else {
            status_code = 400;
            response_body = "{\"success\":false,\"error\":\"" + json_escape(res.error_message) + "\"}";
        }
    }
    else if (path == "/api/fea/solve") {
        fea::ThermalProblemConfig cfg;
        if (params.count("N")) cfg.element_count = std::stoi(params["N"]);
        if (params.count("k")) cfg.conductivity_k = std::stod(params["k"]);
        if (params.count("q")) cfg.heat_gen_q = std::stod(params["q"]);
        if (params.count("T0")) cfg.temp_left_C = std::stod(params["T0"]);
        if (params.count("TL")) cfg.temp_right_C = std::stod(params["TL"]);

        auto res = fea::ThermalFeaSolver::solve(cfg);
        if (res.success && res.data) {
            response_body = fea_to_json(*res.data);
        } else {
            status_code = 400;
            response_body = "{\"success\":false,\"error\":\"" + json_escape(res.error_message) + "\"}";
        }
    }
    else if (path == "/api/fea/convergence") {
        auto conv = fea::ThermalFeaSolver::run_convergence_study();
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(8);
        ss << "{\"success\":true,\"records\":[";
        for (size_t i = 0; i < conv.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{\"elements\":" << conv[i].elements << ",\"mesh_size_h\":" << conv[i].mesh_size_h
               << ",\"error_C\":" << conv[i].max_midpoint_error_C << ",\"rate\":" << conv[i].convergence_rate << "}";
        }
        ss << "]}";
        response_body = ss.str();
    }
    else if (path == "/api/multicore/benchmark") {
        int cases = params.count("cases") ? std::stoi(params["cases"]) : 1000;
        int elements = params.count("elements") ? std::stoi(params["elements"]) : 100;
        uint64_t seed = params.count("seed") ? std::stoull(params["seed"]) : 1337;

        auto suite = multicore::MulticoreBatchSolver::run_full_benchmark(cases, elements, seed);
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4);
        ss << "{\"success\":true,\"cases\":" << suite.case_count << ",\"elements\":" << suite.elements_per_case
           << ",\"seed\":" << suite.random_seed << ",\"notes\":\"" << json_escape(suite.analysis_notes) << "\","
           << "\"threads\":[";
        for (size_t i = 0; i < suite.thread_results.size(); ++i) {
            const auto& tr = suite.thread_results[i];
            if (i > 0) ss << ",";
            ss << "{\"workers\":" << tr.worker_count << ",\"median_ms\":" << tr.median_time_ms
               << ",\"speedup\":" << tr.speedup << ",\"efficiency\":" << tr.efficiency
               << ",\"max_diff_C\":" << tr.max_absolute_diff_vs_serial
               << ",\"verified\":" << (tr.verified_agreement ? "true" : "false") << "}";
        }
        ss << "]}";
        response_body = ss.str();
    }
    else if (path == "/api/embedded/devices") {
        auto devices = g_embedded_mgr.discover_devices();
        std::ostringstream ss;
        ss << "{\"success\":true,\"devices\":[";
        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& d = devices[i];
            if (i > 0) ss << ",";
            ss << "{\"id\":\"" << json_escape(d.device_id) << "\","
               << "\"type\":\"" << json_escape(d.device_type) << "\","
               << "\"name\":\"" << json_escape(d.model_name) << "\","
               << "\"host_interface\":\"" << json_escape(d.host_interface) << "\","
               << "\"logic_voltage\":\"" << json_escape(d.logic_voltage) << "\","
               << "\"connector\":\"" << json_escape(d.connector_type) << "\","
               << "\"is_simulated\":true}";
        }
        ss << "]}";
        response_body = ss.str();
    }
    else if (path == "/api/embedded/telemetry") {
        auto telemetry = g_embedded_mgr.poll_all_telemetry();
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3);
        ss << "{\"success\":true,\"telemetry\":[";
        for (size_t i = 0; i < telemetry.size(); ++i) {
            if (i > 0) ss << ",";
            if (telemetry[i].success && telemetry[i].data) {
                const auto& t = *telemetry[i].data;
                ss << "{\"status\":\"OK\",\"device_id\":\"" << json_escape(t.device_id) << "\","
                   << "\"temperature_C\":" << t.temperature_C << ",\"voltage_V\":" << t.voltage_V << ","
                   << "\"sequence\":" << t.packet_sequence << ",\"state_summary\":\"" << json_escape(t.state_summary) << "\","
                   << "\"timestamp\":\"" << json_escape(t.timestamp) << "\",\"is_simulated\":true}";
            } else {
                ss << "{\"status\":\"ERROR\",\"error_message\":\"" << json_escape(telemetry[i].error_message) << "\"}";
            }
        }
        ss << "]}";
        response_body = ss.str();
    }
    else if (path == "/api/embedded/toggle_fault") {
        std::string dev_id = params.count("device") ? params["device"] : "MCU-STM32-01";
        bool enable = params.count("enable") ? (params["enable"] == "1" || params["enable"] == "true") : true;

        auto adapter = g_embedded_mgr.get_adapter(dev_id);
        if (adapter) {
            adapter->inject_fault(enable);
            response_body = "{\"success\":true,\"device\":\"" + json_escape(dev_id) + "\",\"fault_injected\":" + (enable ? "true" : "false") + "}";
        } else {
            status_code = 404;
            response_body = "{\"success\":false,\"error\":\"Device not found: " + json_escape(dev_id) + "\"}";
        }
    }
    else if (path == "/api/backprop/status") {
        auto report = backprop::BackpropEngine::verify_gradients_finite_difference();
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(7);
        ss << "{\"success\":true,\"all_passed\":" << (report.all_passed ? "true" : "false") << ","
           << "\"epsilon\":" << report.epsilon << ",\"tolerance\":" << report.tolerance << ",\"gradients\":[";
        for (size_t i = 0; i < report.entries.size(); ++i) {
            const auto& e = report.entries[i];
            if (i > 0) ss << ",";
            ss << "{\"param\":\"" << json_escape(e.parameter_name) << "\","
               << "\"analytical\":" << e.analytical_grad << ",\"numerical\":" << e.numerical_grad << ","
               << "\"absolute_error\":" << e.absolute_error << ",\"passed\":" << (e.passed ? "true" : "false") << "}";
        }
        ss << "]}";
        response_body = ss.str();
    }
    else if (path == "/api/backprop/train") {
        double lr = params.count("lr") ? std::stod(params["lr"]) : 0.1;
        int steps = params.count("steps") ? std::stoi(params["steps"]) : 100;
        auto history = backprop::BackpropEngine::train_100_steps({0.5, 0.0, 1.0, 0.0}, lr, 1.0, 2.0, steps);

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << "{\"success\":true,\"steps\":[";
        for (size_t i = 0; i < history.size(); ++i) {
            const auto& h = history[i];
            if (i > 0) ss << ",";
            ss << "{\"step\":" << h.step << ",\"w\":" << h.params.w << ",\"b\":" << h.params.b
               << ",\"v\":" << h.params.v << ",\"c\":" << h.params.c << ",\"y_hat\":" << h.forward.y_hat
               << ",\"loss\":" << h.forward.loss << "}";
        }
        ss << "]}";
        response_body = ss.str();
    }
    else {
        status_code = 404;
        status_text = "Not Found";
        response_body = "{\"error\":\"Route not found\"}";
    }

    std::ostringstream http_res;
    http_res << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
             << "Content-Type: " << response_content_type << "\r\n"
             << "Content-Length: " << response_body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Connection: close\r\n\r\n"
             << response_body;

    std::string full_response = http_res.str();
    send(client_socket, full_response.c_str(), static_cast<int>(full_response.size()), 0);
    closesocket(client_socket);
}

int main(int argc, char* argv[]) {
    g_launch_time = std::chrono::steady_clock::now();

    // Check CLI flags
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test") {
            std::cout << "[Workbench] CLI Flag --test passed. Launching test suite...\n";
            // Run tests directly
            return system("bin\\run_tests.exe");
        }
    }

    // Connect mock embedded devices
    g_embedded_mgr.connect_all();

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock.\n";
        return 1;
    }

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        std::cerr << "Error creating socket.\n";
        WSACleanup();
        return 1;
    }

    int port = 8080;
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<u_short>(port));

    // Try binding to port 8080 or next free port
    while (bind(listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        port++;
        if (port > 8095) {
            std::cerr << "Unable to bind to any port in range 8080-8095.\n";
            closesocket(listen_socket);
            WSACleanup();
            return 1;
        }
        server_addr.sin_port = htons(static_cast<u_short>(port));
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed on port " << port << ".\n";
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    std::string url = "http://localhost:" + std::to_string(port);
    std::cout << "========================================================\n";
    std::cout << "   UNIFIED ENGINEERING WORKBENCH SERVER RUNNING        \n";
    std::cout << "   URL: " << url << "\n";
    std::cout << "   Press Ctrl+C to terminate cleanly.                 \n";
    std::cout << "========================================================\n";

    // Launch default browser automatically in one click!
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);

    // Accept loop
    while (true) {
        sockaddr_in client_addr{};
        int client_size = sizeof(client_addr);
        SOCKET client_socket = accept(listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_size);
        if (client_socket == INVALID_SOCKET) {
            break;
        }

        std::thread(handle_client, client_socket).detach();
    }

    closesocket(listen_socket);
    WSACleanup();
    return 0;
}
