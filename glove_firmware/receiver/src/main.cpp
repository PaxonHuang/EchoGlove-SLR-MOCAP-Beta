#include <Arduino.h>
#include "data_structures.h"
#include "ESPNOWReceiver.h"
#include "FramePairer.h"
#include "RelativeFeatures.h"

ESPNOWReceiver receiver;
FramePairer pairer;
RelativeFeatures relFeatures;

void setup() {
    Serial.begin(115200);
    receiver.begin();
    Serial.println("[Receiver] V5.0 started");
}

void loop() {
    GlovePacket pkt;
    while (receiver.receive(pkt)) {
        pairer.feed(pkt);
    }
    FramePair pair;
    if (pairer.getPair(pair)) {
        float relative[6];
        relFeatures.compute(
            pair.left.euler, pair.left.quaternion,
            pair.right.euler, pair.right.quaternion,
            relative, pair.left.gyro, pair.right.gyro
        );
        Serial.printf("[Pair] tick=%u rel[0]=%.3f\n", pair.tick_id, relative[0]);
    }
    pairer.tick(micros());
    delay(1);
}
