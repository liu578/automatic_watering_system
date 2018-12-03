#include "reg52.h"
#include <intrins.h>
#include <stdlib.h>   
#include <stdio.h>
#include <string.h>
#include <math.h> 

#define uchar unsigned char
#define uint unsigned int

typedef union 
{ unsigned int i;
  float f;
} value;

enum {TEMP,HUMI};

sbit DATA = P3^6;
sbit SCK = P3^7;

#define noACK 0
#define ACK   1
                           
#define STATUS_REG_W 0x06   //000   0011    0
#define STATUS_REG_R 0x07   //000   0011    1
#define MEASURE_TEMP 0x03   //000   0001    1
#define MEASURE_HUMI 0x05   //000   0010    1
#define RESET        0x1e   //000   1111    0

/*****ULN2003控制引脚*****/
sbit fu = P2^0;
sbit zheng = P2^1; 

/*****LCD控制引脚*****/        
sbit CS =P2^5;	//片选信号
sbit SID=P2^6;	//串行数据 
sbit SCK_LCD=P2^7;	//串行同步时钟 

/*****LCD功能初始化指令*****/
#define CLEAR_SCREEN 	0x01   //清屏指令：清屏且AC值为00H
#define AC_INIT   		   0x02   //将AC设置为00H。且游标移到原点位置
#define CURSE_ADD  		0x06   //设定游标移到方向及图像整体移动方向（默认游标右移，图像整体不动）
#define FUN_MODE  		0x30   //工作模式：8位基本指令集
#define DISPLAY_ON   	0x0c   //显示开,显示游标，且游标位置反白
#define DISPLAY_OFF  	0x08   //显示关
#define CURSE_DIR  		0x14   //游标向右移动:AC=AC+1
#define SET_CG_AC  		0x40   //设置AC，范围为：00H~3FH
#define SET_DD_AC  		0x80

/*****汉字地址表*****/        
unsigned char code AC_TABLE[]={ 
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,      //第一行汉字位置 
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,      //第二行汉字位置 
0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,      //第三行汉字位置 
0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,      //第四行汉字位置 
}; 

uchar code dis00[] = {"模拟自动浇灌系统"};
uchar code dis01[] = {"单片机课程设计"};
uchar code dis02[] = {"学号:1100220319"};
uchar code dis03[] = {"欢迎使用........"};

uchar code dis1[] = {"温度: 50.0  ℃"};
uchar code dis2[] = {"湿度: 50.0  %RH"};
uchar code dis3[] = {"自动: Y 手动: N"};
uchar code dis4[] = {"浇灌时间：  分钟"};
uchar code error1[] = {"请在自动模式下选择此项操作！"};
uchar code error2[] = {"请在手动模式下选择此项操作！"};
uchar code change_OK[] = {"修改成功！"};
uchar code auto_mode[] = {"选择自动模式！"};
uchar code manual_mode[] = {"选择手动模式！"};

uchar var_buf[10];
uchar mod_flag = 0;	      //定义设置模式，当mod_flag = 0为进入自动模式下；mod_flag = 1进入手动模式；
uchar time_dianji = 30;	  //定义设置电机初始化时间为30分钟
uchar time_var = 30;
uint time0_count;

/*****（ms）延时子程序*****/
void delay(unsigned int t)//约延时t（ms）
{
	unsigned int x;
	unsigned int y;	
	for(x=t;x>0;x--)
		for(y=110;y>0;y--);
}  
/*****串行发送一个字节*****/ 
void SendByte(unsigned char Dbyte) 
{ 
     unsigned char i; 
     for(i=0;i<8;i++) 
     { 
           SCK_LCD = 0; 
           Dbyte=Dbyte<<1;      //左移一位 
           SID = CY;            //移出的位给SID 
           SCK_LCD = 1; 
           SCK_LCD = 0; 
     } 
} 
/*****串行接收一个字节*****/ 
//仅在读取数据的时候用到 
//而读出的数据是一次只能读出4bit的 
unsigned char ReceiveByte(void) 
{ 
     unsigned char i,temp1,temp2; 
     temp1=temp2=0; 
     for(i=0;i<8;i++) 
     { 
           temp1=temp1<<1; 
           SCK_LCD = 0; 
           SCK_LCD = 1;             
           SCK_LCD = 0; 
           if(SID) temp1++; 
     } 
     for(i=0;i<8;i++) 
     { 
           temp2=temp2<<1; 
           SCK_LCD = 0; 
           SCK_LCD = 1; 
           SCK_LCD = 0; 
           if(SID) temp2++; 
     } 
     return ((0xf0&temp1)+(0x0f&temp2)); 
}
/*****检测LCD忙状态*****/ 
void CheckBusy( void ) 
{ 
     do   SendByte(0xfc);      //11111,RW(1),RS(0),0 
     while(0x80&ReceiveByte());      //BF(.7)=1 Busy 
}
/*****写指令*****/ 
void WriteCommand( unsigned char Cbyte ) 
{ 
     CS = 1; 
     CheckBusy(); 
     SendByte(0xf8);            //11111,RW(0),RS(0),0 
     SendByte(0xf0&Cbyte);      //高四位 
     SendByte(0xf0&Cbyte<<4);	//低四位(先执行<<) 
     CS = 0; 
}
/*****写数据*****/ 
void WriteData( unsigned char Dbyte ) 
{ 
     CS = 1; 
     CheckBusy(); 
     SendByte(0xfa);            //11111,RW(0),RS(1),0 
     SendByte(0xf0&Dbyte);      //高四位 
     SendByte(0xf0&Dbyte<<4);	//低四位(先执行<<) 
     CS = 0; 
}    
/*****显示汉字*****/
void PutStr(unsigned char row,unsigned char col,unsigned char *puts) 
{ 
     WriteCommand(0x30);      //8BitMCU,基本指令集合 
     WriteCommand(AC_TABLE[8*row+col]);      //起始位置 
     while(*puts != '\0')      //判断字符串是否显示完毕 
     { 
           if(col==8)            //判断换行 
           {            		 //若不判断,则自动从第一行到第三行 
                 col=0; 
                 row++; 
           } 
           if(row==4) row=0;      //一屏显示完,回到屏左上角 
           WriteCommand(AC_TABLE[8*row+col]); 
           WriteData(*puts);      //一个汉字要写两次 
           puts++; 
           WriteData(*puts); 
           puts++; 
           col++; 
     } 
}
/*****显示时间*****/
void display_time()
{
	memset(var_buf,'\0',sizeof(var_buf));
	var_buf[0] = time_dianji/10 + '0';
	var_buf[1] = time_dianji%10 + '0'; 
	PutStr(3,5,var_buf);
} 
/*****初始化LCD*****/
void LcdInit( void ) 
{ 
    WriteCommand(0x30);      //8BitMCU,基本指令集合 
    WriteCommand(0x03);      //AC归0,不改变DDRAM内容 
    WriteCommand(0x0C);      //显示ON,游标OFF,游标位反白OFF 
    WriteCommand(0x01);      //清屏,AC归0 
    WriteCommand(0x06);      //写入时,游标右移动 

	PutStr(0,0,dis00);		 //初始化自己的显示
	PutStr(1,0,dis01);
	PutStr(1,0,dis02);
	PutStr(2,0,dis03);
	delay(5000);

}

/*****自动控制*****/
void auto_control()
{
	memset(var_buf,'\0',sizeof(var_buf));	//设置
	sprintf(var_buf,": Y",3);
	PutStr(2,2,var_buf);
	memset(var_buf,'\0',sizeof(var_buf));
	sprintf(var_buf,": N",3);
	PutStr(2,6,var_buf);
	mod_flag = 0;
}
/*****手动控制*****/
void manual_control()
{
	memset(var_buf,'\0',sizeof(var_buf));
	sprintf(var_buf,": N",3);
	PutStr(2,2,var_buf);
	memset(var_buf,'\0',sizeof(var_buf));
	sprintf(var_buf,": Y",3);
	PutStr(2,6,var_buf);
	mod_flag = 1;
}

void show_string()
{
	WriteCommand(0x01);      //清屏,AC归0
    WriteCommand(0x06);      //写入时,游标右移动
	PutStr(0,0,dis1);		 //初始化自己的显示
	PutStr(1,0,dis2);
	PutStr(2,0,dis3);
	PutStr(3,0,dis4);
	if(mod_flag == 0)
	{
	   auto_control();
	}
	else if(mod_flag == 1)
	{
	   manual_control();
	}
	display_time();
}

void show_information(uchar i)
{
	WriteCommand(0x01);      //清屏
	WriteCommand(0x06);      //写入时,游标右移动
	switch(i)
	{
	   	case 1:
	   		PutStr(0,0,error2);
			break;
		case 2:
	   		PutStr(0,0,error1);
			break;
		case 3:
	   		PutStr(0,0,change_OK);
			break;
	}
	delay(1000);
	show_string();
}
/*****获取按键值*****/
void get_key()
{
	uchar temp;
	uchar key = 0;
	temp = P1;
	temp &= 0xff; 
	if(temp != 0xff)
	{
		delay(5);	//按键消除抖动
		temp = P1;
		temp &= 0xff; 
		if(temp != 0xff)
		{
			temp = P1;
			switch(temp)
			{
				case 0xfe:	key = 1;	break;
				case 0xfd:	key = 2;	break;
				case 0xfb:	key = 3;	break;
				case 0xf7:	key = 4;	break;
				case 0xef:	key = 5;	break;
				case 0xdf:	key = 6;	break;
				case 0xbf:	key = 7;	break;
				case 0x7f:	key = 8;	break;
			}
			while(temp != 0xff)//判断按键是否释放
			{
				temp = P1;
				temp &= 0xff;
			}
		}
	}
	switch(key)
	{
		case 1:
			WriteCommand(0x01);      //清屏
			WriteCommand(0x06);      //写入时,游标右移动
			PutStr(0,0,auto_mode);		//初始化自己的显示
			delay(1000);
			show_string();
			auto_control();
			break;
		case 2:
			WriteCommand(0x01);       //清屏
			WriteCommand(0x06);       //写入时,游标右移动
			PutStr(0,0,manual_mode);  //初始化自己的显示
			delay(1000);
			show_string();
			manual_control();
			break;
		case 3:
			if(mod_flag == 1)	
				TR1 = 1;
			else
			{
				show_information(1);
			}
			break;
		case 4:
			TR1 = 0;
			TR0 = 0;
			zheng = 0;
			break;
		case 5:
			if(mod_flag == 0)
			{
				show_information(1);
			}
			else
			{
				time_dianji++;
				if(time_dianji == 60)
					time_dianji = 0;
				display_time();
			}
			break;
		case 6:
			if(mod_flag == 0)
			{
				show_information(1);
			}
			else
			{
				time_dianji--;
				if(time_dianji == 0)
					time_dianji = 60;
				display_time();
			}
			break;
		case 7:
			if(mod_flag == 0)
			{
				show_information(1);				
			}
			else
			{
				time_var = time_dianji;
				show_information(3);
			}
			break;
		case 8:
			if(mod_flag == 0)
			{	
				show_information(1);
			}
			else
			{
				TR0 = 1;
				TR1 = 1;
			}
			break;
		default:
			break;
	}
}
/*****定时器0初始化*****/
void time0_init()
{
	TMOD |= 0X01;
	TH0 = (65536-60000)/256;
	TL0 = (65536-60000)%256;
	ET0 = 1;
	TR0 = 0;
}
/*****定时器1初始化*****/
void time1_init()
{
	TMOD |= 0X10;
	TH1 = (65536-30000)/256;
	TL1 = (65536-30000)%256;
	ET1 = 1;
	TR1 = 0;
}

//----------------------------------------------------------------------------------
char s_write_byte(unsigned char value)
//----------------------------------------------------------------------------------
// writes a byte on the Sensibus and checks the acknowledge 
{ 
  unsigned char i,error=0;  
  for (i=0x80;i>0;i/=2)             //shift bit for masking
  { if (i & value) DATA=1;          //masking value with i , write to SENSI-BUS
    else DATA=0;                        
    SCK=1;                          //clk for SENSI-BUS
    _nop_();_nop_();_nop_();        //pulswith approx. 5 us  	
    SCK=0;
  }
  DATA=1;                           //release DATA-line
  SCK=1;                            //clk #9 for ack 
  error=DATA;                       //check ack (DATA will be pulled down by SHT11)
  SCK=0;        
  return error;                     //error=1 in case of no acknowledge
}

//----------------------------------------------------------------------------------
char s_read_byte(unsigned char ack)
//----------------------------------------------------------------------------------
// reads a byte form the Sensibus and gives an acknowledge in case of "ack=1" 
{ 
  unsigned char i,val=0;
  DATA=1;                           //release DATA-line
  for (i=0x80;i>0;i/=2)             //shift bit for masking
  { SCK=1;                      //clk for SENSI-BUS
    if (DATA) val=(val | i);         //read bit  
    SCK=0;  					 
  }
  DATA=!ack;                  //in case of "ack==1" pull down DATA-Line
  SCK=1;                      //clk #9 for ack
  _nop_();_nop_();_nop_();         //pulswith approx. 5 us 
  SCK=0;						    
  DATA=1;                      //release DATA-line
  return val;
}
void s_transstart(void)
{  
   DATA=1; SCK=0;                   //Initial state
   _nop_();
   SCK=1;
   _nop_();
   DATA=0;
   _nop_();
   SCK=0;  
   _nop_();_nop_();_nop_();
   SCK=1;
   _nop_();
   DATA=1;		   
   _nop_();
   SCK=0;		   
}
void s_connectionreset(void)
{  
  unsigned char i; 
  DATA=1; SCK=0;                    //Initial state
  for(i=0;i<9;i++)                  //9 SCK cycles
  { SCK=1;
    SCK=0;
  }
  s_transstart();                   //transmission start
}
char s_measure(unsigned char *p_value, unsigned char *p_checksum, 
unsigned char mode)
{ 
  unsigned error=0;
  unsigned int i;

  s_transstart();                   //transmission start
  switch(mode){                     //send command to sensor
    case TEMP	: error+=s_write_byte(MEASURE_TEMP); break;
    case HUMI	: error+=s_write_byte(MEASURE_HUMI); break;
    default     : break;	 
  }
  for (i=0;i<65535;i++) if(DATA==0) break; //wait until sensor has finished the measurement
  if(DATA) error+=1;                // or timeout (~2 sec.) is reached
  *(p_value)  =s_read_byte(ACK);    //read the first byte (MSB)
  *(p_value+1)=s_read_byte(ACK);    //read the second byte (LSB)
  *p_checksum =s_read_byte(noACK);  //read checksum
  return error;
}
//----------------------------------------------------------------------------------------
void calc_sth11(float *p_humidity ,float *p_temperature)
//----------------------------------------------------------------------------------------
{ const float C1=-4.0;              // for 12 Bit
  const float C2=+0.0405;           // for 12 Bit
  const float C3=-0.0000028;        // for 12 Bit
  const float T1=+0.01;             // for 14 Bit @ 5V
  const float T2=+0.00008;           // for 14 Bit @ 5V	

  float rh=*p_humidity;             // rh:      Humidity [Ticks] 12 Bit 
  float t=*p_temperature;           // t:       Temperature [Ticks] 14 Bit
  float rh_lin;                     // rh_lin:  Humidity linear
  float rh_true;                    // rh_true: Temperature compensated humidity
  float t_C;                        // t_C   :  Temperature [癈]

  t_C=t*0.01 - 40;                  //calc. temperature from ticks to [癈]
  rh_lin=C3*rh*rh + C2*rh + C1;     //calc. humidity from ticks to [%RH]
  rh_true=(t_C-25)*(T1+T2*rh)+rh_lin;   //calc. temperature compensated humidity [%RH]
  if(rh_true>100)rh_true=100;       //cut if the value is outside of
  if(rh_true<0.1)rh_true=0.1;       //the physical possible range

  *p_temperature=t_C;               //return temperature [癈]
  *p_humidity=rh_true;              //return humidity[%RH]
}

//--------------------------------------------------------------------
float calc_dewpoint(float h,float t)
//--------------------------------------------------------------------
{ float logEx,dew_point;
  logEx=0.66077+7.5*t/(237.3+t)+(log10(h)-2);
  dew_point = (logEx - 0.66077)*237.3/(0.66077+7.5-logEx);
  return dew_point;
}

/*****主函数*****/
void main( void ) 
{ 
	value humi_val,temp_val;
  	float dew_point;
  	unsigned char error,checksum;
		zheng = 0;
	fu = 0;
	time0_init();
	time1_init();
	EA = 1;
	LcdInit();
	show_string();
	s_connectionreset(); 
	while(1)
	{
		get_key();
		error=0;
		
		

			
     	error+=s_measure((unsigned char*) &humi_val.i,&checksum,HUMI);  //measure humidity
    	error+=s_measure((unsigned char*) &temp_val.i,&checksum,TEMP);  //measure temperature
				
			
		
    	if(error!=0) s_connectionreset();                 //in case of an error: connection reset
    	else
    	{ 
			humi_val.f=(float)humi_val.i;                   //converts integer to float
      		temp_val.f=(float)temp_val.i;                   //converts integer to float
      		calc_sth11(&humi_val.f,&temp_val.f);            //calculate humidity, temperature
      		dew_point=calc_dewpoint(humi_val.f,temp_val.f); //calculate dew point
			humi_val.f -= 50;
			memset(var_buf,'\0',sizeof(var_buf));
			sprintf(var_buf,"%3.1f",humi_val.f);
			PutStr(1,3,var_buf);
			memset(var_buf,'\0',sizeof(var_buf));
			sprintf(var_buf,"%3.1f",temp_val.f);
			PutStr(0,3,var_buf);
    	}
			if(mod_flag == 0)
		{
			if(humi_val.f <= 30)	//设置自动浇灌的湿度阀值
			{
				TR1 = 1;
			}
			else
			{
				TR1 = 0;
				zheng = 0;
			}
		}
		if(mod_flag == 1)
		{
			if(time_var == 0)
			{	
				TR0 = 0;
				TR1 = 0;//等于0是关闭电机
				zheng = 0;
			}
		}
	} 
}
/*****定时器0中断服务程序*****/
void time0_isr() interrupt 1
{
	TH0 = (65536-60000)/256;
	TL0 = (65536-60000)%256;
	time0_count++;
	if(time0_count == 1000)
	{
		time0_count = 0;
		time_var--;
	}
}
/*****定时器1中断服务程序*****/
void time1_isr() interrupt 3
{
	TH1 = (65536-30000)/256;
	TL1 = (65536-30000)%256;
	if(zheng==0)
	zheng =1;
} 

