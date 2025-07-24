#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "secrets.h"
#include <ctime>

#define Uppin 32
#define Stoppin 33
#define Downpin 26
#define Lighting 27

String ledState;
AsyncWebServer server(80);

// --- 認証 & ログ記録 ---
bool checkAuth(AsyncWebServerRequest *request) {
  writeAccessLog(request);
  const char* realm = "GarageControl";
  if (!request->authenticate(HTTP_USER, HTTP_PASS, realm)) {
    request->requestAuthentication(realm);
    return false;
  }
  return true;
}

// --- 状態置換（HTML用） ---
String processor(const String& var) {
  if (var == "STATE") {
    ledState = digitalRead(Lighting) ? "ON" : "OFF";
    return ledState;
  }
  return String();
}

// --- コマンド ---
void StopSendMessage() { digitalWrite(Stoppin, LOW); delay(500); digitalWrite(Stoppin, HIGH); }
void UpSendMessage()   { digitalWrite(Uppin, LOW); delay(500); digitalWrite(Uppin, HIGH); }
void DownSendMessage() { digitalWrite(Downpin, LOW); delay(500); digitalWrite(Downpin, HIGH); }
void LightSendMessage(){ digitalWrite(Lighting, !digitalRead(Lighting)); }

// --- アクセスログ記録 ---
void writeAccessLog(AsyncWebServerRequest *request) {
  String url = request->url();
  if (url.endsWith(".css") || url.endsWith(".ico")) return;

  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char filename[32];
  strftime(filename, sizeof(filename), "/log_%Y%m%d.txt", t);

  String logEntry = "[" + String(asctime(t));
  logEntry.trim();
  logEntry += "] IP: " + request->client()->remoteIP().toString();
  logEntry += " URL: " + url + "\n";

  File logFile = SPIFFS.open(filename, FILE_APPEND);
  if (logFile) {
    logFile.print(logEntry);
    logFile.close();
    Serial.println("Logged: " + logEntry);
  } else {
    Serial.println("Failed to write log to: " + String(filename));
  }
}

// --- SPIFFSファイル一覧表示（デバッグ用） ---
void listSpiffsFiles() {
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("SPIFFS root open failed");
    return;
  }

  Serial.println("SPIFFS File List:");
  File file = root.openNextFile();
  while (file) {
    Serial.printf(" - %s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }
}

// --- 古いログ削除 ---
void cleanOldLogs() {
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) return;

  time_t now = time(nullptr);
  char oldFilename[32];

  for (int i = 8; i <= 30; ++i) {
    time_t oldTime = now - i * 86400;
    struct tm *oldTm = localtime(&oldTime);
    strftime(oldFilename, sizeof(oldFilename), "/log_%Y%m%d.txt", oldTm);
    if (SPIFFS.exists(oldFilename)) {
      SPIFFS.remove(oldFilename);
      Serial.print("Deleted old log: "); Serial.println(oldFilename);
    }
  }
}

void setup() {
  pinMode(Uppin, OUTPUT);    digitalWrite(Uppin, HIGH);
  pinMode(Downpin, OUTPUT);  digitalWrite(Downpin, HIGH);
  pinMode(Stoppin, OUTPUT);  digitalWrite(Stoppin, HIGH);
  pinMode(Lighting, OUTPUT); digitalWrite(Lighting, HIGH);

  Serial.begin(115200); delay(100); Serial.println("*** Starting ***");

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000); Serial.println("Connecting...");
  }
  Serial.println("Connected: " + WiFi.localIP().toString());

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed"); return;
  }

  configTime(9 * 3600, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  int timeout = 0;
  while (time(nullptr) < 100000 && timeout < 20) {
    delay(500); Serial.print(".");
    timeout++;
  }
  Serial.println("\nTime sync complete");

  cleanOldLogs();
  listSpiffsFiles(); // ← ここでSPIFFS内容表示

  // --- 操作ページ ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/up", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    UpSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    StopSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    DownSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    LightSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // --- ログ一覧ページ ---
  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;

    String html = "<html><head><meta charset='UTF-8'><title>Logs</title></head><body><h2>ログ一覧</h2><ul>";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      String filename = file.name();
      if (filename.startsWith("log_")) {
        html += "<li>" + filename;
        html += " [<a href=\"/view?file=" + filename + "\">📄表示</a>] ";
        html += " [<a href=\"/download?file=" + filename + "\">⬇ダウンロード</a>]</li>";
      }
      file = root.openNextFile();
    }
    html += "</ul></body></html>";
    request->send(200, "text/html; charset=UTF-8", html);
  });

  // --- ログファイル表示 ---
  server.on("/view", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;

    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }

    String filename = request->getParam("file")->value();
    String filepath = "/" + filename;
    if (!filename.startsWith("log_") || !SPIFFS.exists(filepath)) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    File file = SPIFFS.open(filepath, FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "Failed to open file");
      return;
    }

    String html = "<html><head><meta charset='UTF-8'><title>Log View</title></head><body>";
    html += "<h2>" + filename + "</h2><pre>";
    while (file.available()) html += (char)file.read();
    html += "</pre></body></html>";
    file.close();

    request->send(200, "text/html; charset=UTF-8", html);
  });

  // --- ログダウンロード ---
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }

    String filename = request->getParam("file")->value();
    String filepath = "/" + filename;
    if (!SPIFFS.exists(filepath)) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    request->send(SPIFFS, filepath, "text/plain", true);
  });

  server.begin();
}

unsigned long lastCleanup = 0;
const unsigned long cleanupInterval = 3600000; // 1時間 = 3600秒 = 3600000ms

void loop() {
  if (millis() - lastCleanup > cleanupInterval) {
    Serial.println("Periodic log cleanup...");
    cleanOldLogs();
    lastCleanup = millis();
  }

  delay(100);
}