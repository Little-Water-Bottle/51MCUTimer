#include "reg52.h"
#include "intrins.h"

typedef unsigned int u16;	  //对数据类型进行声明定义
typedef unsigned char u8;

//定义P0口的三个引脚，赋予不同的涵义
sbit SER = P0^1;    //p0.1脚控制串行数据输入
sbit SCK = P0^0;    //串行输入时钟
sbit RCK = P0^2;    //存储寄存器时钟

//buton键定义都可以省了，直接通过 位判断
//sbit button1 = P2^0; 	//设置/确认按键
//sbit button2 = P2^1;	//加按键
//sbit button3 = P2^2;	//减按键
//sbit button4 = P2^3;	//查看日期按键	暂时没有实现

//定义按键按下的次数，不同次数选择不同设置
static char button_num1 = 0; 	//判断选则时分秒
static char button_num2 = 0;	//判断切换时间/日期		//还没有处理日期设置和月天数判断
static  char button_flag = 0x00;	//按键选择位标志，通过按键扫描的返回值判断按下的是哪个按键，有防连按功能

u8 code smgduan[17]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,
					0x7f,0x6f,0x77,0x7c,0x39,0x5e,0x79,0x71};//显示0~F的值
char duanMa[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};//选择1-8哪个数码管段
char duanZhi[8]={0,0,0x40,0,0,0x40,0,0};	 	//保存8个数码管的中每个数码管段的数值 0x40:显示横杠

//num1:秒初始值 num2:分初始值 num3:时初始值
u16 num1=55,num2=59,num3=23;
u16 year=99,month=12,day=29; 

u8 day_flag = 0;	//一个月多少天的标志 flag=0 30一个月 flag=1 31一个月
u8 setting_flag = 0;	//进入设置位标志，进入后相应数字位闪烁 0：不闪烁	1：闪烁
//u8 day_time_flag = 0;	//显示时间/日期标志	0：显示时间	1：显示日期

static int i = 0;	//给中断计数使用

//函数声明
void SendTo595(char byteData);


/***********************************************************
*函数名		：delay_us
*功能		：延时
*参数		：i 延时的us数
************************************************************/
//void delay_us(int i)
//{
//	while(i--);
//}

/***********************************************************
*函数名		：display
*功能		：对传如的时分秒进行处理计算，转化为七段数码管要显示的值
*参数		：num1 秒	num2 分	  num3 时
************************************************************/
void display(u16 shi,u16 fen,u16 miao,u16 year,u16 month,u16 day)
{
	//先发送8位位码，后发送8位段码
	//8位数码管需要发送8次
	char i=0;	//给单片机for循环使用，由于Keil4 把 变量定义放for里会报错，只能放函数体前面
	static char shi1,ge1,shi2,ge2,shi3,ge3;

	//////////分离每个时间数字的个位和十位/////////////////
	if(button_num2 == 0)
	{
		shi1=(char)shi/10;
		ge1=(char)shi%10;
	
		shi2=(char)fen/10;
		ge2=(char)fen%10;
	
		shi3=(char)miao/10;
		ge3=(char)miao%10;	
	}
	//////////分离每个日期数字的个位和十位/////////////////
	else if(button_num2 == 1)
	{
		shi1=(char)year/10;
		ge1=(char)year%10;
	
		shi2=(char)month/10;
		ge2=(char)month%10;
	
		shi3=(char)day/10;
		ge3=(char)day%10;	
	}
	////////////////////////=======/////////////////////////

	//////////保存段值/////////////////
	duanZhi[0]=smgduan[shi1];
	duanZhi[1]=smgduan[ge1];
	duanZhi[3]=smgduan[shi2];
	duanZhi[4]=smgduan[ge2];
	duanZhi[6]=smgduan[shi3];
	duanZhi[7]=smgduan[ge3];	
	///////////=======////////////////

	//处理设置位的闪烁
	switch(button_num1)
	{
		case 0: break;
		case 1:	
		if(setting_flag ==1)  //被设置位灭0.5秒
		{
			duanZhi[0]=0;
			duanZhi[1]=0;
		} 
		break;	
		case 2:	
		if(setting_flag == 1) 
		{
			duanZhi[3]=0;
			duanZhi[4]=0;
		} 
		break;
		case 3: 
		if(setting_flag == 1)
		{
			duanZhi[6]=0;
			duanZhi[7]=0;
		} 		 
		break;
		default: break;
	}


	i=0;
 	for(;i<8;i++)	
	{
		SendTo595(~duanMa[i]); 		//送段码
		SendTo595(duanZhi[i]);		//送位码
		
		/*位移寄存器数据准备完毕,转移到存储寄存器*/
	   RCK = 0;         
	   _nop_();
	   _nop_();
	   RCK = 1; 	   //检测到上升沿，让存储寄存器时钟变为高电平
	}		
}


/*******************************************************************************
* 函 数 名         : TimerInit
* 函数功能		   : 定时器0初始化
* 参数			   ：无
*******************************************************************************/
void TimerInit()
{
	TMOD|=0X01;	//选择为定时器0模式，工作方式1，仅用TR0打开启动。
	TH0=0X3C;	//给定时器赋初值，定时50ms 		3CB0
	TL0=0XB0;	//0X3CB0的十进制是15536 从15536计数到65536计数50000次 即50000us=50ms	
	ET0=1;		//打开定时器0中断允许
	EA=1;		//打开总中断
	TR0=1;		//打开定时器	
}

/***********************************************************
*函数名		：KeyScan
*功能		: 按键扫描，返回按下的按键对应的位，并防止连按
*返回值		：返回按键按下的位置
*参数		：无
************************************************************/
char KeyScan() 
{
	static unsigned char klast = 0;	  //记录上一次的按键值
	unsigned char trg = 0,key = 0;		  //trg:得到的返回值，返回值中8位只有一位为1，为1的位代表按下的位，其余位为零
	key = P2 & 0x0F;				  //将按下的位转换为0 没按下的位依然为1
	key ^= 0x0f;					  //异或之后没按下的位转换为1，按下的位转换为0
	trg = key & (key ^ klast);		  //这句是最关键的一句，需要自己理解
	klast = key;
	return trg;	  						//最终返回值是按下哪个按键，返回值对应就是1，没按对应的就是0  eg:按下button3 则返回0x01 0000 0100
}

/*******************************************************************************
* 函 数 名         : button_setting
* 函数功能		   : 实现按键设置/确认功能，按四下设置完毕 重新开始计数
* 参数			   ：无
*******************************************************************************/
void button_setting()
{
	if((button_flag & 0x0f) == 0x01 )		//里面不加括号不行 
	{
		button_num1++;	//选择设置不同位（时 分 秒）
		if(button_num1 == 4)
		{
			button_num1 = 0;
		}	
	}
}
 
/*******************************************************************************
* 函 数 名         : button_data
* 函数功能		   : 时间/日期 切换显示按钮实现
* 参数			   ：无
*******************************************************************************/
void button_data()
{
	if((button_flag & 0x0f) == 0x08)
	{
		button_num2++;
		if(button_num2 == 2)	
		{
			button_num2=0;
			//显示时间
		}
	}
}

/*******************************************************************************
* 函 数 名         : button_up_down
* 函数功能		   : 时间加/减 按键逻辑处理
* 参数			   ：无
*******************************************************************************/
void button_up_down()
{
	if((button_flag & 0x0f) == 0x02)	
	{
		switch(button_num1)	//button_num1 1：时/年   2：分/月   3：秒/日
		{
			case 0:	break;
			case 1:	
//			delay_us(10); while(~button2); 	   //等待按键释放	时加一		//不需要这么写了
			if(button_num2 == 0) 	// button_num2 0 时间 1日期
			{
				num3++;
			} 
			else
			{
				year++;
			}
			 break; 		
			case 2:	

			if(button_num2 == 0)
			{
				num2++;
			} 
			else
			{
				month++;
			}
			 break; 	
			case 3:
			 if(button_num2 == 0)
			{
				num1++;
			} 
			else
			{
				day++;
			}
			 break;		
			default: break;	
		}
		if(num3 == 24) num3 = 0;	//时间和日期超出归零处理			
		if(num2 == 60) num2 = 0;
		if(num1 == 60) num1 = 0;
		if(year == 100) year = 0;
		if(month == 13 ) month = 1;
		if(day == 31) day = 1;
		
	}

	if((button_flag & 0x0f) == 0x04)
	{
		switch(button_num1)
		{
			case 0:		break;
			case 1:
//			delay_us(10); while(~button3); 	   //等待按键释放	时加一			//不需要这么写了
			if(button_num2 == 0)
			{
				num3--;
			} 
			else
			{
				year--;
			}
			 break; 		
			case 2:	
			if(button_num2 == 0)
			{
				num2--;
			} 
			else
			{
				month--;
			}
			 break; 	
			case 3:	
			 if(button_num2 == 0)
			{
				num1--;
			} 
			else
			{
				day--;
			}
			 break;	
			default: break;
		}
		if(num3 == -1) num3 = 23;	//时间和日期减到低循环减
		if(num2 == -1) num2 = 59;
		if(num1 == -1) num1 = 59;
		if(year == -1) year = 99;
		if(month == 0 ) month = 12;
		if(day == 0) day = 30;
	}
}

/*******************************************************************************
* 函 数 名       : data_deal
* 函数功能		 : 日期处理函数，计算日期的当前的日期值
* 参数			 ：无
*******************************************************************************/
void data_deal()
{

	if(day>=30)
	{
		if(day_flag==0)
		{
			month++;
			day=1;
		}
		if(day_flag==1&&day>=31)
		{
			month++;
			day=1;
		}

		if(month>=13)
		{
			month = 0;
			year++;
			if(year>=100)
			{
				year = 0;
			}
		}
			
	}
}

/*******************************************************************************
* 函 数 名       : main
* 函数功能		 : 主函数
* 参数			 ：无
*******************************************************************************/	
void main()
{	
	TimerInit();
	
	while(1)
	{	
		button_flag = KeyScan();	
		button_setting();
		button_data();
		button_up_down();
	
		display(num3,num2,num1,year,month,day); 
			
	}
}


/***********************************************************
*函数名		：SendTo595
*功能		:串行发送8个比特（一个字节）的数据给595，再并行输出
*参数		：byteData 
************************************************************/
void SendTo595(char byteData)
{
   char i=0;
    for(;i<8;i++)
   {
        SER = byteData>>7; 		//取出最高位（第8位）       
        byteData= byteData<<1;  //将第7位移动到最高位    

        SCK = 0;        //变为低电平，为下次准备  ,并延时2个时钟周期 
        _nop_();
        _nop_();

        SCK = 1;         //上升沿，让串行输入时钟变为高电平，
   }  
}


/*******************************************************************************
* 函 数 名         : Timer0()
* 函数功能		   : 定时器0中断函数
* 参数			   ：无
*******************************************************************************/		
void Timer0() interrupt 1
{	 
	 TH0=0x3C;
	 TL0=0xB0;
	 i++;	 
	 //之前没有加入按键的时候我是在主函数中检测时间加一的，
	 //但是我后来发现如果我按住按键不松开的话时间计数会暂停
	 //其次我把所有判断都从if(==)替换为if(>=) 
	 //因为按键按住不松开,计数i的值会超过20，从而无法归零，导致时间计数停止，而i最终将会溢出

	 //说明：中断函数里是不适宜放过多语句的，这也是一开始我没有将时间加一处理放在中断的原因

	//使得setting_flag 0.2秒转换一次
	if(i%10<5)
	{
		setting_flag = 0;
	}
	if(i%10>=5)
	{
		setting_flag = 1;
	}

	 if(i>=20)//20个50毫秒即一秒
		{			
			i=0;
			if(button_num1==0)	
			{
				num1++;	
			}
			
			if(num1>=60)
			{
				num1=0;
				num2++;
				if(num2>=60)//定时一小时自动清零
				{
					num2=0;
					num3++;
					if(num3>=24)
					{
						num3=0;
						day++;
						//日期处理
						data_deal();				
					}
				}	
			}
		}		
}