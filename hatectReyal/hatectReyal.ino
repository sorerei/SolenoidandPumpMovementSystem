#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h> // Library untuk memproses JSON
#include <vector> // Untuk std::vector
#include <algorithm> // Untuk std::sort

// Konfigurasi WiFi
const char* ssid = "fufufafa";        // Ganti dengan nama WiFi Anda
const char* password = "12345678";    // Ganti dengan password WiFi Anda

// Konfigurasi server HTTP
WebServer server(80); // Port HTTP standar (80)

// Definisikan pin untuk pompa dan solenoid
const int pumpPins[] = {2, 4, 5};    // Pompa: Tinggi, Sedang, Rendah
const int solenoidPins[] = {18, 19, 12, 14, 27, 26};  // Solenoid
const int pumpCount = sizeof(pumpPins) / sizeof(pumpPins[0]);
const int solenoidCount = sizeof(solenoidPins) / sizeof(solenoidPins[0]);

// Fungsi untuk menghentikan semua pompa dan solenoid
void stopAll() {
  // Matikan semua pompa
  for (int i = 0; i < pumpCount; i++) {
    digitalWrite(pumpPins[i], LOW);
  }

  // Matikan semua solenoid
  for (int i = 0; i < solenoidCount; i++) {
    digitalWrite(solenoidPins[i], LOW);
  }
}

// Fungsi untuk mengontrol pompa dan solenoid berdasarkan damage_level dan array ID
void controlPumpAndSolenoids(const char* damage_level, JsonArray ids) {
  // Nonaktifkan semua pompa dan solenoid terlebih dahulu
  stopAll();

  // Aktifkan pompa berdasarkan damage_level
  if (strcmp(damage_level, "LOW") == 0) {
    digitalWrite(pumpPins[2], HIGH); // Pompa rendah
    Serial.println("Pompa rendah aktif.");
  } else if (strcmp(damage_level, "MEDIUM") == 0) {
    digitalWrite(pumpPins[1], HIGH); // Pompa sedang
    Serial.println("Pompa sedang aktif.");
  } else if (strcmp(damage_level, "HIGH") == 0) {
    digitalWrite(pumpPins[0], HIGH); // Pompa tinggi
    Serial.println("Pompa tinggi aktif.");
  }

  // Simpan ID ke dalam std::vector
  std::vector<int> sortedIds;
  for (int id : ids) {
    sortedIds.push_back(id);  // Tambahkan ID ke dalam vector
  }

  // Urutkan ID secara ascending
  std::sort(sortedIds.begin(), sortedIds.end());

  // Aktifkan solenoid berdasarkan ID yang diurutkan
  for (int id : sortedIds) {
    // Pastikan ID dalam rentang yang valid
    int solenoidIndex = (id - 1) % solenoidCount;  // Menyusun ID menjadi rentang valid

    if (solenoidIndex >= 0 && solenoidIndex < solenoidCount) {
      digitalWrite(solenoidPins[solenoidIndex], HIGH);
      Serial.print("Solenoid aktif: ");
      Serial.println(solenoidPins[solenoidIndex]);
    } else {
      Serial.print("ID solenoid tidak valid: ");
      Serial.println(id);
    }
  }
}

// Fungsi untuk menangani permintaan POST
void handleSpray() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

  // Jika metode adalah OPTIONS, kirim respons kosong
  if (server.method() == HTTP_OPTIONS) {
    server.send(204); // No Content
    return;
  }

  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    Serial.println("Data diterima:");
    Serial.println(body);

    // Parsing JSON
    DynamicJsonDocument doc(2048); // Tingkatkan ukuran buffer jika data besar
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
      Serial.print("Gagal mem-parsing JSON: ");
      Serial.println(error.c_str());
      server.send(400, "application/json", "{\"message\":\"Invalid JSON format\"}");
      return;
    }

    // Ambil data dari JSON
    JsonArray dataArray = doc["data"].as<JsonArray>();

    // Kelompokkan berdasarkan damage_level
    DynamicJsonDocument groupedLevels(1024); // Buffer untuk data yang dikelompokkan
    for (JsonObject obj : dataArray) {
      const char* damage_level = obj["damage_level"];
      int id = obj["id"];

      if (!groupedLevels.containsKey(damage_level)) {
        groupedLevels.createNestedArray(damage_level);
      }
      groupedLevels[damage_level].add(id);
    }

    // Urutan prioritas: low -> medium -> high
    const char* levels[] = {"LOW", "MEDIUM", "HIGH"};

    // Penyemprotan berdasarkan damage_level dalam urutan prioritas
    for (const char* level : levels) {
      if (groupedLevels.containsKey(level)) {
        JsonArray ids = groupedLevels[level].as<JsonArray>();

        Serial.print("Menyemprot untuk damage_level: ");
        Serial.println(level);
        controlPumpAndSolenoids(level, ids);
        delay(15000); // Semprot selama 15 detik
        stopAll();    // Matikan pompa dan solenoid
      }
    }

    server.send(200, "application/json", "{\"message\":\"Data diterima dan diproses\"}");
  } else {
    server.send(400, "application/json", "{\"message\":\"Invalid request\"}");
  }
}

// Fungsi untuk menangani permintaan OPTIONS
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204); // No Content
}

void setup() {
  Serial.begin(115200);

  // Hubungkan ke WiFi
  WiFi.begin(ssid, password);
  Serial.println("Menghubungkan ke WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nTerhubung ke WiFi");
  Serial.print("Alamat IP ESP32: ");
  Serial.println(WiFi.localIP());

  // Atur pin sebagai OUTPUT
  for (int i = 0; i < pumpCount; i++) {
    pinMode(pumpPins[i], OUTPUT);
    digitalWrite(pumpPins[i], LOW); // Pastikan pompa mati
  }

  for (int i = 0; i < solenoidCount; i++) {
    pinMode(solenoidPins[i], OUTPUT);
    digitalWrite(solenoidPins[i], LOW); // Pastikan solenoid mati
  }

  // Atur route untuk endpoint /spray
  server.on("/spray", HTTP_OPTIONS, handleOptions); // OPTIONS handler
  server.on("/spray", HTTP_POST, handleSpray);     // POST handler

  // Jalankan server
  server.begin();
  Serial.println("Server HTTP dimulai dan siap menerima permintaan");
}

void loop() {
  // Jalankan server HTTP
  server.handleClient();
}
