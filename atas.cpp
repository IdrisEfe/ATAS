#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>
#include <vector>

// Toplanma noktası koordinatları
#define TOPLANMA_LAT 40.9769
#define TOPLANMA_LON 28.8692

WebServer server(80);
bool sdOk = false;

// SD kart yoksa kayıtlar burada tutulur
std::vector<String> pendingRegistrations;

// Wi-Fi AP bilgileri
const char* ssid = "TOPLANMA_ALANI_1";
const char* password = "afet1234";

// HTML kayıt formu
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

// Kayıt işlemi
void handleRegister() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "İsim giriniz!");
    return;
  }

  String name = server.arg("name");
  String timestamp = String(millis());
  String record = name + "," + timestamp;

  if (sdOk) {
    File file = SD.open("/log.txt", FILE_APPEND);
    if (file) {
      // Bekleyen kayıtları önce yaz
      for (auto &entry : pendingRegistrations) {
        file.println(entry);
      }
      pendingRegistrations.clear();

      // Yeni kaydı yaz
      file.println(record);
      file.close();

      server.send(200, "text/plain", "Kayıt başarılı! Teşekkürler.");
    } else {
      // SD kart açık ama dosya açılamadı, hafızaya ekle
      pendingRegistrations.push_back(record);
      server.send(200, "text/plain", "SD karta erişim hatası, kayıt geçici olarak saklandı.");
    }
  } else {
    // SD kart yok, hafızaya ekle
    pendingRegistrations.push_back(record);
    server.send(200, "text/plain", "SD kart bağlı değil, kayıt geçici olarak saklandı.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // SPIFFS başlat
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS başlatılamadı!");
  }

  // Wi-Fi AP başlat
  WiFi.softAP(ssid, password);
  Serial.println("SoftAP aktif: " + WiFi.softAPIP().toString());

  // SD kart başlat
  sdOk = SD.begin();
  if (sdOk) Serial.println("SD kart başarılı şekilde başlatıldı.");
  else Serial.println("SD kart başlatılamadı.");

  // HTTP yolları
  server.on("/", handleIndex);
  server.on("/form", handleRoot);
  server.on("/register", HTTP_POST, handleRegister);

  server.begin();
  Serial.println("Web server çalışıyor.");
}

void loop() {
  server.handleClient();

  // SD kart kapalıysa her döngüde açmayı dene
  if (!sdOk) {
    sdOk = SD.begin();
    if (sdOk) {
      Serial.println("SD kart bağlandı, bekleyen kayıtlar yazılıyor...");
      File file = SD.open("/log.txt", FILE_APPEND);
      if (file) {
        for (auto &entry : pendingRegistrations) {
          file.println(entry);
        }
        file.close();
        pendingRegistrations.clear();
      } else {
        Serial.println("SD kart bağlandı ama dosya açılamadı.");
      }
    }
  }
}
