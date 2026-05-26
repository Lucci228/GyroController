# Tilt Game - Web Version

## Setup

### 1. Configurare WiFi
Editează `src/main.cpp` și actualizează:
```cpp
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
```

### 2. Upload
1. Conectează NodeMCU v2 la USB
2. Deschide proiectul în VS Code cu PlatformIO
3. Apasă "Upload" (PlatformIO > Upload)

### 3. Accesare
1. ESP se va conecta la WiFi și va afișa IP-ul pe LCD
2. Deschide browser-ul și navighează la `http://<IP_ESP>/`
   - Exemplu: `http://192.168.1.100/`
3. WebSocket-ul se conectează automat

## Arhitectură

### Hardware
- **ESP8266 (NodeMCU v2)**
- **MPU6050** (accelerometru/giroscop) pe I2C (D2=SDA, D1=SCL)
- **LCD 16x2** pe I2C (0x27)
- **LEDs** (D6=Red, D7=Green, D8=Blue)

### Logică
- **ESP trimite datele senzor** în JSON peste WebSocket (port 81)
- **Browser execută logica jocului** (mișcare jucător, spawn țintă, scor)
- **LCD arată doar direcția** de mișcare

### Comunicație WebSocket
Format JSON trimis de ESP:
```json
{
  "ax": -512,
  "ay": 256,
  "az": 16384,
  "gravityX": -500.5,
  "gravityY": 250.3,
  "gravityZ": 16300.2,
  "direction": "FATA"
}
```

Direcțiile posibile:
- `NEUTRU` - Nicio mișcare
- `FATA`, `SPATE`, `STANGA`, `DREAPTA` - Mișcare simpla
- `FATA-STANGA`, `FATA-DREAPTA`, `SPATE-STANGA`, `SPATE-DREAPTA` - Diagonale

## Gameplay

- Jucătorul este reprezentat cu **P**
- Ținta este reprezentată cu ***
- Când jucătorul ajunge la țintă, se afișează **#** și ținta se respawnează
- Scorul crește cu 1 la fiecare țintă capturată

## Calibrare

După upload, ESP-ul va detecta nivelul neutru (drept) timp de 1 secundă.
Ține accelerometrul **drept** și stabil în timp de calibrare!

## Debugging

- Deschide Serial Monitor (115200 baud) pentru a vedea log-uri
- Verifica conectarea WiFi și IP-ul afișat pe LCD
- Verifica conexiunea WebSocket în console-ul browser-ului (F12)
