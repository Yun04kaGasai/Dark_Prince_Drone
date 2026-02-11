#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

const char* ssid = "Hoida";
const char* password = "12573653";

#define MOTOR_A 2
#define MOTOR_B 3
#define MOTOR_C 4
#define MOTOR_D 5

#define GYRO_SCALE 131.0
#define KP_ROLL 1.8
#define KP_PITCH 1.8
#define MAX_CORRECTION 40

WebServer server(80);

int16_t gyroX, gyroY;
int16_t accelX, accelY, accelZ;
float angleRoll = 0, anglePitch = 0;
unsigned long lastTime = 0;
int baseThrust = 0;

void readMPU6050() {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 14, true);
  accelX = Wire.read() << 8 | Wire.read();
  accelY = Wire.read() << 8 | Wire.read();
  accelZ = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();
  gyroX = Wire.read() << 8 | Wire.read();
  gyroY = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();
}

void updateAngles() {
  unsigned long now = micros();
  float dt = (now - lastTime) / 1000000.0;
  lastTime = now;

  float gyroRoll = gyroX / GYRO_SCALE;
  float gyroPitch = gyroY / GYRO_SCALE;

  float accelRoll = atan2(accelY, accelZ) * 180.0 / PI;
  float accelPitch = atan2(-accelX, sqrt(accelY * accelY + accelZ * accelZ)) * 180.0 / PI;
  angleRoll = 0.98 * (angleRoll + gyroRoll * dt) + 0.02 * accelRoll;
  anglePitch = 0.98 * (anglePitch + gyroPitch * dt) + 0.02 * accelPitch;
}

void applyStabilization() {
  float rollCorrection = constrain(angleRoll * KP_ROLL, -MAX_CORRECTION, MAX_CORRECTION);
  float pitchCorrection = constrain(anglePitch * KP_PITCH, -MAX_CORRECTION, MAX_CORRECTION);

  int motorA = constrain(baseThrust - rollCorrection - pitchCorrection, 0, 255);
  int motorB = constrain(baseThrust + rollCorrection + pitchCorrection, 0, 255);
  int motorC = constrain(baseThrust + rollCorrection - pitchCorrection, 0, 255);
  int motorD = constrain(baseThrust - rollCorrection + pitchCorrection, 0, 255);

  analogWrite(MOTOR_A, motorA);
  analogWrite(MOTOR_B, motorB);
  analogWrite(MOTOR_C, motorC);
  analogWrite(MOTOR_D, motorD);
}

void setup() {
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

  Wire.begin(6, 7);
  Wire.setClock(400000);

  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru"><head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Дрон</title>
  <style>
    body { font-family: Arial; text-align: center; background: #2c3e50; color: white; padding: 20px; }
    h2 { margin: 20px 0; }
    button { width: 90%; padding: 16px; margin: 10px 0; font-size: 20px; border: none; border-radius: 8px; }
    .up   { background: #2ecc71; }
    .left { background: #3498db; }
    .right{ background: #3498db; }
    .land { background: #9b59b6; }
    .stop { background: #e74c3c; }
  </style>
</head>
<body>
  <h2>🚁 Управление дроном</h2>
  <button class="up"    onclick="send(255,255,255,255)">Взлёт / Макс. тяга</button><br>
  <button class="left"  onclick="send(200,255,255,200)">Поворот влево</button><br>
  <button class="right" onclick="send(255,200,200,255)">Поворот вправо</button><br>
  <button class="land"  onclick="send(80,80,80,80)">Мягкая посадка</button><br>
  <button class="stop"  onclick="send(0,0,0,0)">Аварийная остановка</button>
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
    baseThrust = (a + b + c + d) / 4;
  });

  server.begin();
  lastTime = micros();
}

void loop() {
  server.handleClient();
  readMPU6050();  updateAngles();
  applyStabilization();
  delay(2);
}