// 使用硬件串口1 与 HC-05 通信
#define BT Serial1

void setup() {
  Serial.begin(57600);   // PC 调试口
  BT.begin(9600);        // HC-05 默认波特率
  pinMode(LED_BUILTIN, OUTPUT);
}

String buf;
void loop() {
  // 从 BT 串口接收数据
  while (BT.available()) {
    char c = BT.read();
    if (c == '\n') {
      Serial.print("Recv>> "); Serial.println(buf);
      // TODO: 根据 buf 内容驱动步进电机或流体系统
      buf = "";
    } else buf += c;
  }
  // 也可将传感器数据发给 Unity：
  // float weight = LoadCell.getData();
  // BT.println(weight);
}
