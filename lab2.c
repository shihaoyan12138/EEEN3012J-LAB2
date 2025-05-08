
#include <C8051F020.h>
/* ---------- ???? ---------- */
typedef unsigned char  u8;
typedef unsigned int   i8;
typedef unsigned int   u16;
typedef   signed int   s16;

sbit SS=P0^7;

volatile u16 across_count_lw = 0; 
volatile i8 across_count_high = 0; 
volatile u16 frequency_lw = 0;
volatile i8 frequency_high = 0;
volatile u16 disp_num = 0;

void init_System(){
	WDTCN = 0xDE;	//watchdog
	WDTCN = 0xAD;	//watchdog
}
/* ---------- ADC ???? ---------- */
static void init_ADC(void){
    REF0CN=0x03; 
	  AMX0CF=0x02; 
	  ADC0CF=0x04; 
	  AMX0SL=0x02; 
	  ADC0CN=0x80;
}
static s16 ADC_Read(void){
    s16 v;
    AD0INT=0; AD0BUSY=1;
    while(!AD0INT);
    ((u8*)&v)[0]=ADC0H;
    ((u8*)&v)[1]=ADC0L;
    return v;
}

/* ---------- ????(??) ---------- */
#define RISE  100
#define FALL -100
static bit half_state=0;
static bit Zero_Crossing(void){
    s16 x=ADC_Read();
    if((half_state==0 && x>RISE) || (half_state==1 && x<FALL)){
        half_state^=1;
			  if(across_count_lw == 0xFFFF){
					 across_count_high++;
					 across_count_lw = 0;
				}else{
					across_count_lw++;
        }
				return 1;
    }
    return 0;
}

/* ---------- 1 ??? (Timer2) ---------- */
#define XTAL 22118400UL
#define TCLK (XTAL/12)
#define DIV  32
#define PRE (0xFFFF - (TCLK/DIV) + 1)
#define RH  (u8)(PRE>>8)
#define RL  (u8)(PRE&0xFF)
static volatile bit sec_flag=0;
void timer2_isr(void) interrupt 5 using 1 {
    static u8 t=0;
    if(++t >= DIV){
        sec_flag = 1;
        t = 0;
    }
    TF2 = 0;
}
static void init_timer2(void){
    CKCON=0x00;
    CT2=0; 
	  CPRL2=0;
    RCAP2H=RH; 
	  RCAP2L=RL;
    ET2=1; 
	  EA=1; 
	  TR2=1;
}


static void enable_xtal(void){
    OSCXCN=(6<<4)|(7<<0);
    while(!(OSCXCN&0x80));
    OSCICN|=0x08;
}
void Delay(int i){
	while(i--);
}

void SPI_Send(int val_dat){
	SS = 0;
	SPI0DAT = val_dat;
	while(TXBSY);
	SS = 1;
	Delay(20);
}

void Print(u16 val_disp_num, i8 high){
	if((val_disp_num < 1000)&&(high == 0)){
		SPI_Send(0x76);		//cursor command
		SPI_Send(0x79);		//cursor command
		SPI_Send(0);
		SPI_Send(0x0F);
	  SPI_Send(val_disp_num/100%10);
	  SPI_Send(val_disp_num/10%10);
	  SPI_Send(val_disp_num%10);
	}else if((1000<= val_disp_num) &&(val_disp_num < 10000)&&(high == 0)){
		SPI_Send(0x77);		//dicimal command
		SPI_Send(0x02);		//dicimal data: 0~63
    SPI_Send(0x79);		//cursor command
	  SPI_Send(0);
		SPI_Send(0x0F);
		SPI_Send(val_disp_num/1000%10);
	  SPI_Send(val_disp_num/100%10);
	  SPI_Send(val_disp_num/10%10);
	} else if(((10000<= val_disp_num)&&(high == 0))||((high == 1)&&(val_disp_num < 0x86A0))){
		SPI_Send(0x77);		//dicimal command
		SPI_Send(0x04);		//dicimal data: 0~63
    SPI_Send(0x79);		//cursor command
	  SPI_Send(0);
			if(high == 0){
				SPI_Send(0x0F);
				SPI_Send(val_disp_num/10000%10);
				SPI_Send(val_disp_num/1000%10);
				SPI_Send(val_disp_num/100%10);
			}else if(high == 1){
				disp_num = ((val_disp_num + 36)/100);
				SPI_Send(0x0F);
				SPI_Send(disp_num/100%10);
				SPI_Send(disp_num/10%10);
				SPI_Send(disp_num%10);
			}
		
	}else {
		SPI_Send(0x77);
		SPI_Send(0x06);		//dicimal data: 0~63
    
		SPI_Send(0x79);		//cursor command
	  SPI_Send(0);
		disp_num = (655*high + (val_disp_num +36*high)/100)/10;
		SPI_Send(0x0F);
		SPI_Send(0x01);
		SPI_Send(disp_num/10%10);
		SPI_Send(disp_num%10);
	}
	Delay(100);
}

void init_IO(){
	P0MDOUT = 0xFF;
	
	XBR0 = 0x06;	//crossbar for SPI
	XBR2 = 0x40;	//cro3ssbar enable

	SPI0CFG = 0x07;
	SPI0CN = 0x07;
	SPI0CKR = 0x09;	//SPI_clk == sys_clk/((0x09+1)*2)
	SS = 1;

	SPI_Send(0x7A);	//brightness control command
	SPI_Send(150);	//brightness control d 0~255
	SPI_Send(0x76);	//clear display
}

void update_frequency_display(void) {
    if (sec_flag) {  // 1????
        sec_flag = 0;
        if((across_count_high == 1)||(across_count_high ==3)){
           frequency_lw = 0x8000+((across_count_lw) / 2);
				   frequency_high = ((across_count_high - 1)/2);
			  }else{
				   frequency_lw = across_count_lw / 2;
				   frequency_high = across_count_high/2;
				}
        
        across_count_lw = 0;
				across_count_high = 0;
        Print(frequency_lw,frequency_high);  
    }
}


void main(){
    init_System();  //init_system, ???watchdog
    init_IO();  //?????????
    init_ADC();
    enable_xtal();
    init_timer2();
	  while(1){
			Zero_Crossing();
			update_frequency_display();
		}
}