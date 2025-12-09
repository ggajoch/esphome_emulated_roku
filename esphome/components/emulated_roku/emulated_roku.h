#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/network/util.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <lwip/igmp.h>
#include <lwip/ip_addr.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_wifi.h>

namespace esphome {
namespace emulated_roku {

// SSDP Constants
static const uint16_t SSDP_PORT = 1900;
static const IPAddress SSDP_MULTICAST_ADDR(239, 255, 255, 250);

// Device description matching real Roku 4 format exactly
static const char* ROKU_DEVICE_INFO_TEMPLATE = R"(<?xml version="1.0" encoding="UTF-8" ?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <specVersion>
    <major>1</major>
    <minor>0</minor>
  </specVersion>
  <device>
    <deviceType>urn:roku-com:device:player:1-0</deviceType>
    <friendlyName>%s</friendlyName>
    <manufacturer>Roku</manufacturer>
    <manufacturerURL>http://www.roku.com/</manufacturerURL>
    <modelDescription>Roku Streaming Player Network Media</modelDescription>
    <modelName>Roku 4</modelName>
    <modelNumber>4400X</modelNumber>
    <modelURL>http://www.roku.com/</modelURL>
    <serialNumber>%s</serialNumber>
    <UDN>uuid:roku:ecp:%s</UDN>
    <serviceList>
      <service>
        <serviceType>urn:roku-com:service:ecp:1</serviceType>
        <serviceId>urn:roku-com:serviceId:ecp1-0</serviceId>
        <controlURL/>
        <eventSubURL/>
        <SCPDURL>ecp_SCPD.xml</SCPDURL>
      </service>
    </serviceList>
  </device>
</root>)";

static const char* ROKU_APPS_TEMPLATE = R"(<apps>
    <app id="1" version="1.0.0">Emulated App 1</app>
    <app id="2" version="1.0.0">Emulated App 2</app>
</apps>)";

static const char* ROKU_ACTIVE_APP_TEMPLATE = R"(<active-app>
    <app>Roku</app>
</active-app>)";

static const char* ROKU_DEVICE_INFO_QUERY = R"(<?xml version="1.0" encoding="UTF-8" ?>
<device-info>
  <udn>%s</udn>
  <serial-number>%s</serial-number>
  <device-id>%s</device-id>
  <advertising-id>%s</advertising-id>
  <vendor-name>Roku</vendor-name>
  <model-number>4400X</model-number>
  <model-name>Roku 4</model-name>
  <model-region>US</model-region>
  <is-tv>true</is-tv>
  <is-stick>false</is-stick>
  <ui-resolution>1080p</ui-resolution>
  <supports-ethernet>true</supports-ethernet>
  <wifi-mac>%s</wifi-mac>
  <wifi-driver>realtek</wifi-driver>
  <has-wifi-extender>false</has-wifi-extender>
  <has-wifi-5G-support>true</has-wifi-5G-support>
  <can-use-wifi-extender>true</can-use-wifi-extender>
  <ethernet-mac>%s</ethernet-mac>
  <network-type>wifi</network-type>
  <friendly-device-name>%s</friendly-device-name>
  <friendly-model-name>Roku 4</friendly-model-name>
  <default-device-name>%s</default-device-name>
  <user-device-name>%s</user-device-name>
  <user-device-location></user-device-location>
  <build-number>AEA.09044.70</build-number>
  <software-version>9.4.0</software-version>
  <software-build>4170</software-build>
  <secure-device>true</secure-device>
  <language>en</language>
  <country>US</country>
  <locale>en_US</locale>
  <time-zone-auto>true</time-zone-auto>
  <time-zone>US/Pacific</time-zone>
  <time-zone-name>United States/Pacific</time-zone-name>
  <time-zone-tz>America/Los_Angeles</time-zone-tz>
  <time-zone-offset>-480</time-zone-offset>
  <clock-format>12-hour</clock-format>
  <uptime>86400</uptime>
  <power-mode>%s</power-mode>
  <supports-suspend>true</supports-suspend>
  <supports-find-remote>true</supports-find-remote>
  <supports-tv-power-control>true</supports-tv-power-control>
  <find-remote-is-possible>true</find-remote-is-possible>
  <supports-audio-guide>true</supports-audio-guide>
  <supports-rva>true</supports-rva>
  <developer-enabled>true</developer-enabled>
  <keyed-developer-id></keyed-developer-id>
  <search-enabled>true</search-enabled>
  <search-channels-enabled>true</search-channels-enabled>
  <voice-search-enabled>true</voice-search-enabled>
  <notifications-enabled>true</notifications-enabled>
  <notifications-first-use>false</notifications-first-use>
  <supports-private-listening>true</supports-private-listening>
  <headphones-connected>false</headphones-connected>
  <supports-ecs-textedit>true</supports-ecs-textedit>
  <supports-ecs-microphone>true</supports-ecs-microphone>
  <supports-wake-on-wlan>true</supports-wake-on-wlan>
  <has-play-on-roku>true</has-play-on-roku>
  <has-mobile-screensaver>false</has-mobile-screensaver>
  <support-url>roku.com/support</support-url>
  <grandcentral-version>4.8.25</grandcentral-version>
  <trc-version>3.0</trc-version>
  <trc-channel-version>4.2.6</trc-channel-version>
  <davinci-version>2.8.20</davinci-version>
</device-info>)";

class EmulatedRokuComponent : public Component {
 public:
  void set_device_name(const std::string &name) { device_name_ = name; }
  void set_port(uint16_t port) { port_ = port; }
  
  void add_on_key_press_callback(std::function<void(std::string, std::string)> callback) {
    key_press_callback_.add(std::move(callback));
  }

  void setup() override {
    // Generate a simple UUID from MAC address
    snprintf(uuid_, sizeof(uuid_), "roku-ecp-%08X", (uint32_t)ESP.getEfuseMac());
    snprintf(usn_, sizeof(usn_), "ESP32-%08X", (uint32_t)ESP.getEfuseMac());
    ESP_LOGI("emulated_roku", "Emulated Roku '%s' initialized", device_name_.c_str());
  }

  void loop() override {
    // Deferred initialization - wait for WiFi to connect
    if (!initialized_) {
      if (network::is_connected()) {
        start_servers();
        initialized_ = true;
      }
      return;
    }
    
    if (server_ != nullptr) {
      server_->handleClient();
    }
    handle_ssdp();
  }

  float get_setup_priority() const override {
    return setup_priority::AFTER_WIFI;
  }

 protected:
  std::string device_name_{"ESPHome Roku"};
  uint16_t port_{8060};
  char uuid_[32];
  char usn_[32];
  char local_ip_[16];
  char mac_addr_[18];
  WebServer *server_{nullptr};
  WiFiUDP notify_udp_;
  int mcast_sock_{-1};  // Raw socket for multicast SSDP
  CallbackManager<void(std::string, std::string)> key_press_callback_;
  unsigned long last_notify_{0};
  unsigned long last_igmp_refresh_{0};
  bool ssdp_started_{false};
  bool initialized_{false};
  static const unsigned long NOTIFY_INTERVAL = 5000; // 5 seconds for discovery
  static const unsigned long IGMP_REFRESH_INTERVAL = 60000; // Refresh IGMP membership every 60 seconds

  void start_servers() {
    // Cache the IP and MAC now that WiFi is connected
    // Use ESPHome network utilities which are more reliable than WiFi class
    auto ip_addresses = network::get_ip_addresses();
    if (!ip_addresses.empty() && ip_addresses[0].is_set()) {
      snprintf(local_ip_, sizeof(local_ip_), "%s", ip_addresses[0].str().c_str());
    } else {
      // Fallback to WiFi class
      IPAddress local = WiFi.localIP();
      snprintf(local_ip_, sizeof(local_ip_), "%d.%d.%d.%d", local[0], local[1], local[2], local[3]);
    }
    
    // Get MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_addr_, sizeof(mac_addr_), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    ESP_LOGI("emulated_roku", "WiFi connected, starting Emulated Roku on %s:%d (MAC: %s)", 
             local_ip_, port_, mac_addr_);
    
    // Create the web server
    server_ = new WebServer(port_);
    
    setup_ssdp();
    setup_http_server();
    
    ESP_LOGI("emulated_roku", "Emulated Roku started successfully");
  }

  void setup_ssdp() {
    // Use raw BSD sockets for multicast - more reliable than WiFiUDP
    create_multicast_socket();
  }

  void create_multicast_socket() {
    // Close existing socket if any
    if (mcast_sock_ >= 0) {
      lwip_close(mcast_sock_);
      mcast_sock_ = -1;
    }
    
    // Create UDP socket
    mcast_sock_ = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mcast_sock_ < 0) {
      ESP_LOGE("emulated_roku", "Failed to create multicast socket");
      return;
    }
    
    // Allow address reuse
    int reuse = 1;
    lwip_setsockopt(mcast_sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Bind to SSDP port on all interfaces (important: bind to INADDR_ANY, not local IP)
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SSDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Bind to all interfaces
    
    if (lwip_bind(mcast_sock_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      ESP_LOGE("emulated_roku", "Failed to bind multicast socket: %d", errno);
      lwip_close(mcast_sock_);
      mcast_sock_ = -1;
      return;
    }
    
    // Join multicast group - this is the key part
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
    mreq.imr_interface.s_addr = inet_addr(local_ip_);
    
    if (lwip_setsockopt(mcast_sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
      ESP_LOGE("emulated_roku", "Failed to join multicast group: %d", errno);
      lwip_close(mcast_sock_);
      mcast_sock_ = -1;
      return;
    }
    
    // Set socket to non-blocking
    int flags = lwip_fcntl(mcast_sock_, F_GETFL, 0);
    lwip_fcntl(mcast_sock_, F_SETFL, flags | O_NONBLOCK);
    
    ESP_LOGI("emulated_roku", "SSDP multicast socket created and joined group on %s", local_ip_);
    ssdp_started_ = true;
    last_igmp_refresh_ = millis();
  }

  void refresh_multicast_membership() {
    // Periodically refresh IGMP membership to ensure we stay in the group
    unsigned long now = millis();
    if (now - last_igmp_refresh_ > IGMP_REFRESH_INTERVAL) {
      if (mcast_sock_ >= 0) {
        // Re-join the multicast group to refresh membership
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
        mreq.imr_interface.s_addr = inet_addr(local_ip_);
        
        // Leave and rejoin to refresh
        lwip_setsockopt(mcast_sock_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        if (lwip_setsockopt(mcast_sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
          ESP_LOGW("emulated_roku", "IGMP refresh failed, recreating socket");
          create_multicast_socket();
        } else {
          ESP_LOGD("emulated_roku", "Refreshed IGMP membership");
        }
      }
      last_igmp_refresh_ = now;
    }
  }

  void setup_http_server() {
    // Root - device description
    server_->on("/", HTTP_GET, [this]() {
      char buffer[1024];
      snprintf(buffer, sizeof(buffer), ROKU_DEVICE_INFO_TEMPLATE, 
               device_name_.c_str(), usn_, uuid_);
      server_->send(200, "text/xml", buffer);
    });

    // Key press handlers - using path prefix matching
    server_->on("/keypress/", HTTP_POST, [this]() { handle_key_command("keypress"); });
    server_->on("/keydown/", HTTP_POST, [this]() { handle_key_command("keydown"); });
    server_->on("/keyup/", HTTP_POST, [this]() { handle_key_command("keyup"); });

    // Launch app handler
    server_->on("/launch/", HTTP_POST, [this]() {
      String uri = server_->uri();
      int idx = uri.lastIndexOf('/');
      if (idx >= 0) {
        String app_id = uri.substring(idx + 1);
        ESP_LOGI("emulated_roku", "Launch: %s", app_id.c_str());
      }
      server_->send(200, "text/plain", "OK");
    });

    // Query apps
    server_->on("/query/apps", HTTP_GET, [this]() {
      server_->send(200, "text/xml", ROKU_APPS_TEMPLATE);
    });

    // Query active app
    server_->on("/query/active-app", HTTP_GET, [this]() {
      server_->send(200, "text/xml", ROKU_ACTIVE_APP_TEMPLATE);
    });

    // Query device info
    server_->on("/query/device-info", HTTP_GET, [this]() {
      char buffer[3000];
      snprintf(buffer, sizeof(buffer), ROKU_DEVICE_INFO_QUERY,
               uuid_, usn_, usn_, usn_,  // udn, serial, device-id, advertising-id
               mac_addr_, mac_addr_, // wifi-mac, ethernet-mac
               device_name_.c_str(), device_name_.c_str(), device_name_.c_str(), // friendly, default, user
               "PowerOn"); // power-mode - always report as on
      server_->send(200, "text/xml", buffer);
    });

    // App icon (return a placeholder)
    server_->on("/query/icon/", HTTP_GET, [this]() {
      // 1x1 transparent PNG
      static const uint8_t placeholder_icon[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
        0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00,
        0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49,
        0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
      };
      server_->send_P(200, "image/png", (const char*)placeholder_icon, sizeof(placeholder_icon));
    });

    // Input handler (for search/voice)
    server_->on("/input", HTTP_POST, [this]() {
      server_->send(200, "text/plain", "OK");
    });

    // Search handler
    server_->on("/search", HTTP_POST, [this]() {
      server_->send(200, "text/plain", "OK");
    });

    // Handle 404 - also check for key/launch patterns here
    server_->onNotFound([this]() {
      String uri = server_->uri();
      HTTPMethod method = server_->method();
      
      // Log ALL requests for debugging
      ESP_LOGI("emulated_roku", "HTTP %s %s", 
               method == HTTP_GET ? "GET" : (method == HTTP_POST ? "POST" : "OTHER"),
               uri.c_str());
      
      // Check for keypress/keydown/keyup patterns
      if (uri.startsWith("/keypress/") || uri.startsWith("/keydown/") || uri.startsWith("/keyup/")) {
        if (method == HTTP_POST) {
          if (uri.startsWith("/keypress/")) {
            handle_key_command("keypress");
          } else if (uri.startsWith("/keydown/")) {
            handle_key_command("keydown");
          } else {
            handle_key_command("keyup");
          }
          return;
        }
      }
      
      // Check for launch pattern
      if (uri.startsWith("/launch/") && method == HTTP_POST) {
        int idx = uri.lastIndexOf('/');
        if (idx >= 0) {
          String app_id = uri.substring(idx + 1);
          ESP_LOGI("emulated_roku", "Launch: %s", app_id.c_str());
        }
        server_->send(200, "text/plain", "OK");
        return;
      }
      
      // Check for icon pattern
      if (uri.startsWith("/query/icon/") && method == HTTP_GET) {
        static const uint8_t placeholder_icon[] = {
          0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
          0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
          0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
          0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00,
          0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49,
          0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
        };
        server_->send_P(200, "image/png", (const char*)placeholder_icon, sizeof(placeholder_icon));
        return;
      }
      
      // Unknown request
      server_->send(200, "text/plain", "OK");
    });

    server_->begin();
    ESP_LOGD("emulated_roku", "HTTP server started on port %d", port_);
  }

  void handle_key_command(const char* type) {
    String uri = server_->uri();
    int idx = uri.lastIndexOf('/');
    if (idx >= 0) {
      String key = uri.substring(idx + 1);
      // URL decode if needed (handle Lit_ keys for text input)
      key = url_decode(key);
      
      ESP_LOGI("emulated_roku", "%s: %s", type, key.c_str());
      
      // Call the callback with type and key
      this->key_press_callback_.call(std::string(type), std::string(key.c_str()));
    }
    server_->send(200, "text/plain", "OK");
  }

  String url_decode(const String& str) {
    String decoded;
    char temp[] = "00";
    for (size_t i = 0; i < str.length(); i++) {
      if (str[i] == '%' && i + 2 < str.length()) {
        temp[0] = str[i + 1];
        temp[1] = str[i + 2];
        decoded += (char)strtol(temp, nullptr, 16);
        i += 2;
      } else if (str[i] == '+') {
        decoded += ' ';
      } else {
        decoded += str[i];
      }
    }
    return decoded;
  }

  void handle_ssdp() {
    if (!ssdp_started_ || mcast_sock_ < 0) {
      // Try to recover SSDP if it failed to start
      static unsigned long last_retry = 0;
      unsigned long now = millis();
      if (now - last_retry > 10000) {  // Retry every 10 seconds
        ESP_LOGW("emulated_roku", "SSDP not started, attempting to initialize");
        create_multicast_socket();
        last_retry = now;
      }
      return;
    }
    
    // Refresh IGMP membership periodically
    refresh_multicast_membership();
    
    // Handle incoming SSDP requests using raw socket
    char buffer[512];
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    
    int len = lwip_recvfrom(mcast_sock_, buffer, sizeof(buffer) - 1, 0, 
                            (struct sockaddr*)&remote_addr, &addr_len);
    
    if (len > 0) {
      buffer[len] = '\0';
      
      // Check for M-SEARCH request
      if (strstr(buffer, "M-SEARCH") != nullptr && 
          (strstr(buffer, "roku:ecp") != nullptr || strstr(buffer, "ssdp:all") != nullptr)) {
        char remote_ip[16];
        inet_ntoa_r(remote_addr.sin_addr, remote_ip, sizeof(remote_ip));
        uint16_t remote_port = ntohs(remote_addr.sin_port);
        
        ESP_LOGI("emulated_roku", "M-SEARCH from %s:%d", remote_ip, remote_port);
        send_ssdp_response(remote_addr);
      }
    }

    // Periodic SSDP notify
    unsigned long now = millis();
    if (now - last_notify_ > NOTIFY_INTERVAL || last_notify_ == 0) {
      send_ssdp_notify();
      last_notify_ = now;
    }
  }

  void send_ssdp_response(struct sockaddr_in &remote_addr) {
    // Match exact format from Python emulated_roku library that works with Harmony
    char response[512];
    snprintf(response, sizeof(response),
      "HTTP/1.1 200 OK\r\n"
      "Cache-Control: max-age = 300\r\n"
      "ST: roku:ecp\r\n"
      "SERVER: Roku/12.0.0 UPnP/1.0 Roku/12.0.0\r\n"
      "Ext:\r\n"
      "Location: http://%s:%d/\r\n"
      "USN: uuid:roku:ecp:%s\r\n"
      "\r\n",
      local_ip_,
      port_,
      usn_
    );

    // Send response back to requester using the same socket
    lwip_sendto(mcast_sock_, response, strlen(response), 0, 
                (struct sockaddr*)&remote_addr, sizeof(remote_addr));
    
    char remote_ip[16];
    inet_ntoa_r(remote_addr.sin_addr, remote_ip, sizeof(remote_ip));
    ESP_LOGI("emulated_roku", "Sent SSDP response to %s:%d", 
             remote_ip, ntohs(remote_addr.sin_port));
  }

  void send_ssdp_notify() {
    // Match exact format from Python emulated_roku library
    char notify[512];
    snprintf(notify, sizeof(notify),
      "NOTIFY * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "Cache-Control: max-age = 300\r\n"
      "NT: roku:ecp\r\n"
      "NTS: ssdp:alive\r\n"
      "SERVER: Roku/12.0.0 UPnP/1.0 Roku/12.0.0\r\n"
      "Location: http://%s:%d/\r\n"
      "USN: uuid:roku:ecp:%s\r\n"
      "\r\n",
      local_ip_,
      port_,
      usn_
    );

    // Send to standard SSDP multicast
    notify_udp_.beginPacket(SSDP_MULTICAST_ADDR, SSDP_PORT);
    notify_udp_.write((uint8_t*)notify, strlen(notify));
    notify_udp_.endPacket();
    
    // Also send to subnet broadcast as fallback for networks that block multicast
    IPAddress broadcast(10, 1, 1, 255);
    notify_udp_.beginPacket(broadcast, SSDP_PORT);
    notify_udp_.write((uint8_t*)notify, strlen(notify));
    notify_udp_.endPacket();
    
    // Send directly to Harmony Hub (10.1.1.104)
    IPAddress harmony_hub(10, 1, 1, 104);
    notify_udp_.beginPacket(harmony_hub, SSDP_PORT);
    notify_udp_.write((uint8_t*)notify, strlen(notify));
    notify_udp_.endPacket();
    
    ESP_LOGD("emulated_roku", "Sent SSDP notify (multicast + broadcast + harmony)");
  }
};

class KeyPressTrigger : public Trigger<std::string, std::string> {
 public:
  explicit KeyPressTrigger(EmulatedRokuComponent *parent) {
    parent->add_on_key_press_callback([this](std::string type, std::string key) {
      this->trigger(type, key);
    });
  }
};

}  // namespace emulated_roku
}  // namespace esphome
