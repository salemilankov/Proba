/* 
 * File:   main_tempomat.c
 * Author: Milankov
 *
 * Created on 03.06.2015., 15.40
 */
 // github proba
 //test

#include <stdio.h>
#include <stdlib.h>
#include "driverGLCD.h"
#include "adc.h"
#include "timer1.h"

_FOSC(CSW_FSCM_OFF & XT_PLL4);		//instruction takt je isti kao i kristal
_FWDT(WDT_OFF);

#define TEMP_K 40.9 //konstanta kojom se deli rezultat a/d konverzije da bi temp. bila u opsegu 0-100 stepeni
#define RPM_K 0.75  //konstanta kojom se deli rezultat a/d konverzije da bi RPM bio u opsegu 900-4000
#define V_K1 0.0129 //konstanta koja skalira brzinu u zavisnosti od RPM
#define V_K2 0.0112
#define V_K3 0.0161
#define V_K4 0.0225
#define V_K5 0.0258
#define SHIFT_UP PORTDbits.RD8  //tasteri koji simuliraju promenu stepena prenosa
#define SHIFT_DOWN PORTDbits.RD9
#define TEMPOMAT_SWITCH  PORTFbits.RF6  //pin na koji se prikljucuje prekidac za ukljucivanje tempomata
#define SET_SPEED PORTBbits.RB9
#define RESET_SPEED PORTBbits.RB10
#define BRAKE_SWITCH PORTBbits.RB11
#define LED PORTBbits.RB12
#define DELAY_SHIFT 300


unsigned int vreme, meriSe;
unsigned int temp1, temp2, temperatura, rpm, br1, i, trenutni_rpm;
unsigned int stepen_prenosa, brzina, zeljena_brzina, trenutna_brzina, brzina_korak;
unsigned int rpm_korak;
unsigned char stanje_tempomat, shift_up, shift_down, set_speed, reset_speed, brake_switch;

//funkcija za kasnjenje od ms milisekundi(prilikom inicijalizacije tajmera 1 upisati 10000)
void delay_ms(unsigned int ms)
{
    meriSe=1;
    vreme=0;
    while(vreme < ms);
    meriSe=0;
    vreme=0;
}


        //konfiguracija tajmera 1 (komentarisem ako koristim programsko kasnjenje)
void T1_Config() {
	ConfigIntTimer1(T1_INT_PRIOR_1 & T1_INT_ON);//prioritet i ukljucivanje tajmera
	WriteTimer1(0);	//resetovanje brojaca tajmera
	//OpenTimer1(konfiguracioni bitovi, vrednost tajmera1)
	OpenTimer1(T1_ON & T1_GATE_OFF & T1_IDLE_CON & T1_PS_1_1 & T1_SYNC_EXT_OFF & T1_SOURCE_INT,39062 );
	T1CONbits.TCKPS = 3;  // Set timer 1 prescaler (0=1:1, 1=1:8, 2=1:64, 3=1:256)
	T1CONbits.TON = 0;      // Disable timer1
}


void initTIMER1(int period)//konfiguracija Timer1
{
	unsigned int match_value;//vrednost koja se stavlja u period

	ConfigIntTimer1(T1_INT_PRIOR_1 & T1_INT_ON);//prioritet i ukljucivanje tajmera
	WriteTimer1(0);
	match_value = period ;
	OpenTimer1(T1_ON & T1_GATE_OFF & T1_IDLE_CON & T1_PS_1_1 & T1_SYNC_EXT_OFF & T1_SOURCE_INT,match_value );
//        T1CONbits.TON = 0; ako koristim Zolijevu funkciju ovo odkomentarisem
}

        //konfiguracija prekida tajmera 1
void __attribute__((__interrupt__)) _T1Interrupt(void)
{
	TMR1=0;
	if (meriSe==1)
		vreme++;
	else
		vreme=0;
IFS0bits.T1IF = 0;
}

//konfiguracija prekida AD konvertora
void __attribute__((__interrupt__)) _ADCInterrupt(void) {

	temp1 = ADCBUF0;
	temp2 = ADCBUF1;

    IFS0bits.ADIF = 0;
}

//konfiguracija pinova
void configureIOPins(){
    TRISDbits.TRISD8 = 1;   //1-ulazni pinovi, 0-izlazni
    TRISDbits.TRISD9 = 1;
    TRISFbits.TRISF6 = 1;
    TRISBbits.TRISB9 = 1;
    TRISBbits.TRISB10 = 1;
    TRISBbits.TRISB11 = 1;
    TRISBbits.TRISB12 = 0;
    ADPCFGbits.PCFG9 = 1;
    ADPCFGbits.PCFG10 = 1;
    ADPCFGbits.PCFG11 = 1;
    ADPCFGbits.PCFG12 = 1;


}

void Write_GLCD_2cifre(unsigned int data)
{
unsigned char temp;

temp=data/1000;
//Glcd_PutChar(temp+'0');
//data=data-temp*1000;
//temp=data/100;
//Glcd_PutChar(temp+'0');
data=data-temp*100;
temp=data/10;
Glcd_PutChar(temp+'0');
data=data-temp*10;
Glcd_PutChar(data+'0');
}

void Write_GLCD_3cifre(unsigned int data)
{
unsigned char temp;

temp=data/1000;
//Glcd_PutChar(temp+'0');
data=data-temp*1000;
temp=data/100;
Glcd_PutChar(temp+'0');
data=data-temp*100;
temp=data/10;
Glcd_PutChar(temp+'0');
data=data-temp*10;
Glcd_PutChar(data+'0');
}

void Write_GLCD(unsigned int data)
{
unsigned char temp;

temp=data/1000;
Glcd_PutChar(temp+'0');
data=data-temp*1000;
temp=data/100;
Glcd_PutChar(temp+'0');
data=data-temp*100;
temp=data/10;
Glcd_PutChar(temp+'0');
data=data-temp*10;
Glcd_PutChar(data+'0');
}

//funkcija koja preracunava temperaturu i ispisuje podatak na ekran
void izmeriTemperaturu(){
    temperatura=temp1/TEMP_K;   //skaliranje
    GoToXY(0,0);

    if (temperatura < 100)
    {
        GLCD_Printf ("Temperatura:  ");
        Write_GLCD_2cifre(temperatura);
    }
        else
        {
            GLCD_Printf ("Temperatura: ");
            Write_GLCD_3cifre(temperatura);
        }

    GLCD_Printf(" deg");

}

//funkcija koja racuna broj obrtaja i ispisuje vrednost na ekran
void izmeriObrtaje(){
    rpm=900+RPM_K*temp2;    //skaliranje
}

void ispisiObrtaje(){
    GoToXY(0,1);

    if(rpm < 1000 )
    {
        GLCD_Printf("Broj obrtaja: ");
        Write_GLCD_3cifre(rpm);

    }
        else
        {
                GLCD_Printf("Broj obrtaja:");
                Write_GLCD(rpm);
        }

    GLCD_Printf(" RPM");
}

//funkcija koja odredjuje stepen prenosa i ispisuje ga na displej
void odrediStepenPrenosa(){

    if(SHIFT_UP)    //pokusaj softverskog debouncing-a 
    {
        delay_ms(20);
        if(SHIFT_UP) shift_up=1;
        while(SHIFT_UP);
        delay_ms(20);
    }

    if(SHIFT_DOWN)    //pokusaj softverskog debouncing-a 
    {
        delay_ms(20);
        if(SHIFT_DOWN) shift_down=1;
        while(SHIFT_DOWN);
        delay_ms(20);
    }


    if((shift_up) && (stepen_prenosa < 5))
    {
        stepen_prenosa++;
    }
     

    if((shift_down) && (stepen_prenosa > 0))
    {
        stepen_prenosa--;
    }

    GoToXY(0,2);
    GLCD_Printf("Stepen prenosa: ");
    if(stepen_prenosa<1)
    {
        Glcd_PutChar('N');
    }
        else
        {
        Glcd_PutChar(stepen_prenosa + '0');
        }

    if(shift_up)        //naredna dva if simuliraju promenu viseg u nizi stepen prenosa pri istom RPM
    {
        shift_up = 0;
        trenutna_brzina = brzina;
        izracunajBrzinu();
        brzina_korak = (brzina - trenutna_brzina)/10;
       
        if (rpm < 1200)     //da ne padnu obrtaji previse
        {
            rpm -=  400;
            rpm_korak = 40;
        }
            else 
            {
                rpm -= 800;
                rpm_korak = 80;
            }

        for(i=0;i<10;i++)
        {
            trenutna_brzina += brzina_korak;
            brzina = trenutna_brzina;
            ispisiBrzinu();
            rpm += rpm_korak;
            ispisiObrtaje();
            delay_ms(DELAY_SHIFT);
        }
    }

    if(shift_down)
    {
        shift_down = 0;
        trenutna_brzina = brzina;
        izracunajBrzinu();
        brzina_korak = (trenutna_brzina - brzina)/10;
        rpm = rpm + 800;
        for(i=0;i<10;i++)
        {
            trenutna_brzina -= brzina_korak;
            brzina = trenutna_brzina;
            ispisiBrzinu();
            rpm -= 80;
            ispisiObrtaje();
            delay_ms(DELAY_SHIFT);
        }
    }
}

//funkcija koja ispisuje brzinu na displej
void ispisiBrzinu(){
    GoToXY(0,3);
    if (brzina < 100)
    {
        GLCD_Printf("Brzina:  ");
        Write_GLCD_2cifre(brzina);
        GLCD_Printf(" kmph");
    }
        else
        {
            GLCD_Printf("Brzina: ");
            Write_GLCD_3cifre(brzina);
            GLCD_Printf(" kmph");
        }
}

//funkcija koja racuna brzinu i poziva funkciju za ispis brzine na displej
void izracunajBrzinu(){
    switch(stepen_prenosa)
    {
        case 0:
            brzina = 0;
            break;
        case 1:
            brzina = V_K1*(rpm - 900);  //y = k(x-x0)+y0
            break;
        case 2:
            brzina=V_K2*(rpm - 900) + 25;;
            break;
        case 3:
            brzina=V_K3*(rpm - 900) + 40;
            break;
        case 4:
            brzina=V_K4*(rpm - 900) + 50;
            break;
        case 5:
            brzina=V_K5*(rpm - 900) + 80;
            break;
    }
}

void ispisiStanjeTempomata(){
    GoToXY(0,4);
    GLCD_Printf("Tempomat: ");

    if (stanje_tempomat) GLCD_Printf("  ON");
        else GLCD_Printf(" OFF");
}

void setujBrzinu(){

    if (SET_SPEED)
    {
        delay_ms(20);
        if (SET_SPEED) set_speed = 1;
        while(SET_SPEED);
        delay_ms(20);
     }

    if(!set_speed)
    {
        GoToXY(0,5);
        GLCD_Printf("Brzina nije setovana!");
    }
    trenutni_rpm = rpm; //pamtimo rpm da bi mogli da uocimo dodavanje gasa

    while (set_speed)
    {
        GoToXY(0,5);
        GLCD_Printf("Brzina je setovana!  ");
        ispisiBrzinu();
        ispisiObrtaje();
        odrediStepenPrenosa();
        izmeriTemperaturu();
        ispisiStanjeTempomata();
        izmeriObrtaje();

        if (RESET_SPEED)
        {
            delay_ms(20);
            if (RESET_SPEED) reset_speed = 1;
            while(RESET_SPEED);
            delay_ms(20);
        }

    if (BRAKE_SWITCH)
        {
            delay_ms(20);
            if (BRAKE_SWITCH) brake_switch = 1;
            while(BRAKE_SWITCH);
            delay_ms(20);
        }

        if(((rpm - 50) > trenutni_rpm) || (brake_switch) || (reset_speed))  //rpm-50 zbog smetnji koje stizu na a/d konvertor
        {
            set_speed = 0;
            brake_switch = 0;
            reset_speed = 0;
        }
    }
}

void GLCDTest(){
        LcdSetDot(0,0);
        delay_ms(1000);
        LcdSetDot(10,0);
        delay_ms(1000);
        LcdSetDot(0,10);
        delay_ms(1000);
}

void resetParameters(){
    stepen_prenosa=0;
    temperatura=0;
    rpm=0;
    shift_up=0;
    shift_down=0;
    brzina=0;
    zeljena_brzina=0;
    trenutna_brzina=0;
    brzina_korak=0;
    rpm_korak=0;
    stanje_tempomat=0;
    trenutni_rpm=0;
    set_speed=0;
    reset_speed=0;
    brake_switch=0;
}

/*
 *vazna napomena, funkcija GoToXY(x,y); tretira y kao broj reda!!!
 */
int main() {

    initTIMER1(10000);  //inicijalizacija tajmera 1
    ConfigureADCPins(); //konfigurisanje pinova za A/D konverziju (RB6, RB7)
    ADCinit();  //inicijalizacija A/D konvertora
    ConfigureLCDPins(); //konfiguracija pinova koji se koriste za LCD displej
    configureIOPins();  //konfiguracija UI pinova
    GLCD_LcdInit(); //inicijalizacija GLCD-a
    GLCD_ClrScr();  // brise ako je bilo neke brlje na ekranu
    resetParameters();  //inicijalizacija promenljivih
    ADCON1bits.ADON=1;   //startujemo A/D konverziju


    while(1){
        izmeriTemperaturu();
        delay_ms(20);
        izmeriObrtaje();
        ispisiObrtaje();
        delay_ms(20);
        odrediStepenPrenosa();
        delay_ms(20);
        izracunajBrzinu();
        ispisiBrzinu();

        if(TEMPOMAT_SWITCH)
        {
            stanje_tempomat = 1;
            ispisiStanjeTempomata();
            setujBrzinu();
        }
            else
            {
            stanje_tempomat = 0;
            ispisiStanjeTempomata();
            }

    }
   
}

