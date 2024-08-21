#include <WiFi.h>
#include <WebServer.h>
#include "secrets.h"  // add WLAN Credentials in here.

#define Uppin 32
#define Stoppin 33
#define Downpin 26
#define Lighting 27

WebServer Server(80);         //  ポート番号（HTTP）


//  クライアントにウェブページ（HTML）を返す関数
void SendMessage() {
  //  レスポンス文字列の生成（'\n' は改行; '\' は行継続）
  Serial.println("SendMessage");
  String message =   "<html lang=\"ja\">\n\
    <meta charset=\"utf-8\">\n\
    <center>\
    <h2>シャッターボタン</h2>\
    <p><button type='button' onclick='location.href=\"/up\"' \
      style='width:200px;height:40px;'>上昇</button></p>\
    <p><button type='button' onclick='location.href=\"/\"' \
      style='width:200px;height:40px;'>停止</button></p>\
    <p><button type='button' onclick='location.href=\"/down\"' \
      style='width:200px;height:40px;'>下降</button></p>\
    <p><button type='button' onclick='location.href=\"/lighting\"' \
      style='width:200px;height:40px;'>照明</button></p>\
  </center>";
  //  クライアントにレスポンスを返す
  Server.send(200, "text/html", message);
  //  停止0.5s押し
  digitalWrite(Stoppin, LOW);
  delay(500);
  digitalWrite(Stoppin, HIGH);
}

void UpSendMessage() {
  Serial.println("UpSendMessage");
  String message =   "<html lang=\"ja\">\n\
    <meta charset=\"utf-8\">\n\
    <center>\
    <h2>シャッターボタン</h2>\
    <p><button type='button' onclick='location.href=\"/up\"' \
      style='width:200px;height:40px;'>上昇</button></p>\
    <p><button type='button' onclick='location.href=\"/\"' \
      style='width:200px;height:40px;'>停止</button></p>\
    <p><button type='button' onclick='location.href=\"/down\"' \
      style='width:200px;height:40px;'>下降</button></p>\
    <p><button type='button' onclick='location.href=\"/lighting\"' \
      style='width:200px;height:40px;'>照明</button></p>\
  </center>";
  //  クライアントにレスポンスを返す
  Server.send(200, "text/html", message);
  //  上昇0.5s押し
  digitalWrite(Uppin, LOW);
  delay(500);
  digitalWrite(Uppin, HIGH);
}

void DownSendMessage() {
  Serial.println("DownSendMessage");
  String message =   "<html lang=\"ja\">\n\
    <meta charset=\"utf-8\">\n\
    <center>\
    <h2>シャッターボタン</h2>\
    <p><button type='button' onclick='location.href=\"/up\"' \
      style='width:200px;height:40px;'>上昇</button></p>\
    <p><button type='button' onclick='location.href=\"/\"' \
      style='width:200px;height:40px;'>停止</button></p>\
    <p><button type='button' onclick='location.href=\"/down\"' \
      style='width:200px;height:40px;'>下降</button></p>\
    <p><button type='button' onclick='location.href=\"/lighting\"' \
      style='width:200px;height:40px;'>照明</button></p>\
  </center>";
  //  クライアントにレスポンスを返す
  Server.send(200, "text/html", message);
  //  下降0.5s押し
  digitalWrite(Downpin, LOW);
  delay(500);
  digitalWrite(Downpin, HIGH);
}

void LightSendMessage() {
  Serial.println("LightSendMessage");
  String message =   "<html lang=\"ja\">\n\
    <meta charset=\"utf-8\">\n\
    <center>\
    <h2>シャッターボタン</h2>\
    <p><button type='button' onclick='location.href=\"/up\"' \
      style='width:200px;height:40px;'>上昇</button></p>\
    <p><button type='button' onclick='location.href=\"/\"' \
      style='width:200px;height:40px;'>停止</button></p>\
    <p><button type='button' onclick='location.href=\"/down\"' \
      style='width:200px;height:40px;'>下降</button></p>\
    <p><button type='button' onclick='location.href=\"/lighting\"' \
      style='width:200px;height:40px;'>照明</button></p>\
  </center>";
  //  クライアントにレスポンスを返す
  Server.send(200, "text/html", message);
  //  トグル動作
  static char toggle=true;
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

  
}

//  クライアントにエラーメッセージを返す関数
void SendNotFound() {
  Serial.println("SendNotFound");
  Server.send(404, "text/plain", "404 not found...");
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
  delay(100);                         //  100ms ほど待ってからログ出力可
  Serial.println("\n*** Starting ***");
  //  無線 LAN に接続
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);             
  Serial.println("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Can't connect");
    }
  }
  Serial.println("Connected");
  Serial.println(WiFi.localIP());     //  ESP 自身の IP アドレスをログ出力
  //  ウェブサーバの設定
  Server.on("/", SendMessage);         //  ルートアクセス時の応答関数を設定
  Server.on("/up", UpSendMessage);
  Server.on("/down", DownSendMessage);
  Server.on("/lighting", LightSendMessage);
  Server.onNotFound(SendNotFound);  //  不正アクセス時の応答関数を設定
  Server.begin();                     //  ウェブサーバ開始

}
void loop() {
  //  クライアントからの要求を処理する
  Server.handleClient();
}


