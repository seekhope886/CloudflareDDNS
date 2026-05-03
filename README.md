# CloudflareDDNS
DDNS by ESP32 c3 super mini
異構尋找
本篇說明以ESP32作為主體，通過Web Server架設一個DDNS服務並監控WIFI連接與IP變動。首先，開發者需配置本地伺服器，確保可透過Web瀏覽器訪問並管理DDNS資料。

市場需求分析
在當前網絡環境下，IPv4地址正在逐漸減少，這意味著未來接入數字世界時可能需要更有效的解決方案來保護自己的網路連接。DDNS（Domain Name System Update）提供了一種簡便的方法來管理IP地址變動，而ESP32則以其強大且節能的特性被廣泛應用於各種低功率IoT設備中。

重要技術點
WiFi連接與監控：確保ESP32能夠正確連接到WIFI網路，並實時監控該網路的狀態和連接情況。
DDNS更新管理：通過Cloudflare進行DNS記錄更新，以實現自動化IP地址管理，提高效率和簡便性。
Web Server架設與管理：透過ESP32控制一個Web Server來展示DDNS信息、維護WIFI連接狀態等功能。
程式碼詳解
1. 創建Web Server
void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  // ... other routes ...
}
2. 獲取公網IP地址
String getWANIP() {
  HTTPClient http;
  http.begin("http://checkip.amazonaws.com");
  int httpCode = http.GET();
  String ip = "";
  if (httpCode == HTTP_CODE_OK) {
    ip = http.getString();
    ip.trim();
    Serial.println(ip);
  } else {
    Serial.printf("取得 IP 失敗，錯誤碼: %d\n", httpCode);
  }
  http.end();
  return ip;
}
3. 更新DDNS
void updateCloudflareDDNS(String ip) {
  // ... (同上)
}
4. 監控IP變動與WIFI連接狀態
String currentWANIP = "";
unsigned long lastWiFiCheck = 0;
bool ddnsUpdateStatus = false;

void checkDDNS() {
  if (wifiConnected && WiFi.status() == WL_CONNECTED) {
    Serial.println("[DDNS] 檢查 IP...");
    
    String wanIP = getWANIP();
    if (wanIP.length() > 0) {
      currentWANIP = wanIP;
      Serial.println("[DDNS] 目前公網 IP: " + wanIP);

      if (wanIP != lastWiFiCheck && lastWiFiUpdate == 0) {
        Serial.println("[DDNS] IP 變動，更新 Cloudflare...");
        updateCloudflareDDNS(wanIP);
        lastWiFiUpdate = millis();
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
5. 創建並啟動Web Server
void setup() {
  // ... (同上)
  
  startWebServer();
}

unsigned long lastDDNSCheck = 0;
unsigned long lastWiFiCheck = 0;

void loop() {
  server.handleClient();

  unsigned long now = millis();

  if (now - lastWiFiCheck > 5000) {
    maintainWiFi();
    lastWiFiCheck = now;
  }

  if (now - lastDDNSCheck > DDNS_CHECK_INTERVAL) {
    checkDDNS();
    lastDDNSCheck = now;
  }
}
結論
通過上述程式碼，我們實現了一個簡單但功能強大的DDNS和WIFI監控系統。該系統透過ESP32作為主體，能夠自動更新DNS記錄、確保WiFi連接穩定，並提供靈活的Web管理接口。這在智能家居、IoT設備連接和維護方面都具有重要的應用價值。
