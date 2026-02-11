#include <WiFi.h>
#include <WebServer.h>

// === Настройки Wi-Fi === !!!!!!!!!!
const char* ssid = "Hoida";
const char* password = "12573653";

// === Пины моторов (ESP32-C3 Mini) ===
#define MOTOR_A 2   // Перед-лево
#define MOTOR_B 3   // Зад-право
#define MOTOR_C 4   // Перед-право
#define MOTOR_D 5   // Зад-лево

WebServer server(80);

void setup() {
  Serial.begin(9600);
  Serial.println("Запуск дрона");
  pinMode(MOTOR_A, OUTPUT);
  pinMode(MOTOR_B, OUTPUT);
  pinMode(MOTOR_C, OUTPUT);
  pinMode(MOTOR_D, OUTPUT);
  analogWriteResolution(MOTOR_A, 8);
  analogWriteResolution(MOTOR_B, 8);
  analogWriteResolution(MOTOR_C, 8);
  analogWriteResolution(MOTOR_D, 8);
  analogWriteFrequency(MOTOR_A, 20000);
  analogWriteFrequency(MOTOR_B, 20000);
  analogWriteFrequency(MOTOR_C, 20000);
  analogWriteFrequency(MOTOR_D, 20000);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n Подключено к сети");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Дрон</title>
  <style>
    body { font-family: Arial; text-align: center; background: #f0f0f0; padding: 15px; }
    h2 { color: #2c3e50; }
    button { width: 90%; padding: 14px; margin: 8px 0; font-size: 18px; border: none; border-radius: 6px; color: white; }
    .up    { background: #27ae60; }
    .left  { background: #3498db; }
    .right { background: #3498db; }
    .land  { background: #8e44ad; }
    .stop  { background: #e74c3c; }
  </style>
</head>
<body>
  <h2>Управление дроном(Временно на WIFI, простите не успеваем BLE)</h2>
  <button class="up"    onclick="send(255,255,255,255)">Взлёт</button><br>
  <button class="left"  onclick="send(150,200,200,150)">Влево</button><br>
  <button class="right" onclick="send(200,150,150,200)">Вправо</button><br>
  <button class="land"  onclick="send(250,250,250,250)">Посадка</button><br>
  <button class="stop"  onclick="send(0,0,0,0)">Стоп</button>

  <script>
    function send(a, b, c, d) {
      fetch(`/motor?a=${a}&b=${b}&c=${c}&d=${d}`)
        .then(() => console.log('OK'))
        .catch(e => console.error(e));
    }
  </script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html; charset=utf-8", html);
  });

  server.on("/motor", HTTP_GET, []() {
    int a = server.arg("a").toInt();
    int b = server.arg("b").toInt();
    int c = server.arg("c").toInt();
    int d = server.arg("d").toInt();

    a = constrain(a, 0, 200);
    b = constrain(b, 0, 200);
    c = constrain(c, 0, 200);
    d = constrain(d, 0, 200);

    analogWrite(MOTOR_A, a);
    analogWrite(MOTOR_B, b);
    analogWrite(MOTOR_C, c);
    analogWrite(MOTOR_D, d);

    Serial.printf("Моторы: %d %d %d %d\n", a, b, c, d);
    server.send(200, "text/plain; charset=utf-8", "OK");
  });

  server.begin();
}

void loop() {
  server.handleClient();
}