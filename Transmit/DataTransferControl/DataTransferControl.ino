#define Bluetooth Serial1  // 直接使用 Mega 2560 的 Serial1 作为蓝牙串口
#define dirPin1 22
#define stepPin1 23
#define dirPin2 24
#define stepPin2 25
#define dirPin3 26
#define stepPin3 27

#include <HX711_ADC.h>

// HX711 负载传感器引脚
const int HX711_dout_1 = 2;  // 负载传感器 1 数据输出
const int HX711_sck_1 = 3;   // 负载传感器 1 时钟信号
const int HX711_dout_2 = 4;  // 负载传感器 2 数据输出
const int HX711_sck_2 = 5;   // 负载传感器 2 时钟信号

HX711_ADC LoadCell_1(HX711_dout_1, HX711_sck_1);  
HX711_ADC LoadCell_2(HX711_dout_2, HX711_sck_2);  

String receivedData = "";
float receivedWeight1 = 0.0;
float receivedWeight2 = 0.0;
float setpoint1 = -100;  // 目标重量（腔室 1）
float setpoint2 = -100;  // 目标重量（腔室 2）
float threshold = 5;     // 误差阈值

void setup() {
  Serial.begin(57600);   // PC 调试串口
  Bluetooth.begin(9600); // 蓝牙模块通信串口
  
  pinMode(stepPin1, OUTPUT);
  pinMode(dirPin1, OUTPUT);
  pinMode(stepPin2, OUTPUT);
  pinMode(dirPin2, OUTPUT);
  
  LoadCell_1.begin();
  LoadCell_2.begin();
  
  Serial.println("System Initialized...");
}

void loop() {
  // 读取 Unity 发送的蓝牙数据
  while (Bluetooth.available()) {
    char c = Bluetooth.read();
    if (c == '\n') {  // 解析数据
      parseBluetoothData(receivedData);
      receivedData = "";  // 清空数据
    } else {
      receivedData += c;
    }
  }

  // 读取传感器数据
  LoadCell_1.update();
  LoadCell_2.update();
  float weight1 = LoadCell_1.getData();
  float weight2 = LoadCell_2.getData();

  // 控制步进电机（双腔室）
  controlPump(weight1, setpoint1, stepPin1, dirPin1);
  controlPump(weight2, setpoint2, stepPin2, dirPin2);
}

// **解析 Unity 发送的蓝牙数据**
void parseBluetoothData(String data) {
  int commaIndex = data.indexOf(',');
  if (commaIndex != -1) {
    receivedWeight1 = data.substring(0, commaIndex).toFloat();
    receivedWeight2 = data.substring(commaIndex + 1).toFloat();
    
    Serial.print("Received Weight 1: ");
    Serial.print(receivedWeight1);
    Serial.print("  Received Weight 2: ");
    Serial.println(receivedWeight2);
  }
}

// **步进电机控制逻辑**
void controlPump(float currentWeight, float targetWeight, int stepPin, int dirPin) {
  float error = targetWeight - currentWeight;
  
  if (abs(error) > threshold) {
    digitalWrite(dirPin, error > 0 ? HIGH : LOW);
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(400);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(400);
  }
}
