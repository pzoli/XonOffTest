#include <Arduino.h>

#define XOFF 0x13
#define XON  0x11
#define BUFFER_HIGH_WATERMARK 20
#define BUFFER_LOW_WATERMARK  16 

bool xoffSent = false;

void checkFlowControl(int available) {
    //if (available > 0) {
    //    Serial.print(F("Available bytes: "));
    //    Serial.println(available);
    //}
    if (!xoffSent && available >= BUFFER_HIGH_WATERMARK) {
        Serial.write(XOFF);
        //Serial.println(F("Flow control: XOFF sent"));
        xoffSent = true;
    } else if (xoffSent && available <= BUFFER_LOW_WATERMARK) {
        Serial.write(XON);
        //Serial.println(F("Flow control: XON sent"));
        xoffSent = false;
    }
}

int idx = 0;
String inputValues[3] = {"", "", ""};

void setup() {
    Serial.begin(115200);
    while (!Serial){}
    Serial.println(F("Initialized, waiting for connection..."));
}

byte buffer[128];

void loop() {
    int available = Serial.available();
    checkFlowControl(available);
    if (available > 0) {
        Serial.readBytes(buffer, available);
        for(int i = 0; i < available; i++) {
            char ch = buffer[i];
            if (idx < 3 && ch != ';' && ch != '\n' && ch != '\r' && ch != '\t') {
                inputValues[idx] += ch;
            }
            if (ch == '\n') {
                Serial.println(String(F("Name: ")) + inputValues[0]);
                delay(100);
                Serial.println(String(F("Phone: ")) + inputValues[1]);
                delay(100);
                Serial.println(String(F("Email: ")) + inputValues[2]);
            }
            if (ch == ';') {
                idx++;
            }
            if (ch == '\r') {
                inputValues[0] = "";
                inputValues[1] = "";
                inputValues[2] = "";
                idx = 0;
            }
        }
    }
}