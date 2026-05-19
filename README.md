A project that uses a matrix keyboard to control a servo motor
PCB9685板子连接：
绿色端子接5v供电
SCL-PB6
SDA-PB7
OE GND 共地
VCC 3V
各个舵机分别接入PCB9685板子的pwm口上
键盘键值        D #  0  *
               C 9  8  7 
		           B 6  5  4 
		           A 3  2  1
 1~6 负责选中舵机
 长按A舵机往正方向运动3°每秒
 长按B舵机往负方向运动速度同上
 后续可以加入其它功能
 OLED负责显示选中几号舵机以及当前角度
 目前机械臂初始状态还没设置

              
