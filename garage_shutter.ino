#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "esp_system.h"
#include "SPIFFS.h"
#include "secrets.h"
#include <ctime>
#include <vector>
#include <algorithm>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define Uppin 32
#define Stoppin 33
#define Downpin 26
#define Lighting 27

String ledState;
AsyncWebServer server(80);

// ---- セッション（複数同時OK）----
struct Session {
  String id;
  String username;
  unsigned long startMs;
};
std::vector<Session> g_sessions;

const unsigned long SESSION_TTL_MS = 24UL * 60UL * 60UL * 1000UL; // 24h
const size_t MAX_SESSIONS = 32;

// ---------- ユーティリティ ----------
String urlEncode(const String& s) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (('a'<=c && c<='z') || ('A'<=c && c<='Z') || ('0'<=c && c<='9') ||
        c=='-' || c=='_' || c=='.' || c=='~' || c=='/') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

String makeSessionId() {
  uint32_t r1 = esp_random();
  uint32_t r2 = esp_random();
  char buf[33];
  snprintf(buf, sizeof(buf), "%08X%08X%08X%08X", r1, r2, (unsigned)millis(), (unsigned)esp_random());
  return String(buf);
}

String getCookie(AsyncWebServerRequest *request, const String& key) {
  if (!request->hasHeader("Cookie")) return "";
  const AsyncWebHeader* h = request->getHeader("Cookie");
  String cookie = h->value();
  int p = 0;
  while (p < cookie.length()) {
    int eq = cookie.indexOf('=', p);
    if (eq == -1) break;
    int sc = cookie.indexOf(';', eq + 1);
    String k = cookie.substring(p, eq); k.trim();
    String v = (sc == -1) ? cookie.substring(eq + 1) : cookie.substring(eq + 1, sc);
    v.trim();
    if (k == key) return v;
    p = (sc == -1) ? cookie.length() : (sc + 1);
  }
  return "";
}

// ---- ユーザー認証（複数ユーザー対応）----
bool authenticateUser(const String& username, const String& password) {
  for (int i = 0; i < USER_COUNT; i++) {
    if (username == USERS[i].username && password == USERS[i].password) {
      return true;
    }
  }
  return false;
}

// ---- セッション掃除 & 検索 ----
void cleanupExpiredSessions() {
  unsigned long nowMs = millis();
  g_sessions.erase(
    std::remove_if(g_sessions.begin(), g_sessions.end(), [&](const Session& s){
      return (unsigned long)(nowMs - s.startMs) > SESSION_TTL_MS;
    }),
    g_sessions.end()
  );
}

int findSessionIndexById(const String& sid) {
  for (size_t i=0;i<g_sessions.size();++i) {
    if (g_sessions[i].id == sid) return (int)i;
  }
  return -1;
}

bool isLoggedIn(AsyncWebServerRequest *request) {
  cleanupExpiredSessions();
  String sid = getCookie(request, "GCSESSID");
  if (sid == "") return false;
  return findSessionIndexById(sid) >= 0;
}

String getCurrentUsername(AsyncWebServerRequest *request) {
  cleanupExpiredSessions();
  String sid = getCookie(request, "GCSESSID");
  if (sid == "") return "Unknown";
  
  int idx = findSessionIndexById(sid);
  if (idx >= 0) {
    return g_sessions[idx].username;
  }
  return "Unknown";
}

void startSession(AsyncWebServerRequest *request, const String& username, const String& redirectTo = "/") {
  cleanupExpiredSessions();
  Session s{ makeSessionId(), username, millis() };
  if (g_sessions.size() >= MAX_SESSIONS) {
    g_sessions.erase(g_sessions.begin());
  }
  g_sessions.push_back(s);

  AsyncWebServerResponse *res = request->beginResponse(303);
  res->addHeader("Set-Cookie", "GCSESSID=" + s.id + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=86400");
  res->addHeader("Location", redirectTo);
  request->send(res);
}

void clearSession(AsyncWebServerRequest *request) {
  String sid = getCookie(request, "GCSESSID");
  if (sid.length()) {
    int idx = findSessionIndexById(sid);
    if (idx >= 0) g_sessions.erase(g_sessions.begin() + idx);
  }
  AsyncWebServerResponse *res = request->beginResponse(303);
  res->addHeader("Set-Cookie", "GCSESSID=deleted; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
  res->addHeader("Location", "/login");
  request->send(res);
}

String sanitizeLogFilename(String name) {
  if (name.startsWith("/")) name = name.substring(1);
  if (name.indexOf('/') != -1) return "";
  if (!name.startsWith("log_")) return "";
  for (size_t i=0;i<name.length();++i) {
    char c = name[i];
    bool ok = (c=='_' || c=='.' ||
               (c>='0' && c<='9') ||
               (c>='A' && c<='Z') ||
               (c>='a' && c<='z'));
    if (!ok) return "";
  }
  return name;
}

// ---------- 認証 & ログ記録 ----------
void writeAccessLog(AsyncWebServerRequest *request) {
  String url = request->url();
  if (url.endsWith(".css") || url.endsWith(".ico")) return;

  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char filename[32];
  strftime(filename, sizeof(filename), "/log_%Y%m%d.txt", t);

  String username = getCurrentUsername(request);
  String logEntry = "[" + String(asctime(t));
  logEntry.trim();
  logEntry += "] User: " + username;
  logEntry += " IP: " + request->client()->remoteIP().toString();
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

bool checkAuth(AsyncWebServerRequest *request) {
  writeAccessLog(request);
  if (isLoggedIn(request)) return true;

  String redirectTo = "/login?redirect=" + urlEncode(request->url());
  AsyncWebServerResponse *res = request->beginResponse(302);
  res->addHeader("Location", redirectTo);
  request->send(res);
  return false;
}

String processor(const String& var) {
  if (var == "STATE") {
    ledState = digitalRead(Lighting) ? "ON" : "OFF";
    return ledState;
  }
  return String();
}

// ---------- コマンド ----------
void StopSendMessage() { digitalWrite(Stoppin, LOW); delay(500); digitalWrite(Stoppin, HIGH); }
void UpSendMessage()   { digitalWrite(Uppin, LOW); delay(500); digitalWrite(Uppin, HIGH); }
void DownSendMessage() { digitalWrite(Downpin, LOW); delay(500); digitalWrite(Downpin, HIGH); }
void LightSendMessage(){ digitalWrite(Lighting, !digitalRead(Lighting)); }

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

void redirectToIndex(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *res = request->beginResponse(303);
  res->addHeader("Location", "/");
  request->send(res);
}

void sendDiscordUpNotify(const String& username) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (!https.begin(client, DISCORD_WEBHOOK_URL)) {
    Serial.println("Discord webhook begin failed");
    return;
  }

  https.addHeader("Content-Type", "application/json");

  String payload =
  "{"
  "\"content\": \"🚪 **ガレージが開けられました**\\n👤 操作者: " + username + "\""
  "}";

  int httpCode = https.POST(payload);
  Serial.printf("Discord notify result: %d\n", httpCode);

  https.end();
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
  listSpiffsFiles();

  Serial.printf("Registered users: %d\n", USER_COUNT);
  for (int i = 0; i < USER_COUNT; i++) {
    Serial.printf(" - %s\n", USERS[i].username);
  }

  // ============================================================
  // iPhone ショートカット用 API エンドポイント
  // ============================================================

  // --- API: ガレージ開ける ---
  server.on("/api/up", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->authenticate(USERS[0].username, USERS[0].password)) {
      request->requestAuthentication();
      return;
    }

    String username = USERS[0].username;
    if (request->hasParam("user")) {
      username = request->getParam("user")->value();
    }

    UpSendMessage();
    sendDiscordUpNotify(username);
    
    String response = "{\"status\":\"success\",\"action\":\"up\",\"user\":\"" + username + "\"}";
    request->send(200, "application/json", response);
  });

  // --- API: ガレージ閉める ---
  server.on("/api/down", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->authenticate(USERS[0].username, USERS[0].password)) {
      request->requestAuthentication();
      return;
    }

    String username = USERS[0].username;
    if (request->hasParam("user")) {
      username = request->getParam("user")->value();
    }

    DownSendMessage();
    
    String response = "{\"status\":\"success\",\"action\":\"down\",\"user\":\"" + username + "\"}";
    request->send(200, "application/json", response);
  });

  // --- API: ガレージ停止 ---
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->authenticate(USERS[0].username, USERS[0].password)) {
      request->requestAuthentication();
      return;
    }

    String username = USERS[0].username;
    if (request->hasParam("user")) {
      username = request->getParam("user")->value();
    }

    StopSendMessage();
    
    String response = "{\"status\":\"success\",\"action\":\"stop\",\"user\":\"" + username + "\"}";
    request->send(200, "application/json", response);
  });

  // --- API: 照明トグル ---
  server.on("/api/light", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->authenticate(USERS[0].username, USERS[0].password)) {
      request->requestAuthentication();
      return;
    }

    String username = USERS[0].username;
    if (request->hasParam("user")) {
      username = request->getParam("user")->value();
    }

    LightSendMessage();
    String state = digitalRead(Lighting) ? "ON" : "OFF";
    
    String response = "{\"status\":\"success\",\"action\":\"light\",\"state\":\"" + state + "\",\"user\":\"" + username + "\"}";
    request->send(200, "application/json", response);
  });

  // --- API: ステータス取得（認証なし）---
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String lightState = digitalRead(Lighting) ? "ON" : "OFF";
    String response = "{\"status\":\"success\",\"light\":\"" + lightState + "\"}";
    request->send(200, "application/json", response);
  });

  // ============================================================
  // Web UI エンドポイント（既存）
  // ============================================================

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
    if (isLoggedIn(request)) {
      auto *res = request->beginResponse(303);
      res->addHeader("Location", "/");
      request->send(res);
      return;
    }
    AsyncWebServerResponse *res = request->beginResponse(SPIFFS, "/login.html", "text/html; charset=utf-8");
    res->addHeader("Cache-Control", "no-store");
    request->send(res);
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    String u = request->arg("username");
    String p = request->arg("password");
    String redirect = request->hasParam("redirect", true) ? request->getParam("redirect", true)->value() : "/";
    if (!redirect.startsWith("/")) redirect = "/";

    if (authenticateUser(u, p)) {
      Serial.printf("Login success: %s\n", u.c_str());
      startSession(request, u, redirect.length() ? redirect : "/");
    } else {
      Serial.printf("Login failed: %s\n", u.c_str());
      AsyncWebServerResponse *res = request->beginResponse(303);
      res->addHeader("Location", "/login?err=1");
      request->send(res);
    }
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    clearSession(request);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/login.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/login.css", "text/css");
  });

  server.on("/up", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    String username = getCurrentUsername(request);
    UpSendMessage();
    sendDiscordUpNotify(username);
    redirectToIndex(request);
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    StopSendMessage();
    redirectToIndex(request);
  });

  server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    DownSendMessage();
    redirectToIndex(request);
  });

  server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    LightSendMessage();
    redirectToIndex(request);
  });

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
  Serial.println("Server started with API endpoints!");
  Serial.println("API Base URL: http://" + WiFi.localIP().toString() + "/api/");
}

unsigned long lastCleanup = 0;
const unsigned long cleanupInterval = 3600000;

void loop() {
  if (millis() - lastCleanup > cleanupInterval) {
    Serial.println("Periodic log cleanup...");
    cleanOldLogs();
    cleanupExpiredSessions();
    lastCleanup = millis();
  }
  delay(100);
}