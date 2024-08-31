#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "secrets.h"  // add WLAN Credentials and Host info in here.
//#include "esp_task_wdt.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define Uppin 32
#define Stoppin 33
#define Downpin 26
#define Lighting 27

// LEDの状態を保持する変数
String ledState;

// Socket通信用のclient
WiFiClient client;

// ポート80にサーバーを設定
AsyncWebServer server(80);

// Socket通信のポートとホスト
const int port = PORT;
const char* host = HOST;

/*
// Watchdogの設定
esp_task_wdt_config_t config;
*/

// 実際のピン出力によってhtmlファイル内のSTATEの文字を変える
String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if(digitalRead(Lighting)){
      ledState = "ON";
    }
    else{
      ledState = "OFF";
    }
    Serial.print(ledState);
    return ledState;
  }
  return String();
}

void StopSendMessage() {
  Serial.println("Stop Command");
  //  停止0.5s押し
  digitalWrite(Stoppin, LOW);
  delay(500);
  digitalWrite(Stoppin, HIGH);
}

void UpSendMessage() {
  Serial.println("Up Command");
  //  上昇0.5s押し
  digitalWrite(Uppin, LOW);
  delay(500);
  digitalWrite(Uppin, HIGH);
}

void DownSendMessage() {
  Serial.println("Down Command");
  digitalWrite(Downpin, LOW);
  delay(500);
  digitalWrite(Downpin, HIGH);
}

void LightSendMessage() {
  Serial.println("Light Command");
  if(digitalRead(Lighting)){
    digitalWrite(Lighting, LOW);
  }
  else{
    digitalWrite(Lighting, HIGH);
  }
}

///ESP32 Monitoring////
// Google Apps ScriptのURL
const char* googleScriptURL = gooScrURL;


//  メインプログラム
void setup() {
  pinMode(Uppin, OUTPUT);
  digitalWrite(Uppin, HIGH);
  pinMode(Downpin, OUTPUT);
  digitalWrite(Downpin, HIGH);
  pinMode(Stoppin, OUTPUT);
  digitalWrite(Stoppin, HIGH);
  pinMode(Lighting, OUTPUT);
  digitalWrite(Lighting, HIGH);
  //  シリアルモニタ（動作ログ）
  Serial.begin(115200);               //  ESP 標準の通信速度 115200
  delay(100);                         //  100ms ほど待ってからログ出力可
  Serial.println("\n*** Starting ***");

  // SPIFFSのセットアップ
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  //  無線 LAN に接続
  WiFi.mode(WIFI_STA);        
  WiFi.begin(SSID, PASS);             
  Serial.println("Connecting Wifi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Can't connect");
    }
  }
  Serial.println("Connected");
  Serial.println(WiFi.localIP());     //  ESP 自身の IP アドレスをログ出力

  // サーバーのルートディレクトリにアクセスされた時のレスポンス
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // style.cssにアクセスされた時のレスポンス
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // UPボタンが押された時のレスポンス
  server.on("/up", HTTP_GET, [](AsyncWebServerRequest *request){
    UpSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // STOPボタンが押された時のレスポンス
  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    StopSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // DOWNボタンが押された時のレスポンス
  server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request){
    DownSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // LEDボタンが押された時のレスポンス
  server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
    LightSendMessage();
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // サーバースタート
  server.begin();

  // Google Spread Sheetにデータの送信
  sendDataToGoogleSheets("Init");
    
}
void loop() {
  Serial.println("loop start");
  sendDataToGoogleSheets("Start");
  //  クライアントからの要求を処理する
  if (!client.connected()){
      sendDataToGoogleSheets("Client Connected Fail");
    if (client.connect(host, port)) {
      Serial.println("Connected to server");
    } else {
      Serial.println("Connection to server failed");
    }
  }

  if (client.connected()){
    sendDataToGoogleSheets("Client Connected Succeeded");
    Serial.println("Client Conected");
    //esp_task_wdt_reset();
    delay(1);
    if (client.available()) {
      sendDataToGoogleSheets("Client Available Succeeded");
      String message = client.readStringUntil('\n'); // 改行文字まで読み込む
      Serial.print("Received message: ");
      Serial.println(message);
      if(message == "Up"){
        UpSendMessage();
      }else if(message == "Stop"){
        StopSendMessage();
      }else if(message == "Down"){
        DownSendMessage();
      }else if(message == "Light"){
        LightSendMessage();
      }
      // client.write("Command Received");
    } else {
    sendDataToGoogleSheets("Client Available Failed");
    }
  }
  delay(100);
}

//Google SpreadへのStatus送信関数
void sendDataToGoogleSheets(const char* Status) {
    HTTPClient http; // HTTPClientオブジェクトを作成
    String url = googleScriptURL; // Google Apps ScriptのURLを設定

    // JSONデータの作成
    String jsonData;
    StaticJsonDocument<200> doc;
    doc["Status"] = Status; // Statusの値をJSONに追加
    serializeJson(doc, jsonData); // JSONデータを文字列にシリアライズ

    http.begin(url); // HTTP接続を開始
    http.addHeader("Content-Type", "application/json"); // ヘッダーにContent-Typeを設定

    // HTTP POSTリクエストを送信
    int httpResponseCode = http.POST(jsonData);

    // レスポンスコードをチェック
    if (httpResponseCode > 0) {
      String response = http.getString(); // レスポンスを取得
      Serial.println(httpResponseCode); // レスポンスコードをシリアルモニタに出力
      Serial.println(response); // レスポンス内容をシリアルモニタに出力
    } else {
      Serial.print("Error on sending POST: "); // エラーメッセージをシリアルモニタに出力
      Serial.println(httpResponseCode);
    }

    http.end(); // HTTP接続を終了
}
