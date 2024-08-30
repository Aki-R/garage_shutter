#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "secrets.h"  // add WLAN Credentials in here.

#define Uppin 32
#define Stoppin 33
#define Downpin 26
#define Lighting 27

// LEDの状態を保持する変数
String ledState;

// ポート80にサーバーを設定
AsyncWebServer server(80);

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

  // SPIFFSのセットアップ
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // WiFiに接続
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // ESP32のローカルアドレスを表示
  Serial.println(WiFi.localIP());

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
    digitalWrite(Uppin, LOW);    
    delay(500);
    digitalWrite(DUppin, HIGH);
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // STOPボタンが押された時のレスポンス
  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(Stoppin, LOW);    
    delay(500);
    digitalWrite(Stoppin, HIGH);
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // DOWNボタンが押された時のレスポンス
  server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(Downpin, LOW);    
    delay(500);
    digitalWrite(Downpin, HIGH); 
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // LEDボタンが押された時のレスポンス
  server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
    static bool toggle = true;
    if(toggle)
    {
      digitalWrite(Lighting, HIGH);
      toggle=false;
    }
    else
    {
      digitalWrite(Lighting, LOW);
      toggle=true;
    }
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // サーバースタート
  server.begin();
}
void loop() {

}


