#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>

// Toplanma alanı koordinatları (Bakırköy örneği)
#define TOPLANMA_LAT 40.9769
#define TOPLANMA_LON 28.8692

WebServer server(80);

// Wi-Fi Access Point ayarları
const char* ssid = "TOPLANMA_ALANI_1";
const char* password = "afet1234";

// Modern ve şık CSS (inline olarak, sayfalarda kullanılacak)
const char* webStyle = R"rawliteral(
body {
  font-family: 'Segoe UI', Arial, sans-serif;
  background: linear-gradient(120deg, #e0eafc, #cfdef3);
  margin: 0;
  padding: 0;
}
h2, h3 {
  color: #2c3e50;
  text-align: center;
  margin-top: 32px;
}
#main-container {
  max-width: 420px;
  margin: 40px auto;
  background: #fff;
  border-radius: 16px;
  box-shadow: 0 4px 24px rgba(44,62,80,0.08);
  padding: 32px 24px 24px 24px;
}
form {
  display: flex;
  flex-direction: column;
  gap: 18px;
}
input[type="text"] {
  padding: 10px 12px;
  border: 1px solid #bfc9d1;
  border-radius: 6px;
  font-size: 1rem;
  transition: border 0.2s;
}
input[type="text"]:focus {
  border: 1.5px solid #3498db;
  outline: none;
}
input[type="submit"] {
  background: linear-gradient(90deg, #3498db, #6dd5fa);
  color: #fff;
  border: none;
  border-radius: 6px;
  padding: 12px;
  font-size: 1rem;
  font-weight: bold;
  cursor: pointer;
  transition: background 0.2s;
}
input[type="submit"]:hover {
  background: linear-gradient(90deg, #2980b9, #3498db);
}
a {
  display: inline-block;
  margin: 12px 0;
  color: #3498db;
  text-decoration: none;
  font-weight: 500;
  transition: color 0.2s;
}
a:hover {
  color: #217dbb;
  text-decoration: underline;
}
#map {
  border-radius: 12px;
  box-shadow: 0 2px 12px rgba(44,62,80,0.10);
  margin: 0 auto 24px auto;
}
)rawliteral";

// Leaflet harita kütüphanesi için temel CSS (kısaltılmış)
const char* leafletCss = R"rawliteral(
.leaflet-container { height: 100%; width: 100%; position: relative; outline: none; }
.leaflet-tile { position: absolute; left: 0; top: 0; }
.leaflet-marker-icon, .leaflet-marker-shadow { display: block; }
.leaflet-pane, .leaflet-tile, .leaflet-marker-icon, .leaflet-marker-shadow { position: absolute; left: 0; top: 0; }
)rawliteral";

// Leaflet harita kütüphanesi için temel JS (minified, 7x7 tile grid)
const char* leafletJs = R"rawliteral(
// Basit Leaflet loader (CDN yerine temel fonksiyonlar)
var L = window.L = {};
L.map = function(id) { return new window.LMap(id); };
function LMap(id) {
  this._el = document.getElementById(id);
  this.setView = function(center, zoom) {
    this.center = center; this.zoom = zoom;
    return this;
  };
  this.addLayer = function(layer) { layer.addTo(this); return this; };
}
L.tileLayer = function(url, opts) {
  return {
    addTo: function(map) {
      var z = map.zoom, c = map.center;
      // 7x7 grid, center at (38023, 24576)
      var centerX = 38023, centerY = 24576;
      var tileSize = 256;
      var gridSize = 7;
      var offset = Math.floor(gridSize / 2);
      var mapDiv = map._el;
      mapDiv.style.position = 'relative';
      mapDiv.style.width = (tileSize * gridSize) + 'px';
      mapDiv.style.height = (tileSize * gridSize) + 'px';
      mapDiv.style.margin = '0 auto';
      for (var dx = -offset; dx <= offset; dx++) {
        for (var dy = -offset; dy <= offset; dy++) {
          var x = centerX + dx;
          var y = centerY + dy;
          var tileUrl = url.replace('{z}', z).replace('{x}', x).replace('{y}', y);
          var img = document.createElement('img');
          img.src = tileUrl;
          img.style.width = tileSize + 'px';
          img.style.height = tileSize + 'px';
          img.style.position = 'absolute';
          img.style.left = ((dx + offset) * tileSize) + 'px';
          img.style.top = ((dy + offset) * tileSize) + 'px';
          mapDiv.appendChild(img);
        }
      }
    }
  };
};
L.marker = function(latlng) {
  return {
    addTo: function(map) {
      var marker = document.createElement('div');
      marker.style.position = 'absolute';
      marker.style.left = '50%'; marker.style.top = '50%';
      marker.style.transform = 'translate(-50%, -100%)';
      marker.style.width = '24px'; marker.style.height = '24px';
      marker.style.background = 'red'; marker.style.borderRadius = '50%';
      marker.title = 'Konum';
      map._el.appendChild(marker);
      return this;
    },
    bindPopup: function(text) { return this; }
  };
};
)rawliteral";

// Kayıt formu HTML (kullanıcıdan isim alır)
const char* htmlForm = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Kayıt Formu</title>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <style>%WEBSTYLE%</style>
</head>
<body>
  <div id='main-container'>
    <h2>Toplanma Alanı Kayıt Formu</h2>
    <form action='/register' method='post'>
      İsim: <input type='text' name='name'>
      <input type='submit' value='Kaydol'>
    </form>
  </div>
</body>
</html>
)rawliteral";

// Harita sayfası HTML (Leaflet ile, kullanıcı ve toplanma noktası)
const char* mapPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Toplanma Noktası</title>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <style>%LEAFLET_CSS%</style>
  <style>%WEBSTYLE%</style>
</head>
<body>
  <h3>Toplanma Alanı Haritası</h3>
  <div id='map' style='height:90vh;width:100%;position:relative;'></div>
  <script>%LEAFLET_JS%</script>
  <script>
    const map = L.map('map').setView([%LAT%, %LON%], 16);
    L.tileLayer('/tilesText/{z}/{x}/{y}.png', {
      maxZoom: 19,
      tileSize: 256
    }).addTo(map);
    L.marker([%LAT%, %LON%]).addTo(map).bindPopup('Toplanma Noktası');
    if (navigator.geolocation) {
      navigator.geolocation.getCurrentPosition(function(pos) {
        const userLat = pos.coords.latitude;
        const userLon = pos.coords.longitude;
        L.marker([userLat, userLon]).addTo(map).bindPopup('Siz buradasınız');
      });
    }
  </script>
</body>
</html>
)rawliteral";

// Ana sayfa: yönlendirme ve ana menü
void handleIndex() {
  String page = "<meta charset='utf-8'>";
  page += "<div id='main-container'>"
                "<h2>Toplanma Alanı</h2>"
                "<a href='/form'>Kayıt Ol</a><br>"
                "<a href='/map'>Haritayı Gör</a><br>"
                "<a href='/usersearch' style='display:inline-block;margin-top:12px;padding:10px 18px;background:#3498db;color:#fff;border-radius:6px;text-decoration:none;font-weight:500;'>Kullanıcı Ara</a>"
                "</div>"
                "<style>" + String(webStyle) + "</style>";
  server.send(200, "text/html; charset=utf-8", page);
}

// Kayıt formu sayfası
void handleRoot() {
  String page = "<meta charset='utf-8'>" + String(htmlForm);
  page.replace("%WEBSTYLE%", webStyle);
  server.send(200, "text/html; charset=utf-8", page);
}

// Harita sayfası
void handleMap() {
  String page = "<meta charset='utf-8'>" + String(mapPage);
  page.replace("%LAT%", String(TOPLANMA_LAT, 6));
  page.replace("%LON%", String(TOPLANMA_LON, 6));
  page.replace("%LEAFLET_CSS%", leafletCss);
  page.replace("%LEAFLET_JS%", leafletJs);
  page.replace("%WEBSTYLE%", webStyle);
  server.send(200, "text/html; charset=utf-8", page);
}

// Kayıt işlemi (isim, zaman, alan kaydeder)
void handleRegister() {
  String name = server.arg("name");
  String timestamp = String(millis());
  String area = ssid; // Alan bilgisini ssid olarak kullan
  String record = "{\"name\":\"" + name + "\",\"timestamp\":\"" + timestamp + "\",\"area\":\"" + area + "\"}";
  if (!SD.begin()) {
    server.send(500, "text/plain; charset=utf-8", "SD kart başlatılamadı!");
    return;
  }
  File file = SD.open("/users.json", FILE_APPEND);
  bool ok = false;
  if (file) {
    file.println(record);
    file.close();
    ok = true;
  }
  // Kayıt sonucu için kullanıcıya mesaj göster
  String html = "<meta charset='utf-8'><div id='main-container' style='text-align:center;'>";
  if (ok) {
    html += "<div style='margin:32px auto 18px auto;width:80px;height:80px;background:#e0f7e9;border-radius:50%;display:flex;align-items:center;justify-content:center;box-shadow:0 2px 12px rgba(44,62,80,0.10);'><svg width='48' height='48' viewBox='0 0 24 24' fill='none' stroke='#27ae60' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'><path d='M20 6L9.5 17L4 11.5'/></svg></div>";
    html += "<h2>Kayıt Başarılı!</h2>";
    html += "<div style='font-size:1.1rem;margin:18px 0 0 0;'>Hoş geldiniz, <b>" + name + "</b>!<br>Kaydınız başarıyla alınmıştır.</div>";
    html += "<a href='/' style='margin-top:24px;display:inline-block;'>Ana Sayfa</a>";
  } else {
    html += "<h2>Kayıt Başarısız</h2><div style='color:#c0392b;'>SD karta erişilemedi.</div>";
    html += "<a href='/form' style='margin-top:24px;display:inline-block;'>Tekrar Dene</a>";
  }
  html += "</div><style>" + String(webStyle) + "</style>";
  server.send(ok ? 200 : 500, "text/html; charset=utf-8", html);
}

// Kullanıcı arama işlemi (isimle arama yapar)
void handleUserSearch() {
  if (!server.hasArg("name")) {
    server.send(400, "text/html; charset=utf-8", "<meta charset='utf-8'><div id='main-container'><h2>İsim giriniz!</h2><a href='/usersearch'>Geri Dön</a></div><style>" + String(webStyle) + "</style>");
    return;
  }
  String searchName = server.arg("name");
  if (!SD.begin()) {
    server.send(500, "text/html; charset=utf-8", "<meta charset='utf-8'><div id='main-container'><h2>SD kart başlatılamadı!</h2></div><style>" + String(webStyle) + "</style>");
    return;
  }
  File file = SD.open("/users.json", FILE_READ);
  if (!file) {
    server.send(404, "text/html; charset=utf-8", "<meta charset='utf-8'><div id='main-container'><h2>Kayıt dosyası yok.</h2></div><style>" + String(webStyle) + "</style>");
    return;
  }
  bool found = false;
  String line;
  String foundUser;
  // Dosyada aranan ismi bul
  while (file.available()) {
    line = file.readStringUntil('\n');
    if (line.length() == 0) continue;
    int idx = line.indexOf("\"name\":\"");
    if (idx >= 0) {
      int start = idx + 8;
      int end = line.indexOf('"', start);
      String userName = line.substring(start, end);
      if (userName.equalsIgnoreCase(searchName) || userName.startsWith(searchName)) {
        found = true;
        foundUser = line;
        break;
      }
    }
  }
  file.close();
  if (found) {
    // JSON'dan alanları ayıkla
    String name, timestamp, area;
    int n1 = foundUser.indexOf("\"name\":\"");
    int n2 = foundUser.indexOf('"', n1 + 8);
    name = foundUser.substring(n1 + 8, n2);
    int t1 = foundUser.indexOf("\"timestamp\":\"");
    int t2 = foundUser.indexOf('"', t1 + 13);
    timestamp = foundUser.substring(t1 + 13, t2);
    int a1 = foundUser.indexOf("\"area\":\"");
    int a2 = foundUser.indexOf('"', a1 + 8);
    area = foundUser.substring(a1 + 8, a2);
    // Zamanı insan okunur hale getir
    unsigned long ts = timestamp.toInt();
    unsigned long seconds = ts / 1000;
    unsigned long ms = ts % 1000;
    unsigned long mins = seconds / 60;
    unsigned long hours = mins / 60;
    unsigned long days = hours / 24;
    hours = hours % 24;
    mins = mins % 60;
    seconds = seconds % 60;
    char dateStr[64];
    snprintf(dateStr, sizeof(dateStr), "%lu gün, %02lu:%02lu:%02lu.%03lu", days, hours, mins, seconds, ms);
    // Sonucu kullanıcıya göster
    String html = "<meta charset='utf-8'><div id='main-container' style='text-align:center;'>";
    html += "<h2>Kullanıcı Bulundu</h2>";
    html += "<div style='font-size:1.1rem;margin:18px 0;padding:18px 0;background:#f4f8fb;border-radius:10px;box-shadow:0 2px 8px rgba(44,62,80,0.07);'>";
    html += "<b>İsim:</b> " + name + "<br>";
    html += "<b>Kayıt Zamanı:</b> " + String(dateStr) + "<br>";
    html += "<b>Alan:</b> " + area + "<br>";
    html += "</div>";
    html += "<a href='/usersearch' style='margin-top:18px;display:inline-block;'>Yeni Arama</a>";
    html += "<a href='/' style='margin-left:18px;display:inline-block;'>Ana Sayfa</a>";
    html += "</div><style>" + String(webStyle) + "</style>";
    server.send(200, "text/html; charset=utf-8", html);
  } else {
    server.send(404, "text/html; charset=utf-8", "<meta charset='utf-8'><div id='main-container'><h2>Kullanıcı bulunamadı.</h2><a href='/usersearch'>Geri Dön</a></div><style>" + String(webStyle) + "</style>");
  }
}

// Kullanıcı arama formu (isimle arama yapılır)
void handleUserSearchPage() {
  String page = "<meta charset='utf-8'>";
  page += "<div id='main-container'>"
          "<h2>Kullanıcı Ara</h2>"
          "<form action='/usersearch' method='get'>"
          "İsim: <input type='text' name='name'>"
          "<input type='submit' value='Ara'>"
          "</form>"
          "<a href='/' style='margin-top:16px;display:inline-block;'>Ana Sayfa</a>"
          "</div>"
          "<style>" + String(webStyle) + "</style>";
  server.send(200, "text/html; charset=utf-8", page);
}

// Tile dosyalarını SD karttan sunar (harita için)
void handleTile() {
  String path = server.uri(); // Örnek: /tilesText/16/38023/24576.png
  if (!SD.begin()) {
    server.send(500, "text/plain", "SD kart başlatılamadı!");
    return;
  }
  File file = SD.open(path.c_str());
  if (!file) {
    server.send(404, "text/plain", "Görsel bulunamadı");
    return;
  }
  server.streamFile(file, "image/png");
  file.close();
}

// Başlatma fonksiyonu (Wi-Fi, dosya sistemi, HTTP yolları)
void setup() {
  Serial.begin(115200);
  delay(1000);
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS başlatılamadı!");
    server.on("/", []() {
      server.send(200, "text/html", "<h2>SD kart veya dosya sistemi başlatılamadı!</h2>");
    });
    server.begin();
    return;
  }
  WiFi.softAP(ssid, password);
  Serial.println("SoftAP aktif: " + WiFi.softAPIP().toString());
  server.on("/", handleIndex);
  server.on("/form", handleRoot);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/map", handleMap);
  server.on("/usersearch", HTTP_GET, [](void){
    if (server.hasArg("name")) {
      handleUserSearch();
    } else {
      handleUserSearchPage();
    }
  });
  server.on("/tilesText/", HTTP_GET, handleTile); // wildcard for folder
  server.onNotFound([]() {
    String uri = server.uri();
    if (uri.startsWith("/tilesText/")) {
      handleTile();
    } else {
      server.send(404, "text/plain", "Sayfa bulunamadı");
    }
  });
  server.begin();
  Serial.println("Web server çalışıyor.");
}

// Ana döngü (HTTP isteklerini işler)
void loop() {
  server.handleClient();
}
