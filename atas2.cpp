#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <time.h>
#include <SD.h>
#include <SPI.h>

// Toplanma noktası koordinatları
#define TOPLANMA_LAT 40.9769
#define TOPLANMA_LON 28.8692

WebServer server(80);

// Wi-Fi AP bilgileri
const char* ssid = "TOPLANMA_ALANI_1";
const char* password = "afet1234";

// HTML kayıt formu (ESP32 belleğinde)
const char* htmlForm = R"rawliteral(
<!DOCTYPE html>
<html>
  <body>
    <h2>Toplanma Alanı Kayıt Formu</h2>
    <form action="/register" method="post">
      İsim: <input type="text" name="name">
      <input type="submit" value="Kaydol">
    </form>
  </body>
</html>
)rawliteral";

// Harita sayfası (Leaflet ile)
const char* mapPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Toplanma Noktası</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.7.1/dist/leaflet.css" />
  <style>
    #map { height: 90vh; width: 100%; }
  </style>
</head>
<body>
  <h3>Toplanma Alanı Haritası</h3>
  <div id="map"></div>

  <script src="https://unpkg.com/leaflet@1.7.1/dist/leaflet.js"></script>
  <script>
    // Haritayı başlat ve merkezle
    const map = L.map('map').setView([%LAT%, %LON%], 16);

    // OpenStreetMap katmanı ekle
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      maxZoom: 19
    }).addTo(map);

    // Toplanma noktası işaretçisi
    const musterPoint = L.marker([%LAT%, %LON%]).addTo(map)
      .bindPopup('Toplanma Noktası').openPopup();

    // Kullanıcının konumunu göster
    navigator.geolocation.getCurrentPosition(function(pos) {
      const userLat = pos.coords.latitude;
      const userLon = pos.coords.longitude;

      L.marker([userLat, userLon]).addTo(map)
        .bindPopup('Siz buradasınız').openPopup();

      map.setView([userLat, userLon], 16);
    });
  </script>
</body>
</html>
)rawliteral";

// Ana sayfa: Kayıt ve harita linkleri
void handleIndex() {
  String page = "<h2>Toplanma Alanı</h2>"
                "<a href='/form'>Kayıt Ol</a><br>"
                "<a href='/map'>Haritayı Gör</a>";
  server.send(200, "text/html", page);
}

// Kayıt formu sayfası
void handleRoot() {
  server.send(200, "text/html", htmlForm);
}

// Harita sayfası
void handleMap() {
  String page = mapPage;
  page.replace("%LAT%", String(TOPLANMA_LAT, 6));
  page.replace("%LON%", String(TOPLANMA_LON, 6));
  server.send(200, "text/html", page);
}

// Kayıt işlemi (SPIFFS'e yazılır)
void handleRegister() {
  String name = server.arg("name");

  // Basit zaman damgası (gerçek saat yoksa millis() kullanılır)
  String timestamp = String(millis());

  File file = SPIFFS.open("/log.txt", FILE_APPEND);
  if (file) {
    file.println(name + "," + timestamp);
    file.close();
    server.send(200, "text/plain", "Kayıt başarılı! Teşekkürler.");
  } else {
    server.send(500, "text/plain", "Dosyaya erişilemedi.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Dosya sistemi başlat
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS başlatılamadı!");
    return;
  }

  // Wi-Fi Access Point başlat
  WiFi.softAP(ssid, password);
  Serial.println("SoftAP aktif: " + WiFi.softAPIP().toString());

  // HTTP yollar
  server.on("/", handleIndex);
  server.on("/form", handleRoot);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/map", handleMap);
  server.begin();
  Serial.println("Web server çalışıyor.");
}

void loop() {
  server.handleClient();
}
