#include <Arduino.h>

// ── Flow control ────────────────────────────────────────────────
#define XOFF                0x13
#define XON                 0x11
#define BUFFER_HIGH_WATERMARK 42   // ~75% of 64-byte HW buffer
#define BUFFER_LOW_WATERMARK  16   // resume with comfortable headroom

// ── Parser state ────────────────────────────────────────────────
#define NUM_FIELDS  3
#define FIELD_NAME  0
#define FIELD_PHONE 1
#define FIELD_EMAIL 2

static char   fields[NUM_FIELDS][64];  // fixed-size field buffers
static uint8_t fieldLens[NUM_FIELDS];  // current length of each field
static uint8_t fieldIdx  = 0;
static bool   msgReady   = false;

// Pending log line accumulated during parsing
static char   logBuf[128];
static bool   hasLog = false;

// Flow control state
static bool   xoffSent = false;

// ── Helpers ─────────────────────────────────────────────────────
static void pauseTransmitter() {
    if (!xoffSent) {
        Serial.write(XOFF);
        xoffSent = true;
    }
}

static void resumeTransmitter() {
    if (xoffSent) {
        Serial.write(XON);
        xoffSent = false;
    }
}

static void checkFlowControl() {
    int avail = Serial.available();
    if (!xoffSent && avail >= BUFFER_HIGH_WATERMARK) {
        pauseTransmitter();
    } else if (xoffSent && avail <= BUFFER_LOW_WATERMARK) {
        resumeTransmitter();
    }
}

static void resetParser() {
    for (uint8_t i = 0; i < NUM_FIELDS; i++) {
        fields[i][0] = '\0';
        fieldLens[i] = 0;
    }
    fieldIdx = 0;
    msgReady = false;
}

// Append one character to the current field (with overflow guard)
static void appendChar(char ch) {
    if (fieldIdx >= NUM_FIELDS) return;
    uint8_t len = fieldLens[fieldIdx];
    if (len < sizeof(fields[0]) - 1) {
        fields[fieldIdx][len]     = ch;
        fields[fieldIdx][len + 1] = '\0';
        fieldLens[fieldIdx]++;
    }
}

// ── Setup ────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    resetParser();
    Serial.println(F("Initialized, waiting for connection..."));
}

// ── Main loop ────────────────────────────────────────────────────
#define READ_CHUNK 32
static byte readBuf[READ_CHUNK];

void loop() {
    checkFlowControl();

    int avail = Serial.available();
    if (avail <= 0) return;

    // Read a bounded chunk to keep loop responsive
    int toRead = min(avail, READ_CHUNK);
    int got    = Serial.readBytes(readBuf, toRead);

    // ── Parsing loop – NO Serial I/O in here ──────────────────
    for (int i = 0; i < got; i++) {
        char ch = (char)readBuf[i];

        switch (ch) {
            case ';':
                // Advance to next field
                if (fieldIdx < NUM_FIELDS - 1) fieldIdx++;
                break;

            case '\n':
                // Complete message received
                msgReady = true;
                break;

            case '\r':
                // Reset on carriage-return (handles \r\n and bare \r)
                resetParser();
                break;

            default:
                appendChar(ch);
                break;
        }
    }
    // ── End parsing loop ──────────────────────────────────────

    // Emit output only after full message is parsed
    if (msgReady) {
        pauseTransmitter();   // hold sender while we do Serial I/O

        Serial.print(F("Name: "));  Serial.println(fields[FIELD_NAME]);
        Serial.print(F("Phone: ")); Serial.println(fields[FIELD_PHONE]);
        Serial.print(F("Email: ")); Serial.println(fields[FIELD_EMAIL]);

        resetParser();
        resumeTransmitter();  // let sender continue
    }
}