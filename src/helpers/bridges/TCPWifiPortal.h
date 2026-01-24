#pragma once

#ifdef WITH_TCP_WIFI_BRIDGE

#include <DNSServer.h>
#include <WiFi.h>

// Forward declaration
struct TCPBridgeConfig;

/**
 * TCPWifiPortal - Self-contained captive portal for WiFi/TCP bridge configuration.
 *
 * When WiFi is not configured, this class creates an access point and captive portal
 * where users can enter WiFi credentials and TCP bridge settings. Once configured,
 * the device reboots and connects to the configured WiFi network.
 *
 * Flow:
 * 1. Start AP "MeshCore-TCP-Setup"
 * 2. User connects and is redirected to 192.168.4.1
 * 3. User enters WiFi SSID, password, TCP mode, host, port
 * 4. Config saved, device reboots
 */
class TCPWifiPortal {
public:
  TCPWifiPortal(TCPBridgeConfig* config);

  void begin();         // Start AP + DNS + web server
  void loop();          // Handle DNS and client requests
  bool isConfigured();  // Returns true when user submits form
  void stop();          // Shutdown portal

  static const char* getAPSSID() { return "MeshCore-TCP-Setup"; }

private:
  TCPBridgeConfig* _config;
  DNSServer _dns;
  WiFiServer* _webServer;
  bool _configured;
  bool _running;

  void handleClient(WiFiClient& client);
  void sendConfigPage(WiFiClient& client);
  void sendRedirect(WiFiClient& client);
  bool parseFormData(const String& body);
  String urlDecode(const String& input);
};

#endif  // WITH_TCP_WIFI_BRIDGE
