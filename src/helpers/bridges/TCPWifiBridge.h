#pragma once

#ifdef WITH_TCP_WIFI_BRIDGE

#include "BridgeBase.h"
#include "TCPWifiPortal.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

/**
 * TCPBridgeConfig - Self-contained configuration for TCP WiFi Bridge.
 *
 * This config is stored in its own file (/tcp_bridge.cfg), separate from
 * the main NodePrefs, for maximum encapsulation and minimal core changes.
 *
 * With auto-discovery, only WiFi credentials are needed - peer discovery
 * and role assignment (server/client) happen automatically.
 */
struct TCPBridgeConfig {
  char wifi_ssid[33];     // WiFi SSID
  char wifi_pass[65];     // WiFi password
  uint16_t tcp_port;      // TCP port (default 5555)
  uint16_t udp_port;      // UDP discovery port (default 5556)
  uint32_t guard;         // Config validation magic
};

/**
 * TCPWifiBridge - WiFi-based TCP bridge for linking repeaters over IP networks.
 *
 * This bridge enables two MeshCore repeaters to relay packets over a TCP/IP
 * connection, linking geographically separate LoRa mesh networks via WiFi/internet.
 *
 * Features:
 * - AUTO-DISCOVERY: Repeaters find each other via UDP broadcast on the local network
 * - Automatic role assignment: lower IP becomes server, higher IP becomes client
 * - Paired repeaters stay silent to discovery (only free repeaters respond)
 * - Captive portal for WiFi configuration (only SSID/password needed)
 * - Same framing protocol as RS232Bridge (magic + length + payload + checksum)
 *
 * Auto-Discovery Protocol:
 *   1. Broadcast "MESHCORE_DISCOVER" on UDP port 5556
 *   2. Free (unpaired) repeaters reply with "MESHCORE_REPLY|<node_id>"
 *   3. Paired repeaters stay silent (don't respond)
 *   4. Lower IP becomes server (listens), higher IP becomes client (connects)
 *
 * Architecture:
 *   Repeater A (lower IP=server)           Repeater B (higher IP=client)
 *   ┌─────────────────┐                    ┌─────────────────┐
 *   │   LoRa Radio    │                    │   LoRa Radio    │
 *   │       ↓↑        │                    │       ↓↑        │
 *   │    MyMesh       │                    │    MyMesh       │
 *   │       ↓↑        │                    │       ↓↑        │
 *   │  TCPWifiBridge  │◄──── TCP/IP ──────►│  TCPWifiBridge  │
 *   │   (auto-role)   │     Port 5555      │   (auto-role)   │
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
  bool isRunning() const override;
  const char* getStatusString() const override;

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
  static constexpr uint32_t CONFIG_GUARD = 0xBD1D9E02;  // Magic value for config validation (bumped version)
  static constexpr uint16_t DEFAULT_TCP_PORT = 5555;
  static constexpr uint16_t DEFAULT_UDP_PORT = 5556;
  static constexpr uint16_t DISCOVERY_INTERVAL_MS = 3000;   // Broadcast discovery every 3 seconds
  static constexpr uint16_t CONNECT_RETRY_MS = 2000;        // Retry connection every 2 seconds
  static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;

  // Discovery protocol constants
  static constexpr const char* DISCOVER_MSG = "MESHCORE_DISCOVER";
  static constexpr const char* REPLY_PREFIX = "MESHCORE_REPLY|";

  // Configuration and state
  TCPBridgeConfig _config;
  TCPWifiPortal _portal;
  bool _in_portal_mode;

  // Networking - TCP
  WiFiServer* _server;
  WiFiClient _client;
  uint8_t _rx_buffer[MAX_TCP_PACKET_SIZE];
  uint16_t _rx_buffer_pos;
  bool _wifi_connected;

  // Networking - UDP discovery
  WiFiUDP _udp;
  IPAddress _peer_ip;           // Discovered peer's IP
  bool _peer_discovered;        // Have we found a peer?
  bool _is_server;              // Our role: true=server (lower IP), false=client
  unsigned long _last_discovery;
  unsigned long _last_connect_attempt;

  // Configuration management
  bool loadConfig();
  void saveConfig();
  void initDefaultConfig();

  // Connection management
  bool connectWifi();
  void startServer();
  void tryConnect();
  void processIncomingData();

  // Auto-discovery
  void startDiscovery();
  void loopDiscovery();
  void sendDiscoveryBroadcast();
  void handleDiscoveryPacket();
  void sendDiscoveryReply(IPAddress& sender);
  bool isPaired() { return _client.connected(); }
  IPAddress getBroadcastAddress();
  void getMacString(char* buf);

  // CLI command handlers
  void cmdStatus(char* reply);
  void cmdWifiReset(char* reply);
};

#endif  // WITH_TCP_WIFI_BRIDGE
