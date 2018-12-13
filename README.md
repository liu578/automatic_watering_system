# automatic_watering_system

-----

* designed controller board, motor driver board and power supply circuits

* used SHT10 digital humidity sensor for temperature and humidity sampling, L298N for motor driving

* wrote C codes for MCU 8051 for sensors, motors and display

-----

以STC89C52单片机为控制电路的核心，采用模块化的设计方案。采用LM2596开关电源芯片为核心的开关电源模块为整个系统供电。利用SHT10湿度采集模块将检测到的植物生长环境中的湿度数据传送给单片机，单片机将采集到的数据与所设定植物生长环境的警戒值进行比较；若采集数据低于警戒值，则启动L298N模块驱动直流电机进行灌溉；若采集数据高于警戒值， L298N驱动模块将自动停止灌溉。LCD12864液晶显示模块将采集数据、灌溉信息以及操作模式显示实时显示在液晶显示屏上。在时间控制方式下，用户可通过需要自行设定键盘输入进行灌溉的时间，按动控制确定的灌溉按键则按照设定的时间进行灌溉，时间自减为0的时候则自动停止灌溉。
