/*
 * MG90 / MG90S 舵机控制 —— ESP32-S3 (Arduino IDE)
 *
 * 接线：
 *   舵机 橙/黄线 (PWM 信号) -> GPIO9
 *   舵机 红线 (VCC)         -> 外部 5V 电源正极（不要用 ESP32-S3 的 3.3V！）
 *   舵机 棕/黑线 (GND)      -> 外部 5V 电源负极，并与 ESP32-S3 的 GND 共地
 *
 * 说明：
 *   - MG90 是 180° 位置舵机（不是 360° 连续旋转版）。
 *   - 信号线是 3.3V 逻辑电平即可被舵机识别，无需电平转换。
 *   - 舵机堵转电流可达 1A 以上，必须用独立 5V 供电，否则会导致 ESP32-S3 复位或重启。
 *
 * Arduino IDE 设置（工具菜单）：
 *   Board            : ESP32S3 Dev Module
 *   USB CDC On Boot  : Enabled（若用原生 USB 口查看串口日志）
 *   Flash Size       : 按模块选择（常见 8MB/16MB）
 *   PSRAM            : 按模块选择（常见 OPI PSRAM）
 *   Upload Speed     : 921600（不稳定就降到 460800 / 115200）
 *
 * 依赖库：ESP32Servo（库管理器搜索 "ESP32Servo" 安装，作者 Kevin Harrington）
 */

#include <ESP32Servo.h>

// ---------------- 可调参数 ----------------
static const int SERVO_PIN = 9;  // 舵机信号线接在 GPIO9

// MG90S 常见脉宽范围，单位微秒。若舵机行程不足或抖动，可微调这两个值。
static const int SERVO_MIN_US = 500;
static const int SERVO_MAX_US = 2500;

// 垃圾桶盖子的两个位置（角度可根据机械结构改）
static const int LID_CLOSE_DEG = 0;
static const int LID_OPEN_DEG = 90;

// 舵机运动到位后的保持时间（毫秒）
static const int MOVE_TIME_MS = 600;
// ---------------- 可调参数结束 ----------------

Servo lidServo;

// 把舵机转动到指定角度，并等待它走到位
void moveTo(int deg) {
  deg = constrain(deg, 0, 180);
  lidServo.write(deg);
  Serial.print("Servo -> ");
  Serial.print(deg);
  Serial.println(" deg");
  delay(MOVE_TIME_MS);
}

void openLid() {
  moveTo(LID_OPEN_DEG);
}

void closeLid() {
  moveTo(LID_CLOSE_DEG);
}

// 演示用：来回扫动一次
void sweepDemo() {
  Serial.println("Sweep demo");
  for (int deg = 0; deg <= 180; deg += 5) {
    lidServo.write(deg);
    delay(15);
  }
  for (int deg = 180; deg >= 0; deg -= 5) {
    lidServo.write(deg);
    delay(15);
  }
  lidServo.write(LID_CLOSE_DEG);
  delay(MOVE_TIME_MS);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // ESP32Servo 使用 LEDC 硬件 PWM，50Hz 是舵机标准频率
  lidServo.setPeriodHertz(50);
  lidServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);

  Serial.println();
  Serial.println("MG90 lid servo ready on GPIO9");
  Serial.println("Commands: o = open, c = close, s = sweep, 0-180 = go to angle");

  // 上电先归位到关闭状态
  lidServo.write(LID_CLOSE_DEG);
  delay(MOVE_TIME_MS);
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) {
    return;
  }

  char c = tolower(cmd[0]);
  if (c == 'o') {
    openLid();
  } else if (c == 'c') {
    closeLid();
  } else if (c == 's') {
    sweepDemo();
  } else if (isdigit(c)) {
    moveTo(cmd.toInt());
  } else {
    Serial.println("Unknown command");
  }
}
