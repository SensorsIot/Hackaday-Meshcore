#ifdef WITH_TCP_WIFI_BRIDGE

#include "TCPWifiPortal.h"
#include "TCPWifiBridge.h"

// HTML page stored in PROGMEM to save RAM
// Simplified: only WiFi credentials needed - peer discovery is automatic
static const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>MeshCore TCP Bridge Setup</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a2e; color: #eee; }
    .container { max-width: 400px; margin: 0 auto; }
    h1 { color: #00d4ff; font-size: 1.5em; text-align: center; }
    h2 { color: #aaa; font-size: 1em; margin-top: 20px; border-bottom: 1px solid #333; padding-bottom: 5px; }
    input { width: 100%; padding: 12px; margin: 8px 0; box-sizing: border-box;
            background: #16213e; border: 1px solid #0f3460; color: #eee; border-radius: 4px; }
    input:focus { border-color: #00d4ff; outline: none; }
    button { width: 100%; padding: 14px; margin-top: 20px; background: #00d4ff; color: #1a1a2e;
             border: none; border-radius: 4px; font-size: 1.1em; cursor: pointer; font-weight: bold; }
    button:hover { background: #00b8e6; }
    label { color: #aaa; font-size: 0.9em; }
    .note { color: #888; font-size: 0.85em; margin-top: 15px; padding: 10px;
            background: #16213e; border-radius: 4px; border-left: 3px solid #00d4ff; }
    .auto-badge { display: inline-block; background: #00d4ff; color: #1a1a2e;
                  padding: 2px 8px; border-radius: 10px; font-size: 0.7em;
                  margin-left: 5px; vertical-align: middle; }
  </style>
</head>
<body>
  <div class="container">
    <h1>MeshCore TCP Bridge</h1>
    <form method="POST" action="/save">
      <h2>WiFi Settings</h2>
      <label>WiFi Network (SSID)</label>
      <input type="text" name="ssid" maxlength="32" required placeholder="Enter WiFi name">
      <label>WiFi Password</label>
      <input type="password" name="pass" maxlength="64" placeholder="Enter WiFi password">

      <h2>Peer Discovery <span class="auto-badge">AUTO</span></h2>
      <div class="note">
        <strong>No manual configuration needed!</strong><br><br>
        This repeater will automatically discover and connect to other
        MeshCore repeaters on the same WiFi network.<br><br>
        Just configure WiFi on both repeaters - they'll find each other.
      </div>

      <button type="submit">Save &amp; Connect</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

static const char SUCCESS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Configuration Saved</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a2e; color: #eee; text-align: center; }
    .container { max-width: 400px; margin: 50px auto; }
    h1 { color: #00ff88; }
    p { color: #aaa; }
    .info { background: #16213e; padding: 15px; border-radius: 4px; margin-top: 20px; text-align: left; }
    .info li { margin: 8px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Configuration Saved!</h1>
    <p>The device will now reboot and connect to your WiFi network.</p>
    <div class="info">
      <strong>What happens next:</strong>
      <ul>
        <li>Device connects to WiFi</li>
        <li>Broadcasts discovery to find peers</li>
        <li>Automatically pairs with available repeater</li>
      </ul>
    </div>
    <p>You can close this page.</p>
  </div>
</body>
</html>
)rawliteral";

// Constants for portal
static const uint16_t PORTAL_DNS_PORT = 53;
static const uint16_t PORTAL_HTTP_PORT = 80;
static const char* PORTAL_AP_SSID = "MeshCore-TCP-Setup";

TCPWifiPortal::TCPWifiPortal(TCPBridgeConfig* config)
    : _config(config), _webServer(nullptr), _configured(false), _running(false) {}

void TCPWifiPortal::begin() {
  if (_running) return;

  // Start WiFi in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(PORTAL_AP_SSID);
  delay(100);  // Allow AP to start

  // Start DNS server to redirect all requests to our IP
  _dns.start(PORTAL_DNS_PORT, "*", WiFi.softAPIP());

  // Start web server
  _webServer = new WiFiServer(PORTAL_HTTP_PORT);
  _webServer->begin();

  _running = true;
  _configured = false;

  Serial.printf("[TCPPortal] Started AP: %s, IP: %s\n", PORTAL_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void TCPWifiPortal::loop() {
  if (!_running) return;

  // Process DNS requests (for captive portal redirect)
  _dns.processNextRequest();

  // Check for web clients
  WiFiClient client = _webServer->available();
  if (client) {
    handleClient(client);
  }
}

void TCPWifiPortal::handleClient(WiFiClient& client) {
  String request = "";
  String body = "";
  bool isPost = false;
  int contentLength = 0;

  // Read HTTP request
  unsigned long timeout = millis() + 2000;
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();

      if (request.isEmpty()) {
        request = line;
        isPost = line.startsWith("POST");
      }

      // Parse Content-Length header
      if (line.startsWith("Content-Length:")) {
        contentLength = line.substring(15).toInt();
      }

      // Empty line marks end of headers
      if (line.isEmpty()) {
        // Read body for POST requests
        if (isPost && contentLength > 0) {
          body = "";
          while (body.length() < (unsigned int)contentLength && millis() < timeout) {
            if (client.available()) {
              body += (char)client.read();
            }
          }
        }
        break;
      }
    }
  }

  // Route request
  if (request.indexOf("POST /save") >= 0) {
    if (parseFormData(body)) {
      // Send success page
      String response = "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: text/html\r\n";
      response += "Connection: close\r\n\r\n";
      client.print(response);
      client.print(FPSTR(SUCCESS_PAGE));
      client.flush();
      delay(100);
      _configured = true;
    } else {
      sendConfigPage(client);
    }
  } else if (request.indexOf("/generate_204") >= 0 || request.indexOf("/hotspot-detect") >= 0 ||
             request.indexOf("/connecttest") >= 0 || request.indexOf("/redirect") >= 0 ||
             request.indexOf("/ncsi") >= 0) {
    // Captive portal detection - redirect to config page
    sendRedirect(client);
  } else {
    // Serve config page for all other requests
    sendConfigPage(client);
  }

  client.stop();
}

void TCPWifiPortal::sendConfigPage(WiFiClient& client) {
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: text/html\r\n";
  response += "Connection: close\r\n\r\n";
  client.print(response);
  client.print(FPSTR(CONFIG_PAGE));
}

void TCPWifiPortal::sendRedirect(WiFiClient& client) {
  String response = "HTTP/1.1 302 Found\r\n";
  response += "Location: http://192.168.4.1/\r\n";
  response += "Connection: close\r\n\r\n";
  client.print(response);
}

bool TCPWifiPortal::parseFormData(const String& body) {
  // Parse URL-encoded form data: ssid=xxx&pass=xxx
  // Simplified: no more mode/port/host fields - all automatic

  // Extract SSID
  int ssidStart = body.indexOf("ssid=");
  if (ssidStart < 0) return false;
  ssidStart += 5;
  int ssidEnd = body.indexOf('&', ssidStart);
  if (ssidEnd < 0) ssidEnd = body.length();
  String ssid = urlDecode(body.substring(ssidStart, ssidEnd));
  if (ssid.isEmpty()) return false;

  // Extract password
  String pass = "";
  int passStart = body.indexOf("pass=");
  if (passStart >= 0) {
    passStart += 5;
    int passEnd = body.indexOf('&', passStart);
    if (passEnd < 0) passEnd = body.length();
    pass = urlDecode(body.substring(passStart, passEnd));
  }

  // Save to config (only WiFi credentials)
  strncpy(_config->wifi_ssid, ssid.c_str(), sizeof(_config->wifi_ssid) - 1);
  _config->wifi_ssid[sizeof(_config->wifi_ssid) - 1] = '\0';

  strncpy(_config->wifi_pass, pass.c_str(), sizeof(_config->wifi_pass) - 1);
  _config->wifi_pass[sizeof(_config->wifi_pass) - 1] = '\0';

  Serial.printf("[TCPPortal] Config received: SSID=%s (auto-discovery enabled)\n", _config->wifi_ssid);

  return true;
}

String TCPWifiPortal::urlDecode(const String& input) {
  String decoded = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '+') {
      decoded += ' ';
    } else if (c == '%' && i + 2 < input.length()) {
      String hex = input.substring(i + 1, i + 3);
      decoded += (char)strtol(hex.c_str(), nullptr, 16);
      i += 2;
    } else {
      decoded += c;
    }
  }
  return decoded;
}

bool TCPWifiPortal::isConfigured() {
  return _configured;
}

void TCPWifiPortal::stop() {
  if (!_running) return;

  _dns.stop();
  if (_webServer) {
    _webServer->stop();
    delete _webServer;
    _webServer = nullptr;
  }
  WiFi.softAPdisconnect(true);
  _running = false;

  Serial.println("[TCPPortal] Stopped");
}

#endif  // WITH_TCP_WIFI_BRIDGE
