#include <Arduino.h>

// ===== CONFIG =====
static const uint8_t I2C_ADDR = 0x3C;
static const uint8_t PIN_SDA = A4;
static const uint8_t PIN_SCL = A5;

// Un poco más tolerante
static const uint8_t I2C_DELAY_US = 20;

// ----- Serial robustness -----
static const uint32_t SERIAL_BAUD = 115200;
static const uint16_t CMD_MAX_LEN = 96;
static const uint16_t CMD_IDLE_TIMEOUT_MS = 250;

// ---------- Open drain ----------
static inline void sdaLow()  { pinMode(PIN_SDA, OUTPUT); digitalWrite(PIN_SDA, LOW); }
static inline void sdaHigh() { pinMode(PIN_SDA, INPUT); }
static inline void sclLow()  { pinMode(PIN_SCL, OUTPUT); digitalWrite(PIN_SCL, LOW); }
static inline void sclHigh() { pinMode(PIN_SCL, INPUT); }

static inline uint8_t readSDA() { return digitalRead(PIN_SDA); }
static inline void i2cDelay() { delayMicroseconds(I2C_DELAY_US); }

void i2cStart() {
  sdaHigh(); sclHigh(); i2cDelay();
  sdaLow();  i2cDelay();
  sclLow();  i2cDelay();
}

void i2cStop() {
  sdaLow();  i2cDelay();
  sclHigh(); i2cDelay();
  sdaHigh(); i2cDelay();
}

bool i2cWriteByte(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    (b & 0x80) ? sdaHigh() : sdaLow();
    i2cDelay();
    sclHigh(); i2cDelay();
    sclLow();  i2cDelay();
    b <<= 1;
  }

  sdaHigh(); i2cDelay();
  sclHigh(); i2cDelay();
  bool ack = (readSDA() == LOW);
  sclLow();  i2cDelay();
  return ack;
}

bool i2cWriteTx(const uint8_t *buf, uint16_t len) {
  i2cStart();
  if (!i2cWriteByte((I2C_ADDR << 1) | 0)) {
    i2cStop(); return false;
  }
  for (uint16_t i = 0; i < len; i++) {
    if (!i2cWriteByte(buf[i])) {
      i2cStop(); return false;
    }
  }
  i2cStop();
  return true;
}

// ===== CAPTURA REAL =====
static const uint8_t preamble[] = {
  0x3F, 0x48, 0x53, 0x6C, 0x87, 0x98, 0xC0, 0xE0
};

static const uint8_t pktA[145] = {
  0x6C,0xB4,0xB4,0xB4,0xB4,0xB4,0xB4,0x00,0x00,0x2D,0x2D,0x2D,0x2D,
  0x2D,0x2D,0x00,0x00,0x2D,0x2D,0x2D,0x2D,0x00,0x2D,0x2D,0x00,
  0x2D,0x2D,0x2D,0x2D,0x2D,0x2D,0x00,0x00,0x2D,0x2D,0x2D,0x2D,
  0x2D,0x00,0x00,0x00,0x00,0x00,0x2D,0x2D,0x2D,0x2D,0x2E,0x17,
  0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x8B,
  0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,
  0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0x8B,0xCD,0x8B,0xCA,
  0x04,0xC3,0x41,0xC0,0x41,0xC3,0x44,0xC5,0x00,0x00,0x00,0x00,
  0x07,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
  0xA0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static uint8_t pktB[145];

bool sendAll(const uint8_t *pkt) {
  return i2cWriteTx(preamble, sizeof(preamble)) &&
         i2cWriteTx(pkt, 145);
}

// ===== APAGADO TOTAL =====
void allOff() {
  memset(pktB, 0x00, sizeof(pktB));
  sendAll(pktB);
  Serial.println("ALL OFF");
}

// ===== Serial robusto =====
String readLine() {
  static String s;
  static uint32_t lastCharMs = 0;

  while (Serial.available()) {
    char c = (char)Serial.read();
    lastCharMs = millis();

    if (c == '\0') continue;

    if (c == '\r' || c == '\n') {
      if (s.length() == 0) continue;
      String o = s;
      s = "";
      return o;
    }

    if (c == '\b' || c == 127) {
      if (s.length() > 0) s.remove(s.length() - 1);
      continue;
    }

    if (s.length() < CMD_MAX_LEN) {
      s += c;
    } else {
      s = "";
      Serial.println("ERR: comando muy largo");
    }
  }

  if (s.length() > 0 && (millis() - lastCharMs) > CMD_IDLE_TIMEOUT_MS) {
    String o = s;
    s = "";
    return o;
  }

  return "";
}

bool blink = false;
uint32_t lastBlink = 0;
bool state = false;

void printHelp() {
  Serial.println("Comandos:");
  Serial.println("  a | b");
  Serial.println("  blink on | blink off");
  Serial.println("  set i v");
  Serial.println("  halo v");
  Serial.println("  off        (apaga todos los LEDs)");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(800);

  Serial.println();
  Serial.println("BOOT");

  sdaHigh();
  sclHigh();

  memcpy(pktB, pktA, sizeof(pktA));

  printHelp();
  sendAll(pktA);
}

void loop() {
  if (blink && millis() - lastBlink > 500) {
    lastBlink = millis();
    state = !state;
    sendAll(state ? pktA : pktB);
  }

  String l = readLine();
  if (!l.length()) return;
  l.trim();

  if (l == "help" || l == "?") {
    printHelp();

  } else if (l == "a") {
    sendAll(pktA);
    Serial.println("A");

  } else if (l == "b") {
    sendAll(pktB);
    Serial.println("B");

  } else if (l == "blink on") {
    blink = true;
    Serial.println("blink ON");

  } else if (l == "blink off") {
    blink = false;
    Serial.println("blink OFF");

  } else if (l == "off") {
    allOff();

  } else if (l.startsWith("set ")) {
    int i, v;
    if (sscanf(l.c_str(), "set %d %d", &i, &v) == 2) {
      if (i >= 0 && i < 145) {
        pktB[i] = constrain(v, 0, 255);
        sendAll(pktB);
        Serial.print("pktB["); Serial.print(i); Serial.println("]");
      } else {
        Serial.println("ERR: i fuera de rango");
      }
    } else {
      Serial.println("ERR: uso: set i v");
    }

  } else if (l.startsWith("halo ")) {
    int v;
    if (sscanf(l.c_str(), "halo %d", &v) == 1) {
      uint8_t vv = constrain(v, 0, 255);
      for (int i = 35; i < 110; i++) pktB[i] = vv;
      sendAll(pktB);
      Serial.println("halo");
    } else {
      Serial.println("ERR: uso: halo v");
    }

  } else {
    Serial.print("ERR: comando desconocido: ");
    Serial.println(l);
  }
}

