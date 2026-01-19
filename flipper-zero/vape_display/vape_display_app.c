#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <string.h>

#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <input/input.h>

#define I2C_ADDR 0x3C

// Arduino-ish timing (más lento, más estable)
#define I2C_DELAY_US 20
#define PREAMBLE_GAP_US 33

// Cuántas veces “sellamos” el frame cuando cambia algo
#define PUSH_REPEATS 8
#define PUSH_GAP_MS  2

// External pins: C0=SCL, C1=SDA
static const GpioPin* PIN_SCL = &gpio_ext_pc0;
static const GpioPin* PIN_SDA = &gpio_ext_pc1;

// ===== Protocol =====
static const uint8_t preamble[] = {0x3F, 0x48, 0x53, 0x6C, 0x87, 0x98, 0xC0, 0xE0};

// Frame buffer
static uint8_t pktB[145];
static const uint8_t* current_pkt = pktB;

// UI state
static uint8_t brightness = 0xFF;
static uint8_t step = 0;
static char status_line[64] = "BOOT";

// ===== 7-seg masks (a,b,c,d,e,f,g) LSB=a, dp=base+7 =====
static const uint8_t SEG7_MASK[10] = {
    0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
    0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111
};

static inline void setMono(uint8_t idx, uint8_t v) {
    if(idx < 145) pktB[idx] = v;
}

static void clearFrame(void) {
    memset(pktB, 0x00, sizeof(pktB));
}

// Slots -> base index
static bool slotToBase(const char* slot, uint8_t* base) {
    if(strcmp(slot, "t0") == 0) { *base = 1;  return true; }
    if(strcmp(slot, "t1") == 0) { *base = 9;  return true; }
    if(strcmp(slot, "b0") == 0) { *base = 17; return true; }
    if(strcmp(slot, "b1") == 0) { *base = 25; return true; }
    return false;
}

static void set7Seg(uint8_t base, int digit, uint8_t v, bool dp) {
    for(uint8_t k = 0; k < 8; k++) setMono((uint8_t)(base + k), 0);
    if(digit < 0 || digit > 9) return;

    uint8_t m = SEG7_MASK[digit];
    for(uint8_t s = 0; s < 7; s++) {
        if(m & (1 << s)) setMono((uint8_t)(base + s), v);
    }
    if(dp) setMono((uint8_t)(base + 7), v);
}

// ===== Mono groups (from your map) =====
static const uint8_t uiTop[]         = { 1,2,3,4,5,6,7,8, 9,10,11,12,13,14,15,16 };
static const uint8_t uiBottom[]      = { 17,18,19,20,21,22,23,24, 25,26,27,28,29,30,31,32 };
static const uint8_t bar33_37[]      = {33,34,35,36,37};
static const uint8_t bar38_42[]      = {38,39,40,41,42};
static const uint8_t bar43_48[]      = {43,44,45,46,47,48};
static const uint8_t dropletCenter[] = {59,60,61};
static const uint8_t midBox[]        = {62,63,64,79,80};
static const uint8_t boltOuter[]     = {65,66,67,68,69,70,71,72,73,74};
static const uint8_t boltInner[]     = {75,76,77,78};

static void monoFill(const uint8_t* idx, uint8_t n, uint8_t v) {
    for(uint8_t i = 0; i < n; i++) setMono(idx[i], v);
}

// ===== Halo RGB mapping (from your photo) =====
// 12 LEDs: (R,G,B) = (81..92, 97..108, 113..124)
typedef struct { uint8_t r, g, b; } RGBIdx;
static const RGBIdx outerRing[12] = {
    {81, 97, 113}, {82, 98, 114}, {83, 99, 115}, {84,100,116},
    {85,101,117}, {86,102,118}, {87,103,119}, {88,104,120},
    {89,105,121}, {90,106,122}, {91,107,123}, {92,108,124},
};

static void rgbSetOuterOne(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
    if(i >= 12) return;
    if(outerRing[i].r < 145) pktB[outerRing[i].r] = r;
    if(outerRing[i].g < 145) pktB[outerRing[i].g] = g;
    if(outerRing[i].b < 145) pktB[outerRing[i].b] = b;
}

static void rgbFillOuter(uint8_t r, uint8_t g, uint8_t b) {
    for(uint8_t i = 0; i < 12; i++) rgbSetOuterOne(i, r, g, b);
}

// ===== Bit-bang I2C (open-drain) =====
static inline void i2c_delay(void) { furi_delay_us(I2C_DELAY_US); }

static inline void sda_release(void) {
    furi_hal_gpio_write(PIN_SDA, true);
    furi_hal_gpio_init(PIN_SDA, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
}
static inline void sda_low(void) {
    furi_hal_gpio_write(PIN_SDA, false);
    furi_hal_gpio_init(PIN_SDA, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
}
static inline void scl_release(void) {
    furi_hal_gpio_write(PIN_SCL, true);
    furi_hal_gpio_init(PIN_SCL, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
}
static inline void scl_low(void) {
    furi_hal_gpio_write(PIN_SCL, false);
    furi_hal_gpio_init(PIN_SCL, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
}
static inline bool read_sda(void) {
    furi_hal_gpio_init(PIN_SDA, GpioModeInput, GpioPullNo, GpioSpeedLow);
    return furi_hal_gpio_read(PIN_SDA);
}

static void i2c_start(void) {
    sda_release(); scl_release(); i2c_delay();
    sda_low(); i2c_delay();
    scl_low(); i2c_delay();
}
static void i2c_stop(void) {
    sda_low(); i2c_delay();
    scl_release(); i2c_delay();
    sda_release(); i2c_delay();
}
static bool i2c_write_byte(uint8_t b) {
    for(int i = 0; i < 8; i++) {
        (b & 0x80) ? sda_release() : sda_low();
        i2c_delay();
        scl_release(); i2c_delay();
        scl_low(); i2c_delay();
        b <<= 1;
    }
    sda_release(); i2c_delay();
    scl_release(); i2c_delay();
    bool ack = (read_sda() == 0);
    scl_low(); i2c_delay();
    return ack;
}
static bool i2c_write_tx(const uint8_t* buf, size_t len) {
    i2c_start();
    if(!i2c_write_byte((I2C_ADDR << 1) | 0)) { i2c_stop(); return false; }
    for(size_t i = 0; i < len; i++) {
        if(!i2c_write_byte(buf[i])) { i2c_stop(); return false; }
    }
    i2c_stop();
    return true;
}

// Exact trace: 8x(1 byte) + gap + 145 bytes
static bool sendAll_trace_style(const uint8_t* pkt145) {
    for(size_t i = 0; i < sizeof(preamble); i++) {
        if(!i2c_write_tx(&preamble[i], 1)) return false;
        i2c_delay();
    }
    furi_delay_us(PREAMBLE_GAP_US);
    return i2c_write_tx(pkt145, 145);
}

// “Arduino show”: manda el frame varias veces para que quede estable
static void push_frame(uint8_t repeats) {
    for(uint8_t i = 0; i < repeats; i++) {
        (void)sendAll_trace_style(current_pkt);
        furi_delay_ms(PUSH_GAP_MS);
    }
}

// ===== Steps / Scenes =====
static const uint8_t STEP_COUNT = 25;

static void render_step(uint8_t s) {
    clearFrame();

    uint8_t base;
    switch(s) {
    case 0:
        slotToBase("t0", &base);
        set7Seg(base, 1, brightness, false);
        snprintf(status_line, sizeof(status_line), "t0=1");
        break;
    case 1:
        slotToBase("t1", &base);
        set7Seg(base, 9, brightness, false);
        snprintf(status_line, sizeof(status_line), "t1=9");
        break;
    case 2:
        slotToBase("b0", &base);
        set7Seg(base, 0, brightness, false);
        slotToBase("b1", &base);
        set7Seg(base, 6, brightness, false);
        snprintf(status_line, sizeof(status_line), "b0=0 b1=6");
        break;
    case 3:
        monoFill(uiTop, 16, brightness);
        snprintf(status_line, sizeof(status_line), "uiTop");
        break;
    case 4:
        monoFill(uiBottom, 16, brightness);
        snprintf(status_line, sizeof(status_line), "uiBottom");
        break;
    case 5:
        monoFill(bar33_37, 5, brightness);
        snprintf(status_line, sizeof(status_line), "bar33_37");
        break;
    case 6:
        monoFill(bar38_42, 5, brightness);
        snprintf(status_line, sizeof(status_line), "bar38_42");
        break;
    case 7:
        monoFill(bar43_48, 6, brightness);
        snprintf(status_line, sizeof(status_line), "bar43_48");
        break;
    case 8:
        monoFill(dropletCenter, 3, brightness);
        snprintf(status_line, sizeof(status_line), "droplet");
        break;
    case 9:
        monoFill(midBox, 5, brightness);
        snprintf(status_line, sizeof(status_line), "midBox");
        break;
    case 10:
        monoFill(boltOuter, 10, brightness);
        snprintf(status_line, sizeof(status_line), "boltOuter");
        break;
    case 11:
        monoFill(boltInner, 4, brightness);
        snprintf(status_line, sizeof(status_line), "boltInner");
        break;

    case 12:
        rgbFillOuter(0, 0, brightness);
        snprintf(status_line, sizeof(status_line), "HALO blue");
        break;
    case 13:
        rgbFillOuter(brightness, 0, 0);
        snprintf(status_line, sizeof(status_line), "HALO red");
        break;
    case 14:
        rgbFillOuter(0, brightness, 0);
        snprintf(status_line, sizeof(status_line), "HALO green");
        break;
    case 15:
        rgbFillOuter(brightness, brightness, brightness);
        snprintf(status_line, sizeof(status_line), "HALO white");
        break;
    case 16:
        rgbFillOuter(brightness, 0, brightness);
        snprintf(status_line, sizeof(status_line), "HALO purple");
        break;
    case 17:
        rgbFillOuter(0, brightness, brightness);
        snprintf(status_line, sizeof(status_line), "HALO cyan");
        break;
    case 18:
        rgbFillOuter(brightness, brightness, 0);
        snprintf(status_line, sizeof(status_line), "HALO yellow");
        break;
    case 19:
        for(uint8_t i = 0; i < 12; i++) {
            if(i & 1) rgbSetOuterOne(i, brightness, 0, 0);
            else      rgbSetOuterOne(i, 0, 0, brightness);
        }
        snprintf(status_line, sizeof(status_line), "HALO alt R/B");
        break;
    case 20:
        for(uint8_t i = 0; i < 12; i++) {
            if(i < 6) rgbSetOuterOne(i, 0, 0, brightness);
            else      rgbSetOuterOne(i, brightness, 0, 0);
        }
        snprintf(status_line, sizeof(status_line), "HALO half B/R");
        break;
    case 21:
        rgbSetOuterOne(0, brightness, brightness, brightness);
        snprintf(status_line, sizeof(status_line), "HALO one white");
        break;
    case 22:
        rgbSetOuterOne(0, brightness, brightness, brightness);
        rgbSetOuterOne(4, brightness, brightness, brightness);
        rgbSetOuterOne(8, brightness, brightness, brightness);
        snprintf(status_line, sizeof(status_line), "HALO 3 white");
        break;
    case 23:
        slotToBase("t0", &base);
        set7Seg(base, 7, brightness, false);
        rgbFillOuter(0, 0, brightness);
        snprintf(status_line, sizeof(status_line), "t0=7 + HALO blue");
        break;
    case 24:
        monoFill(boltOuter, 10, brightness);
        rgbFillOuter(brightness, 0, 0);
        snprintf(status_line, sizeof(status_line), "boltOuter + HALO red");
        break;
    default:
        snprintf(status_line, sizeof(status_line), "step?");
        break;
    }
}

static void reset_off(void) {
    clearFrame();
    step = 0;
    snprintf(status_line, sizeof(status_line), "OFF/reset");
}

// ===== UI =====
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Vape (Arduino-style)");

    canvas_set_font(canvas, FontSecondary);

    char l1[48];
    snprintf(l1, sizeof(l1), "Step %u/%u", (unsigned)(step + 1), (unsigned)STEP_COUNT);
    canvas_draw_str(canvas, 2, 28, l1);

    char l2[48];
    snprintf(l2, sizeof(l2), "Brightness %u", (unsigned)brightness);
    canvas_draw_str(canvas, 2, 40, l2);

    char l3[48];
    snprintf(l3, sizeof(l3), "%.47s", status_line);
    canvas_draw_str(canvas, 2, 52, l3);
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, input_event, FuriWaitForever);
}

int32_t vape_display_app(void* p) {
    UNUSED(p);

    current_pkt = pktB;

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, NULL);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Device boot window
    furi_delay_ms(800);

    // Start
    step = 0;
    render_step(step);
    push_frame(PUSH_REPEATS);

    bool running = true;
    InputEvent event;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyRight:
                    step = (uint8_t)((step + 1) % STEP_COUNT);
                    render_step(step);
                    push_frame(PUSH_REPEATS);
                    break;

                case InputKeyLeft:
                    step = (step == 0) ? (STEP_COUNT - 1) : (uint8_t)(step - 1);
                    render_step(step);
                    push_frame(PUSH_REPEATS);
                    break;

                case InputKeyUp:
                    brightness = (uint8_t)((brightness >= 240) ? 255 : (brightness + 16));
                    render_step(step);
                    push_frame(PUSH_REPEATS);
                    break;

                case InputKeyDown:
                    brightness = (uint8_t)((brightness <= 16) ? 0 : (brightness - 16));
                    render_step(step);
                    push_frame(PUSH_REPEATS);
                    break;

                case InputKeyOk:
                    // re-apply current step
                    render_step(step);
                    push_frame(PUSH_REPEATS);
                    break;

                case InputKeyBack:
                    reset_off();
                    push_frame(PUSH_REPEATS);
                    break;

                default:
                    break;
                }
            } else if(event.type == InputTypeLong && event.key == InputKeyBack) {
                running = false;
            }

            view_port_update(view_port);
        } else {
            // no event: still refresh UI occasionally
            view_port_update(view_port);
        }
    }

    // Turn off on exit
    reset_off();
    push_frame(PUSH_REPEATS);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}
