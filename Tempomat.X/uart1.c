#include <p30fxxxx.h>
#include "uart1.h"

//funkciija za inicijalizaciju UARTa

void initUART1(){
	U1BRG=0x0030;//baud rate 9600
	U1MODEbits.ALTIO=1;
	IEC0bits.U1RXIE=1;
	U1STA&=0xfffc;
	U1MODEbits.UARTEN=1;
	U1STAbits.UTXEN=1;
}

//funkcija koja salje karakter na PC 
void writeUART1(unsigned int data){

	while (U1STAbits.TRMT==0);
    if(U1MODEbits.PDSEL == 3)
        U1TXREG = data;
    else
        U1TXREG = data & 0xFF;
}


//funkcija koja decimalan broj pretvara u string i salje ga na PC
void writeUART1decToString(unsigned int data){

	unsigned char temp;

	temp=data/1000;
	WriteUART1(temp+'0');
	data=data-temp*1000;
	temp=data/100;
	WriteUART1(temp+'0');
	data=data-temp*100;
	temp=data/10;
	WriteUART1(temp+'0');
	data=data-temp*10;
	WriteUART1(data+'0');
}