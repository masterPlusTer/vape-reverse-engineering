#include <Arduino.h>
#include <avr/pgmspace.h>

// ===================== CONFIG =====================
static const uint8_t I2C_ADDR = 0x3C;
static const uint8_t PIN_SDA  = A4;
static const uint8_t PIN_SCL  = A5;

static const uint8_t I2C_DELAY_US = 20;
static const uint32_t SERIAL_BAUD = 115200;

static const uint16_t CMD_MAX_LEN = 160;
static const uint16_t CMD_IDLE_TIMEOUT_MS = 250;

// ===================== INDICATORS (2 segments each) =====================
// PON AQUI los 2 indices del “primer digito” (2 segmentos) de cada línea.
// 255 = "no configurado" (sentinel)
static const uint8_t TOP_IND_A = 255;  // <-- CAMBIA por el indice real
static const uint8_t TOP_IND_B = 255;  // <-- CAMBIA por el indice real
static const uint8_t BOT_IND_A = 255;  // <-- CAMBIA por el indice real
static const uint8_t BOT_IND_B = 255;  // <-- CAMBIA por el indice real

// ===================== MAP TYPES =====================
struct RGBIdx { uint8_t r, g, b; };

// ===================== I2C bitbang (open drain) =====================
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
  if (!i2cWriteByte((I2C_ADDR << 1) | 0)) { i2cStop(); return false; }
  for (uint16_t i = 0; i < len; i++) {
    if (!i2cWriteByte(buf[i])) { i2cStop(); return false; }
  }
  i2cStop();
  return true;
}

// ===================== PROTO / PACKETS =====================
static const uint8_t preamble[] = { 0x3F, 0x48, 0x53, 0x6C, 0x87, 0x98, 0xC0, 0xE0 };

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
  return i2cWriteTx(preamble, sizeof(preamble)) && i2cWriteTx(pkt, 145);
}
static inline void show() { sendAll(pktB); }

// ===================== MAPS (PROGMEM) =====================
const RGBIdx outerRing[] PROGMEM = {
  { 92,108,124 }, { 91,107,123 }, { 90,106,122 }, { 89,105,121 },
  { 88,104,120 }, { 87,103,119 }, { 86,102,118 }, { 85,101,117 },
  { 84,100,116 }, { 83, 99,115 }, { 82, 98,114 }, { 81, 97,113 }
};
static const uint8_t OUTER_N = 12;

// MONO groups en PROGMEM (los del papel)
const uint8_t uiTop[]         PROGMEM = { 1,2,3,4,5,6,7,8, 9,10,11,12,13,14,15,16 };
const uint8_t uiBottom[]      PROGMEM = { 17,18,19,20,21,22,23,24, 25,26,27,28,29,30,31,32 };
const uint8_t bar33_37[]      PROGMEM = {33,34,35,36,37};
const uint8_t bar38_42[]      PROGMEM = {38,39,40,41,42};
const uint8_t bar43_48[]      PROGMEM = {43,44,45,46,47,48};
const uint8_t dropletCenter[] PROGMEM = {59,60,61};
const uint8_t midBox[]        PROGMEM = {62,63,64,79,80};
const uint8_t boltOuter[]     PROGMEM = {65,66,67,68,69,70,71,72,73,74};
const uint8_t boltInner[]     PROGMEM = {75,76,77,78};

struct MonoGroup { const char* name; const uint8_t* idx; uint8_t n; };
const MonoGroup monoGroups[] = {
  {"uiTop",     uiTop,         16},
  {"uiBottom",  uiBottom,      16},
  {"bar33_37",  bar33_37,      5},
  {"bar38_42",  bar38_42,      5},
  {"bar43_48",  bar43_48,      6},
  {"droplet",   dropletCenter, 3},
  {"midBox",    midBox,        5},
  {"boltOuter", boltOuter,     10},
  {"boltInner", boltInner,     4},
};
static const uint8_t MONO_GROUPS_N = sizeof(monoGroups)/sizeof(monoGroups[0]);

bool getMonoGroup(const char* name, const uint8_t*& out, uint8_t& n) {
  for (uint8_t i=0;i<MONO_GROUPS_N;i++) {
    if (strcmp(name, monoGroups[i].name) == 0) { out = monoGroups[i].idx; n = monoGroups[i].n; return true; }
  }
  return false;
}

// ===================== API =====================
static inline void setMono(uint8_t idx, uint8_t v) {
  if (idx < 145) pktB[idx] = v;
}

static inline void setLedFromProgmem(uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b) {
  RGBIdx led;
  memcpy_P(&led, &outerRing[ledIndex], sizeof(RGBIdx));
  pktB[led.r] = r;
  pktB[led.g] = g;
  pktB[led.b] = b;
}

void rgbFillOuter(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i=0;i<OUTER_N;i++) setLedFromProgmem(i, r,g,b);
}

void monoFillProg(const uint8_t* group, uint8_t n, uint8_t v) {
  for (uint8_t i=0;i<n;i++) {
    uint8_t idx = pgm_read_byte(&group[i]);
    setMono(idx, v);
  }
}

// ===================== 7-SEG DIGITS =====================
// bits a,b,c,d,e,f,g (LSB=a). dp es base+7
static const uint8_t SEG7_MASK[10] PROGMEM = {
  0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
  0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111
};

void set7Seg(uint8_t base, int digit, uint8_t v, bool dp=false) {
  for (uint8_t k=0;k<8;k++) setMono(base + k, 0);
  if (digit < 0 || digit > 9) return;

  uint8_t m = pgm_read_byte(&SEG7_MASK[digit]);
  for (uint8_t s=0;s<7;s++) if (m & (1 << s)) setMono(base + s, v);
  if (dp) setMono(base + 7, v);
}

static inline bool slotToBase(const char* slot, uint8_t &base) {
  if (strcmp(slot, "t0") == 0) { base = 1;  return true; }
  if (strcmp(slot, "t1") == 0) { base = 9;  return true; }
  if (strcmp(slot, "b0") == 0) { base = 17; return true; }
  if (strcmp(slot, "b1") == 0) { base = 25; return true; }
  return false;
}

// ===================== 2-SEG INDICATOR =====================
static inline void setIndicator(bool top, bool on, uint8_t v) {
  uint8_t vv = on ? v : 0;

  if (top) {
    if (TOP_IND_A != 255) setMono(TOP_IND_A, vv);
    if (TOP_IND_B != 255) setMono(TOP_IND_B, vv);
  } else {
    if (BOT_IND_A != 255) setMono(BOT_IND_A, vv);
    if (BOT_IND_B != 255) setMono(BOT_IND_B, vv);
  }
}

// ===================== LINE RENDER =====================
// line top 188 [v] [dpMask]
// dpMask: bit0=dp del medio, bit1=dp del derecho
void renderLine(bool top, const char* threeDigits, uint8_t v, uint8_t dpMask) {
  if (!threeDigits || strlen(threeDigits) < 3) return;

  char a = threeDigits[0];
  char b = threeDigits[1];
  char c = threeDigits[2];
  if (a < '0' || a > '9' || b < '0' || b > '9' || c < '0' || c > '9') return;

  bool indOn = (a != '0');  // 0=off, cualquier otro=on
  int d1 = b - '0';
  int d2 = c - '0';

  uint8_t base1 = top ? 1  : 17;
  uint8_t base2 = top ? 9  : 25;

  setIndicator(top, indOn, v);
  set7Seg(base1, d1, v, (dpMask & 0x01));
  set7Seg(base2, d2, v, (dpMask & 0x02));
}

// ===================== ALL OFF (HARD) =====================
void allOff() {
  memcpy(pktB, pktA, sizeof(pktA));

  // apaga TODO el payload para evitar "49 sigue encendido" y similares
  for (uint8_t i = 1; i < 145; i++) pktB[i] = 0;

  show();
  Serial.println(F("ALL OFF"));
}

// ===================== COMMAND INPUT (NO String) =====================
static char cmd[CMD_MAX_LEN];
static uint16_t cmdLen = 0;
static uint32_t lastCharMs = 0;

bool readCommandLine(char* out, uint16_t outLen) {
  while (Serial.available()) {
    char c = (char)Serial.read();
    lastCharMs = millis();

    if (c == '\0') continue;

    if (c == '\r' || c == '\n') {
      if (cmdLen == 0) continue;
      cmd[cmdLen] = 0;
      strncpy(out, cmd, outLen);
      cmdLen = 0;
      return true;
    }

    if (c == '\b' || c == 127) {
      if (cmdLen > 0) cmdLen--;
      continue;
    }

    if (cmdLen < outLen - 1) cmd[cmdLen++] = c;
    else cmdLen = 0;
  }

  if (cmdLen > 0 && (millis() - lastCharMs) > CMD_IDLE_TIMEOUT_MS) {
    cmd[cmdLen] = 0;
    strncpy(out, cmd, outLen);
    cmdLen = 0;
    return true;
  }

  return false;
}

void help() {
  Serial.println(F("Comandos:"));
  Serial.println(F("  help | ?"));
  Serial.println(F("  show"));
  Serial.println(F("  off"));
  Serial.println(F("  raw <idx> <v>"));
  Serial.println(F("  rgbfill outer <r> <g> <b>"));
  Serial.println(F("  mono <grupo> <idx> <v>"));
  Serial.println(F("  monofill <grupo> <v>"));
  Serial.println(F("  digit <slot> <n> [v] [dp]   slot: t0,t1,b0,b1"));
  Serial.println(F("  line <top|bottom> <abc> [v] [dpMask]   ej: line top 188"));
}

// ===================== Arduino =====================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(600);

  Serial.println();
  Serial.println(F("BOOT"));
  help();

  sdaHigh();
  sclHigh();

  memcpy(pktB, pktA, sizeof(pktA));
  show();
}

void loop() {
  char line[CMD_MAX_LEN];
  if (!readCommandLine(line, sizeof(line))) return;

  // trim simple
  while (*line == ' ') memmove(line, line+1, strlen(line));
  uint16_t L = strlen(line);
  while (L && (line[L-1] == ' ' || line[L-1] == '\t')) line[--L] = 0;
  if (!L) return;

  if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) { help(); return; }
  if (strcmp(line, "show") == 0) { show(); Serial.println(F("SHOW")); return; }
  if (strcmp(line, "off")  == 0) { allOff(); return; }

  // raw <index> <value>   (test directo)
  if (strncmp(line, "raw ", 4) == 0) {
    int idx, v;
    if (sscanf(line, "raw %d %d", &idx, &v) == 2) {
      if (idx < 0 || idx >= 145) { Serial.println(F("ERR: idx fuera de rango")); return; }
      pktB[idx] = (uint8_t)constrain(v, 0, 255);
      show();
      Serial.println(F("OK"));
    } else {
      Serial.println(F("ERR: uso: raw <idx> <v>"));
    }
    return;
  }

  // line <top|bottom> <abc> [v] [dpMask]
  if (strncmp(line, "line ", 5) == 0) {
    char which[8];
    char abc[8];
    int v = 255;
    int dpMask = 0;

    int got = sscanf(line, "line %7s %7s %d %d", which, abc, &v, &dpMask);
    if (got >= 2) {
      bool top;
      if (strcmp(which, "top") == 0) top = true;
      else if (strcmp(which, "bottom") == 0) top = false;
      else { Serial.println(F("ERR: usa top o bottom")); return; }

      uint8_t vv = (uint8_t)constrain(v, 0, 255);
      uint8_t dd = (uint8_t)constrain(dpMask, 0, 3);

      renderLine(top, abc, vv, dd);
      show();
      Serial.println(F("OK"));
    } else {
      Serial.println(F("ERR: uso: line <top|bottom> <abc> [v] [dpMask]"));
    }
    return;
  }

  // digit <slot> <n> [v] [dp]
  if (strncmp(line, "digit ", 6) == 0) {
    char slot[8];
    int n, v = 255, dp = 0;
    int got = sscanf(line, "digit %7s %d %d %d", slot, &n, &v, &dp);
    if (got >= 2) {
      uint8_t base;
      if (!slotToBase(slot, base)) { Serial.println(F("ERR: slot invalido (t0,t1,b0,b1)")); return; }
      uint8_t vv = (uint8_t)constrain(v, 0, 255);
      set7Seg(base, n, vv, (dp != 0));
      show();
      Serial.println(F("OK"));
    } else {
      Serial.println(F("ERR: uso: digit <slot> <n> [v] [dp]"));
    }
    return;
  }

  // rgbfill outer r g b
  if (strncmp(line, "rgbfill outer", 13) == 0) {
    int r,g,b;
    if (sscanf(line, "rgbfill outer %d %d %d", &r, &g, &b) == 3) {
      rgbFillOuter((uint8_t)constrain(r,0,255), (uint8_t)constrain(g,0,255), (uint8_t)constrain(b,0,255));
      show();
      Serial.println(F("OK"));
    } else {
      Serial.println(F("ERR: uso: rgbfill outer <r> <g> <b>"));
    }
    return;
  }

  // mono <grupo> <idx> <v>
  if (strncmp(line, "mono ", 5) == 0) {
    char grp[24];
    int idx, v;
    if (sscanf(line, "mono %23s %d %d", grp, &idx, &v) == 3) {
      const uint8_t* group = nullptr;
      uint8_t n = 0;
      if (!getMonoGroup(grp, group, n)) { Serial.println(F("ERR: grupo MONO desconocido")); return; }
      if (idx < 0 || idx >= n) { Serial.println(F("ERR: idx fuera de rango")); return; }
      uint8_t real = pgm_read_byte(&group[idx]);
      setMono(real, (uint8_t)constrain(v,0,255));
      show();
      Serial.println(F("OK"));
    } else {
      Serial.println(F("ERR: uso: mono <grupo> <idx> <v>"));
    }
    return;
  }

  // monofill <grupo> <v>
  if (strncmp(line, "monofill ", 9) == 0) {
    char grp[24];
    int v;
    if (sscanf(line, "monofill %23s %d", grp, &v) == 2) {
      const uint8_t* group = nullptr;
      uint8_t n = 0;
      if (!getMonoGroup(grp, group, n)) { Serial.println(F("ERR: grupo MONO desconocido")); return; }
      monoFillProg(group, n, (uint8_t)constrain(v,0,255));
      show();
      Serial.println(F("OK"));
    } else {
      Serial.println(F("ERR: uso: monofill <grupo> <v>"));
    }
    return;
  }

  Serial.print(F("ERR: comando desconocido: "));
  Serial.println(line);
}
