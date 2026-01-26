#ifdef WITH_TCP_WIFI_BRIDGE

#include "TCPWifiBridge.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#ifndef BRIDGE_DEBUG_PRINTLN
#define BRIDGE_DEBUG_PRINTLN(fmt, ...) Serial.printf("[TCPBridge] " fmt, ##__VA_ARGS__)
#endif

// OTA web server (static so it persists)
static AsyncWebServer* _ota_server = nullptr;

bool TCPWifiBridge::isRunning() const {
  // Check both initialized AND actually connected to peer
  return _initialized && const_cast<WiFiClient&>(_client).connected();
}

const char* TCPWifiBridge::getStatusString() const {
  if (!_initialized) return "Off";
  if (_in_portal_mode) return "Portal Mode";
  if (!_wifi_connected) return "WiFi Disconn";
  if (const_cast<WiFiClient&>(_client).connected()) return "Connected";
  if (_peer_discovered) return "Connecting...";
  return "Searching...";
}

TCPWifiBridge::TCPWifiBridge(NodePrefs* prefs, mesh::PacketManager* mgr, mesh::RTCClock* rtc)
    : BridgeBase(prefs, mgr, rtc),
      _portal(&_config),
      _in_portal_mode(false),
      _server(nullptr),
      _rx_buffer_pos(0),
      _wifi_connected(false),
      _peer_discovered(false),
      _is_server(false),
      _last_discovery(0),
      _last_connect_attempt(0) {
  initDefaultConfig();
}

TCPWifiBridge::~TCPWifiBridge() {
  end();
}

void TCPWifiBridge::initDefaultConfig() {
  memset(&_config, 0, sizeof(_config));
  _config.tcp_port = DEFAULT_TCP_PORT;
  _config.udp_port = DEFAULT_UDP_PORT;
  _config.guard = CONFIG_GUARD;
}

bool TCPWifiBridge::loadConfig() {
  File f = SPIFFS.open(CONFIG_FILE, "r");
  if (!f) {
    BRIDGE_DEBUG_PRINTLN("No config file found\n");
    return false;
  }

  size_t bytesRead = f.read((uint8_t*)&_config, sizeof(_config));
  f.close();

  if (bytesRead != sizeof(_config) || _config.guard != CONFIG_GUARD) {
    BRIDGE_DEBUG_PRINTLN("Invalid config (guard mismatch), using defaults\n");
    initDefaultConfig();
    return false;
  }

  BRIDGE_DEBUG_PRINTLN("Config loaded: ssid=%s, tcp_port=%d, udp_port=%d\n",
                       _config.wifi_ssid, _config.tcp_port, _config.udp_port);
  return true;
}

void TCPWifiBridge::saveConfig() {
  _config.guard = CONFIG_GUARD;
  File f = SPIFFS.open(CONFIG_FILE, "w");
  if (!f) {
    BRIDGE_DEBUG_PRINTLN("Failed to open config file for writing\n");
    return;
  }

  f.write((uint8_t*)&_config, sizeof(_config));
  f.close();
  BRIDGE_DEBUG_PRINTLN("Config saved\n");
}

void TCPWifiBridge::begin() {
  if (_initialized) return;

  BRIDGE_DEBUG_PRINTLN("Initializing with auto-discovery...\n");

  // Load configuration
  loadConfig();

  // Check if WiFi is configured
  if (strlen(_config.wifi_ssid) == 0) {
    BRIDGE_DEBUG_PRINTLN("No WiFi configured, starting portal\n");
    _portal.begin();
    _in_portal_mode = true;
    return;
  }

  // Try to connect to WiFi
  if (!connectWifi()) {
    BRIDGE_DEBUG_PRINTLN("WiFi connection failed, starting portal\n");
    _portal.begin();
    _in_portal_mode = true;
    return;
  }

  // Start discovery process
  startDiscovery();

  _initialized = true;
  BRIDGE_DEBUG_PRINTLN("Initialized, discovering peers...\n");
}

void TCPWifiBridge::end() {
  if (_in_portal_mode) {
    _portal.stop();
    _in_portal_mode = false;
  }

  if (_client.connected()) {
    _client.stop();
  }

  if (_server) {
    _server->stop();
    delete _server;
    _server = nullptr;
  }

  _udp.stop();
  WiFi.disconnect(true);
  _wifi_connected = false;
  _peer_discovered = false;
  _initialized = false;
  _rx_buffer_pos = 0;

  BRIDGE_DEBUG_PRINTLN("Stopped\n");
}

bool TCPWifiBridge::connectWifi() {
  BRIDGE_DEBUG_PRINTLN("Connecting to WiFi: %s\n", _config.wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(_config.wifi_ssid, _config.wifi_pass);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > WIFI_CONNECT_TIMEOUT_MS) {
      BRIDGE_DEBUG_PRINTLN("WiFi connection timeout\n");
      return false;
    }
    delay(100);
  }

  _wifi_connected = true;
  BRIDGE_DEBUG_PRINTLN("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

  // Start OTA server on port 80
  if (!_ota_server) {
    _ota_server = new AsyncWebServer(80);
    _ota_server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", "<h2>MeshCore TCP Bridge</h2><p><a href='/update'>Firmware Update</a></p>");
    });
    AsyncElegantOTA.begin(_ota_server);
    _ota_server->begin();
    BRIDGE_DEBUG_PRINTLN("OTA available at http://%s/update\n", WiFi.localIP().toString().c_str());
  }

  return true;
}

// ============================================================================
// Auto-Discovery
// ============================================================================

void TCPWifiBridge::startDiscovery() {
  // Start UDP listener for discovery
  _udp.begin(_config.udp_port);
  BRIDGE_DEBUG_PRINTLN("Discovery started on UDP port %d\n", _config.udp_port);

  // Reset discovery state
  _peer_discovered = false;
  _peer_ip = IPAddress(0, 0, 0, 0);
  _last_discovery = 0;

  // Always start TCP server - we may become the server role
  startServer();
}

IPAddress TCPWifiBridge::getBroadcastAddress() {
  IPAddress ip = WiFi.localIP();
  IPAddress subnet = WiFi.subnetMask();
  IPAddress broadcast;
  for (int i = 0; i < 4; i++) {
    broadcast[i] = ip[i] | ~subnet[i];
  }
  return broadcast;
}

void TCPWifiBridge::sendDiscoveryBroadcast() {
  IPAddress broadcast = getBroadcastAddress();

  _udp.beginPacket(broadcast, _config.udp_port);
  _udp.print(DISCOVER_MSG);
  _udp.endPacket();

  BRIDGE_DEBUG_PRINTLN("Discovery broadcast sent to %s:%d\n",
                       broadcast.toString().c_str(), _config.udp_port);
}

void TCPWifiBridge::getMacString(char* buf) {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(buf, 13, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void TCPWifiBridge::sendDiscoveryReply(IPAddress& sender) {
  // Use MAC address as unique identifier
  char macStr[13];
  getMacString(macStr);

  _udp.beginPacket(sender, _config.udp_port);
  _udp.print(REPLY_PREFIX);
  _udp.print(macStr);
  _udp.endPacket();

  BRIDGE_DEBUG_PRINTLN("Discovery reply sent to %s (MAC: %s)\n", sender.toString().c_str(), macStr);
}

void TCPWifiBridge::handleDiscoveryPacket() {
  int packetSize = _udp.parsePacket();
  if (packetSize == 0) return;

  char buffer[64];
  int len = _udp.read(buffer, sizeof(buffer) - 1);
  if (len <= 0) return;
  buffer[len] = '\0';

  IPAddress senderIP = _udp.remoteIP();

  // Ignore packets from ourselves
  if (senderIP == WiFi.localIP()) return;

  BRIDGE_DEBUG_PRINTLN("Discovery packet from %s: %s\n", senderIP.toString().c_str(), buffer);

  // Handle discovery request
  if (strcmp(buffer, DISCOVER_MSG) == 0) {
    // Only reply if we're NOT paired
    if (!isPaired()) {
      sendDiscoveryReply(senderIP);
    } else {
      BRIDGE_DEBUG_PRINTLN("Ignoring discovery (already paired)\n");
    }
    return;
  }

  // Handle discovery reply
  if (strncmp(buffer, REPLY_PREFIX, strlen(REPLY_PREFIX)) == 0) {
    // Only process if we're not already paired
    if (isPaired()) {
      BRIDGE_DEBUG_PRINTLN("Ignoring reply (already paired)\n");
      return;
    }

    // We found a peer!
    _peer_ip = senderIP;
    _peer_discovered = true;

    // Determine role based on IP comparison
    // Lower IP = server (waits for connection)
    // Higher IP = client (initiates connection)
    uint32_t myIP = (uint32_t)WiFi.localIP();
    uint32_t peerIP = (uint32_t)senderIP;
    _is_server = (myIP < peerIP);

    BRIDGE_DEBUG_PRINTLN("Peer discovered: %s, I am %s (my IP: %s)\n",
                         senderIP.toString().c_str(),
                         _is_server ? "SERVER" : "CLIENT",
                         WiFi.localIP().toString().c_str());
  }
}

void TCPWifiBridge::loopDiscovery() {
  // Always check for incoming discovery packets
  handleDiscoveryPacket();

  // If not paired, periodically send discovery broadcasts
  if (!isPaired() && !_peer_discovered) {
    unsigned long now = millis();
    if (now - _last_discovery >= DISCOVERY_INTERVAL_MS) {
      _last_discovery = now;
      sendDiscoveryBroadcast();
    }
  }
}

// ============================================================================
// Connection Management
// ============================================================================

void TCPWifiBridge::startServer() {
  if (_server) {
    _server->stop();
    delete _server;
  }

  _server = new WiFiServer(_config.tcp_port);
  _server->begin();
  BRIDGE_DEBUG_PRINTLN("TCP server listening on port %d\n", _config.tcp_port);
}

void TCPWifiBridge::tryConnect() {
  if (_client.connected()) return;
  if (!_peer_discovered) return;
  if (_is_server) return;  // Server doesn't initiate connections

  unsigned long now = millis();
  if (now - _last_connect_attempt < CONNECT_RETRY_MS) return;
  _last_connect_attempt = now;

  BRIDGE_DEBUG_PRINTLN("Connecting to peer %s:%d\n", _peer_ip.toString().c_str(), _config.tcp_port);

  if (_client.connect(_peer_ip, _config.tcp_port)) {
    BRIDGE_DEBUG_PRINTLN("Connected to peer!\n");
    _rx_buffer_pos = 0;
  } else {
    BRIDGE_DEBUG_PRINTLN("Connection failed, will retry...\n");
  }
}

void TCPWifiBridge::loop() {
  // Portal mode - waiting for configuration
  if (_in_portal_mode) {
    _portal.loop();
    if (_portal.isConfigured()) {
      saveConfig();
      BRIDGE_DEBUG_PRINTLN("Configuration complete, rebooting...\n");
      delay(500);
      ESP.restart();
    }
    return;
  }

  if (!_initialized) return;

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (_wifi_connected) {
      BRIDGE_DEBUG_PRINTLN("WiFi disconnected\n");
      _wifi_connected = false;
      _peer_discovered = false;
      if (_client.connected()) {
        _client.stop();
      }
    }
    return;
  }
  _wifi_connected = true;

  // Run discovery process
  loopDiscovery();

  // Server: accept incoming connections
  if (_server && !_client.connected()) {
    WiFiClient newClient = _server->available();
    if (newClient) {
      _client = newClient;
      _rx_buffer_pos = 0;
      _peer_ip = _client.remoteIP();
      _peer_discovered = true;
      _is_server = true;
      BRIDGE_DEBUG_PRINTLN("Client connected from %s\n", _client.remoteIP().toString().c_str());
    }
  }

  // Client: initiate connection to discovered peer
  if (_peer_discovered && !_is_server) {
    tryConnect();
  }

  // Process incoming TCP data
  if (_client.connected()) {
    processIncomingData();
  } else if (_peer_discovered) {
    // Connection lost, reset discovery to find peer again
    BRIDGE_DEBUG_PRINTLN("Connection lost, restarting discovery\n");
    _peer_discovered = false;
    _last_discovery = 0;  // Trigger immediate discovery
  }
}

void TCPWifiBridge::processIncomingData() {
  while (_client.available()) {
    uint8_t b = _client.read();

    if (_rx_buffer_pos < 2) {
      // STATE 1: Waiting for magic word (2 bytes: 0xC0, 0x3E)
      if ((_rx_buffer_pos == 0 && b == ((BRIDGE_PACKET_MAGIC >> 8) & 0xFF)) ||
          (_rx_buffer_pos == 1 && b == (BRIDGE_PACKET_MAGIC & 0xFF))) {
        _rx_buffer[_rx_buffer_pos++] = b;
      } else {
        // Invalid magic byte, reset and start over
        _rx_buffer_pos = 0;
        if (b == ((BRIDGE_PACKET_MAGIC >> 8) & 0xFF)) {
          _rx_buffer[_rx_buffer_pos++] = b;
        }
      }
    } else {
      // STATE 2 & 3: Reading length, payload, and checksum
      _rx_buffer[_rx_buffer_pos++] = b;

      if (_rx_buffer_pos >= 4) {
        // Extract length field (bytes 2-3)
        uint16_t len = (_rx_buffer[2] << 8) | _rx_buffer[3];

        // Validate length
        if (len > (MAX_TRANS_UNIT + 1)) {
          BRIDGE_DEBUG_PRINTLN("RX invalid length %d, resetting\n", len);
          _rx_buffer_pos = 0;
          continue;
        }

        // Check if complete packet received
        if (_rx_buffer_pos == len + TCP_OVERHEAD) {
          // Extract checksum (last 2 bytes)
          uint16_t received_checksum = (_rx_buffer[4 + len] << 8) | _rx_buffer[5 + len];

          // Validate checksum against payload
          if (validateChecksum(_rx_buffer + 4, len, received_checksum)) {
            BRIDGE_DEBUG_PRINTLN("RX, len=%d crc=0x%04x\n", len, received_checksum);
            mesh::Packet* pkt = _mgr->allocNew();
            if (pkt) {
              if (pkt->readFrom(_rx_buffer + 4, len)) {
                onPacketReceived(pkt);
              } else {
                BRIDGE_DEBUG_PRINTLN("RX failed to parse packet\n");
                _mgr->free(pkt);
              }
            }
          } else {
            BRIDGE_DEBUG_PRINTLN("RX checksum mismatch, rcv=0x%04x\n", received_checksum);
          }
          _rx_buffer_pos = 0;  // Reset for next packet
        }
      }
    }
  }
}

void TCPWifiBridge::sendPacket(mesh::Packet* packet) {
  if (!_initialized) return;
  if (!packet) return;
  if (!_client.connected()) return;

  // Check if packet already seen (prevent retransmission)
  if (!_seen_packets.hasSeen(packet)) {
    uint8_t buffer[MAX_TCP_PACKET_SIZE];

    // Write mesh packet starting at offset 4 (after header fields)
    uint16_t len = packet->writeTo(buffer + 4);

    // Validate payload size
    if (len > (MAX_TRANS_UNIT + 1)) {
      BRIDGE_DEBUG_PRINTLN("TX packet too large (payload=%d, max=%d)\n", len, MAX_TRANS_UNIT + 1);
      return;
    }

    // Build packet header
    buffer[0] = (BRIDGE_PACKET_MAGIC >> 8) & 0xFF;  // Magic high byte: 0xC0
    buffer[1] = BRIDGE_PACKET_MAGIC & 0xFF;         // Magic low byte: 0x3E
    buffer[2] = (len >> 8) & 0xFF;                  // Length high byte
    buffer[3] = len & 0xFF;                         // Length low byte

    // Calculate Fletcher-16 checksum over payload
    uint16_t checksum = fletcher16(buffer + 4, len);
    buffer[4 + len] = (checksum >> 8) & 0xFF;  // Checksum high byte
    buffer[5 + len] = checksum & 0xFF;         // Checksum low byte

    // Transmit complete framed packet
    _client.write(buffer, len + TCP_OVERHEAD);

    BRIDGE_DEBUG_PRINTLN("TX, len=%d crc=0x%04x\n", len, checksum);
  }
}

// ============================================================================
// CLI Command Handlers
// ============================================================================

bool TCPWifiBridge::handleCommand(const char* cmd, char* reply) {
  // Skip leading spaces
  while (*cmd == ' ') cmd++;

  if (strcmp(cmd, "status") == 0) {
    cmdStatus(reply);
    return true;
  }
  if (strcmp(cmd, "wifi reset") == 0) {
    cmdWifiReset(reply);
    return true;
  }

  // Unknown command - show help
  sprintf(reply,
          "TCP bridge commands:\n"
          "  tcp status      - Show connection status\n"
          "  tcp wifi reset  - Clear WiFi config, restart portal\n");
  return true;
}

void TCPWifiBridge::cmdStatus(char* reply) {
  char* p = reply;

  p += sprintf(p, "TCP Bridge Status (Auto-Discovery):\n");
  p += sprintf(p, "  TCP Port: %d\n", _config.tcp_port);
  p += sprintf(p, "  UDP Port: %d (discovery)\n", _config.udp_port);

  if (_in_portal_mode) {
    p += sprintf(p, "  State: Portal mode (AP: %s)\n", TCPWifiPortal::getAPSSID());
  } else if (!_wifi_connected) {
    p += sprintf(p, "  State: WiFi disconnected\n");
    p += sprintf(p, "  SSID: %s\n", _config.wifi_ssid);
  } else {
    p += sprintf(p, "  WiFi: Connected to %s\n", _config.wifi_ssid);
    p += sprintf(p, "  My IP: %s\n", WiFi.localIP().toString().c_str());

    if (_client.connected()) {
      p += sprintf(p, "  Peer: %s (CONNECTED)\n", _peer_ip.toString().c_str());
      p += sprintf(p, "  Role: %s\n", _is_server ? "server" : "client");
    } else if (_peer_discovered) {
      p += sprintf(p, "  Peer: %s (discovered, connecting...)\n", _peer_ip.toString().c_str());
      p += sprintf(p, "  Role: %s\n", _is_server ? "server" : "client");
    } else {
      p += sprintf(p, "  Peer: Searching...\n");
    }
  }
}

void TCPWifiBridge::cmdWifiReset(char* reply) {
  // Clear WiFi credentials
  memset(_config.wifi_ssid, 0, sizeof(_config.wifi_ssid));
  memset(_config.wifi_pass, 0, sizeof(_config.wifi_pass));
  saveConfig();
  sprintf(reply, "WiFi configuration cleared. Restarting into portal mode...");

  // Schedule restart
  delay(500);
  ESP.restart();
}

#endif  // WITH_TCP_WIFI_BRIDGE
