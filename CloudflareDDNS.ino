#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <esp_task_wdt.h> // ESP32 引入watchdog

// ===== 请在此填入您的 Cloudflare API 资讯 =====
const char* CF_TOKEN = "";      // Cloudflare API Token
const char* CF_ZONE_ID = "";   // Zone ID
const char* CF_RECORD_ID = "";  // DNS Record ID
const char* CF_DOMAIN = "";
// =============================================

#define DDNS_CHECK_INTERVAL 600000  // 10 分鐘
#define SMART_CONFIG_TIMEOUT 60000 // 1 分鐘 SmartConfig 超時
// 定義看門狗超時時間（單位：毫秒，例如 20000 毫秒 = 20 秒）
#define WDT_TIMEOUT_MS 20000

// 全局變數
String currentWANIP = "";
String lastDDNSIP = "";
unsigned long lastDDNSUpdate = 0;
bool ddnsUpdateStatus = false;
String ddnsStatusMessage = "等待更新...";
bool wifiConnected = false;

WebServer server(80);

// ===== WiFi 連接函式 =====
void connectWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  delay(100);

  if (SPIFFS.exists("/wifi.txt")) {
    File f = SPIFFS.open("/wifi.txt", "r");
    if (f) {
      String savedSSID = f.readStringUntil('\n');
      String savedPASS = f.readStringUntil('\n');
      savedSSID.trim();
      savedPASS.trim();
      f.close();

      if (savedSSID.length() > 0) {
        Serial.println("[WiFi] 已保存憑證，嚐試連接: " + savedSSID);
        WiFi.begin(savedSSID.c_str(), savedPASS.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
          delay(500);
          Serial.print(".");
          attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("\n[WiFi] 已連接: " + WiFi.SSID());
          wifiConnected = true;
          return;
        }
      }
    }
  }
  
  WiFi.begin();
  // 等待 5 秒確認是否能自動連上
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 10) {
    vTaskDelay(pdMS_TO_TICKS(500));
    retry++;
    }
//如果沒連上網路進行自動配網設定,手機須要安裝ESPtouch App
  if (WiFi.status() != WL_CONNECTED) {

  // 开启 SmartConfig
  Serial.println("[WiFi] 開啟 SmartConfig，請使用 Esptouch App 配置");
  WiFi.beginSmartConfig();
  
  unsigned long startTime = millis();
  while (!WiFi.smartConfigDone()) {
    delay(100);
    if (millis() - startTime > SMART_CONFIG_TIMEOUT) {
      Serial.println("[WiFi] SmartConfig 超時");
      break;
    }
  }

  if (WiFi.smartConfigDone()) {
    Serial.println("[WiFi] SmartConfig 完成，保存憑證...");
    WiFi.stopSmartConfig();
    WiFi.persistent(true);

    File f = SPIFFS.open("/wifi.txt", "w");
    if (f) {
      f.println(WiFi.SSID());
      f.println(WiFi.psk());
      f.close();
      Serial.println("[WiFi] 憑證已保存");
    }

    delay(1000);
    WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    wifiConnected = true;
  }
} else {wifiConnected = true;}
}

void maintainWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] 斷開，重新連接...");
    WiFi.reconnect();
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] 重新連接成功");
      wifiConnected = true;
    } else {
      wifiConnected = false;
    }
  } else {
    wifiConnected = true;
  }
}

// ===== DDNS 函数 =====
String getWANIP() {
  HTTPClient http;
  http.begin("http://checkip.amazonaws.com");
  http.setTimeout(5000); 
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
  int httpCode = http.GET();
  String ip = "";
  if (httpCode == HTTP_CODE_OK) {
    http.getStream().setTimeout(2);
    char ipBuffer[16] = {0};
    int bytesRead = http.getStream().readBytesUntil('\n', ipBuffer, sizeof(ipBuffer) - 1);
    
    if (bytesRead > 0) {
      ip = String(ipBuffer);
      ip.trim(); // 確保徹底清除可能殘留的 \r 或空白
    }
    
    Serial.println(ip);
  } else {
    Serial.printf("取得 IP 失敗，錯誤碼: %d\n", httpCode);
  }
  http.end();
  return ip;
}

void updateCloudflareDDNS(String ip) {
  if (strlen(CF_TOKEN) == 0 || strlen(CF_ZONE_ID) == 0 || strlen(CF_RECORD_ID) == 0) {
    ddnsStatusMessage = "錯誤: API 資料未配置";
    ddnsUpdateStatus = false;
    return;
  }
  Serial.printf("\n[DDNS] 開始更新，目前 Heap 狀態: %d\n", ESP.getFreeHeap());

  WiFiClientSecure* client = new WiFiClientSecure();
  HTTPClient http;

  client->setInsecure();

  String url = "https://api.cloudflare.com/client/v4/zones/" + String(CF_ZONE_ID) + "/dns_records/" + String(CF_RECORD_ID);
  http.begin(*client, url);

  http.addHeader("Authorization", "Bearer " + String(CF_TOKEN));
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"type\":\"A\",\"name\":\"" + String(CF_DOMAIN) + "\",\"content\":\"" + ip + "\",\"ttl\":120,\"proxied\":false}";

  int httpResponseCode = http.PUT(payload);

  if (httpResponseCode > 0) {
    if (http.getStream().find("\"success\":true")) {
      ddnsStatusMessage = "更新成功: " + ip;
      ddnsUpdateStatus = true;
      Serial.println("[DDNS] 更新成功: " + ip);
    } else {
      ddnsStatusMessage = "更新失敗";
      ddnsUpdateStatus = false;
      Serial.printf("[DDNS] 更新失敗！HTTP 狀態碼: %d\n", httpResponseCode);
    }
  } else {
    ddnsStatusMessage = "HTTP 錯誤: " + String(httpResponseCode);
    ddnsUpdateStatus = false;
    Serial.printf("[DDNS] 網路連線錯誤碼: %d\n", httpResponseCode);
  }
  
  http.end();
  delete client; 
}

void checkDDNS() {
  if (wifiConnected && WiFi.status() == WL_CONNECTED) {
    Serial.println("[DDNS] 檢查 IP...");

    String wanIP = getWANIP();
    if (wanIP.length() > 0) {
      currentWANIP = wanIP;
      Serial.println("[DDNS] 目前公網 IP: " + wanIP);

      if (wanIP != lastDDNSIP || lastDDNSUpdate == 0) {
        Serial.println("[DDNS] IP 變動，更新 Cloudflare...");
        updateCloudflareDDNS(wanIP);
        lastDDNSIP = wanIP;
        lastDDNSUpdate = millis();
      } else {
        ddnsStatusMessage = "IP 沒變動";
        ddnsUpdateStatus = true;
      }
    } else {
      ddnsStatusMessage = "無法取得公網 IP";
      ddnsUpdateStatus = false;
    }
  } else {
    ddnsStatusMessage = "WiFi 未連接";
    ddnsUpdateStatus = false;
  }
}

// ===== Web Server 處理函式 =====
void handleRoot() {
  String clientIP = server.client().remoteIP().toString();  
  Serial.println("收到請求！來自 IP: " + clientIP);
    
  File file = SPIFFS.open("/index.html", "r");
  if (file) {
    server.sendHeader("Connection", "close"); 
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.sendHeader("Connection", "close"); 
    server.send(200, "text/plain", "index.html not found");
  }
}

void handleStyle() {
  File file = SPIFFS.open("/style.css", "r");
  if (file) {
    server.streamFile(file, "text/css");
    file.close();
  } else {
    server.send(200, "text/plain", "style.css not found");
  }
}

void handleStatus() {
  File file = SPIFFS.open("/config.html", "r");
  if (file) {
    server.streamFile(file, "text/html");
    file.close();
  } else {
    server.send(200, "text/plain", "config.html not found");
  }
}

void handleApiStatus() {
  String json = "{";
  json += "\"ip\":\"" + currentWANIP + "\",";
  json += "\"domain\":\"" + String(CF_DOMAIN) + "\",";
  json += "\"lastUpdate\":" + String(lastDDNSUpdate) + ",";
  json += "\"status\":" + String(ddnsUpdateStatus ? "true" : "false") + ",";
  json += "\"message\":\"" + ddnsStatusMessage + "\",";
  json += "\"wifiConnected\":" + String(wifiConnected ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiUpdate() {
  if (wifiConnected && WiFi.status() == WL_CONNECTED) {
    String wanIP = getWANIP();
    if (wanIP.length() > 0) {
      currentWANIP = wanIP;
      updateCloudflareDDNS(wanIP);
      lastDDNSIP = wanIP;
      lastDDNSUpdate = millis();
    }
  }
  server.send(200, "application/json", "{\"status\":" + String(ddnsUpdateStatus ? "true" : "false") + ",\"message\":\"" + ddnsStatusMessage + "\"}");
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleStyle);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/update", HTTP_GET, handleApiUpdate);
  server.begin();
  Serial.println("[Web] Web Server 已啟動");
}

// ===== Setup =====
unsigned long lastDDNSCheck = 0;
unsigned long lastWiFiCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================");
  Serial.println("  ESP32 C3 DDNS + Web Server");
  Serial.println("========================================\n");

  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] 初始化失敗");
  } else {
    Serial.println("[SPIFFS] 已連線");
  }

  connectWiFi();
  if (wifiConnected) {
    Serial.printf("[WiFi] 本地 IP: %s\n", WiFi.localIP().toString().c_str());
    startWebServer();
  }
// 初始化看門狗
  // 適用於 ESP32 Core 3.0.x 的新版看門狗初始化
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT_MS,     // 設定超時時間 (毫秒)
    .idle_core_mask = (1 << 0),       // 監控 Core 0 (ESP32-C3 是單核心)
    .trigger_panic = true             // 超時後觸發重啟
  };
  
  esp_task_wdt_init(&wdt_config);     // 傳入結構體指標
  esp_task_wdt_add(NULL);             // 將目前的主執行緒（Loop Task）加入監控  

}

void loop() {
  esp_task_wdt_reset();

  server.handleClient();

  unsigned long now = millis();

  // WiFi 維護 (每 5 秒)
  if (now - lastWiFiCheck > 5000) {
    maintainWiFi();
    lastWiFiCheck = now;
  }

  // DDNS 檢查 (每 5 分鐘)
  if (now - lastDDNSCheck > DDNS_CHECK_INTERVAL) {
    checkDDNS();
    lastDDNSCheck = now;
  }

  delay(10);
}
