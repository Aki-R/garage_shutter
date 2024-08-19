#include <WiFi.h>
#include <WebServer.h>
#include "secrets.h"  // add WLAN Credentials in here.

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
  </center>";
  //  クライアントにレスポンスを返す
  Server.send(200, "text/html", message);
  //  停止0.5s押し
  digitalWrite(5, LOW);
  delay(500);
  digitalWrite(5, HIGH);
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
  </center>";
  //  クライアントにレスポンスを返す
  Server.send(200, "text/html", message);
  //  上昇0.5s押し
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
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
  </center>";
  //  クライアントにレスポンスを返す
  Server.send(200, "text/html", message);
  //  下降0.5s押し
  digitalWrite(6, HIGH);
  delay(500);
  digitalWrite(6, LOW);
}

//  クライアントにエラーメッセージを返す関数
void SendNotFound() {
  Serial.println("SendNotFound");
  Server.send(404, "text/plain", "404 not found...");
}

//  メインプログラム
void setup() {
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  pinMode(6, OUTPUT);
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
  Server.onNotFound(SendNotFound);  //  不正アクセス時の応答関数を設定
  Server.begin();                     //  ウェブサーバ開始

}
void loop() {
  //  クライアントからの要求を処理する
  Server.handleClient();
}


