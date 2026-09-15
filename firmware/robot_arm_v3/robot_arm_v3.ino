// ============================================================================
//  ESP32 ROBOT ARM CONTROLLER v3.0 — GROUND-UP REBUILD
//  Channels: 15 (Base) → 9 (Gripper)
//  Protocol: $CMD,arg1,arg2*\n  (115200 baud)
//  Driver:   Direct PCA9685 register access via Wire.h — ZERO external libs
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
// ============================================================================

#include <Wire.h>

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
//  0°   → ~500µs  → 500/20000 * 4096 ≈ 102 ticks
//  180° → ~2500µs → 2500/20000 * 4096 ≈ 512 ticks
#define TICK_MIN      102
#define TICK_MAX      512
#define TICK_CENTER   307

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
                                    // Only re-write when this changes → eliminates
                                    // redundant I2C traffic that causes bus noise

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
int  sweepDir = 1;                  // +1 going up, -1 going down, 0 = done
int  sweepPhase = 0;                // 0=up, 1=down, 2=center, 3=done
unsigned long lastSweepMs = 0;
#define SWEEP_STEP_MS 30

// ─── E-Stop ─────────────────────────────────────────────────────────────────
bool frozen = false;


// ============================================================================
//  SECTION A: MINIMAL PCA9685 DRIVER
// ============================================================================

// 9-clock bus recovery for hung I2C slaves
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

  // Re-init Wire
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
}

// Write a single register byte
bool pca_writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

// Read a single register byte
uint8_t pca_readReg(uint8_t reg) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  Wire.requestFrom((uint8_t)PCA_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

// Atomic 4-byte burst write: ON_L, ON_H, OFF_L, OFF_H
// Auto-increment (AI bit in MODE1) advances the register pointer automatically
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
      Serial.printf("[I2C] Retry failed on CH%d, recovering bus...\n", ch);
      pca_busRecover();
    }
  }

  Serial.printf("[I2C FAIL] CH%d write failed after 3 attempts!\n", ch);
  return false;
}

// Set channel to full-off (0% duty, no holding current)
void pca_fullOff(uint8_t ch) {
  if (ch > 15) return;
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(REG_LED0_ON_L + (ch * 4));
  Wire.write(0x00);  // ON_L
  Wire.write(0x00);  // ON_H
  Wire.write(0x00);  // OFF_L
  Wire.write(0x10);  // OFF_H bit4 = FULL_OFF
  Wire.endTransmission();
}

// Check if PCA9685 is alive and awake
bool pca_alive() {
  uint8_t mode1 = pca_readReg(REG_MODE1);
  if (mode1 == 0xFF) return false;          // I2C read failed entirely
  if (mode1 & MODE1_SLEEP) return false;    // Chip fell asleep (brownout?)
  if (!(mode1 & MODE1_AI)) return false;    // Auto-increment got cleared
  return true;
}

// Full cold-start initialization
bool pca_init() {
  pca_busRecover();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  // Verify device present
  Wire.beginTransmission(PCA_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[ERROR] PCA9685 not found at 0x40!");
    return false;
  }

  // Reset
  pca_writeReg(REG_MODE1, 0x00);
  delay(10);

  // Sleep → set prescaler → wake
  pca_writeReg(REG_MODE1, MODE1_SLEEP);
  delay(5);
  pca_writeReg(REG_PRESCALE, 121);   // 50 Hz: round(25MHz / (4096 × 50)) - 1
  delay(5);
  pca_writeReg(REG_MODE1, MODE1_AI | MODE1_ALLCALL);  // Wake with auto-increment
  delay(5);

  // MODE2: totem-pole, OCH=0 (update on STOP = atomic)
  pca_writeReg(REG_MODE2, MODE2_OUTDRV);
  delay(5);

  Serial.println("[PCA] Initialized: 50Hz, AI=1, OUTDRV=1, OCH=0");
  return true;
}

// Recovery: re-init registers WITHOUT resetting servo positions
// This is the KEY fix for the "shoulder full send" bug.
// Old code called pca_init() which reset everything to 90°.
// This version preserves currentAngle[] and re-applies them.
void pca_recover() {
  Serial.println("[WATCHDOG] PCA9685 anomaly detected — recovering registers...");

  pca_busRecover();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  // Re-init mode registers only
  pca_writeReg(REG_MODE1, MODE1_SLEEP);
  delay(5);
  pca_writeReg(REG_PRESCALE, 121);
  delay(5);
  pca_writeReg(REG_MODE1, MODE1_AI | MODE1_ALLCALL);
  delay(5);
  pca_writeReg(REG_MODE2, MODE2_OUTDRV);
  delay(5);

  // Re-apply CURRENT angles (not defaults!) with staggered timing
  for (int i = 0; i < NUM_JOINTS; i++) {
    if (jointEnabled[i]) {
      servo_applyHardware(i);
      delay(20);
    }
  }

  Serial.println("[WATCHDOG] Recovery complete — angles preserved.");
}


// ============================================================================
//  SECTION B: SERVO ABSTRACTION LAYER
// ============================================================================

// Convert degrees (0-180) to PCA9685 ticks
int servo_degreesToTicks(int deg) {
  deg = constrain(deg, 0, 180);
  return map(deg, 0, 180, TICK_MIN, TICK_MAX);
}

// Apply current angle for joint[index] to hardware
// ONLY writes to PCA9685 if the tick value actually changed (tick cache)
void servo_applyHardware(int idx) {
  if (idx < 0 || idx >= NUM_JOINTS || !jointEnabled[idx]) return;

  uint8_t ch = jointChannel[idx];
  int ticks = servo_degreesToTicks((int)round(currentAngle[idx]));

  // Skip write if tick value hasn't changed
  if (ticks == lastWrittenTicks[idx]) return;
  lastWrittenTicks[idx] = ticks;

  // onTime is ALWAYS 0 for standard RC servo PWM.
  // Phase-staggering caused catastrophic comparator-miss glitches in PCA9685:
  // if an I2C update arrives while counter is near onTime/offTime, the output
  // misses the match and stays HIGH for an entire 4096-tick (20ms) cycle!
  // That 20ms full-duty pulse causes the servo to violently jerk and spike current.
  // With onTime = 0, the active pulse is strictly within ticks 0..512 (first 2.5ms).
  // The remaining 17.5ms of every 20ms frame is 100% LOW, allowing completely glitch-free updates.
  pca_write4(ch, 0, (uint16_t)ticks);
}

// Set target angle for a joint (clamped to limits)
void servo_setTarget(int idx, int angle) {
  if (idx < 0 || idx >= NUM_JOINTS) return;
  if (frozen) return;   // Ignore commands while E-stopped

  angle = constrain(angle, jointMin[idx], jointMax[idx]);
  targetAngle[idx] = angle;
  jointEnabled[idx] = true;
}

// Get current rounded angle for a joint
int servo_getAngle(int idx) {
  if (idx < 0 || idx >= NUM_JOINTS) return 90;
  return (int)round(currentAngle[idx]);
}

// Map PCA9685 channel number (9-15) to joint index (0-6), returns -1 if invalid
int channelToIndex(int ch) {
  for (int i = 0; i < NUM_JOINTS; i++) {
    if (jointChannel[i] == ch) return i;
  }
  return -1;
}

// Slew rate tick: call every SLEW_INTERVAL_MS. Smoothly ramps current → target.
// Linear rate: advances at a constant, responsive speed without sluggish crawling tails.
void servo_slewTick() {
  slewActive = false;

  for (int i = 0; i < NUM_JOINTS; i++) {
    if (!jointEnabled[i]) continue;

    float diff = (float)targetAngle[i] - currentAngle[i];
    float absDiff = fabs(diff);

    if (absDiff < 0.05f) continue;    // Already at target, skip

    slewActive = true;

    // Linear step: constant speed up to target, no creeping or dragging
    float step = (absDiff < slewStepDeg) ? absDiff : slewStepDeg;

    if (diff > 0) {
      currentAngle[i] += step;
    } else {
      currentAngle[i] -= step;
    }

    servo_applyHardware(i);
  }
}

// E-Stop: freeze all joints at their current positions
void servo_freezeAll() {
  frozen = true;
  for (int i = 0; i < NUM_JOINTS; i++) {
    targetAngle[i] = (int)round(currentAngle[i]);
  }
  Serial.println("[ESTOP] All joints frozen at current positions.");
}

// Unfreeze: resume accepting commands
void servo_unfreeze() {
  frozen = false;
  Serial.println("[ESTOP] Unfrozen — accepting commands.");
}

// Disable all PWM outputs (full-off, no holding torque)
void servo_disableAll() {
  for (int i = 0; i < NUM_JOINTS; i++) {
    pca_fullOff(jointChannel[i]);
    jointEnabled[i] = false;
    lastWrittenTicks[i] = -1;  // Invalidate cache so re-enable writes fresh
  }
  Serial.println("[OFF] All PWM outputs disabled.");
}

// Re-enable all joints and apply current angles
void servo_enableAll() {
  for (int i = 0; i < NUM_JOINTS; i++) {
    jointEnabled[i] = true;
    servo_applyHardware(i);
    delay(20);
  }
  Serial.println("[ON] All joints re-enabled at current angles.");
}

// Center all joints to their defaults (used at startup)
void servo_centerAll() {
  for (int i = 0; i < NUM_JOINTS; i++) {
    currentAngle[i] = (float)jointDefault[i];
    targetAngle[i] = jointDefault[i];
    jointEnabled[i] = true;
    servo_applyHardware(i);
    delay(35);    // Stagger power-up to reduce inrush current
  }
}


// ============================================================================
//  SECTION C: SERIAL COMMAND PARSER (Non-blocking FSM)
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

// Execute parsed command
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
      Serial.printf("ACK:%d:%d\n", ch, targetAngle[idx]);
    } else {
      Serial.printf("ERR:SET:bad_args ch=%d angle=%d\n", ch, angle);
    }
    return;
  }

  // ── $ALL,<angle>* ── (FIX: angle is in arg1, not arg2!)
  if (strcasecmp(cmdBuf, "ALL") == 0) {
    int angle = atoi(arg1Buf);    // ← THIS WAS THE BUG: v2 read arg2 (empty=0)
    if (angle >= 0 && angle <= 180) {
      for (int i = 0; i < NUM_JOINTS; i++) {
        servo_setTarget(i, angle);
      }
      Serial.printf("ACK:ALL:%d\n", angle);
    } else {
      Serial.printf("ERR:ALL:bad_angle %d\n", angle);
    }
    return;
  }

  // ── $HOME* ── (Move all joints to default Home pose from default.png)
  if (strcasecmp(cmdBuf, "HOME") == 0) {
    for (int i = 0; i < NUM_JOINTS; i++) {
      servo_setTarget(i, jointDefault[i]);
    }
    Serial.println("ACK:HOME");
    return;
  }

  // ── $STOP* ── (E-Stop)
  if (strcasecmp(cmdBuf, "STOP") == 0) {
    servo_freezeAll();
    Serial.println("ACK:STOP");
    return;
  }

  // ── $GO* ── (Unfreeze)
  if (strcasecmp(cmdBuf, "GO") == 0) {
    servo_unfreeze();
    Serial.println("ACK:GO");
    return;
  }

  // ── $OFF* ── (Disable all PWM)
  if (strcasecmp(cmdBuf, "OFF") == 0) {
    servo_disableAll();
    Serial.println("ACK:OFF");
    return;
  }

  // ── $ON* ── (Re-enable all PWM)
  if (strcasecmp(cmdBuf, "ON") == 0) {
    servo_enableAll();
    Serial.println("ACK:ON");
    return;
  }

  // ── $ISO,<ch>* ── (Isolate: enable one channel at 90°, full-off all others)
  if (strcasecmp(cmdBuf, "ISO") == 0) {
    int ch = atoi(arg1Buf);
    int idx = channelToIndex(ch);
    if (idx >= 0) {
      Serial.printf("[DIAG] Isolating CH%d, disabling others...\n", ch);
      for (int i = 0; i < NUM_JOINTS; i++) {
        if (i == idx) {
          currentAngle[i] = 90.0f;
          targetAngle[i] = 90;
          jointEnabled[i] = true;
          servo_applyHardware(i);
        } else {
          pca_fullOff(jointChannel[i]);
          jointEnabled[i] = false;
          lastWrittenTicks[i] = -1;
        }
      }
      Serial.printf("ACK:ISO:%d\n", ch);
    } else {
      Serial.printf("ERR:ISO:bad_ch %d\n", ch);
    }
    return;
  }

  // ── $QUERY* ── (Report all current angles)
  if (strcasecmp(cmdBuf, "QUERY") == 0) {
    Serial.print("POS");
    for (int i = 0; i < NUM_JOINTS; i++) {
      Serial.printf(":%d:%d", jointChannel[i], servo_getAngle(i));
    }
    Serial.println();
    return;
  }

  // ── $SPEED,<deg_per_sec>* ── (Adjust slew speed on the fly, e.g. $SPEED,100*)
  if (strcasecmp(cmdBuf, "SPEED") == 0) {
    int spd = atoi(arg1Buf);
    if (spd >= 10 && spd <= 500) {
      // 50 ticks per second (20ms interval) -> deg_per_tick = deg_per_sec / 50
      slewStepDeg = (float)spd / 50.0f;
      Serial.printf("ACK:SPEED:%d\n", spd);
    } else {
      Serial.printf("ERR:SPEED:range_10_500\n");
    }
    return;
  }

  // ── $SWEEP,<ch>* ── (Non-blocking sweep)
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
      Serial.printf("[SWEEP] Starting non-blocking sweep on CH%d...\n", ch);
      Serial.println("ACK:SWEEP:START");
    } else {
      Serial.printf("ERR:SWEEP:bad_ch %d\n", ch);
    }
    return;
  }

  // ── $RAW,<ch>,<ticks>* ── (Direct tick write)
  if (strcasecmp(cmdBuf, "RAW") == 0) {
    int ch = atoi(arg1Buf);
    int ticks = atoi(arg2Buf);
    if (ch >= 0 && ch <= 15 && ticks >= 50 && ticks <= 600) {
      int idx = channelToIndex(ch);
      if (idx >= 0) jointEnabled[idx] = true;

      pca_write4(ch, 0, (uint16_t)ticks);
      Serial.printf("ACK:RAW:%d:%d\n", ch, ticks);
    } else {
      Serial.printf("ERR:RAW:bad_args ch=%d ticks=%d\n", ch, ticks);
    }
    return;
  }

  // ── $DIAG* ── (Print diagnostic info)
  if (strcasecmp(cmdBuf, "DIAG") == 0) {
    uint8_t m1 = pca_readReg(REG_MODE1);
    uint8_t m2 = pca_readReg(REG_MODE2);
    uint8_t pre = pca_readReg(REG_PRESCALE);
    Serial.println("--- DIAGNOSTICS ---");
    Serial.printf("MODE1=0x%02X (AI=%d SLEEP=%d)\n", m1, (m1>>5)&1, (m1>>4)&1);
    Serial.printf("MODE2=0x%02X (OCH=%d OUTDRV=%d)\n", m2, (m2>>3)&1, (m2>>2)&1);
    Serial.printf("PRESCALE=%d (should be 121 for 50Hz)\n", pre);
    Serial.printf("Frozen=%d  SlewActive=%d  SweepRunning=%d\n", frozen, slewActive, sweepRunning);
    for (int i = 0; i < NUM_JOINTS; i++) {
      Serial.printf("  J%d CH%d: curr=%.1f tgt=%d en=%d lim=[%d,%d]\n",
        i, jointChannel[i], currentAngle[i], targetAngle[i],
        jointEnabled[i], jointMin[i], jointMax[i]);
    }
    Serial.println("--- END DIAG ---");
    Serial.println("ACK:DIAG");
    return;
  }

  Serial.printf("ERR:UNKNOWN_CMD '%s'\n", cmdBuf);
}

// FSM byte-by-byte parser
void parseSerialByte(char c) {
  // '$' always resets and starts a new command
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
        // Command with zero args (e.g. $STOP*, $QUERY*, $OFF*, $ON*, $GO*)
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
        // Command with 1 arg (e.g. $ALL,90* or $SWEEP,15*)
        executeCommand();
        resetParser();
      } else if (arg1Len < 7) {
        arg1Buf[arg1Len++] = c;
      }
      break;

    case PS_ARG2:
      if (c == '*' || c == '\n' || c == '\r') {
        // Command with 2 args (e.g. $SET,15,90* or $RAW,15,300*)
        executeCommand();
        resetParser();
      } else if (arg2Len < 7) {
        arg2Buf[arg2Len++] = c;
      }
      break;

    case PS_IDLE:
    default:
      // Legacy support: 'C <ch> <angle>\n'
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
//  SECTION D: NON-BLOCKING SWEEP STATE MACHINE
// ============================================================================

void sweepTick() {
  if (!sweepRunning || sweepJointIdx < 0) return;

  unsigned long now = millis();
  if (now - lastSweepMs < SWEEP_STEP_MS) return;
  lastSweepMs = now;

  uint8_t ch = jointChannel[sweepJointIdx];

  switch (sweepPhase) {
    case 0:   // Sweeping UP: 0 → 180
      currentAngle[sweepJointIdx] = (float)sweepAngle;
      targetAngle[sweepJointIdx] = sweepAngle;
      servo_applyHardware(sweepJointIdx);
      Serial.printf("CH%d: %d deg (ticks: %d)\n", ch, sweepAngle, servo_degreesToTicks(sweepAngle));
      sweepAngle += 5;
      if (sweepAngle > 180) {
        sweepAngle = 180;
        sweepPhase = 1;
      }
      break;

    case 1:   // Sweeping DOWN: 180 → 0
      currentAngle[sweepJointIdx] = (float)sweepAngle;
      targetAngle[sweepJointIdx] = sweepAngle;
      servo_applyHardware(sweepJointIdx);
      Serial.printf("CH%d: %d deg (ticks: %d)\n", ch, sweepAngle, servo_degreesToTicks(sweepAngle));
      sweepAngle -= 5;
      if (sweepAngle < 0) {
        sweepAngle = jointDefault[sweepJointIdx];
        sweepPhase = 2;
      }
      break;

    case 2:   // Return to default
      currentAngle[sweepJointIdx] = (float)sweepAngle;
      targetAngle[sweepJointIdx] = sweepAngle;
      servo_applyHardware(sweepJointIdx);
      Serial.printf("ACK:SWEEP:%d:DONE\n", ch);
      sweepRunning = false;
      sweepJointIdx = -1;
      break;
  }
}


// ============================================================================
//  SECTION E: MAIN SETUP & CONTROL LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  ESP32 Robot Arm Controller v3.0");
  Serial.println("  Channels: 15 (Base) -> 9 (Gripper)");
  Serial.println("  Protocol: $CMD,arg1,arg2*");
  Serial.println("========================================");

  // Initialize all state to defaults
  for (int i = 0; i < NUM_JOINTS; i++) {
    currentAngle[i] = (float)jointDefault[i];
    targetAngle[i] = jointDefault[i];
    jointEnabled[i] = false;
    lastWrittenTicks[i] = -1;  // Force first write to always go through
  }

  // Initialize PCA9685 and center all servos
  if (pca_init()) {
    servo_centerAll();
    Serial.println("[READY] All servos centered. Awaiting commands.");
    Serial.println("  $SET,<ch>,<angle>*   Set single servo");
    Serial.println("  $ALL,<angle>*        Set all servos");
    Serial.println("  $STOP*               E-Stop freeze");
    Serial.println("  $GO*                 Unfreeze");
    Serial.println("  $QUERY*              Report positions");
    Serial.println("  $DIAG*               Hardware diagnostics");
    Serial.println("  $SWEEP,<ch>*         Test sweep");
    Serial.println("  $RAW,<ch>,<ticks>*   Direct tick write");
    Serial.println("  $OFF* / $ON*         Disable/Enable PWM");
    Serial.println("========================================");
  } else {
    Serial.println("[FATAL] PCA9685 initialization failed!");
    Serial.println("Check: I2C wiring, power supply, OE pin → GND");
  }

  lastSlewMs = millis();
  lastWatchdogMs = millis();
}

void loop() {
  // 1. Drain serial buffer through FSM parser (non-blocking)
  while (Serial.available() > 0) {
    parseSerialByte((char)Serial.read());
  }

  unsigned long now = millis();

  // 2. Slew-rate interpolator: smooth motion every 10ms
  if (now - lastSlewMs >= SLEW_INTERVAL_MS) {
    lastSlewMs = now;
    servo_slewTick();
  }

  // 3. Non-blocking sweep state machine
  sweepTick();

  // 4. Watchdog health check every 5 seconds
  //    ONLY runs when no slew is active to avoid I2C bus collision
  if (now - lastWatchdogMs >= WATCHDOG_INTERVAL_MS) {
    lastWatchdogMs = now;

    if (!slewActive && !sweepRunning) {
      if (!pca_alive()) {
        pca_recover();    // Re-init registers, PRESERVE current angles
      }
    }
  }
}
