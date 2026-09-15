// ============================================================================
//  ESP32 ROBOT ARM CONTROLLER v3.0 (WiFi + USB Serial Edition)
//  Channels: 15 (Base) → 9 (Gripper)
//  Protocol: $CMD,arg1,arg2*\n
//  Wireless: Built-in Access Point ("RobotArm-Hotkeys") + WebSockets (Port 81)
//            Built-in Web Server (Port 80) serving complete Hotkey Controller UI
//  Driver:   Zero-Glitch Direct PCA9685 Register Driver via Wire.h (onTime=0)
// ============================================================================
//
//  HARDWARE WIRING:
//    ESP32 GPIO 21 (SDA) → PCA9685 SDA
//    ESP32 GPIO 22 (SCL) → PCA9685 SCL
//    ESP32 GND            → PCA9685 GND  → Power Supply GND (common ground!)
//    Power Supply 5-6V    → PCA9685 V+ screw terminal
//    PCA9685 OE pin       → GND (active low = outputs enabled)
//
//  CHANNEL MAP:
//    CH 15: Base Turntable Yaw       (MG996R)   0-180°
//    CH 14: Shoulder Pitch (Grey)    (MG996R)  10-170°
//    CH 13: Elbow Pitch (Blue)       (MG996R)  10-170°
//    CH 12: Forearm Pitch (Grey)     (MG996R)  10-170°
//    CH 11: Wrist Roll              (MG90S)    0-180°
//    CH 10: Wrist Pitch             (MG90S)   10-170°
//    CH  9: Gripper Claw            (SG90)     0-90°
//
//  HOW TO CONNECT WIRELESSLY:
//    1. Power on the ESP32.
//    2. On your phone, tablet, or PC, connect to WiFi: "RobotArm-Hotkeys" (Pass: 12345678).
//    3. Open any web browser to: http://192.168.4.1
//    4. The full hotkey controller connects automatically over WebSocket!
// ============================================================================

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoWebsockets.h>
#include "web_page.h"

using namespace websockets;

// ─── WiFi AP Configuration (Direct Wireless Access Point) ───────────────────
const char* AP_SSID = "RobotArm-Hotkeys";
const char* AP_PASS = "12345678";        // Must be at least 8 chars

// ─── Optional: Connect to Home / Workshop WiFi ──────────────────────────────
// Set to true and fill in your WiFi details to connect to your home router too
const bool  CONNECT_TO_STA = false;
const char* STA_SSID = "YourHomeWiFi";
const char* STA_PASS = "YourWiFiPassword";

// ─── Web & WebSocket Servers ────────────────────────────────────────────────
WebServer httpServer(80);
WebsocketsServer wsServer;
#define MAX_WS_CLIENTS 4
WebsocketsClient wsClients[MAX_WS_CLIENTS];

// ─── Pin & Address Configuration ────────────────────────────────────────────
#define I2C_SDA       21
#define I2C_SCL       22
#define PCA_ADDR      0x40

// ─── PCA9685 Registers ─────────────────────────────────────────────────────
#define REG_MODE1     0x00
#define REG_MODE2     0x01
#define REG_LED0_ON_L 0x06
#define REG_PRESCALE  0xFE

// ─── PCA9685 Bits ───────────────────────────────────────────────────────────
#define MODE1_AI      0x20    // Auto-increment
#define MODE1_SLEEP   0x10    // Sleep (must set to change prescaler)
#define MODE1_ALLCALL 0x01    // Respond to all-call address
#define MODE2_OUTDRV  0x04    // Totem-pole outputs
// OCH bit = 0 (default) → outputs update on I2C STOP, not ACK = atomic

// ─── Servo PWM Calibration (50 Hz = 20ms period, 12-bit = 4096 ticks) ─────
#define TICK_MIN      102     // ~500µs (0 deg)
#define TICK_MAX      512     // ~2500µs (180 deg)
#define TICK_CENTER   307     // ~1500µs (90 deg)

// ─── Arm Channel Definitions ───────────────────────────────────────────────
#define NUM_JOINTS    7

// Index 0=Base(CH15), 1=Shoulder(CH14), ... 6=Gripper(CH9)
const uint8_t jointChannel[NUM_JOINTS] = {15, 14, 13, 12, 11, 10, 9};
const int     jointMin[NUM_JOINTS]     = { 0, 10, 10, 10,  0, 10,  0};
const int     jointMax[NUM_JOINTS]     = {180,170,170,170,180,170, 90};
// Default Home pose matching default.png: CH15=90, CH14=90, CH13=60, CH12=170, CH11=90, CH10=125, CH9=45
const int     jointDefault[NUM_JOINTS] = {90, 90, 60, 170, 90, 125, 45};

// ─── Servo State ────────────────────────────────────────────────────────────
float currentAngle[NUM_JOINTS];     // Smoothly interpolated current position
int   targetAngle[NUM_JOINTS];      // Commanded target position
bool  jointEnabled[NUM_JOINTS];     // Is this joint actively driven?
int   lastWrittenTicks[NUM_JOINTS]; // Cache: last tick value written to PCA9685

// ─── Slew Rate Limiter ──────────────────────────────────────────────────────
#define SLEW_INTERVAL_MS  20        // 20ms = 50Hz (synchronized to PCA9685 50Hz PWM frame rate)
float slewStepDeg = 1.6f;           // Degrees per 20ms tick (1.6° = 80°/sec, fast & responsive)
unsigned long lastSlewMs = 0;

// ─── Watchdog ───────────────────────────────────────────────────────────────
#define WATCHDOG_INTERVAL_MS 5000
unsigned long lastWatchdogMs = 0;
bool slewActive = false;            // Flag to avoid I2C collision with watchdog

// ─── Non-blocking Sweep State Machine ───────────────────────────────────────
bool sweepRunning = false;
int  sweepJointIdx = -1;
int  sweepAngle = 0;
int  sweepPhase = 0;                // 0=up, 1=down, 2=center
unsigned long lastSweepMs = 0;
#define SWEEP_STEP_MS 30

// ─── E-Stop ─────────────────────────────────────────────────────────────────
bool frozen = false;


// ============================================================================
//  SECTION A: MINIMAL ZERO-GLITCH PCA9685 DRIVER
// ============================================================================

void pca_busRecover() {
  Wire.end();
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, OUTPUT);

  for (int i = 0; i < 9; i++) {
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(5);
  }
  // STOP condition
  pinMode(I2C_SDA, OUTPUT);
  digitalWrite(I2C_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(I2C_SDA, HIGH);
  delayMicroseconds(5);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
}

bool pca_writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

uint8_t pca_readReg(uint8_t reg) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  Wire.requestFrom((uint8_t)PCA_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

bool pca_write4(uint8_t ch, uint16_t onTime, uint16_t offTime) {
  if (ch > 15) return false;

  for (int attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(PCA_ADDR);
    Wire.write(REG_LED0_ON_L + (ch * 4));
    Wire.write((uint8_t)(onTime & 0xFF));
    Wire.write((uint8_t)((onTime >> 8) & 0x0F));
    Wire.write((uint8_t)(offTime & 0xFF));
    Wire.write((uint8_t)((offTime >> 8) & 0x0F));
    uint8_t err = Wire.endTransmission();

    if (err == 0) return true;

    delayMicroseconds(100);
    if (attempt == 1) {
      Serial.printf("[I2C] Retry on CH%d, recovering bus...\n", ch);
      pca_busRecover();
    }
  }

  Serial.printf("[I2C FAIL] CH%d write failed!\n", ch);
  return false;
}

void pca_fullOff(uint8_t ch) {
  if (ch > 15) return;
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(REG_LED0_ON_L + (ch * 4));
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x10); // Bit 4 = Full OFF
  Wire.endTransmission();
}

bool pca_alive() {
  uint8_t mode1 = pca_readReg(REG_MODE1);
  if (mode1 == 0xFF) return false;
  if (mode1 & MODE1_SLEEP) return false;
  if (!(mode1 & MODE1_AI)) return false;
  return true;
}

bool pca_init() {
  pca_busRecover();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  Wire.beginTransmission(PCA_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[ERROR] PCA9685 not found at 0x40!");
    return false;
  }

  pca_writeReg(REG_MODE1, 0x00);
  delay(10);
  pca_writeReg(REG_MODE1, MODE1_SLEEP);
  delay(5);
  pca_writeReg(REG_PRESCALE, 121);   // 50 Hz
  delay(5);
  pca_writeReg(REG_MODE1, MODE1_AI | MODE1_ALLCALL);
  delay(5);
  pca_writeReg(REG_MODE2, MODE2_OUTDRV);
  delay(5);

  Serial.println("[PCA] Initialized: 50Hz, AI=1, OUTDRV=1, OCH=0");
  return true;
}

void pca_recover() {
  Serial.println("[WATCHDOG] Anomaly detected — recovering registers...");
  pca_busRecover();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  pca_writeReg(REG_MODE1, MODE1_SLEEP);
  delay(5);
  pca_writeReg(REG_PRESCALE, 121);
  delay(5);
  pca_writeReg(REG_MODE1, MODE1_AI | MODE1_ALLCALL);
  delay(5);
  pca_writeReg(REG_MODE2, MODE2_OUTDRV);
  delay(5);

  for (int i = 0; i < NUM_JOINTS; i++) {
    if (jointEnabled[i]) {
      lastWrittenTicks[i] = -1;
      servo_applyHardware(i);
      delay(20);
    }
  }
  Serial.println("[WATCHDOG] Recovery complete — angles preserved.");
}


// ============================================================================
//  SECTION B: SERVO ABSTRACTION LAYER
// ============================================================================

int servo_degreesToTicks(int deg) {
  deg = constrain(deg, 0, 180);
  return map(deg, 0, 180, TICK_MIN, TICK_MAX);
}

// Apply current angle for joint[index] to hardware with onTime=0 (zero glitch)
void servo_applyHardware(int idx) {
  if (idx < 0 || idx >= NUM_JOINTS || !jointEnabled[idx]) return;

  uint8_t ch = jointChannel[idx];
  int ticks = servo_degreesToTicks((int)round(currentAngle[idx]));

  if (ticks == lastWrittenTicks[idx]) return;
  lastWrittenTicks[idx] = ticks;

  // onTime = 0 is universal standard for RC servos (clean, glitch-free)
  pca_write4(ch, 0, (uint16_t)ticks);
}

void servo_setTarget(int idx, int angle) {
  if (idx < 0 || idx >= NUM_JOINTS) return;
  if (frozen) return;

  angle = constrain(angle, jointMin[idx], jointMax[idx]);
  targetAngle[idx] = angle;
  jointEnabled[idx] = true;
}

int servo_getAngle(int idx) {
  if (idx < 0 || idx >= NUM_JOINTS) return 90;
  return (int)round(currentAngle[idx]);
}

int channelToIndex(int ch) {
  for (int i = 0; i < NUM_JOINTS; i++) {
    if (jointChannel[i] == ch) return i;
  }
  return -1;
}

void servo_slewTick() {
  slewActive = false;

  for (int i = 0; i < NUM_JOINTS; i++) {
    if (!jointEnabled[i]) continue;

    float diff = (float)targetAngle[i] - currentAngle[i];
    float absDiff = fabs(diff);

    if (absDiff < 0.05f) continue;

    slewActive = true;

    float step = (absDiff < slewStepDeg) ? absDiff : slewStepDeg;

    if (diff > 0) {
      currentAngle[i] += step;
    } else {
      currentAngle[i] -= step;
    }

    servo_applyHardware(i);
  }
}

void servo_freezeAll() {
  frozen = true;
  for (int i = 0; i < NUM_JOINTS; i++) {
    targetAngle[i] = (int)round(currentAngle[i]);
  }
}

void servo_unfreeze() {
  frozen = false;
}

void servo_disableAll() {
  for (int i = 0; i < NUM_JOINTS; i++) {
    pca_fullOff(jointChannel[i]);
    jointEnabled[i] = false;
    lastWrittenTicks[i] = -1;
  }
}

void servo_enableAll() {
  for (int i = 0; i < NUM_JOINTS; i++) {
    jointEnabled[i] = true;
    lastWrittenTicks[i] = -1;
    servo_applyHardware(i);
    delay(20);
  }
}

void servo_centerAll() {
  for (int i = 0; i < NUM_JOINTS; i++) {
    currentAngle[i] = (float)jointDefault[i];
    targetAngle[i] = jointDefault[i];
    jointEnabled[i] = true;
    lastWrittenTicks[i] = -1;
    servo_applyHardware(i);
    delay(35);
  }
}


// ============================================================================
//  SECTION C: BROADCAST & SERIAL/WEBSOCKET REPLIES
// ============================================================================

void sendReply(const String& line) {
  Serial.println(line);
  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (wsClients[i].available()) {
      wsClients[i].send(line);
    }
  }
}


// ============================================================================
//  SECTION D: COMMAND PARSER (Shared by Serial & WiFi)
// ============================================================================

enum ParseState { PS_IDLE, PS_CMD, PS_ARG1, PS_ARG2 };

ParseState ps = PS_IDLE;
char cmdBuf[8];   int cmdLen = 0;
char arg1Buf[8];  int arg1Len = 0;
char arg2Buf[8];  int arg2Len = 0;

void resetParser() {
  ps = PS_IDLE;
  cmdLen = 0;
  arg1Len = 0;
  arg2Len = 0;
}

void executeCommand() {
  cmdBuf[cmdLen] = '\0';
  arg1Buf[arg1Len] = '\0';
  arg2Buf[arg2Len] = '\0';

  // ── $SET,<ch>,<angle>* ──
  if (strcasecmp(cmdBuf, "SET") == 0) {
    int ch = atoi(arg1Buf);
    int angle = atoi(arg2Buf);
    int idx = channelToIndex(ch);
    if (idx >= 0 && angle >= 0 && angle <= 180) {
      servo_setTarget(idx, angle);
      sendReply("ACK:" + String(ch) + ":" + String(targetAngle[idx]));
    } else {
      sendReply("ERR:SET:bad_args ch=" + String(ch) + " angle=" + String(angle));
    }
    return;
  }

  // ── $ALL,<angle>* ──
  if (strcasecmp(cmdBuf, "ALL") == 0) {
    int angle = atoi(arg1Buf);
    if (angle >= 0 && angle <= 180) {
      for (int i = 0; i < NUM_JOINTS; i++) {
        servo_setTarget(i, angle);
      }
      sendReply("ACK:ALL:" + String(angle));
    } else {
      sendReply("ERR:ALL:bad_angle " + String(angle));
    }
    return;
  }

  // ── $HOME* ── (Home Pose from default.png)
  if (strcasecmp(cmdBuf, "HOME") == 0) {
    for (int i = 0; i < NUM_JOINTS; i++) {
      servo_setTarget(i, jointDefault[i]);
    }
    sendReply("ACK:HOME");
    return;
  }

  // ── $STOP* ── (E-Stop)
  if (strcasecmp(cmdBuf, "STOP") == 0) {
    servo_freezeAll();
    sendReply("ACK:STOP");
    return;
  }

  // ── $GO* ── (Unfreeze)
  if (strcasecmp(cmdBuf, "GO") == 0) {
    servo_unfreeze();
    sendReply("ACK:GO");
    return;
  }

  // ── $OFF* ──
  if (strcasecmp(cmdBuf, "OFF") == 0) {
    servo_disableAll();
    sendReply("ACK:OFF");
    return;
  }

  // ── $ON* ──
  if (strcasecmp(cmdBuf, "ON") == 0) {
    servo_enableAll();
    sendReply("ACK:ON");
    return;
  }

  // ── $ISO,<ch>* ──
  if (strcasecmp(cmdBuf, "ISO") == 0) {
    int ch = atoi(arg1Buf);
    int idx = channelToIndex(ch);
    if (idx >= 0) {
      for (int i = 0; i < NUM_JOINTS; i++) {
        if (i == idx) {
          currentAngle[i] = 90.0f;
          targetAngle[i] = 90;
          jointEnabled[i] = true;
          lastWrittenTicks[i] = -1;
          servo_applyHardware(i);
        } else {
          pca_fullOff(jointChannel[i]);
          jointEnabled[i] = false;
          lastWrittenTicks[i] = -1;
        }
      }
      sendReply("ACK:ISO:" + String(ch));
    } else {
      sendReply("ERR:ISO:bad_ch " + String(ch));
    }
    return;
  }

  // ── $QUERY* ──
  if (strcasecmp(cmdBuf, "QUERY") == 0) {
    String pos = "POS";
    for (int i = 0; i < NUM_JOINTS; i++) {
      pos += ":" + String(jointChannel[i]) + ":" + String(servo_getAngle(i));
    }
    sendReply(pos);
    return;
  }

  // ── $SPEED,<deg_per_sec>* ──
  if (strcasecmp(cmdBuf, "SPEED") == 0) {
    int spd = atoi(arg1Buf);
    if (spd >= 10 && spd <= 500) {
      slewStepDeg = (float)spd / 50.0f;
      sendReply("ACK:SPEED:" + String(spd));
    } else {
      sendReply("ERR:SPEED:range_10_500");
    }
    return;
  }

  // ── $SWEEP,<ch>* ──
  if (strcasecmp(cmdBuf, "SWEEP") == 0) {
    int ch = atoi(arg1Buf);
    int idx = channelToIndex(ch);
    if (idx >= 0) {
      sweepJointIdx = idx;
      sweepAngle = 0;
      sweepPhase = 0;
      sweepRunning = true;
      lastSweepMs = millis();
      jointEnabled[idx] = true;
      sendReply("ACK:SWEEP:START");
    } else {
      sendReply("ERR:SWEEP:bad_ch " + String(ch));
    }
    return;
  }

  // ── $RAW,<ch>,<ticks>* ──
  if (strcasecmp(cmdBuf, "RAW") == 0) {
    int ch = atoi(arg1Buf);
    int ticks = atoi(arg2Buf);
    if (ch >= 0 && ch <= 15 && ticks >= 50 && ticks <= 600) {
      int idx = channelToIndex(ch);
      if (idx >= 0) jointEnabled[idx] = true;
      pca_write4(ch, 0, (uint16_t)ticks);
      sendReply("ACK:RAW:" + String(ch) + ":" + String(ticks));
    } else {
      sendReply("ERR:RAW:bad_args");
    }
    return;
  }

  // ── $DIAG* ──
  if (strcasecmp(cmdBuf, "DIAG") == 0) {
    uint8_t m1 = pca_readReg(REG_MODE1);
    uint8_t m2 = pca_readReg(REG_MODE2);
    uint8_t pre = pca_readReg(REG_PRESCALE);
    String diag = "DIAG:MODE1=0x" + String(m1, HEX) + ",MODE2=0x" + String(m2, HEX) + ",PRE=" + String(pre) +
                  ",SPEED=" + String((int)(slewStepDeg * 50)) + ",CLIENTS=";
    int count = 0;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) if (wsClients[i].available()) count++;
    diag += String(count);
    sendReply(diag);
    sendReply("ACK:DIAG");
    return;
  }

  sendReply("ERR:UNKNOWN_CMD '" + String(cmdBuf) + "'");
}

void parseSerialByte(char c) {
  if (c == '$') {
    resetParser();
    ps = PS_CMD;
    return;
  }

  switch (ps) {
    case PS_CMD:
      if (c == ',' || c == ' ') {
        ps = PS_ARG1;
      } else if (c == '*' || c == '\n' || c == '\r') {
        executeCommand();
        resetParser();
      } else if (cmdLen < 7) {
        cmdBuf[cmdLen++] = c;
      }
      break;

    case PS_ARG1:
      if (c == ',' || c == ' ') {
        ps = PS_ARG2;
      } else if (c == '*' || c == '\n' || c == '\r') {
        executeCommand();
        resetParser();
      } else if (arg1Len < 7) {
        arg1Buf[arg1Len++] = c;
      }
      break;

    case PS_ARG2:
      if (c == '*' || c == '\n' || c == '\r') {
        executeCommand();
        resetParser();
      } else if (arg2Len < 7) {
        arg2Buf[arg2Len++] = c;
      }
      break;

    case PS_IDLE:
    default:
      if (c == 'C' || c == 'c') {
        resetParser();
        cmdBuf[0] = 'S'; cmdBuf[1] = 'E'; cmdBuf[2] = 'T';
        cmdLen = 3;
        ps = PS_ARG1;
      }
      break;
  }
}


// ============================================================================
//  SECTION E: NON-BLOCKING SWEEP STATE MACHINE
// ============================================================================

void sweepTick() {
  if (!sweepRunning || sweepJointIdx < 0) return;

  unsigned long now = millis();
  if (now - lastSweepMs < SWEEP_STEP_MS) return;
  lastSweepMs = now;

  uint8_t ch = jointChannel[sweepJointIdx];

  switch (sweepPhase) {
    case 0:
      currentAngle[sweepJointIdx] = (float)sweepAngle;
      targetAngle[sweepJointIdx] = sweepAngle;
      servo_applyHardware(sweepJointIdx);
      sweepAngle += 5;
      if (sweepAngle > 180) {
        sweepAngle = 180;
        sweepPhase = 1;
      }
      break;

    case 1:
      currentAngle[sweepJointIdx] = (float)sweepAngle;
      targetAngle[sweepJointIdx] = sweepAngle;
      servo_applyHardware(sweepJointIdx);
      sweepAngle -= 5;
      if (sweepAngle < 0) {
        sweepAngle = jointDefault[sweepJointIdx];
        sweepPhase = 2;
      }
      break;

    case 2:
      currentAngle[sweepJointIdx] = (float)sweepAngle;
      targetAngle[sweepJointIdx] = sweepAngle;
      servo_applyHardware(sweepJointIdx);
      sendReply("ACK:SWEEP:" + String(ch) + ":DONE");
      sweepRunning = false;
      sweepJointIdx = -1;
      break;
  }
}


// ============================================================================
//  SECTION F: WEBSOCKET & WEB SERVER ENGINE
// ============================================================================

void onWsMessage(WebsocketsClient& client, WebsocketsMessage message) {
  String data = message.data();
  for (unsigned int i = 0; i < data.length(); i++) {
    parseSerialByte(data[i]);
  }
}

void handleWebSockets() {
  if (wsServer.poll()) {
    WebsocketsClient newClient = wsServer.accept();
    if (newClient.available()) {
      bool accepted = false;
      for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (!wsClients[i].available()) {
          wsClients[i] = newClient;
          wsClients[i].onMessage(onWsMessage);
          
          // Send initial joint positions on connect
          String pos = "POS";
          for (int j = 0; j < NUM_JOINTS; j++) {
            pos += ":" + String(jointChannel[j]) + ":" + String(servo_getAngle(j));
          }
          wsClients[i].send(pos);
          accepted = true;
          Serial.printf("[WiFi] Client connected to slot %d\n", i);
          break;
        }
      }
      if (!accepted) {
        newClient.close();
      }
    }
  }

  for (int i = 0; i < MAX_WS_CLIENTS; i++) {
    if (wsClients[i].available()) {
      wsClients[i].poll();
    }
  }
}

// ============================================================================
//  SECTION G: MAIN SETUP & LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================================");
  Serial.println("  🦾 ESP32 ROBOT ARM CONTROLLER v3.0 (WiFi + USB Edition)");
  Serial.println("  Direct PCA9685 Zero-Glitch Driver | WebSockets Port 81");
  Serial.println("========================================================");

  // Initialize joint states
  for (int i = 0; i < NUM_JOINTS; i++) {
    currentAngle[i] = (float)jointDefault[i];
    targetAngle[i] = jointDefault[i];
    jointEnabled[i] = false;
    lastWrittenTicks[i] = -1;
  }

  // 1. Start WiFi Access Point
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress apIP = WiFi.softAPIP();

  Serial.print("[WiFi] Access Point Created: ");
  Serial.println(AP_SSID);
  Serial.print("[WiFi] AP IP Address:        ");
  Serial.println(apIP);

  // Optional: Station mode
  if (CONNECT_TO_STA) {
    Serial.printf("[WiFi] Connecting to %s...", STA_SSID);
    WiFi.begin(STA_SSID, STA_PASS);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[WiFi] Connected to Home WiFi!");
      Serial.print("[WiFi] Station IP Address:  ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\n[WiFi] Could not connect to Home WiFi. AP mode remains active.");
    }
  }

  // 2. Start HTTP Web Server on Port 80 (serves complete Hotkey Controller!)
  httpServer.on("/", HTTP_GET, []() {
    httpServer.send_P(200, "text/html", INDEX_HTML);
  });
  httpServer.begin();
  Serial.println("[HTTP] Web Controller live on port 80 (http://192.168.4.1)");

  // 3. Start WebSocket Server on Port 81
  wsServer.listen(81);
  Serial.println("[WS]   WebSocket Server live on port 81 (ws://192.168.4.1:81)");

  // 4. Initialize PCA9685 & Servos
  if (pca_init()) {
    servo_centerAll();
    Serial.println("[READY] All 7 Servos in Home Pose [90, 90, 60, 170, 90, 125, 45].");
    Serial.println("Ready for USB Serial or WiFi WebSocket commands!");
    Serial.println("========================================================\n");
  } else {
    Serial.println("[FATAL] Check PCA9685 wiring and power supply!");
  }

  lastSlewMs = millis();
  lastWatchdogMs = millis();
}

void loop() {
  // 1. Drain USB Serial stream
  while (Serial.available() > 0) {
    parseSerialByte((char)Serial.read());
  }

  // 2. Poll WiFi WebServer & WebSockets
  httpServer.handleClient();
  handleWebSockets();

  // 3. Smooth Slew-Rate Interpolator (50Hz = 20ms)
  unsigned long now = millis();
  if (now - lastSlewMs >= SLEW_INTERVAL_MS) {
    lastSlewMs = now;
    servo_slewTick();
  }

  // 4. Non-blocking test sweep tick
  sweepTick();

  // 5. Watchdog health check every 5 seconds (only when bus idle)
  if (now - lastWatchdogMs >= WATCHDOG_INTERVAL_MS) {
    lastWatchdogMs = now;
    if (!slewActive && !sweepRunning) {
      if (!pca_alive()) {
        pca_recover();
      }
    }
  }
}
