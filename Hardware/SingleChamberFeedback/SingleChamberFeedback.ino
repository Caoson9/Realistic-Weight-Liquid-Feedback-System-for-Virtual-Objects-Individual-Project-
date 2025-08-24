#include <HX711_ADC.h>
#include <EEPROM.h>

// —— LoadCell 1 引脚 —— 
const int DOUT1 = 2;
const int SCK1  = 3;
HX711_ADC LoadCell1(DOUT1, SCK1);

// —— 泵 1 步进电机引脚 —— 
const int dirPin1  = 22;
const int stepPin1 = 23;

// —— 串口和目标重量 —— 
// 这里我们用 USB 串口 Serial，如果你用蓝牙请改成 Serial1
#define BT Serial
float targetGrams = 0.0;

// —— 泵步进控制参数 —— 
const unsigned long stepInterval = 200; // 微秒，调小可提速
unsigned long prevStepTime = 0;
bool  stepHigh = false;

// —— 校准偏移 —— 
float offset1 = 0.0;

void setup() {
  Serial.begin(57600);
  pinMode(dirPin1, OUTPUT);
  pinMode(stepPin1, OUTPUT);

  // 初始化 LoadCell1
  LoadCell1.begin();
  LoadCell1.start(2000, true);  // 稳定 + 去皮
  while (!LoadCell1.update());
  offset1 = LoadCell1.getData(); // 记录初始偏移

  Serial.println("SingleChamberFeedback Ready");
}

void loop() {
  // —— 1) 解析串口指令 —— 
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    int comma = line.indexOf(',');
    if (comma > 0) {
      int state = line.substring(0, comma).toInt();
      float mass  = line.substring(comma + 1).toFloat();
      if (state == 1) {
        targetGrams = mass * 1000.0;  // kg→g
      } else {
        targetGrams = 0.0;
      }
      Serial.print("New Target: "); Serial.println(targetGrams);
    }
  }

  // —— 2) 读取当前重量 —— 
  static bool newData = false;
  if (LoadCell1.update()) newData = true;
  if (newData) {
    float current = LoadCell1.getData() - offset1;
    newData = false;
    // —— 3) 闭环控制 Pump 1 —— 
    unsigned long now = micros();
    if (current < targetGrams - 5) {
      // 泵入
      digitalWrite(dirPin1, HIGH);
      if (now - prevStepTime >= stepInterval) {
        digitalWrite(stepPin1, stepHigh? LOW: HIGH);
        stepHigh = !stepHigh;
        prevStepTime = now;
      }
    }
    else if (current > targetGrams + 5) {
      // 泵出
      digitalWrite(dirPin1, LOW);
      if (now - prevStepTime >= stepInterval) {
        digitalWrite(stepPin1, stepHigh? LOW: HIGH);
        stepHigh = !stepHigh;
        prevStepTime = now;
      }
    }
    else {
      // 稳定态: 关断步进信号
      digitalWrite(stepPin1, LOW);
    }
    // —— 4) 调试输出 —— 
    Serial.print("Current(g):"); Serial.print(current);
    Serial.print("  Target(g):"); Serial.println(targetGrams);
  }
}
