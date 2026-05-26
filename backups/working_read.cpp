#include <Arduino.h>
#include <Wire.h>
#include <I2Cdev.h>
#include <MPU6050.h>
#include <LiquidCrystal_I2C.h>

MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Dacă nu apare nimic, încearcă 0x3F

int16_t ax, ay, az;
int16_t gx, gy, gz;

long axOffset = 0;
long ayOffset = 0;
long azOffset = 0;

float gravityX = 0;
float gravityY = 0;
float gravityZ = 0;

unsigned long lastLcdUpdate = 0;
unsigned long lastMoveTime = 0;

const int deadZone = 1800;
const float diagonalRatio = 0.6f;
const float gravityAlpha = 0.98f;
const unsigned long lcdUpdateInterval = 60;
const unsigned long moveInterval = 140;
const int lcdWidth = 16;
const int lcdHeight = 2;

int playerX = 7;
int playerY = 1;
int targetX = 12;
int targetY = 0;
int score = 0;

// Pini LED-uri pe NodeMCU
const int red = D6;
const int green = D7;
const int blue = D5;

void calibrateNeutralPosition() {
  const int sampleCount = 200;
  long sumAx = 0;
  long sumAy = 0;
  long sumAz = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrare...");
  lcd.setCursor(0, 1);
  lcd.print("Tine-l drept");

  for (int i = 0; i < sampleCount; i++) {
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    sumAx += ax;
    sumAy += ay;
    sumAz += az;
    delay(5);
  }

  axOffset = sumAx / sampleCount;
  ayOffset = sumAy / sampleCount;
  azOffset = sumAz / sampleCount;
  gravityX = axOffset;
  gravityY = ayOffset;
  gravityZ = azOffset;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Offset gata");
  delay(500);
  lcd.clear();
}

String getDirectionLabel(long dx, long dy) {
  long absDx = abs(dx);
  long absDy = abs(dy);

  if (absDx < deadZone && absDy < deadZone) {
    return "NEUTRU";
  }

  if (absDx > absDy * (1.0f + diagonalRatio)) {
    return dx > 0 ? "STANGA" : "DREAPTA";
  }

  if (absDy > absDx * (1.0f + diagonalRatio)) {
    return dy > 0 ? "SPATE" : "FATA";
  }

  if (dx > 0 && dy > 0) return "SPATE-STANGA";
  if (dx < 0 && dy > 0) return "SPATE-DREAPTA";
  if (dx > 0 && dy < 0) return "FATA-STANGA";
  return "FATA-DREAPTA";
}

void spawnTarget() {
  targetX = random(lcdWidth);
  targetY = random(lcdHeight);

  if (targetX == playerX && targetY == playerY) {
    targetX = (targetX + 5) % lcdWidth;
  }
}

void movePlayer(const String &direction) {
  if (direction == "NEUTRU") {
    return;
  }

  if (direction == "FATA") playerY--;
  else if (direction == "SPATE") playerY++;
  else if (direction == "STANGA") playerX--;
  else if (direction == "DREAPTA") playerX++;
  else if (direction == "FATA-DREAPTA") {
    playerY--;
    playerX++;
  } else if (direction == "FATA-STANGA") {
    playerY--;
    playerX--;
  } else if (direction == "SPATE-DREAPTA") {
    playerY++;
    playerX++;
  } else if (direction == "SPATE-STANGA") {
    playerY++;
    playerX--;
  }

  if (playerX < 0) playerX = 0;
  if (playerX >= lcdWidth) playerX = lcdWidth - 1;
  if (playerY < 0) playerY = 0;
  if (playerY >= lcdHeight) playerY = lcdHeight - 1;
}

void renderDirection(const String &direction) {
  char row0[lcdWidth + 1];
  char row1[lcdWidth + 1];

  snprintf(row0, sizeof(row0), "%-16s", "DIR:");
  snprintf(row1, sizeof(row1), "%-16.16s", direction.c_str());

  lcd.setCursor(0, 0);
  lcd.print(row0);
  lcd.setCursor(0, 1);
  lcd.print(row1);
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  randomSeed(micros());

  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);

  // PE NodeMCU v2, I2C se pornește specificând pinii: SDA = D2, SCL = D1
  Wire.begin(D2, D1);   

  // Inițializăm ecranul LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD FUNCTIONEAZA");
  lcd.setCursor(0, 1);
  lcd.print("TEST START");
  delay(2000);

  // Inițializăm senzorul MPU6050 direct
  mpu.initialize();
  calibrateNeutralPosition();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CALIBRARE GATA");
  lcd.setCursor(0, 1);
  lcd.print("LCD OK");
  delay(2000);
  renderDirection("NEUTRU");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Citim datele de la accelerometru
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  unsigned long now = millis();

  gravityX = gravityAlpha * gravityX + (1.0f - gravityAlpha) * ax;
  gravityY = gravityAlpha * gravityY + (1.0f - gravityAlpha) * ay;
  gravityZ = gravityAlpha * gravityZ + (1.0f - gravityAlpha) * az;

  long dx = (long)(ax - gravityX);
  long dy = (long)(ay - gravityY);
  long dz = (long)(az - gravityZ);
  String direction = getDirectionLabel(dx, dy);

  long motionStrength = abs(dx) + abs(dy) + abs(dz);

  // Controlul LED-urilor (D6, D7, D8)
  digitalWrite(red, abs(dx) > deadZone ? HIGH : LOW);
  digitalWrite(green, abs(dy) > deadZone ? HIGH : LOW);
  digitalWrite(blue, motionStrength > deadZone * 2 ? HIGH : LOW);

  // Controlul buzzerului pasiv pe D5

  // Trimitem datele în Serial
  Serial.print("X: "); Serial.print(dx);
  Serial.print(" | Y: "); Serial.print(dy);
  Serial.print(" | Dir: "); Serial.println(direction);

  // Afișăm doar direcția pe LCD fără să blocăm loop-ul
  if (now - lastLcdUpdate >= lcdUpdateInterval) {
    lastLcdUpdate = now;
    renderDirection(direction);
  }
}