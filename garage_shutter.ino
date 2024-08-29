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
 Serial.begin(115200);
  pinMode(Lighting, OUTPUT); //GPIO02をアウトプットに

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

  // Onボタンが押された時のレスポンス
  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(Lighting, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Offボタンが押された時のレスポンス
  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(Lighting, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // サーバースタート
  server.begin();
}
void loop() {

}


