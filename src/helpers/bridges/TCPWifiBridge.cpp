#ifdef WITH_TCP_WIFI_BRIDGE

#include "TCPWifiBridge.h"

#ifndef BRIDGE_DEBUG_PRINTLN
#define BRIDGE_DEBUG_PRINTLN(fmt, ...) Serial.printf("[TCPBridge] " fmt, ##__VA_ARGS__)
#endif

TCPWifiBridge::TCPWifiBridge(NodePrefs* prefs, mesh::PacketManager* mgr, mesh::RTCClock* rtc)
    : BridgeBase(prefs, mgr, rtc),
      _portal(&_config),
      _in_portal_mode(false),
      _server(nullptr),
      _rx_buffer_pos(0),
      _last_reconnect(0),
      _wifi_connected(false) {
  initDefaultConfig();
}

TCPWifiBridge::~TCPWifiBridge() {
  end();
}

void TCPWifiBridge::initDefaultConfig() {
  memset(&_config, 0, sizeof(_config));
  _config.mode = 0;  // Server mode
  _config.port = DEFAULT_PORT;
  _config.reconnect_ms = DEFAULT_RECONNECT_MS;
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
    BRIDGE_DEBUG_PRINTLN("Invalid config, using defaults\n");
    initDefaultConfig();
    return false;
  }

  BRIDGE_DEBUG_PRINTLN("Config loaded: mode=%d, port=%d, ssid=%s\n", _config.mode, _config.port,
                       _config.wifi_ssid);
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

  BRIDGE_DEBUG_PRINTLN("Initializing...\n");

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

  // Start server or prepare client
  if (_config.mode == 0) {
    startServer();
  }

  _initialized = true;
  BRIDGE_DEBUG_PRINTLN("Initialized in %s mode\n", _config.mode == 0 ? "server" : "client");
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

  WiFi.disconnect(true);
  _wifi_connected = false;
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
  return true;
}

void TCPWifiBridge::startServer() {
  if (_server) {
    _server->stop();
    delete _server;
  }

  _server = new WiFiServer(_config.port);
  _server->begin();
  BRIDGE_DEBUG_PRINTLN("Server listening on port %d\n", _config.port);
}

void TCPWifiBridge::tryConnect() {
  if (_client.connected()) return;

  unsigned long now = millis();
  if (now - _last_reconnect < _config.reconnect_ms) return;
  _last_reconnect = now;

  BRIDGE_DEBUG_PRINTLN("Connecting to %s:%d\n", _config.host, _config.port);

  if (_client.connect(_config.host, _config.port)) {
    BRIDGE_DEBUG_PRINTLN("Connected to server\n");
    _rx_buffer_pos = 0;
  } else {
    BRIDGE_DEBUG_PRINTLN("Connection failed\n");
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
      if (_client.connected()) {
        _client.stop();
      }
    }
    // Try to reconnect WiFi
    unsigned long now = millis();
    if (now - _last_reconnect > _config.reconnect_ms) {
      _last_reconnect = now;
      if (WiFi.reconnect()) {
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 5000) {
          delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
          _wifi_connected = true;
          BRIDGE_DEBUG_PRINTLN("WiFi reconnected\n");
        }
      }
    }
    return;
  }
  _wifi_connected = true;

  // Server mode - accept incoming connections
  if (_config.mode == 0 && _server) {
    if (!_client.connected()) {
      WiFiClient newClient = _server->available();
      if (newClient) {
        _client = newClient;
        _rx_buffer_pos = 0;
        BRIDGE_DEBUG_PRINTLN("Client connected from %s\n", _client.remoteIP().toString().c_str());
      }
    }
  }

  // Client mode - maintain connection
  if (_config.mode == 1) {
    tryConnect();
  }

  // Process incoming data
  if (_client.connected()) {
    processIncomingData();
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

  if (strncmp(cmd, "mode ", 5) == 0) {
    cmdMode(cmd + 5, reply);
    return true;
  }
  if (strncmp(cmd, "port ", 5) == 0) {
    cmdPort(cmd + 5, reply);
    return true;
  }
  if (strncmp(cmd, "host ", 5) == 0) {
    cmdHost(cmd + 5, reply);
    return true;
  }
  if (strcmp(cmd, "status") == 0) {
    cmdStatus(reply);
    return true;
  }
  if (strcmp(cmd, "save") == 0) {
    cmdSave(reply);
    return true;
  }
  if (strcmp(cmd, "wifi reset") == 0) {
    cmdWifiReset(reply);
    return true;
  }

  // Unknown command - show help
  sprintf(reply,
          "TCP bridge commands:\n"
          "  tcp mode 0|1    - Set mode (0=server, 1=client)\n"
          "  tcp port <num>  - Set TCP port (default 5555)\n"
          "  tcp host <ip>   - Set remote host (client mode)\n"
          "  tcp status      - Show connection status\n"
          "  tcp save        - Save configuration\n"
          "  tcp wifi reset  - Clear WiFi config, restart portal\n");
  return true;
}

void TCPWifiBridge::cmdMode(const char* arg, char* reply) {
  int mode = atoi(arg);
  if (mode < 0 || mode > 1) {
    sprintf(reply, "Invalid mode. Use 0 (server) or 1 (client)");
    return;
  }
  _config.mode = mode;
  sprintf(reply, "Mode set to %s. Use 'tcp save' to persist.", mode == 0 ? "server" : "client");
}

void TCPWifiBridge::cmdPort(const char* arg, char* reply) {
  int port = atoi(arg);
  if (port < 1 || port > 65535) {
    sprintf(reply, "Invalid port. Use 1-65535");
    return;
  }
  _config.port = port;
  sprintf(reply, "Port set to %d. Use 'tcp save' to persist.", port);
}

void TCPWifiBridge::cmdHost(const char* arg, char* reply) {
  // Skip leading spaces
  while (*arg == ' ') arg++;

  if (strlen(arg) == 0 || strlen(arg) >= sizeof(_config.host)) {
    sprintf(reply, "Invalid host. Max %d characters.", (int)(sizeof(_config.host) - 1));
    return;
  }
  strncpy(_config.host, arg, sizeof(_config.host) - 1);
  _config.host[sizeof(_config.host) - 1] = '\0';
  sprintf(reply, "Host set to %s. Use 'tcp save' to persist.", _config.host);
}

void TCPWifiBridge::cmdStatus(char* reply) {
  char* p = reply;

  p += sprintf(p, "TCP Bridge Status:\n");
  p += sprintf(p, "  Mode: %s\n", _config.mode == 0 ? "server" : "client");
  p += sprintf(p, "  Port: %d\n", _config.port);

  if (_config.mode == 1) {
    p += sprintf(p, "  Remote Host: %s\n", _config.host);
  }

  if (_in_portal_mode) {
    p += sprintf(p, "  State: Portal mode (AP: %s)\n", TCPWifiPortal::getAPSSID());
  } else if (!_wifi_connected) {
    p += sprintf(p, "  State: WiFi disconnected\n");
    p += sprintf(p, "  SSID: %s\n", _config.wifi_ssid);
  } else {
    p += sprintf(p, "  WiFi: Connected to %s\n", _config.wifi_ssid);
    p += sprintf(p, "  IP: %s\n", WiFi.localIP().toString().c_str());

    if (_client.connected()) {
      p += sprintf(p, "  TCP: Connected");
      if (_config.mode == 0) {
        p += sprintf(p, " (client: %s)", _client.remoteIP().toString().c_str());
      }
      p += sprintf(p, "\n");
    } else {
      p += sprintf(p, "  TCP: Disconnected\n");
    }
  }
}

void TCPWifiBridge::cmdSave(char* reply) {
  saveConfig();
  sprintf(reply, "Configuration saved. Restart bridge for changes to take effect.");
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
