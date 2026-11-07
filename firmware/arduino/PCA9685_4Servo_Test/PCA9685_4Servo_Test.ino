/*
 * PCA9685 four-servo test for ESP32-S3 (Arduino IDE).
 * Wire library only. Change the pins to match your board and wiring.
 *
 * ESP32-S3 3.3V -> PCA9685 VCC; GND -> GND; SDA/SCL -> SDA/SCL.
 * Separate regulated 5V supply -> PCA9685 V+ and GND. Share ground.
 * Servos -> channels 0-3: signal to S, red to V+, brown/black to GND.
 * Check your module's pin labels and the servo's rated supply voltage.
 * Test with the mechanism disconnected and stop if a servo binds.
 */
#include <Wire.h>

const int SDA_PIN = 8;  // Example pins; change for your ESP32-S3 board.
const int SCL_PIN = 9;
const uint8_t PCA_ADDRESS = 0x40; // Default, all address jumpers open.
const int SERVO_MIN_US = 1000;     // Adjust only after checking the servo.
const int SERVO_MAX_US = 2000;
const float PWM_HZ = 50.0f;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PCA_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool setServo(uint8_t channel, int degrees) {
  degrees = constrain(degrees, 0, 180);
  int pulseUs = map(degrees, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  uint16_t ticks = (uint16_t)(pulseUs * PWM_HZ * 4096.0f / 1000000.0f + 0.5f);
  Wire.beginTransmission(PCA_ADDRESS);
  Wire.write(0x06 + 4 * channel); // LEDn_ON_L, auto-increment enabled.
  Wire.write(0); Wire.write(0);   // ON = 0
  Wire.write(ticks & 0xff); Wire.write(ticks >> 8); // OFF = ticks
  return Wire.endTransmission() == 0;
}

// Write all four channels in one transaction. MODE2 OCH=0 applies the
// updated output registers on the same I2C STOP condition.
bool setAllServos(int degrees) {
  degrees = constrain(degrees, 0, 180);
  int pulseUs = map(degrees, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  uint16_t ticks = (uint16_t)(pulseUs * PWM_HZ * 4096.0f / 1000000.0f + 0.5f);
  Wire.beginTransmission(PCA_ADDRESS);
  Wire.write(0x06);
  for (uint8_t channel = 0; channel < 4; ++channel) {
    Wire.write(0); Wire.write(0);
    Wire.write(ticks & 0xff); Wire.write(ticks >> 8);
  }
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Give the serial monitor time to attach after reset.
  Serial.println("\nPCA9685 servo test starting");
  Serial.printf("SDA=GPIO%d SCL=GPIO%d address=0x%02X\n", SDA_PIN, SCL_PIN, PCA_ADDRESS);
  Wire.begin(SDA_PIN, SCL_PIN);
  // MODE1: sleep + auto-increment, set 50 Hz for nominal 25 MHz oscillator,
  // then wake with auto-increment enabled.
  if (!writeRegister(0x00, 0x30) || !writeRegister(0x01, 0x04) ||
      !writeRegister(0xFE, 121) ||
      !writeRegister(0x00, 0x20)) {
    Serial.println("PCA9685 not responding. Check wiring and address.");
    while (true) { delay(3000); Serial.println("Check PCA9685 VCC, GND, SDA, SCL and address; reset to retry."); }
  }
  delay(1); // Allow oscillator to stabilize.
  Serial.println("Testing channels 0-3 one at a time: 90, 60, 120, 90 degrees.");
}

void loop() {
  const int positions[] = {90, 60, 120, 90};
  for (uint8_t channel = 0; channel < 4; ++channel) {
    for (int position : positions) {
      Serial.printf("Channel %u -> %d degrees\n", channel, position);
      if (!setServo(channel, position)) {
        Serial.println("I2C write failed. Check wiring and power; reset to retry.");
        while (true) delay(1000);
      }
      delay(1000);
    }
  }
  Serial.println("Testing all four channels together.");
  for (int position : positions) {
    Serial.printf("All channels -> %d degrees\n", position);
    if (!setAllServos(position)) {
      Serial.println("I2C write failed. Check wiring and power; reset to retry.");
      while (true) delay(1000);
    }
    delay(1000);
  }
  delay(2000);
}
