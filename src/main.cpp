#include <Arduino.h>
#include <Wire.h>
#include <I2Cdev.h>
#include <MPU6050.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ============================================================
//  CONFIGURARE WI-FI — schimba cu datele tale (vezi include/credentials_private.h)
// ============================================================
#include "credentials_private.h"
// ============================================================

MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebSocketsServer webSocket = WebSocketsServer(81);

int16_t ax, ay, az;
int16_t gx, gy, gz;

long axOffset = 0;
long ayOffset = 0;
long azOffset = 0;

float gravityX = 0;
float gravityY = 0;
float gravityZ = 0;

unsigned long lastWebUpdate = 0;
unsigned long lastMoveTime  = 0;
unsigned long lastLcdUpdate  = 0;
unsigned long lastMpuRead = 0;
const unsigned long mpuReadInterval = 30; // ms between MPU reads (~33Hz)

// Direcție stabilă pentru LED
String lastDirection = "NEUTRU";
unsigned long directionStableSince = 0;
const unsigned long ledStableThreshold = 1000; // ms required to light LEDs

int zeroReadCount = 0;

const int   deadZone      = 1800;
const float diagonalRatio = 0.6f;
const float gravityAlpha  = 0.98f;
const unsigned long moveInterval = 140;

const int gameWidth  = 32;
const int gameHeight = 32;

int playerX = 15;
int playerY = 15;
int targetX = 25;
int targetY = 5;
int score   = 0;

const int red   = D6;
const int green = D7;
const int blue  = D5;

// ============================================================
//  LCD — afisare identica cu working_read.cpp
// ============================================================
void renderDirection(const String &direction) {
  char row0[17];
  char row1[17];

  snprintf(row0, sizeof(row0), "%-16s", "DIR:");
  snprintf(row1, sizeof(row1), "%-16.16s", direction.c_str());

  lcd.setCursor(0, 0);
  lcd.print(row0);
  lcd.setCursor(0, 1);
  lcd.print(row1);
}

// ============================================================
//  CALIBRARE
// ============================================================
void calibrateNeutralPosition() {
  const int sampleCount = 200;
  long sumAx = 0, sumAy = 0, sumAz = 0;

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Calibrare...");
  lcd.setCursor(0, 1); lcd.print("Tine-l drept");

  for (int i = 0; i < sampleCount; i++) {
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    sumAx += ax; sumAy += ay; sumAz += az;
    delay(5);
  }

  axOffset = sumAx / sampleCount;
  ayOffset = sumAy / sampleCount;
  azOffset = sumAz / sampleCount;
  gravityX = axOffset;
  gravityY = ayOffset;
  gravityZ = azOffset;

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Offset gata");
  delay(500);
  lcd.clear();
}

// ============================================================
//  LOGICA JOC
// ============================================================
String getDirectionLabel(long dx, long dy) {
  long absDx = abs(dx);
  long absDy = abs(dy);
  if (absDx < deadZone && absDy < deadZone) {
    return "NEUTRU";
  }
  // Remap axes: treat dx as forward/back and dy as left/right
  if (absDx > absDy * (1.0f + diagonalRatio))
    return dx > 0 ? "FATA" : "SPATE";
  if (absDy > absDx * (1.0f + diagonalRatio))
    return dy > 0 ? "STANGA" : "DREAPTA";
  if (dx > 0 && dy > 0) return "FATA-STANGA";
  if (dx < 0 && dy > 0) return "SPATE-STANGA";
  if (dx > 0 && dy < 0) return "FATA-DREAPTA";
  return "SPATE-DREAPTA";
}

void spawnTarget() {
  targetX = random(gameWidth);
  targetY = random(gameHeight);
  if (targetX == playerX && targetY == playerY)
    targetX = (targetX + 8) % gameWidth;
}

void movePlayer(const String &direction) {
  if (direction == "NEUTRU") return;
  if      (direction == "FATA")          playerY--;
  else if (direction == "SPATE")         playerY++;
  else if (direction == "STANGA")        playerX--;
  else if (direction == "DREAPTA")       playerX++;
  else if (direction == "FATA-DREAPTA")  { playerY--; playerX++; }
  else if (direction == "FATA-STANGA")   { playerY--; playerX--; }
  else if (direction == "SPATE-DREAPTA") { playerY++; playerX++; }
  else if (direction == "SPATE-STANGA")  { playerY++; playerX--; }

  if (playerX < 0)           playerX = 0;
  if (playerX >= gameWidth)  playerX = gameWidth - 1;
  if (playerY < 0)           playerY = 0;
  if (playerY >= gameHeight) playerY = gameHeight - 1;
}

// ============================================================
//  WEBSOCKET EVENT
// ============================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED)
    Serial.printf("[WS] Client %u conectat\n", num);
  else if (type == WStype_DISCONNECTED)
    Serial.printf("[WS] Client %u deconectat\n", num);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  randomSeed(micros());

  pinMode(red,   OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue,  OUTPUT);

  Wire.begin(D2, D1);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Tilt Game");
  lcd.setCursor(0, 1); lcd.print("Conectare WiFi..");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectare la "); Serial.println(ssid);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print("."); tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi OK!");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi esuat!");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi ESUAT");
    lcd.setCursor(0, 1); lcd.print("Mod offline");
  }

  delay(1500);

  mpu.initialize();
  // Reduce internal sample rate to stabilize readings on ESP8266
  mpu.setRate(19); // Sample rate = GyroRate/(1+SMPLRT_DIV) => ~50Hz when DLPF enabled
  calibrateNeutralPosition();
  spawnTarget();

  // Doar WebSocket — fara HTTP server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket pornit pe portul 81");
  Serial.println("Deschide index.html in browser si seteaza IP-ul ESP!");

  lcd.clear();
  renderDirection("NEUTRU");
  directionStableSince = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  webSocket.loop();

  unsigned long now = millis();

  // Throttle MPU reads to reduce I2C load on ESP8266
  if (now - lastMpuRead >= mpuReadInterval) {
    lastMpuRead = now;

    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    if (ax == 0 && ay == 0 && az == 0) {
      zeroReadCount++;
    } else {
      zeroReadCount = 0;
    }
    if (zeroReadCount > 12) {
      Serial.println("MPU zeros, reinitializare...");
      Wire.begin(D2, D1); delay(50);
      mpu.initialize();
      calibrateNeutralPosition();
      zeroReadCount = 0;
    }

    gravityX = gravityAlpha * gravityX + (1.0f - gravityAlpha) * ax;
    gravityY = gravityAlpha * gravityY + (1.0f - gravityAlpha) * ay;
    gravityZ = gravityAlpha * gravityZ + (1.0f - gravityAlpha) * az;

    long dx = (long)(ax - gravityX);
    long dy = (long)(ay - gravityY);
    long dz = (long)(az - gravityZ);
    String direction = getDirectionLabel(dx, dy);
    long motionStrength = abs(dx) + abs(dy) + abs(dz);

    // Actualizeaza timpul cand direcția s-a schimbat
    if (direction != lastDirection) {
      lastDirection = direction;
      directionStableSince = now;
    }

    // LED-urile se aprind doar dacă controller-ul păstrează o direcție (> NEUTRU)
    // pentru mai mult de `ledStableThreshold` milisecunde
    bool directionStable = (direction != "NEUTRU") && (now - directionStableSince >= ledStableThreshold);
    bool showX = directionStable && (abs(dx) > deadZone);
    bool showY = directionStable && (abs(dy) > deadZone);
    bool showMotion = directionStable && (motionStrength > deadZone * 2);

    digitalWrite(red,   showX ? HIGH : LOW);
    digitalWrite(green, showY ? HIGH : LOW);
    digitalWrite(blue,  showMotion ? HIGH : LOW);

    Serial.print("X: "); Serial.print(dx);
    Serial.print(" | Y: "); Serial.print(dy);
    Serial.print(" | Dir: "); Serial.println(direction);

    // LCD
    if (now - lastLcdUpdate >= 60) {
      lastLcdUpdate = now;
      renderDirection(direction);
    }
    // WebSocket la fiecare 80ms
    if (now - lastWebUpdate >= 80) {
      lastWebUpdate = now;
      StaticJsonDocument<128> doc;
      // Send only the movement direction; game logic runs in the browser
      doc["direction"] = direction;
      String json;
      serializeJson(doc, json);
      webSocket.broadcastTXT(json);
    }
  }
}
