#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "MGTS_GPON_5103_24";
const char* password = "CvthnjyjcysqDjkxfhf1989";
const int ledPin = 2;
bool ledState = false;
WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 50px; }
    .button {
      padding: 20px 40px;
      font-size: 20px;
      margin: 10px;
      border: none;
      border-radius: 10px;
      cursor: pointer;
    }
    .on { background-color: #4CAF50; color: white; }
    .off { background-color: #f44336; color: white; }
    .state { 
      padding: 20px; 
      font-size: 24px; 
      font-weight: bold;
      margin: 20px;
      border-radius: 10px;
      background-color: #f0f0f0;
    }
    .wifi-status {
      padding: 10px;
      margin: 10px;
      border-radius: 5px;
      font-size: 14px;
    }
    .connected { background-color: #d4edda; color: #155724; }
    .disconnected { background-color: #f8d7da; color: #721c24; }
  </style>
</head>
<body>
  <h1>Управление светодиодом ESP32</h1>
  <div class="wifi-status connected" id="wifiStatus">WiFi: Подключено</div>
  <div class="state" id="state">Светодиод: ВЫКЛ</div>
  <button class="button on" onclick="controlLED(1)">ВКЛЮЧИТЬ</button>
  <button class="button off" onclick="controlLED(0)">ВЫКЛЮЧИТЬ</button>
  
  <script>
    function controlLED(state) {
      fetch('/led?state=' + state)
        .then(response => response.text())
        .then(data => {
          document.getElementById('state').innerHTML = 
            state == 1 ? 'Светодиод: ВКЛ' : 'Светодиод: ВЫКЛ';
        })
        .catch(error => {
          document.getElementById('wifiStatus').className = 'wifi-status disconnected';
          document.getElementById('wifiStatus').innerHTML = 'WiFi: Ошибка подключения';
        });
    }
    
    setInterval(() => {
      fetch('/wifi-status')
        .then(response => response.text())
        .then(status => {
          if(status === 'connected') {
            document.getElementById('wifiStatus').className = 'wifi-status connected';
            document.getElementById('wifiStatus').innerHTML = 'WiFi: Подключено';
          } else {
            document.getElementById('wifiStatus').className = 'wifi-status disconnected';
            document.getElementById('wifiStatus').innerHTML = 'WiFi: Переподключение...';
          }
        })
        .catch(() => {
          document.getElementById('wifiStatus').className = 'wifi-status disconnected';
          document.getElementById('wifiStatus').innerHTML = 'WiFi: Нет соединения';
        });
    }, 5000);
  </script>
</body>
</html>
)rawliteral";

void connectToWiFi() {
  Serial.println("=== НАЧАЛО ПОДКЛЮЧЕНИЯ К WIFI ===");
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    digitalWrite(ledPin, !digitalRead(ledPin));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WIFI ПОДКЛЮЧЕН");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    digitalWrite(ledPin, LOW);
    Serial.println("=== ПОДКЛЮЧЕНИЕ УСПЕШНО ===");
  } else {
    Serial.println("\n✗ ОШИБКА ПОДКЛЮЧЕНИЯ К WIFI");
    digitalWrite(ledPin, LOW);
    Serial.println("=== ПОДКЛЮЧЕНИЕ ПРОВАЛЕНО ===");
  }
}

void checkWiFiConnection() {
  static unsigned long lastCheckTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastCheckTime > 10000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠ WIFI ОТКЛЮЧЕН, ПЕРЕПОДКЛЮЧЕНИЕ...");
      
      for(int i = 0; i < 5; i++) {
        digitalWrite(ledPin, HIGH);
        delay(100);
        digitalWrite(ledPin, LOW);
        delay(100);
      }
      
      connectToWiFi();
    }
    lastCheckTime = currentTime;
  }
}

void handleRoot() {
  Serial.println("📄 ЗАПРОС: Главная страница");
  server.send(200, "text/html", htmlPage);
}

void handleLED() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    String clientIP = server.client().remoteIP().toString();
    
    Serial.println("----------------------------");
    Serial.print("📨 КОМАНДА ОТ ");
    Serial.println(clientIP);
    Serial.print("ПАРАМЕТР: state=");
    Serial.println(state);
    
    if (state == "1") {
      digitalWrite(ledPin, HIGH);
      ledState = true;
      server.send(200, "text/plain", "LED ON");
      Serial.println("✅ ВЫПОЛНЕНО: Светодиод ВКЛ");
    } else if (state == "0") {
      digitalWrite(ledPin, LOW);
      ledState = false;
      server.send(200, "text/plain", "LED OFF");
      Serial.println("✅ ВЫПОЛНЕНО: Светодиод ВЫКЛ");
    } else {
      server.send(400, "text/plain", "Bad Request");
      Serial.println("❌ ОШИБКА: Неверный параметр state");
    }
    
    Serial.print("ТЕКУЩИЙ СТАТУС: ");
    Serial.println(ledState ? "ВКЛ" : "ВЫКЛ");
    Serial.println("----------------------------");
  } else {
    server.send(400, "text/plain", "Bad Request");
    Serial.println("❌ ОШИБКА: Отсутствует параметр state");
  }
}

void handleStatus() {
  Serial.println("📊 ЗАПРОС: Статус светодиода");
  server.send(200, "text/plain", ledState ? "1" : "0");
}

void handleWiFiStatus() {
  Serial.println("📶 ЗАПРОС: Статус WiFi");
  if (WiFi.status() == WL_CONNECTED) {
    server.send(200, "text/plain", "connected");
  } else {
    server.send(200, "text/plain", "disconnected");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32 LED CONTROLLER ===");
  Serial.println("ИНИЦИАЛИЗАЦИЯ...");
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  Serial.println("✅ Светодиод инициализирован (GPIO2)");
  
  WiFi.setSleep(false);
  connectToWiFi();
  
  server.on("/", handleRoot);
  server.on("/led", handleLED);
  server.on("/status", handleStatus);
  server.on("/wifi-status", handleWiFiStatus);
  
  server.begin();
  Serial.println("✅ HTTP сервер запущен на порту 80");
  
  for(int i = 0; i < 3; i++) {
    digitalWrite(ledPin, HIGH);
    delay(150);
    digitalWrite(ledPin, LOW);
    delay(150);
  }
  Serial.println("✅ Система готова к работе");
  Serial.println("==============================\n");
}

void loop() {
  server.handleClient();
  checkWiFiConnection();
  
  static unsigned long lastPrintTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastPrintTime > 60000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[АКТИВНО] WiFi подключен, RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("[АКТИВНО] WiFi отключен");
    }
    lastPrintTime = currentTime;
  }
  
  delay(10);
}