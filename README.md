# CloudflareDDNS
<b>DDNS by ESP32 c3 super mini</b><br>
<br>
以ESP32作為主體，通過Web Server架設一個DDNS服務並監控WIFI連接與IP變動。首先，開發者需配置本地伺服器，確保可透過Web瀏覽器訪問並管理DDNS資料。<br>
<br>
市場需求分析<br>
在當前網絡環境下，IPv4地址正在逐漸減少，這意味著未來接入數字世界時可能需要更有效的解決方案來保護自己的網路連接。<br>
DDNS（Domain Name System Update）提供了一種簡便的方法來管理IP地址變動，<br>
而ESP32則以其強大且節能的特性被廣泛應用於各種低功率IoT設備中。<br>
<br>
重要技術點<br>
WiFi連接與監控：確保ESP32能夠正確連接到WIFI網路，並實時監控該網路的狀態和連接情況。<br>
DDNS更新管理：通過Cloudflare進行DNS記錄更新，以實現自動化IP地址管理，提高效率和簡便性。<br>
Web Server架設與管理：透過ESP32控制一個Web Server來展示DDNS信息、維護WIFI連接狀態等功能。<br>
<br>
程式碼詳解<br>
1. 創建Web Server<br>
2. 獲取公網IP地址<br>
3. 更新DDNS<br>
4. 監控IP變動與WIFI連接狀態<br>
5. 創建並啟動Web Server<br>
<br>
結論<br>
通過上述程式碼，我們實現了一個簡單但功能強大的DDNS和WIFI監控系統。該系統透過ESP32作為主體，<br>
能夠自動更新DNS記錄、確保WiFi連接穩定，並提供靈活的Web管理接口。<br>
這在智能家居、IoT設備連接和維護方面都具有重要的應用價值。<br>
