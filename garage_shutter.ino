#include <WiFi.h>
#include "secrets.h"  // add WLAN Credentials and Host info in here.

#define Uppin 32
#define Stoppin 33
#define Downpin 26

WifiClinet client;

const int port = PORT;
const char* host = HOST;

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

//  メインプログラム
void setup() {
  pinMode(Uppin, OUTPUT);
  digitalWrite(Uppin, HIGH);
  pinMode(Downpin, OUTPUT);
  digitalWrite(Downpin, HIGH);
  pinMode(Stoppin, OUTPUT);
  digitalWrite(Stoppin, HIGH);
  //  シリアルモニタ（動作ログ）
  Serial.begin(115200);               //  ESP 標準の通信速度 115200
  delay(100);                         //  100ms ほど待ってからログ出力可
  Serial.println("\n*** Starting ***");
  //  無線 LAN に接続
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);             
  Serial.println("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Can't connect");
    }
  }
  Serial.println("Connected");
  Serial.println(WiFi.localIP());     //  ESP 自身の IP アドレスをログ出力
}
void loop() {
  //  クライアントからの要求を処理する
  if (client.connect(host, port)) {
    Serial.println("Connected to server");
  } else {
    Serial.println("Connection to server failed");
  }

  if (client.connected()) {
    if (client.available()) {
      String message = client.readStringUntil('\n'); // 改行文字まで読み込む
      Serial.print("Received message: ");
      Serial.println(message);
      if(message == "Up"){
        UpSendMessage();
      }else if(message == "Stop"){
        StopSendMessage();
      }else if(message == "Down"){
        DownSendMEssage();
      }
    }
  }
}