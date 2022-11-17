#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ESP32Servo.h>
#include <BluetoothSerial.h>
#define Addr 0x1C
#define SDA 18
#define SCL 19

/////////////////////////////////////////////////////////////////////
int angleC2 = 72;//72舵机校对位置，下同， 右前肩部舵机
int angleC4 = 104;//100  右前腿舵机
int angleC5 = 141;//141  右前脚舵机
int angleC12 = 108;//98  右后肩舵机
int angleC13 = 110;//100  右后腿舵机
int angleC14 = 146;//135  右后脚舵机
int angleC15 = 49;//53   左后肩舵机
int angleC16 = 96;//90   颈部舵机
int angleC25 = 70;//70  左后腿舵机
int angleC26 = 51;//37  左后脚舵机
int angleC27 = 120;//120  左前肩舵机
int angleC32 = 66;//70  左前腿舵机
int angleC33 = 46;//43  左前脚舵机
////////////////////////////////////////////////////////////////////////

int act = 1;
const double pi = 3.1416; //设定运算常量圆周率
const int servonum = 13; // 定义舵机数量
BluetoothSerial SerialBT;
Servo servo[servonum];
char val;
const int H = 70;     //步行姿态计算中的离地高度设定，详见“姿态计算解析”教程，请在网盘教程中寻找
const int hipL = 40; //步行姿态计算中的腿部零件长度，详见“姿态计算解析”
const double legL = 58; //步行姿态计算中的脚部零件长度，详见“姿态计算解析”
int soundTriggerPin = 23; //定义超声波模块针脚位置
int soundEchoPin = 22; //定义超声波模块针脚位置
String header;
const char *ssid = "catcontrol"; //热点名称，自定义
const char *password = "12345678";//连接密码，自定义
void Task1code( void *pvParameters );
void Task2code( void *pvParameters );
//wifi在core0，其他在core1；1为大核
WiFiServer server(80);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("1");
  pinMode(soundTriggerPin, OUTPUT);
  pinMode(soundEchoPin, INPUT);
  servo[0].attach(2, 500, 2500); //右前肩舵机，插引脚2，对应校对参数为angleC2
  servo[1].attach(4, 500, 2500); //右前腿舵机，插引脚4，对应校对参数为angleC4
  servo[2].attach(5, 500, 2500); //右前脚舵机，插引脚5，对应校对参数为angleC5
  servo[3].attach(12, 500, 2500); //右后肩舵机，插引脚12，对应校对参数为angleC12
  servo[4].attach(13, 500, 2500); //右后腿舵机，插引脚13，对应校对参数为angleC13
  servo[5].attach(14, 500, 2500); //右后脚舵机，插引脚14，对应校对参数为angleC14
  servo[6].attach(15, 500, 2500); //左后肩舵机，插引脚15，对应校对参数为angleC15
  servo[7].attach(25, 500, 2500); //左后腿舵机，插引脚25，对应校对参数为angleC25
  servo[8].attach(26, 500, 2500); //左后脚舵机，插引脚26，对应校对参数为angleC26
  servo[9].attach(27, 500, 2500); //左前肩舵机，插引脚27，对应校对参数为angleC27
  servo[10].attach(32, 500, 2500); //左前腿舵机，插引脚32，对应校对参数为angleC32
  servo[11].attach(33, 500, 2500); //左前脚舵机，插引脚33，对应校对参数为angleC33
  servo[12].attach(16, 500, 2500); //颈部舵机，插引脚16，对应校对参数为angleC16
  SerialBT.begin("ESP32CAT");
  delay(100);
  Wire.begin(SDA, SCL);
  Wire.beginTransmission(Addr);
  Wire.write(0x2A);
  Wire.write((byte)0x00);
  Wire.endTransmission();
  Wire.beginTransmission(Addr);
  Wire.write(0x2A);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.beginTransmission(Addr);
  Wire.write(0x0E);
  Wire.write((byte)0x00);
  Wire.endTransmission();

  Serial.println();
  Serial.println("Configuring access point...");
  WiFi.softAP(ssid, password);//不写password，即热点开放不加密
  IPAddress myIP = WiFi.softAPIP();//此为默认IP地址192.168.4.1，也可在括号中自行填入自定义IP地址
  Serial.print("AP IP address:");
  Serial.println(myIP);
  server.begin();
  Serial.println("Server started");
  delay(300);
  stand();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);//关闭低电压检测,避免无限重启
  xTaskCreatePinnedToCore(Task1, "Task1", 10000, NULL, 1, NULL,  0);  //最后一个参数至关重要，决定这个任务创建在哪个核上.PRO_CPU 为 0, APP_CPU 为 1,或者 tskNO_AFFINITY 允许任务在两者上运行.
  xTaskCreatePinnedToCore(Task2, "Task2", 10000, NULL, 1, NULL,  1);//
  //实现任务的函数名称（task1）；任务的任何名称（“ task1”等）；分配给任务的堆栈大小，以字为单位；任务输入参数（可以为NULL）；任务的优先级（0是最低优先级）；任务句柄（可以为NULL）；任务将运行的内核ID（0或1）

  delay(2000);
  int part1 = 1, part2 = 1, turncount = 0, turn = -1;

  if (part1)
  {
    Serial.println("1");
    stand();
    vTaskDelay(500);
    int mark = 1;
    while (1) {
      if (turncount == 3) {
        break;
      }
      servo[12].write(angleC16);
      delay(5);
      int Cdistance = CalculateDistance();//记录超声波测距数据

      Serial.println(Cdistance);
      Serial.println();
      if (Cdistance > 15) { //如果测距数据大于15cm则前进

        if (Cdistance > 40)
        {
          if (turncount != 0)
          {
            balance();
          }
          servo[12].write(angleC16);
        }
        for (int i = 0; i < 10; i++)
        {
          servo[0].write(angleC2);
          servo[3].write(angleC12);
          servo[6].write(angleC15);
          servo[9].write(angleC27);
          runA();
          runB();
          runC();
          runD();
          Cdistance = CalculateDistance();
          if (Cdistance <= 15)break;
        }
      } else if (Cdistance <= 15) {              //如果测距数据小于15cm则转向，随机左转或右转
        int Rrdistance = RDistance();   //记录超声波测距数据
        vTaskDelay(300);
        int Lldistance = LDistance();   //记录超声波测距数据
        vTaskDelay(300);
        Serial.print(Lldistance);
        Serial.print("|");
        Serial.println(Rrdistance);
        if (Lldistance > Rrdistance) {
          for (int i = 0; i < 6; i++) {
            vTaskDelay(100);
            turnleft();
            
          } turn = 0; turncount++;
        }
        if (Lldistance < Rrdistance) {
          for (int i = 0; i < 5; i++) {
            vTaskDelay(100);
            turnright();
          } turn = 1; turncount++;
         
        }
        servo[12].write(angleC16);
        delay(300);
        for (int i = 0; i < 5; i++)
          {
            servo[0].write(angleC2);
            servo[3].write(angleC12);
            servo[6].write(angleC15);
            servo[9].write(angleC27);
            runA();
            runB();
            runC();
            runD();
          }
        
      }
      stopaction();
    }
    part1 = 0;
    part2 = 1;
  }
  if (part2)
  {
    vTaskDelay(300);
    while (1)
    {
      int left = LeftDistance();
      int right = RightDistance();
      Serial.print(left);
      Serial.print("--");
      Serial.println(right);
      if (left < 50 || right < 50)
      {
        if (left - right > 10)
        {
          vTaskDelay(100);
          turnleft();
        }
        else if (right - left > 10)
        {
          vTaskDelay(100);
          turnright();
        }
      }
      else if (left > 50 && right > 50)
      {
        for (int i = 0; i < 3; i++)
        {
          servo[0].write(angleC2);
          servo[3].write(angleC12);
          servo[6].write(angleC15);
          servo[9].write(angleC27);
          runA();
          runB();
          runC();
          runD();
        }
        break;
      }
      for (int i = 0; i < 7; i++)
      {
        servo[0].write(angleC2);
        servo[3].write(angleC12);
        servo[6].write(angleC15);
        servo[9].write(angleC27);
        runA();
        runB();
        runC();
        runD();
      }
    }



    if (turn == 0) //开头左转
    {
      for (int i = 0; i < 5; i++) {
        vTaskDelay(100);
        turnright();
      }
    }
    else
    {
      for (int i = 0; i < 6; i++) {
        vTaskDelay(100);
        turnleft();
      }
    }
    while (1)
    {
      double juli = CalculateDistance();
      if (juli >= 20)
      {
        balance();
        for(int i = 0; i < 7; i++)
        {
        servo[0].write(angleC2);
        servo[3].write(angleC12);
        servo[6].write(angleC15);
        servo[9].write(angleC27);
        runA();
        runB();
        runC();
        runD();
        }
      }
      else
      {
        stopaction();
        break;
      }
    }
    //走到黄色星星处

    for (int i = 0; i < 11; i++) {
      vTaskDelay(100);
      turnleft();
    }

    for (int i = 0; i < 5; i++)
    {
      balance();
      servo[12].write(angleC16);
      for (i = 0; i < 10; i++)
      {
        servo[0].write(angleC2);
        servo[3].write(angleC12);
        servo[6].write(angleC15);
        servo[9].write(angleC27);
        runA();
        runB();
        runC();
        runD();
      }
    }
    stopaction();
    delay(300);
    stand();
    vTaskDelay(200);
    shakehands();
    vTaskDelay(300);

    part2 = 0;
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
void balance()//判断是否偏离中心
{
  int left = LeftDistance();
  int right = RightDistance();
  if (left - right > 10)
  {
    vTaskDelay(100);
    turnleft();
  }
  else if (right - left > 10)
  {
    vTaskDelay(100);
    turnright();
  }
}
int avg()//滤波
{
  int sum = 0, dismax = 0, dismin = 1000;
  for (int i = 0; i < 7; i++)
  {
    int Distance = CalculateDistance();
    if (dismax < Distance)dismax = Distance;
    if (dismin > Distance)dismin = Distance;
    sum = sum + Distance;
    delay(300);
  }
  sum = sum - dismin - dismax;
  sum = sum / 5;
  return sum;
}
void runing(int x)//走
{
  for (int i = 0; i < x; i++)
  {
    servo[0].write(angleC2);
    servo[3].write(angleC12);
    servo[6].write(angleC15);
    servo[9].write(angleC27);
    runA();
    runB();
    runC();
    runD();
  }
}
int LeftDistance()//左扭头测距
{
  svmoveb(12, angleC16 + 60);
  int Ldistance;
  unsigned long Time = millis();
  while (Time + 3000 > millis())
  {
    //if(Time+500<millis()&&Time+1500>millis()){Ldistance=CalculateDistance();}
    servo[12].write(angleC16 + 60);
    delay(5);
  }
  Ldistance = CalculateDistance();
  Serial.println(Ldistance);
  svmovea(12, angleC16);
  return Ldistance;
}
int RightDistance()//右扭头测距
{
  svmovea(12, angleC16 - 60);
  int Rdistance;
  unsigned long Time = millis();
  while (Time + 3000 > millis())
  {
    //if(Time+500<millis()&&Time+1500>millis()){Rdistance=CalculateDistance();}
    servo[12].write(angleC16 - 60);
    delay(5);
  }
  Rdistance = CalculateDistance();
  Serial.println(Rdistance);
  svmoveb(12, angleC16);
  return Rdistance;
}
int LDistance()//左扭头测距
{
  svmoveb(12, angleC16 + 75 );
  int Ldistance;
  unsigned long Time = millis();
  while (Time + 3000 > millis())
  {
    //if(Time+500<millis()&&Time+1500>millis()){Ldistance=CalculateDistance();}
    servo[12].write(angleC16 + 75 );
    delay(5);
  }
  Ldistance = CalculateDistance();
  Serial.println(Ldistance);
  svmovea(12, angleC16);
  return Ldistance;
}
int RDistance()//右扭头测距
{
  svmovea(12, angleC16 - 75);
  int Rdistance;
  unsigned long Time = millis();
  while (Time + 3000 > millis())
  {
    //if(Time+500<millis()&&Time+1500>millis()){Rdistance=CalculateDistance();}
    servo[12].write(angleC16 - 75);
    delay(5);
  }
  Rdistance = CalculateDistance();
  Serial.println(Rdistance);
  svmoveb(12, angleC16);
  return Rdistance;
}
void leftturn()
{
  for (int i = 0; i < 6; i++)
  {
    vTaskDelay(100);
    turnleft();
  }
}
void rightturn()
{
  for (int i = 0; i < 4; i++)
  {
    vTaskDelay(100);
    turnright();
  }
}

void Task1(void *pvParameters) {
  //在这里可以添加一些代码，这样的话这个任务执行时会先执行一次这里的内容（当然后面进入while循环之后不会再执行这部分了）
  while (1)
  {
    vTaskDelay(200);
    WiFiClient client = server.available();  //监听连入设备
    if (client) {
      String currentLine = "";
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          header += c;
          if (c == '\n') {
            if (currentLine.length() == 0) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<meta charset=\"UTF-8\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #096625; border: none; color: white; padding: 20px 25px; ");
              client.println("text-decoration: none; font-size: 18px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #555555; border: none; color: white; padding: 16px 20px;text-decoration: none; font-size: 16px; margin: 1px; cursor: pointer;}</style></head>");
              client.println("<body><h1>四足机器人控制</h1>");
              client.println("<p><a href=\"/20/on\"><button class=\"button\">前进</button></a></p>");
              client.println("<p><a href=\"/21/on\"><button class=\"button\">左转</button></a><a href=\"/22/on\"><button class=\"button\">停止</button></a><a href=\"/23/on\"><button class=\"button\">右转</button></a></p>");
              client.println("<p><a href=\"/36/on\"><button class=\"button2\">左转头</button></a><a href=\"/24/on\"><button class=\"button\">后退</button></a><a href=\"/37/on\"><button class=\"button2\">右转头</button></a></p>");
              client.println("<p><a href=\"/25/on\"><button class=\"button2\">步行</button></a><a href=\"/26/on\"><button class=\"button2\">坐姿</button></a><a href=\"/27/on\"><button class=\"button2\">握手</button></a><a  href=\"/28/on\"><button class=\"button2\">跟随</button></a></p>");
              client.println("<p><a href=\"/29/on\"><button class=\"button2\">踏步</button></a><a href=\"/30/on\"><button class=\"button2\">摇摆</button></a><a href=\"/31/on\"><button class=\"button2\">起卧</button></a><a  href=\"/32/on\"><button class=\"button2\">踢球</button></a></p>");
              client.println("<p><a href=\"/33/on\"><button class=\"button2\">自动行走</button></a><a href=\"/34/on\"><button class=\"button2\">站立平衡</button></a><a href=\"/35/on\"><button class=\"button2\">舵机校对</button></a></p>");
              client.println("</body></html>");
              client.println();
              if (header.indexOf("GET /20/on") >= 0) {

                Serial.println(val);
                val = 'f';
                Serial.println(val);
              }
              if (header.indexOf("GET /21/on") >= 0) {
                val = 'l';
                Serial.println(val);
              }
              if (header.indexOf("GET /22/on") >= 0) {
                val = 'x';
                Serial.println(val);
                act = 2;
              }
              if (header.indexOf("GET /23/on") >= 0) {
                val = 'r';
                Serial.println(val);
              }
              if (header.indexOf("GET /24/on") >= 0) {
                val = 'b';
                Serial.println(val);
              }
              if (header.indexOf("GET /25/on") >= 0) {
                val = 'w';
                Serial.println(val);
              }
              if (header.indexOf("GET /26/on") >= 0) {
                val = 't';
                Serial.println(val);
              }
              if (header.indexOf("GET /27/on") >= 0) {
                val = 'h';
                Serial.println(val);
              }
              if (header.indexOf("GET /28/on") >= 0) {
                val = 'm';
                Serial.println(val);
              }
              if (header.indexOf("GET /29/on") >= 0) {
                val = 's';
                Serial.println(val);
              }
              if (header.indexOf("GET /30/on") >= 0) {
                val = 'y';
                Serial.println(val);
              }
              if (header.indexOf("GET /31/on") >= 0) {
                val = 'u';
                Serial.println(val);
              }
              if (header.indexOf("GET /32/on") >= 0) {
                val = 'k';
                Serial.println(val);
              }
              if (header.indexOf("GET /33/on") >= 0) {
                val = 'a';
                Serial.println(val);
              }
              if (header.indexOf("GET /34/on") >= 0) {
                val = 'i';
                Serial.println(val);
              }
              if (header.indexOf("GET /35/on") >= 0) {
                val = 'c';
                Serial.println(val);
              }
              if (header.indexOf("GET /36/on") >= 0) {
                val = 'e';
                Serial.println(val);
              }
              if (header.indexOf("GET /37/on") >= 0) {
                val = 'g';
                Serial.println(val);
              }
              break;
            } else {
              currentLine = ""; //如果收到是换行，则清空字符串变量
            }
          } else if (c != '\r') {
            currentLine += c;
          }

        }
        vTaskDelay(1);
      }
      header = "";
      client.stop(); //断开连接
    }
  }
}
void Task2(void *pvParameters) {
  while (1)
  {
    stand();
    if (Serial.available() > 0) {
      val = Serial.read();
    }
    if (SerialBT.available() > 0) {
      val = SerialBT.read();
    }
    if (val == 'f') {
      Serial.println("前进");
      act = 1;
      while (act == 1) {
        servo[0].write(angleC2);
        servo[3].write(angleC12);
        servo[6].write(angleC15);
        servo[9].write(angleC27);
        runA(); //runA(),runB(),runC(),runD()是双脚离地步行姿态的四个运动阶段，详见教程“姿态计算解析”
        runB();
        runC();
        runD();
      }
    }
    if (val == 'c') {
      Serial.println("calibration");
      calibration();
    }
    if (val == 'b') {
      Serial.println("后退");
      act = 1;
      while (act == 1) {
        servo[0].write(angleC2);
        servo[3].write(angleC12);
        servo[6].write(angleC15);
        servo[9].write(angleC27);
        runbackA();
        runbackB();
        runbackC();
        runbackD();
      }
    }
    if (val == 'l') {
      Serial.println("左转");
      act = 1;
      turnpreparation();
      while (act == 1) {
        turnleft();
        stopaction();
      }
    }
    if (val == 'r') {
      Serial.println("右转");
      act = 1;
      turnpreparation();
      while (act == 1) {
        turnright();
        stopaction();
      }
    }
    if (val == 'w') {
      Serial.println("步行");
      stand();
      act = 1;
      while (act == 1) {
        servo[0].write(angleC2);
        servo[3].write(angleC12);
        servo[6].write(angleC15);
        servo[9].write(angleC27);
        walkA();                      //引用"servocontrol.h"中的代码段，详见servocontrol.h页面相关内容。下同
        walkB();
        walkC();
        walkD();
      }
    }
    if (val == 's') {
      Serial.println("踏步");
      stand();
      act = 1;
      svmovea(1, angleC4 - 75);
      svmoveb(4, angleC13 - 25);
      svmoveb(7, angleC25 + 65);
      svmovea(10, angleC32 + 35);
      while (act == 1) {
        servo[0].write(angleC2);
        servo[3].write(angleC12);
        servo[6].write(angleC15);
        servo[9].write(angleC27);
        stepwalk();
        stopaction();
      }
    }
    if (val == 't') {
      Serial.println("坐姿");
      vTaskDelay(200);
      sit();
      val = 'z';
    }
    if (val == 'g') {
      vTaskDelay(500);
      act = 1;
      Serial.println("head turn right");
      svmovea(12, angleC16 - 60);
      vTaskDelay(1000);
      while (act == 1) {
        vTaskDelay(10);
        stopaction();
      }
    }
    if (val == 'e') {
      vTaskDelay(500);
      act = 1;
      Serial.println("head turn left");
      svmoveb(12, angleC16 + 60);
      vTaskDelay(1000);
      while (act == 1) {
        vTaskDelay(10);
        stopaction();
      }
    }
    if (val == 'h') {
      Serial.println("握手");
      vTaskDelay(200);
      act = 1;
      shakehands();
      vTaskDelay(300);
      while (act == 1) {
        for (int i = 50; i < 90; i++) {
          servo[2].write(i);
          vTaskDelay(15);
        }
        for (int i = 90; i > 50; i--) {
          servo[2].write(i);
          vTaskDelay(15);
        }
        vTaskDelay(200);
        stopaction();
      }
    }
    if (val == 'y') {
      Serial.println("摇摆");
      vTaskDelay(100);
      act = 1;
      turnpreparation();
      vTaskDelay(500);
      swingpreparation();
      while (act == 1) {
        swing();
      }
    }
    if (val == 'u') {
      Serial.println("起卧");
      vTaskDelay(100);
      act = 1;
      stand();
      vTaskDelay(500);
      updownpreparation();
      while (act == 1) {
        updown();
      }
    }
    if (val == 'k') {
      Serial.println("踢球");
      act = 1;
      vTaskDelay(200);
      kick();
      val = 'z';
    }
    if (val == 'm') {
      Serial.println("跟随");
      stand();
      act = 1;
      vTaskDelay(200);
      int a = 3;
      while (act == 1) {
        double Fdistance = CalculateDistance();  //记录超声波测距数据
        Serial.println(Fdistance);
        if (Fdistance < 8) {                     //如果超声波测得的距离小于5cm，则机器人后退
          if (a != 2) {                           //这里设了一个变量开关，是因为发现当机器人在前进和后退这两个状态转换时有由于舵机突然加速而程序崩溃的风险。所以增加了此设定，即当程序发现有前进和后退状态的相互转变时则先进入站立姿态，再过渡到下个动作
            vTaskDelay(100);
            stand();
            vTaskDelay(100);
            servo[0].write(angleC2);
            servo[3].write(angleC12);
            servo[6].write(angleC15);
            servo[9].write(angleC27);
            runbackA();
            runbackB();
            runbackC();
            runbackD();
            a = 2;
            Serial.println("back");
            Serial.println(a);
          } else if (a == 2) {
            servo[0].write(angleC2);
            servo[3].write(angleC12);
            servo[6].write(angleC15);
            servo[9].write(angleC27);
            runbackA();
            runbackB();
            runbackC();
            runbackD();
            Serial.println(a);
          }
        }
        else if (Fdistance > 8 && Fdistance < 30) { //如果超声波测得的距离大于6cm，小于15cm，则机器人前进
          if (a != 1) {
            vTaskDelay(100);
            stand();
            vTaskDelay(100);
            servo[0].write(angleC2);
            servo[3].write(angleC12);
            servo[6].write(angleC15);
            servo[9].write(angleC27);
            runA();
            runB();
            runC();
            runD();
            a = 1;
            Serial.println("前进");
            Serial.println(a);
          } else if (a == 1) {
            servo[0].write(angleC2);
            servo[3].write(angleC12);
            servo[6].write(angleC15);
            servo[9].write(angleC27);
            runA();
            runB();
            runC();
            runD();
            Serial.println(a);
          }
        }
        stopaction();
        vTaskDelay(10);
      }
    }
    if (val == 'a') {
      Serial.println("自动行走");
      act = 1;
      stand();
      vTaskDelay(500);
      while (act == 1) {
        double Fdistance = CalculateDistance();   //记录超声波测距数据
        Serial.println(Fdistance);
        if (Fdistance > 20) {                     //如果测距数据大于20cm则前进
          servo[0].write(angleC2);
          servo[3].write(angleC12);
          servo[6].write(angleC15);
          servo[9].write(angleC27);
          runA();
          runB();
          runC();
          runD();
        } else if (Fdistance <= 20) {              //如果测距数据小于20cm则转向，随机左转或右转
          vTaskDelay(300);
          svmovea(12, angleC16 - 60);
          vTaskDelay(300);
          double Rdistance = CalculateDistance();   //记录超声波测距数据
          vTaskDelay(300);
          svmoveb(12, angleC16 + 60);
          vTaskDelay(300);
          double Ldistance = CalculateDistance();   //记录超声波测距数据
          vTaskDelay(300);
          Serial.print(Rdistance);
          Serial.print("|");
          Serial.println(Ldistance);
          if (Ldistance > Rdistance) {
            for (int i = 0; i < 6; i++) {
              vTaskDelay(100);
              turnleft();
            }
          }
          if (Ldistance < Rdistance) {
            for (int i = 0; i < 4; i++) {
              vTaskDelay(100);
              turnright();
            }
          }
          servo[12].write(angleC16);
        }
        stopaction();
      }
    }
    if (val == 'i') {
      Serial.println("站立平衡");
      vTaskDelay(100);
      act = 1;
      stand();
      int isv1 = angleC4 - 50;
      int isv2 = angleC5 - 85;
      int isv4 = angleC13 - 45;
      int isv5 = angleC14 - 73;
      int isv7 = angleC25 + 45;
      int isv8 = angleC26 + 73;
      int isv10 = angleC32 + 50;
      int isv11 = angleC33 + 85;
      vTaskDelay(2000);
      while (act == 1) {
        unsigned int data[7];
        Wire.requestFrom(Addr, 7);
        if (Wire.available() == 7) {
          data[0] = Wire.read();
          data[1] = Wire.read();
          data[2] = Wire.read();
          data[3] = Wire.read();
          data[4] = Wire.read();
          data[5] = Wire.read();
          data[6] = Wire.read();
        }
        int yAccl = ((data[3] * 256) + data[4]) / 16;
        if (yAccl > 2047) {
          yAccl -= 4096;
        }
        int xAccl = ((data[1] * 256) + data[2]) / 16;
        if (xAccl > 2047) {
          xAccl -= 4096;
        }
        Serial.print("Acceleration in X-Axis : ");
        Serial.println(xAccl);
        Serial.print("Acceleration in Y-Axis : ");
        Serial.println(yAccl);

        if (servo[2].read() < 150 && servo[5].read() < 150 && servo[8].read() > 20 && servo[11].read() > 20) {
          if (yAccl < -50) {
            isv1 = isv1 + 1;
            servo[1].write(isv1);
            isv2 = isv2 + 2;
            servo[2].write(isv2);
            isv4 = isv4 - 1;
            servo[4].write(isv4);
            isv5 = isv5 - 2;
            servo[5].write(isv5);
            isv7 = isv7 + 1;
            servo[7].write(isv7);
            isv8 = isv8 + 2;
            servo[8].write(isv8);
            isv10 = isv10 - 1;
            servo[10].write(isv10);
            isv11 = isv11 - 2;
            servo[11].write(isv11);
            vTaskDelay(10);
          }
          if (yAccl > 100) {
            isv1 = isv1 - 1;
            servo[1].write(isv1);
            isv2 = isv2 - 2;
            servo[2].write(isv2);
            isv4 = isv4 + 1;
            servo[4].write(isv4);
            isv5 = isv5 + 2;
            servo[5].write(isv5);
            isv7 = isv7 - 1;
            servo[7].write(isv7);
            isv8 = isv8 - 2;
            servo[8].write(isv8);
            isv10 = isv10 + 1;
            servo[10].write(isv10);
            isv11 = isv11 + 2;
            servo[11].write(isv11);
            vTaskDelay(10);
          }
          if (xAccl > 100) { //通过xAccl变化判定是否发生左右向倾斜，由于机器人本身不是完全水平，因此判断的区间设在>100和<-100，一旦判断倾斜则转动舵机来弥补这一倾斜
            isv1 = isv1 + 1;
            servo[1].write(isv1);
            isv2 = isv2 + 2;
            servo[2].write(isv2);
            isv4 = isv4 + 1;
            servo[4].write(isv4);
            isv5 = isv5 + 2;
            servo[5].write(isv5);
            isv7 = isv7 + 1;
            servo[7].write(isv7);
            isv8 = isv8 + 2;
            servo[8].write(isv8);
            isv10 = isv10 + 1;
            servo[10].write(isv10);
            isv11 = isv11 + 2;
            servo[11].write(isv11);
            delay(10);
          }

          if (xAccl < -100) {
            isv1 = isv1 - 1;
            servo[1].write(isv1);
            isv2 = isv2 - 2;
            servo[2].write(isv2);
            isv4 = isv4 - 1;
            servo[4].write(isv4);
            isv5 = isv5 - 2;
            servo[5].write(isv5);
            isv7 = isv7 - 1;
            servo[7].write(isv7);
            isv8 = isv8 - 2;
            servo[8].write(isv8);
            isv10 = isv10 - 1;
            servo[10].write(isv10);
            isv11 = isv11 - 2;
            servo[11].write(isv11);
            delay(10);
          }
        }
        if (servo[2].read() >= 150) {
          servo[2].write(150);
        }
        if (servo[5].read() >= 150) {
          servo[5].write(150);
        }
        if (servo[8].read() <= 20) {
          servo[8].write(20);
        }

        if (servo[11].read() <= 20) {
          servo[11].write(20);
        }
        vTaskDelay(10);
        stopaction();

      }
      vTaskDelay(100);
    }
  }
}

int CalculateDistance()   //超声波模块测距，套用官方代码，返回距离数值
{
  digitalWrite(soundTriggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(soundTriggerPin, LOW);
  long duration = pulseIn(soundEchoPin, HIGH);
  float distance = duration * 0.017F;
  return int(distance);
}
void svmovea(int sv, int angleA)
{
  for (double i = servo[sv].read(); i > angleA; i--)
  {
    servo[sv].write(i);
    delay(5);
  }
}

void svmoveb(int sv, int angleA)
{
  for (double i = servo[sv].read(); i < angleA; i++)
  {
    servo[sv].write(i);
    delay(5);
  }
}

void stand() //使机器人从任意状态转变到站立状态。
{
  servo[12].write(angleC16);
  servo[0].write(angleC2);
  if (servo[1].read() < angleC4 - 50) {
    svmoveb(1, angleC4 - 50);
  } else if (servo[1].read() > angleC4 - 50) {
    svmovea(1, angleC4 - 50);
  }
  if (servo[2].read() < angleC5 - 85) {
    svmoveb(2, angleC5 - 85);
  } else if (servo[2].read() > angleC5 - 85) {
    svmovea(2, angleC5 - 85);
  }
  servo[9].write(angleC27);
  if (servo[10].read() < angleC32 + 50) {
    svmoveb(10, angleC32 + 50);
  } else if (servo[10].read() > angleC32 + 50) {
    svmovea(10, angleC32 + 50);
  }
  if (servo[11].read() < angleC33 + 85) {
    svmoveb(11, angleC33 + 85);
  } else if (servo[11].read() > angleC33 + 85) {
    svmovea(11, angleC33 + 85);
  }
  servo[3].write(angleC12);
  if (servo[4].read() < angleC13 - 45) {
    svmoveb(4, angleC13 - 45);
  } else if (servo[4].read() > angleC13 - 45) {
    svmovea(4, angleC13 - 45);
  }
  if (servo[5].read() < angleC14 - 73) {
    svmoveb(5, angleC14 - 73);
  } else if (servo[5].read() > angleC14 - 73) {
    svmovea(5, angleC14 - 73);
  }

  if (servo[7].read() < angleC25 + 45) {
    svmoveb(7, angleC25 + 45);
  } else if (servo[7].read() > angleC25 + 45) {
    svmovea(7, angleC25 + 45);
  }
  if (servo[8].read() < angleC26 + 73) {
    svmoveb(8, angleC26 + 73);
  } else if (servo[8].read() > angleC26 + 73) {
    svmovea(8, angleC26 + 73);
  }
  servo[6].write(angleC15);
}

void stopaction() {
  if (Serial.available() > 0) {
    val = Serial.read();
    if (val == 'x' ) {            //则直接跳出这段一直循环重复的代码
      act = 2;
      stand();
    }
  }
  if (SerialBT.available() > 0) {
    val = SerialBT.read();
    if (val == 'x' ) {            //则直接跳出这段一直循环重复的代码
      act = 2;
      stand();
    }
  }
}

void calibration() { //舵机标准模式
  if (servo[0].read() < angleC2) {
    svmoveb(0, angleC2);
  } else if (servo[0].read() > angleC2) {
    svmovea(0, angleC2);
  }
  if (servo[1].read() < angleC4) {
    svmoveb(1, angleC4);
  } else if (servo[1].read() > angleC4) {
    svmovea(1, angleC4);
  }
  if (servo[2].read() < angleC5) {
    svmoveb(2, angleC5);
  } else if (servo[2].read() > angleC5) {
    svmovea(2, angleC5);
  }
  if (servo[3].read() < angleC12) {
    svmoveb(3, angleC12);
  } else if (servo[3].read() > angleC12) {
    svmovea(3, angleC12);
  }
  if (servo[4].read() < angleC13) {
    svmoveb(4, angleC13);
  } else if (servo[4].read() > angleC13) {
    svmovea(4, angleC13);
  }
  if (servo[5].read() < angleC14) {
    svmoveb(5, angleC14);
  } else if (servo[5].read() > angleC14) {
    svmovea(5, angleC14);
  }
  if (servo[6].read() < angleC15) {
    svmoveb(6, angleC15);
  } else if (servo[6].read() > angleC15) {
    svmovea(6, angleC15);
  }
  if (servo[7].read() < angleC25) {
    svmoveb(7, angleC25);
  } else if (servo[7].read() > angleC25) {
    svmovea(7, angleC25);
  }
  if (servo[8].read() < angleC26) {
    svmoveb(8, angleC26);
  } else if (servo[8].read() > angleC26) {
    svmovea(8, angleC26);
  }
  if (servo[9].read() < angleC27) {
    svmoveb(9, angleC27);
  } else if (servo[9].read() > angleC27) {
    svmovea(9, angleC27);
  }
  if (servo[10].read() < angleC32) {
    svmoveb(10, angleC32);
  } else if (servo[10].read() > angleC32) {
    svmovea(10, angleC32);
  }
  if (servo[11].read() < angleC33) {
    svmoveb(11, angleC33);
  } else if (servo[11].read() > angleC33) {
    svmovea(11, angleC33);
  }
  if (servo[12].read() < angleC16) {
    svmoveb(12, angleC16);
  } else if (servo[12].read() > angleC16) {
    svmovea(12, angleC16);
  }
  act = 1;
  while (act == 1) {
    delay(10);
    stopaction();
  }
}

double SlegangleL(int anglehA, int anglehC, int anglelC)
{
  double angleh = abs(anglehA - anglehC);
  double radn = angleh * 0.9 * pi / 180;
  double L = sin(radn) * hipL;
  double angleleg = asin(L / legL) * 180 / pi;
  double svangleleg = anglelC + (angleleg / 0.9 + angleh);
  return svangleleg;
}

double SlegangleR(int anglehA, int anglehC, int anglelC)
{
  double angleh = abs(anglehA - anglehC);
  double radn = angleh * 0.9 * pi / 180;
  double L = sin(radn) * hipL;
  double angleleg = asin(L / legL) * 180 / pi;
  double svangleleg = anglelC - (angleleg / 0.9 + angleh);
  return svangleleg;
}

void stepwalk() { //原地踏步，详见“姿态计算解析”
  if (act == 1) {
    for (int i = angleC4 - 65, j = angleC32 + 25, k = angleC13 - 30, l = angleC25 + 70; i < angleC4 - 25, j < angleC32 + 65, k > angleC13 - 70, l > angleC25 + 30; i++, j++, k--, l--) {
      servo[1].write(i);
      servo[4].write(k);
      servo[7].write(l);
      servo[10].write(j);
      double a = SlegangleR(i, angleC4, angleC5);
      double b = SlegangleR(k, angleC13, angleC14);
      double c = SlegangleL(l, angleC25, angleC26);
      double d = SlegangleL(j, angleC32, angleC33);
      servo[2].write(a);
      servo[5].write(b);
      servo[8].write(c);
      servo[11].write(d);
      delay(10);
    }
    stopaction();
  }
  if (act == 1) {
    for (int i = angleC4 - 25, j = angleC32 + 65, k = angleC13 - 70, l = angleC25 + 30; i > angleC4 - 65, j > angleC32 + 25, k < angleC13 - 30, l < angleC25 + 70; i--, j--, k++, l++) {
      servo[1].write(i);
      servo[4].write(k);
      servo[7].write(l);
      servo[10].write(j);
      double a = SlegangleR(i, angleC4, angleC5);
      double b = SlegangleR(k, angleC13, angleC14);
      double c = SlegangleL(l, angleC25, angleC26);
      double d = SlegangleL(j, angleC32, angleC33);
      servo[2].write(a);
      servo[5].write(b);
      servo[8].write(c);
      servo[11].write(d);
      delay(10);
    }
    stopaction();
  }
}

double legangleL(int anglehA, int anglehC, int anglelC)
{
  double angleh = abs(anglehA - anglehC);
  double radn = angleh * 0.9 * pi / 180;
  double h1 = cos(radn) * hipL;
  double h2 = H - h1;
  double angleleg = acos(h2 / legL) * 180 / pi;
  double svangleleg = angleleg / 0.9 + angleh + anglelC - 10;
  return svangleleg;
}

double legangleR(int anglehA, int anglehC, int anglelC)
{
  double angleh = abs(anglehA - anglehC);
  double radn = angleh * 0.9 * pi / 180;
  double h1 = cos(radn) * hipL;
  double h2 = H - h1;
  double angleleg = acos(h2 / legL) * 180 / pi;
  double svangleleg = anglelC - (angleleg / 0.9 + angleh) + 10;
  return svangleleg;
}

void runA() {
  //runA(),runB(),runC(),runD()是双脚离地步行姿态的四个运动阶段，，详见“姿态计算解析”
  Serial.println("A");
  for (int a = angleC25 + 22, b = angleC32 + 57, c = angleC13 - 54, d = angleC4 - 25; a < angleC25 + 37, b < angleC32 + 72, c > angleC13 - 69, d > angleC4 - 41; a++, b++, c--, d--)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i - 10); //-10使尾部抬高，使步行姿态更稳
      servo[11].write(j);
      servo[5].write(k + 10); //+10使尾部抬高，使步行姿态更稳
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void runB() {
  Serial.println("B");
  for (int a = angleC25 + 38, b = angleC32 + 72, c = angleC13 - 69, d = angleC4 - 41; a < angleC25 + 53, b > angleC32 + 25, c < angleC13 - 22, d > angleC4 - 57; a++, b = b - 3, c = c + 3, d--)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i - 10); //-10使尾部抬高，使步行姿态更稳
      servo[11].write(j + 10); //+10是此阶段这条腿需要离地前移
      servo[5].write(k); //此处本有+10使尾部抬高，与使腿离地前移的-10相抵消
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void runC() {
  Serial.println("C");
  for (int a = angleC25 + 54, b = angleC32 + 25, c = angleC13 - 22, d = angleC4 - 57; a < angleC25 + 69, b < angleC32 + 40, c > angleC13 - 38, d > angleC4 - 72; a++, b++, c--, d--)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i - 10); //-10使尾部抬高，使步行姿态更稳
      servo[11].write(j);
      servo[5].write(k + 10); //+10使尾部抬高，使步行姿态更稳
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void runD() {
  Serial.println("D");
  for (int a = angleC25 + 69, b = angleC32 + 41, c = angleC13 - 38, d = angleC4 - 72; a > angleC25 + 22, b < angleC32 + 56, c > angleC13 - 54, d < angleC4 - 25; a = a - 3, b++, c--, d = d + 3)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i); //此处本有-10使尾部抬高，与使腿离地前移的+10相抵消
      servo[11].write(j);
      servo[5].write(k + 10); //+10使尾部抬高，使步行姿态更稳
      servo[2].write(l - 10); //-10是此阶段这条腿需要离地前移
      delay(10);
    }
    stopaction();
  }
}

void walkA() {
  for (int a = angleC25 + 69, b = angleC32 + 57, c = angleC13 - 38, d = angleC4 - 25, e = angleC16; a > angleC25 + 22, b < angleC32 + 72, c > angleC13 - 54, d > angleC4 - 41, e > angleC16 - 32; a = a - 3, b++, c--, d--, e = e - 2)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      servo[12].write(e);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i + 10); //此处+10使此腿离地前移
      servo[11].write(j);
      servo[5].write(k);
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void walkB() {
  for (int a = angleC25 + 22, b = angleC32 + 72, c = angleC13 - 54, d = angleC4 - 41, e = angleC16 - 32; a < angleC25 + 37, b > angleC32 + 25, c > angleC13 - 69, d > angleC4 - 57, e < angleC16; a++, b = b - 3, c--, d--, e = e + 2)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      servo[12].write(e);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i);
      servo[11].write(j + 10); //此处+10使此腿离地前移
      servo[5].write(k);
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void walkC() {
  for (int a = angleC25 + 38, b = angleC32 + 25, c = angleC13 - 69, d = angleC4 - 57, e = angleC16; a < angleC25 + 53, b < angleC32 + 40, c < angleC13 - 22, d > angleC4 - 72, e < angleC16 + 32; a++, b++, c = c + 3, d--, e = e + 2)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      servo[12].write(e);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i);
      servo[11].write(j);
      servo[5].write(k - 10); //此处-10使此腿离地前移
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void walkD() {
  for (int a = angleC25 + 54, b = angleC32 + 41, c = angleC13 - 22, d = angleC4 - 72, e = angleC16 + 32; a < angleC25 + 69, b < angleC32 + 56, c > angleC13 - 38, d < angleC4 - 25, e > angleC16; a++, b++, c--, d = d + 3, e = e - 2)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      servo[12].write(e);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i);
      servo[11].write(j);
      servo[5].write(k);
      servo[2].write(l - 10); //此处-10使此腿离地前移
      delay(10);
    }
    stopaction();
  }
}
void runbackA() {
  //runback是后退，就是把run的四个阶段包括舵机控制值全部反过来即可
  for (int a = angleC25 + 69, b = angleC32 + 40, c = angleC13 - 38, d = angleC4 - 72; a > angleC25 + 54, b > angleC32 + 25, c < angleC13 - 22, d < angleC4 - 57; a--, b--, c++, d++)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i - 10); //-10使尾部抬高，使步行姿态更稳
      servo[11].write(j);
      servo[5].write(k + 10); //+10使尾部抬高，使步行姿态更稳
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void runbackB() {
  for (int a = angleC25 + 53, b = angleC32 + 25, c = angleC13 - 22, d = angleC4 - 57; a > angleC25 + 38, b < angleC32 + 72, c > angleC13 - 69, d < angleC4 - 41; a--, b = b + 3, c = c - 3, d++)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i - 10); //-10使尾部抬高，使步行姿态更稳
      servo[11].write(j + 10); //+10是此阶段这条腿需要离地前移
      servo[5].write(k); //此处本有+10使尾部抬高，与使腿离地前移的-10相抵消
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void runbackC() {
  for (int a = angleC25 + 37, b = angleC32 + 72, c = angleC13 - 69, d = angleC4 - 41; a > angleC25 + 22, b > angleC32 + 57, c < angleC13 - 54, d < angleC4 - 25; a--, b--, c++, d++)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i - 10); //-10使尾部抬高，使步行姿态更稳
      servo[11].write(j);
      servo[5].write(k + 10); //+10使尾部抬高，使步行姿态更稳
      servo[2].write(l);
      delay(10);
    }
    stopaction();
  }
}
void runbackD() {
  for (int a = angleC25 + 22, b = angleC32 + 56, c = angleC13 - 54, d = angleC4 - 25; a < angleC25 + 69, b > angleC32 + 41, c < angleC13 - 38, d > angleC4 - 72; a = a + 3, b--, c++, d = d - 3)
  {
    if (act == 1) {
      servo[7].write(a);
      servo[10].write(b);
      servo[4].write(c);
      servo[1].write(d);
      double i = legangleL(a, angleC25, angleC26);
      double j = legangleL(b, angleC32, angleC33);
      double k = legangleR(c, angleC13, angleC14);
      double l = legangleR(d, angleC4, angleC5);
      servo[8].write(i); //此处本有-10使尾部抬高，与使腿离地前移的+10相抵消
      servo[11].write(j);
      servo[5].write(k + 10); //+10使尾部抬高，使步行姿态更稳
      servo[2].write(l - 10); //-10是此阶段这条腿需要离地前移
      delay(10);
    }
    stopaction();
  }
}

void kick() {   //踢球
  stand(); //先转变到站立姿态
  svmovea(1, angleC4 - 75);
  svmoveb(4, angleC13 - 25);
  svmoveb(7, angleC25 + 65);
  svmovea(10, angleC32 + 35);
  stepwalk(); //原地踏步一次，主要是为了一个前脚向内移动过渡而用。直接将一个前脚向内移动会使机器人侧移
  servo[1].write(angleC4 - 10);
  delay(50);
  for (int a = angleC25 + 65; a > angleC25 + 25; a--) { //这一段就是一个前脚向内移动，以便在后面抬起另一个脚时作为重心支撑
    servo[0].write(angleC2 - 20);
    servo[2].write(angleC5 - 40);
    servo[7].write(a);
    double e = SlegangleL(a, angleC25, angleC26);
    servo[8].write(e);
    delay(10);
  }
  delay(1000);
  servo[10].write(155);
  servo[11].write(170);
  delay(1000);
  servo[11].write(120);
  servo[10].write(30);
  delay(1000);
  servo[10].write(angleC32 + 50);
  servo[11].write(angleC33 + 83);
}

void turnright() {  //右转向，采用原地踏步姿态，但需减小踏步步幅，同时改变肩部舵机位置从而达到转向的效果
  servo[0].write(angleC2);
  servo[3].write(angleC12);
  servo[6].write(angleC15);
  servo[9].write(angleC27);
  servo[12].write(angleC16 - 60);
  for (double i = angleC4 - 50, j = angleC32 + 35, k = angleC13 - 25, l = angleC25 + 40, n = angleC2, m = angleC12, p = angleC15, q = angleC27;
       i < angleC4 - 35, j < angleC32 + 50, k > angleC13 - 40, l > angleC25 + 25, n > angleC2 - 15, m < angleC12 + 15, p > angleC15 - 15, q < angleC27 + 15; i++, j++, k--, l--, n--, m++, p--, q++) {
    servo[1].write(i);
    servo[4].write(k);
    servo[7].write(l);
    servo[10].write(j);
    servo[0].write(n);
    servo[3].write(m);
    servo[6].write(p);
    servo[9].write(q);
    double a = SlegangleR(i, angleC4, angleC5);
    double b = SlegangleR(k, angleC13, angleC14);
    double c = SlegangleL(l, angleC25, angleC26);
    double d = SlegangleL(j, angleC32, angleC33);
    servo[2].write(a);
    servo[5].write(b);
    servo[8].write(c);
    servo[11].write(d);
    delay(25);
  }
  for (double i = angleC4 - 35, j = angleC32 + 50, k = angleC13 - 40, l = angleC25 + 25, n = angleC2 - 15, m = angleC12 + 15, p = angleC15 - 15, q = angleC27 + 15; i > angleC4 - 50, j > angleC32 + 35,
       k < angleC13 - 25, l < angleC25 + 40, n<angleC2, m>angleC12, p<angleC15, q>angleC27; i--, j--, k++, l++, n++, m--, p++, q--) {
    servo[1].write(i);
    servo[4].write(k);
    servo[7].write(l);
    servo[10].write(j);
    servo[0].write(angleC2 - 15);
    servo[3].write(m);
    servo[6].write(angleC15 - 15);
    servo[9].write(q);
    double a = SlegangleR(i, angleC4, angleC5);
    double b = SlegangleR(k, angleC13, angleC14);
    double c = SlegangleL(l, angleC25, angleC26);
    double d = SlegangleL(j, angleC32, angleC33);
    servo[2].write(a);
    servo[5].write(b);
    servo[8].write(c);
    servo[11].write(d);
    delay(25);
  }
}
void turnleft() { //左转向，采用原地踏步姿态，但需减小踏步步幅，同时改变肩部舵机位置从而达到转向的效果
  servo[0].write(angleC2);
  servo[3].write(angleC12);
  servo[6].write(angleC15);
  servo[9].write(angleC27);
  servo[12].write(angleC16 + 60);
  for (double i = angleC4 - 50, j = angleC32 + 35, k = angleC13 - 25, l = angleC25 + 40, n = angleC2 - 15, p = angleC15 - 15; i < angleC4 - 35, j < angleC32 + 50, k > angleC13 - 40, l > angleC25 + 25, n < angleC2, p < angleC15; i++, j++, k--, l--, n++, p++) {
    servo[1].write(i);
    servo[4].write(k);
    servo[7].write(l);
    servo[10].write(j);
    servo[0].write(n);
    servo[3].write(angleC12 + 15);
    servo[6].write(p);
    servo[9].write(angleC27 + 15);
    double a = SlegangleR(i, angleC4, angleC5);
    double b = SlegangleR(k, angleC13, angleC14);
    double c = SlegangleL(l, angleC25, angleC26);
    double d = SlegangleL(j, angleC32, angleC33);
    servo[2].write(a);
    servo[5].write(b);
    servo[8].write(c);
    servo[11].write(d);
    delay(25);
  }
  for (double i = angleC4 - 35, j = angleC32 + 50, k = angleC13 - 40, l = angleC25 + 25, n = angleC2, m = angleC12, p = angleC15, q = angleC27 ; i > angleC4 - 50, j > angleC32 + 35, k < angleC13 - 25, l < angleC25 + 40, n > angleC2 - 15, m < angleC12 + 15, p > angleC15 - 15, q < angleC27 + 15; i--, j--, k++, l++, n--, m++, p--, q++) {
    servo[1].write(i);
    servo[4].write(k);
    servo[7].write(l);
    servo[10].write(j);
    servo[0].write(n);
    servo[3].write(m);
    servo[6].write(p);
    servo[9].write(q);
    double a = SlegangleR(i, angleC4, angleC5);
    double b = SlegangleR(k, angleC13, angleC14);
    double c = SlegangleL(l, angleC25, angleC26);
    double d = SlegangleL(j, angleC32, angleC33);
    servo[2].write(a);
    servo[5].write(b);
    servo[8].write(c);
    servo[11].write(d);
    delay(25);
  }
}
void turnpreparation() { //转向预备，机器人脚部腿部舵机变到转向踏步起始位置
  if (servo[1].read() < angleC4 - 35) {
    svmoveb(1, angleC4 - 35);
  } else if (servo[1].read() > angleC4 - 35) {
    svmovea(1, angleC4 - 35);
  }
  if (servo[2].read() < SlegangleR(angleC4 - 35, angleC4, angleC5)) {
    svmoveb(2, SlegangleR(angleC4 - 35, angleC4, angleC5));
  } else if (servo[2].read() > SlegangleR(angleC4 - 35, angleC4, angleC5)) {
    svmovea(2, SlegangleR(angleC4 - 35, angleC4, angleC5));
  }
  if (servo[4].read() < angleC13 - 25) {
    svmoveb(4, angleC13 - 25);
  } else if (servo[4].read() > angleC13 - 25) {
    svmovea(4, angleC13 - 25);
  }
  if (servo[5].read() < SlegangleR(angleC13 - 25, angleC13, angleC14)) {
    svmoveb(5, SlegangleR(angleC13 - 25, angleC13, angleC14));
  } else if (servo[5].read() > SlegangleR(angleC13 - 25, angleC13, angleC14)) {
    svmovea(5, SlegangleR(angleC13 - 25, angleC13, angleC14));
  }
  if (servo[7].read() < angleC25 + 25) {
    svmoveb(7, angleC25 + 25);
  } else if (servo[7].read() > angleC25 + 25) {
    svmovea(7, angleC25 + 25);
  }
  if (servo[8].read() < SlegangleL(angleC25 + 25, angleC25, angleC26)) {
    svmoveb(8, SlegangleL(angleC25 + 25, angleC25, angleC26));
  } else if (servo[8].read() > SlegangleL(angleC25 + 25, angleC25, angleC26)) {
    svmovea(8, SlegangleL(angleC25 + 25, angleC25, angleC26));
  }
  if (servo[10].read() < angleC32 + 35) {
    svmoveb(10, angleC32 + 35);
  } else if (servo[10].read() > angleC32 + 35) {
    svmovea(10, angleC32 + 35);
  }
  if (servo[11].read() < SlegangleL(angleC32 + 35, angleC32, angleC33)) {
    svmoveb(11, SlegangleL(angleC32 + 35, angleC32, angleC33));
  } else if (servo[11].read() > SlegangleL(angleC32 + 35, angleC32, angleC33)) {
    svmovea(11, SlegangleL(angleC32 + 35, angleC32, angleC33));
  }
}

void sit() //坐姿，从坐姿转到其他站立姿态时由于尾部太重舵机无法自己撑起来，需要人手帮助
{
  stand();
  delay(500);
  servo[3].write(angleC12);
  servo[6].write(angleC15);
  for (double a = angleC13 - 45, b = angleC14 - 73, c = angleC25 + 45, d = angleC26 + 73; a > angleC13 - 75, b > angleC14 - 133, c < angleC25 + 75, d < angleC26 + 133; a--, b = b - 2, c++, d = d + 2)
  {
    servo[4].write(a);
    servo[5].write(b);
    servo[7].write(c);
    servo[8].write(d);
    delay(20);
  }
  delay(500);
  servo[0].write(angleC2 - 10);
  servo[1].write(angleC4 - 30);
  servo[9].write(angleC27 + 10);
  servo[10].write(angleC32 + 30);
  for (double i = servo[2].read(), j = servo[11].read(); i < angleC5, j>angleC33; i++, j--)
  {
    servo[2].write(i);
    servo[11].write(j);
    delay(10);
  }
}


void situp()
{
  for (double i = angleC5, j = angleC33; i > angleC5 - 85, j < angleC33 + 85; i--, j++)
  {
    servo[2].write(i);
    servo[11].write(j);
    delay(10);
  }
  for (double a = angleC13 - 75, b = angleC14 - 133, c = angleC25 + 75, d = angleC26 + 133; a < angleC13 - 45, b < angleC14 - 73, c > angleC25 + 45, d > angleC26 + 73; a++, b = b + 2, c--, d = d - 2)
  {
    servo[4].write(a);
    servo[5].write(b);
    servo[7].write(c);
    servo[8].write(d);
    delay(20);
  }
}

void shakehands() //坐姿握手，从坐姿转到其他站立姿态时由于尾部太重舵机无法自己撑起来，需要人手帮助
{
  sit();
  servo[9].write(angleC27 + 20);
  delay(300);
  for (double i = 50; i < 130; i++) {
    servo[1].write(i);
    delay(5);
  }
  delay(300);
  for (double k = angleC2; k > 51; k--) {
    servo[0].write(k);
    delay(10);
  }
  delay(300);
  for (int j = angleC5; j > 50; j--) {
    servo[2].write(j);
    delay(5);
  }
}

void updownpreparation() {
  for (int a = angleC4 - 50, b = angleC5 - 91, c = angleC13 - 55, d = angleC14 - 75, e = angleC25 + 50, f = angleC26 + 83, g = angleC32 + 45, h = angleC33 + 77;
       a < angleC4 - 30, b < angleC5 - 51, c < angleC13 - 35, d < angleC14 - 35, e > angleC25 + 30, f > angleC26 + 43, g > angleC32 + 25, h > angleC33 + 37; a++, b = b + 2, c++, d = d + 2, e--, f = f - 2, g--, h = h - 2) {
    servo[1].write(a);
    servo[2].write(b);
    servo[4].write(c);
    servo[5].write(d);
    servo[7].write(e);
    servo[8].write(f);
    servo[10].write(g);
    servo[11].write(h);
    delay(20);
  }
}

void updown() {
  for (int a = angleC4 - 30, b = angleC5 - 51, c = angleC13 - 35, d = angleC14 - 35, e = angleC25 + 30, f = angleC26 + 43, g = angleC32 + 25, h = angleC33 + 37;
       a > angleC4 - 70, b > angleC5 - 131, c > angleC13 - 75, d > angleC14 - 115, e < angleC25 + 70, f < angleC26 + 123, g < angleC32 + 65, h < angleC33 + 117; a--, b = b - 2, c--, d = d - 2, e++, f = f + 2, g++, h = h + 2) {
    if (act == 1) {
      servo[1].write(a);
      servo[2].write(b);
      servo[4].write(c);
      servo[5].write(d);
      servo[7].write(e);
      servo[8].write(f);
      servo[10].write(g);
      servo[11].write(h);
      delay(20);
    }
    stopaction();
  }

  for (int a = angleC4 - 70, b = angleC5 - 131, c = angleC13 - 75, d = angleC14 - 115, e = angleC25 + 70, f = angleC26 + 123, g = angleC32 + 65, h = angleC33 + 117;
       a < angleC4 - 30, b < angleC5 - 51, c < angleC13 - 35, d < angleC14 - 35, e > angleC25 + 30, f > angleC26 + 43, g > angleC32 + 25, h > angleC33 + 37; a++, b = b + 2, c++, d = d + 2, e--, f = f - 2, g--, h = h - 2) {
    if (act == 1) {
      servo[1].write(a);
      servo[2].write(b);
      servo[4].write(c);
      servo[5].write(d);
      servo[7].write(e);
      servo[8].write(f);
      servo[10].write(g);
      servo[11].write(h);
      delay(20);
    }
    stopaction();
  }
}

void swingpreparation() {
  for (double a = angleC2, b = angleC12, c = angleC15, d = angleC27; a < angleC2 + 12, b > angleC12 - 6, c > angleC15 - 6, d < angleC27 + 12; a++, b = b - 0.5, c = c - 0.5, d++) {
    servo[0].write(a);
    servo[3].write(b);
    servo[6].write(c);
    servo[9].write(d);
    delay(20);
  }
}

void swing() {

  for (double a = angleC2 + 12, b = angleC12 - 6, c = angleC15 - 6, d = angleC27 + 12; a > angleC2 - 28, b < angleC12 + 14, c < angleC15 + 14, d > angleC27 - 28; a--, b = b + 0.5, c = c + 0.5, d--) {
    if (act == 1) {
      servo[0].write(a);
      servo[3].write(b);
      servo[6].write(c);
      servo[9].write(d);
      delay(20);
    }
    stopaction();
  }
  for (double a = angleC2 - 28, b = angleC12 + 14, c = angleC15 + 14, d = angleC27 - 28; a < angleC2 + 12, b > angleC12 - 6, c > angleC15 - 6, d < angleC27 + 12; a++, b = b - 0.5, c = c - 0.5, d++) {
    if (act == 1) {
      servo[0].write(a);
      servo[3].write(b);
      servo[6].write(c);
      servo[9].write(d);
      delay(20);
    }
    stopaction();
  }
}
