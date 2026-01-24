#pragma once

#ifdef WITH_TCP_WIFI_BRIDGE

#include "BridgeBase.h"
#include "TCPWifiPortal.h"
#include <SPIFFS.h>
#include <WiFi.h>

/**
 * TCPBridgeConfig - Self-contained configuration for TCP WiFi Bridge.
 *
 * This config is stored in its own file (/tcp_bridge.cfg), separate from
 * the main NodePrefs, for maximum encapsulation and minimal core changes.
 */
struct TCPBridgeConfig {
  uint8_t mode;           // 0 = server, 1 = client
  uint16_t port;          // TCP port (default 5555)
  char host[40];          // Remote host IP/hostname (client mode)
  uint16_t reconnect_ms;  // Reconnect interval (default 5000)
  char wifi_ssid[33];     // WiFi SSID
  char wifi_pass[65];     // WiFi password
  uint32_t guard;         // Config validation magic
};

/**
 * TCPWifiBridge - WiFi-based TCP bridge for linking repeaters over IP networks.
 *
 * This bridge enables two MeshCore repeaters to relay packets over a TCP/IP
 * connection, linking geographically separate LoRa mesh networks via WiFi/internet.
 *
 * Features:
 * - Server or client mode (point-to-point connection)
 * - Captive portal for WiFi configuration (no compile-time credentials)
 * - Same framing protocol as RS232Bridge (magic + length + payload + checksum)
 * - Self-contained CLI commands (tcp mode, tcp port, tcp host, tcp status)
 *
 * Architecture:
 *   Repeater A (Server)                    Repeater B (Client)
 *   ┌─────────────────┐                    ┌─────────────────┐
 *   │   LoRa Radio    │                    │   LoRa Radio    │
 *   │       ↓↑        │                    │       ↓↑        │
 *   │    MyMesh       │                    │    MyMesh       │
 *   │       ↓↑        │                    │       ↓↑        │
 *   │  TCPWifiBridge  │◄──── TCP/IP ──────►│  TCPWifiBridge  │
 *   │   (Server)      │     Connection     │   (Client)      │
 *   └─────────────────┘                    └─────────────────┘
 */
class TCPWifiBridge : public BridgeBase {
public:
  TCPWifiBridge(NodePrefs* prefs, mesh::PacketManager* mgr, mesh::RTCClock* rtc);
  ~TCPWifiBridge();

  // BridgeBase interface
  void begin() override;
  void end() override;
  void loop() override;
  void sendPacket(mesh::Packet* packet) override;
  void onPacketReceived(mesh::Packet* packet) override { handleReceivedPacket(packet); }

  // Self-contained CLI handler
  bool handleCommand(const char* cmd, char* reply);

  // Config accessors
  TCPBridgeConfig& getConfig() { return _config; }
  bool isInPortalMode() const { return _in_portal_mode; }

private:
  // TCP overhead: magic(2) + len(2) + checksum(2) = 6 bytes
  static constexpr uint16_t TCP_OVERHEAD = BRIDGE_MAGIC_SIZE + BRIDGE_LENGTH_SIZE + BRIDGE_CHECKSUM_SIZE;
  static constexpr uint16_t MAX_TCP_PACKET_SIZE = (MAX_TRANS_UNIT + 1) + TCP_OVERHEAD;
  static constexpr const char* CONFIG_FILE = "/tcp_bridge.cfg";
  static constexpr uint32_t CONFIG_GUARD = 0xBD1D9E01;  // Magic value for config validation
  static constexpr uint16_t DEFAULT_PORT = 5555;
  static constexpr uint16_t DEFAULT_RECONNECT_MS = 5000;
  static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;

  // Configuration and state
  TCPBridgeConfig _config;
  TCPWifiPortal _portal;
  bool _in_portal_mode;

  // Networking
  WiFiServer* _server;
  WiFiClient _client;
  uint8_t _rx_buffer[MAX_TCP_PACKET_SIZE];
  uint16_t _rx_buffer_pos;
  unsigned long _last_reconnect;
  bool _wifi_connected;

  // Configuration management
  bool loadConfig();
  void saveConfig();
  void initDefaultConfig();

  // Connection management
  bool connectWifi();
  void startServer();
  void tryConnect();
  void processIncomingData();

  // CLI command handlers
  void cmdMode(const char* arg, char* reply);
  void cmdPort(const char* arg, char* reply);
  void cmdHost(const char* arg, char* reply);
  void cmdStatus(char* reply);
  void cmdSave(char* reply);
  void cmdWifiReset(char* reply);
};

#endif  // WITH_TCP_WIFI_BRIDGE
