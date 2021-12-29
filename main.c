/*
 * GccApplication1.c
 *
 * Created: 2021/10/19 17:40:19
 * Author : lenovo
 */ 

#define F_CPU 16000000UL
#define BAUD_RATE 9600
#define BAUD_PRESCALER (((F_CPU / (BAUD_RATE * 16UL))) - 1)

#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <time.h>
#include "uart.h"
#include "ST7735.h"
#include "LCD_GFX.h"

char String[25];
uint8_t player_y=53;
uint8_t computer_y=53;
struct Position{
	int ball_x;
	int ball_y;
	};
struct Position ball_position={79,63};
struct Speed{
		int x;
		int y;
};
struct Speed ball_speed={-10,5};
int score_player=0;
int score_computer=0;
int round_player=0;
int round_computer=0;


void Initialize(){
	cli();
	//Set LCD
	lcd_init();
	LCD_setScreen(rgb565(255,255,255));//initialize the screen to white
	LCD_drawString(30,0,"Welcome to Pong!",rgb565(0,0,0),rgb565(255,255,255));
	LCD_drawBlock(155,computer_y,159,computer_y+45,rgb565(255,0,0));//set paddle of computer-red
	LCD_drawCircle(ball_position.ball_x,ball_position.ball_y,5,rgb565(255,0,255));//set ball-purple
	LCD_drawBlock(0,player_y,4,player_y+30,rgb565(0,255,0));//set paddle of player-green
	
	//Set the Joysticker-Connect to A0
	PRR &= ~(1<<PRADC); //clear power reduction for ADC
	
	ADMUX |= (1<<REFS0);
	ADMUX &= ~(1<<REFS1); //select Vref=AVcc
	
	ADCSRA |= (1<<ADPS0);
	ADCSRA |= (1<<ADPS1);
	ADCSRA |= (1<<ADPS2); //set ADC clock divided by 128 times->125kHz
	
	ADMUX &= ~(1<<MUX0);
	ADMUX &= ~(1<<MUX1);
	ADMUX &= ~(1<<MUX2);
	ADMUX &= ~(1<<MUX3); //select channel 0
	
	ADCSRA |= (1<<ADATE);//set to auto trigger
	
	ADCSRB &= ~(1<<ADTS0);
	ADCSRB &= ~(1<<ADTS1);
	ADCSRB &= ~(1<<ADTS2); //set to free running
	
	DIDR0 |= (1<<ADC0D); //disable digital input buffer on ADC pin
	ADCSRA |= (1<<ADEN); //enable ADC
	ADCSRA |= (1<<ADIE); //enable ADC interrupt
	ADCSRA |= (1<<ADSC); //start conversion
	
	//Set LED
	DDRD |= (1<<DDD2);//set green LED to player
	DDRD |= (1<<DDD3);//set red LED to computer
	
	//Set buzzer
	DDRD |= (1<<DDD5);//set PD5 to buzzer
	
	//initialize Timer0
	TCCR0B |= (1<<CS00);
	TCCR0B |= (1<<CS01);
	TCCR0B &= ~(1<<CS02); /*prescale 64 times,250000Hz*/
	
	TCCR0A |= (1<<WGM00);
	TCCR0A &= ~(1<<WGM01);
	TCCR0B |= (1<<WGM02); /*set Timer0 Phase Correct*/
	
	TCCR0A |= (1<<COM0B0);
	TCCR0A |= (1<<COM0B1);/*non-inverting mode*/
	OCR0A = 0;/*(1/440)/(1/250000))/2=71*/
	OCR0B = OCR0A*1/2;
	
	TIMSK0 |= (1<<OCIE0A); /*Enable output compare interrupt*/
	TIFR0 |= (1<<OCF0A);/*clear interrupt flag*/
	
	sei();
	
}

struct Speed set_speed(struct Speed s){
	s.x=rand()%20-10;
	s.y=rand()%20-10;
	if(s.x==0){
		s.x=s.x+5;
	}
	if( (s.x>=-5) && (s.x<=5) ){
		s.y=15;
	}
	return s;
}

void restart(){
	LCD_drawBlock(0,player_y,4,player_y+30,rgb565(255,255,255));
	LCD_drawBlock(155, computer_y, 159, computer_y+45, rgb565(255,255,255));
	LCD_drawCircle(ball_position.ball_x, ball_position.ball_y, 5, rgb565(255,255,255)); //invisible
	score_computer=0;
	score_player=0;
	round_computer=0;
	round_player=0;
	player_y=53;
	computer_y=53;
	ball_position.ball_x=79;
	ball_position.ball_y=63; 
	LCD_drawString(50, 70,"Game Over!",rgb565(0,0,0),rgb565(255,255,255));
	_delay_ms(1000);
	 LCD_setScreen(rgb565(255,255,255));
	LCD_drawBlock(0,player_y,4,player_y+30,rgb565(0,255,0));
	LCD_drawBlock(155, computer_y, 159, computer_y+45, rgb565(255,0,0));
	//LCD_drawCircle(79, 63, 5, rgb565(255,0,255)); //reset
	ball_speed=set_speed(ball_speed);
}

int paddle(int player_y0){
	LCD_drawBlock(0,(uint8_t)player_y0,4,(uint8_t)player_y0+30,rgb565(255,255,255)); //invisible old one
	int player_speed=0;
	if(ADC <500){ //go up
		if(player_y0 >= 5 ){// not touch the upper bound
			player_speed = -7;
		}
		else{
			player_speed = 0;
		}
	}
	if(ADC > 520){ //go down
		if(player_y0+30 <= 122){ //not touch the lower bound
			player_speed = 7;
		}
		else{
			player_speed = 0;
		}
		
	}
	if((ADC>=500) && (ADC<=520)){// stand still
		player_speed = 0;	
	}
	player_y0 = player_y0 + player_speed;
	LCD_drawBlock(0, (uint8_t)player_y0,4, (uint8_t)player_y0+30,rgb565(0,255,0)); // show the paddle
	return player_y0;
}

int computer(int computer_y0){
	int computer_speed=0;
	double random_number=rand()%10;//generate a random number to control the move of computer's paddle
	LCD_drawBlock(155, (uint8_t)computer_y0, 159, (uint8_t)computer_y0+45, rgb565(255,255,255));//invisible old one
	if(random_number <= 4){ // go up
		if(computer_y0 >= 5){//not touch the upper bound
			computer_speed = -7;
		}
		else{
			computer_speed = 0;
		}
	}
	else{// go down
		if(computer_y0+45 <= 122){//not touch the lower bound
			computer_speed = 7;
		}
		else{
			computer_speed = 0;
		}
	}
	computer_y0 = computer_y0 + computer_speed;
	LCD_drawBlock(155, (uint8_t)computer_y0, 159, (uint8_t)computer_y0+45, rgb565(255,0,0));//show the paddle
	return computer_y0;
}

struct Position ball(struct Position p, uint16_t white){
	LCD_drawCircle((uint8_t)p.ball_x, (uint8_t)p.ball_y, 5, white); //invisible
	
	if(p.ball_x <= 5){ // ball fly out of boundary condition
		p.ball_x = 5;
		OCR0A = 71;
		OCR0B = OCR0A/2;
		_delay_ms(300);
		OCR0A = 0;
		OCR0B = OCR0A/2;
		if( (p.ball_y>=player_y) && (p.ball_y<=player_y+30) ){
			ball_speed.x=-1*ball_speed.x;
		}
		else{
			PORTD |= (1<<PORTD3);//computer wins! turn on red LED
			_delay_ms(1000);
			PORTD ^= (1<<PORTD3);
			p.ball_x=79;
			p.ball_y=63;
			ball_speed=set_speed(ball_speed);
			score_computer=score_computer+1;
			if(score_computer>=3){
				score_computer=0;
				score_player=0;
				round_computer=round_computer+1;
				if(round_computer>=2){
					LCD_drawString(50,40,"Computer wins!",rgb565(0,0,0),rgb565(255,255,255));
					restart();
				}
			}		
		}
	}
	if(p.ball_x >= 154){
		p.ball_x = 154;
		OCR0A = 71;
		OCR0B = OCR0A/2;
		_delay_ms(300);
		OCR0A = 0;
		OCR0B = OCR0A/2;
		if( (p.ball_y>=computer_y) && (p.ball_y<=computer_y+45) ){
			ball_speed.x=-1*ball_speed.x;
		}
		else{
			p.ball_x = 154;
			PORTD |= (1<<PORTD2);//player wins! turn on green LED
			_delay_ms(1000);
			PORTD ^= (1<<PORTD2);
			p.ball_x=79;
			p.ball_y=63;
			ball_speed=set_speed(ball_speed);
			score_player=score_player+1;
			if(score_player>=3){
				score_player=0;
				score_computer=0;
				round_player=round_player+1;
				if(round_player>=2){
					LCD_drawString(50,40,"You win!",rgb565(0,0,0),rgb565(255,255,255));
					restart();
				}
			}
		}
	}
	if(p.ball_y <= 5)
	{
		p.ball_y = 5;
		ball_speed.y=-1*ball_speed.y;
		OCR0A = 71;
		OCR0B = OCR0A/2;
		_delay_ms(100);
		OCR0A = 0;
		OCR0B = OCR0A/2;
	}
	if (p.ball_y >= 122)
	{
		p.ball_y =122;
		ball_speed.y=-1*ball_speed.y;
		OCR0A = 71;
		OCR0B = OCR0A/2;
		_delay_ms(100);
		OCR0A = 0;
		OCR0B = OCR0A/2;
	}
	p.ball_x = p.ball_x + ball_speed.x; //relocate x and y
	p.ball_y = p.ball_y + ball_speed.y;
	LCD_drawCircle((uint8_t)p.ball_x, (uint8_t)p.ball_y, 5, rgb565(255,0,255));
	return p;
}

ISR(ADC_vect){
	
}

ISR(TIMER0_COMPA_vect){
		
}

int main(void)
{
    Initialize();
	//UART_init(BAUD_PRESCALER);
	uint16_t black = rgb565(0,0,0);
	uint16_t white = rgb565(255,255,255);
    while (1) 
    {
		
		LCD_drawString(30,0,"Welcome to Pong!",black,white);
		LCD_drawString(50,10,"Round",black,white);
		LCD_drawChar(80,10,0x30+round_player,black,white);
		LCD_drawChar(87,10,0x2d,black,white);
		LCD_drawChar(94,10,0x30+round_computer,black,white);
		
		LCD_drawString(50,20,"Score",black,white);
		LCD_drawChar(80,20,0x30+score_player,black,white);
		LCD_drawChar(87,20,0x2d,black,white);
		LCD_drawChar(94,20,0x30+score_computer,black,white);
		player_y=paddle(player_y);
		computer_y=computer(computer_y);
		ball_position=ball(ball_position, white);
    }
}