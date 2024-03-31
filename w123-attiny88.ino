#include <stdlib.h>
#include <string.h>

// ATTN: ints 0..23
#include "PinChangeInterrupt.h"

//#include <SoftwareSerial.h>

#define DEG 0x8F  // degree symbol (custom)
#define Volt_min 125
#define Volt_max 145

#define DISPLAY_BRD0
#define ADC_IGNORE_THRESH 1000


// ints, <24
#define SPEED_PIN 20
#define RPM_PIN 2
#define FAN_PIN 25
// analog
#define VOLTAGE_PIN A5
// any
#define FL_PIN 18
#define WR_PIN 11
#define RES_PIN 17
// #define SERIAL_PIN 2
// #define OSC_SYNC_PIN 0
#define LED_PIN 0

#define Volt_delay 1500
#define Speed_delay 500
#define Rpm_delay 250
#define Temp_delay 1500

//#define DEBUG

//SoftwareSerial ser (SERIAL_PIN, SERIAL_PIN);

char buf[9];

int bright = 0;
int led = 0;

unsigned long rpm_internal = 0;
volatile unsigned char rpm_period = 100; // 0;

unsigned long speed_internal = 0;
volatile unsigned int speed_period = 0;

volatile int fan_state;

void reset(){
#ifdef RES_PIN
  digitalWrite(RES_PIN,1);
  delay(1);
  digitalWrite(RES_PIN,0);
  delay(1);
  digitalWrite(RES_PIN,1);
  delay(100);
#endif
}

void wr(){
  delay(1);
  digitalWrite(WR_PIN,LOW);
  delay(1);
  digitalWrite(WR_PIN,HIGH);  
  delay(1); 
}

void setAddr(int addr){
  for(int i=16,mask=1;i>12;i--,mask<<=1) // A0..A3
    digitalWrite(i,((addr&mask)==mask));

  digitalWrite(19, ((addr&0x10) == 0x10)); // A4. pin12 behaves inpredictable (occupied?)
}

void setData(int data){
#ifdef DISPLAY_BRD0
  for(int i=3,mask=0x80;i<WR_PIN;i++,mask>>=1)
#else
  for(int i=3,mask=1;i<WR_PIN;i++,mask<<=1)
#endif
    digitalWrite(i,((data&mask) == mask)); 
         
}

void charTo(int addr, int data){
  setData(data);
  setAddr(addr | 0x18);
  wr();
}

void displayStr(String s){
  int l = s.length();
  if(l>=7) l = 7;

  for(int i=0;i<=l;i++)
    charTo(i,s[i]);

// #ifdef DEBUG
//   ser.println(s);
// #endif
}
/*
void flash(int mask){
  digitalWrite(FL_PIN,0);
  int m = mask;
  for(int a=7; a>=0; a--, m>>=1){
    setAddr(a);
    setData(m);
    wr();
  }
  digitalWrite(FL_PIN,1);
}
*/
void degree(){

  setAddr(0);  // UDC AR
  setData(DEG);  // char to draw

  wr();

  setAddr(0x08);// upper
  setData(0x1C);
  wr();
  
  setAddr(0x09);
  setData(0x14);
  wr();
  
  setAddr(0x0A);
  setData(0x1C);
  wr();
  
  for(int i=3;i<7;i++){
    setAddr(0x08+i);  // UDC AR
    setData(0);
    wr();
  }  
}

void blink(){
#ifdef LED_PIN
  if (led){
    led = 0;
    digitalWrite(LED_PIN,0);
  } else {
    led = 1;
    digitalWrite(LED_PIN,1);
  }
#endif
}

void confDisplay(int br){
  digitalWrite(FL_PIN,1);
    

  setAddr(0x10);
  
  setData(br&0x07);
  wr();
  
  delay(50);
}


void rpm_isr(){
    unsigned long m = millis();
    unsigned long diff = m-rpm_internal;
    rpm_period = diff&0xff;
    
    rpm_internal = m;
    // blink();
}

void speed_isr(){
  displayStr("SPEED ISR");
  delay(5000);
  
    unsigned long m = millis();
    if (speed_internal) {
      unsigned long diff = m-speed_internal;
      speed_period = diff;
    }
    speed_internal = m;
}
/*
void fan_isr(){
  fan_state = digitalRead(FAN_PIN);
  blink();
}
*/

void Handler_Rpm(){

  unsigned char x= rpm_period;
  rpm_period = 0;
  
  if (x) {
    unsigned rpm = 60000/x;
    itoa(rpm,buf,10);
    String s = buf;
    s += " rpm  ";
    displayStr(s);
    delay(Rpm_delay);
  }
}

void Handler_Voltage(int alarmOnly){
  String s="  ";
  String tail = "V      ";
  unsigned int v = analogRead(VOLTAGE_PIN);

  v = v*10/64;
  itoa(v,buf,10);
  
    if (v<100) {
      buf[2] = buf[1];
      buf[1] = '.';
      buf[3] = 0;
    } else {
      buf[3] = buf[2];
      buf[2] = '.';
      buf[4] = 0;
    }
  s += buf + tail;      

  if ((v>Volt_min) && (v<Volt_max)){
    if (alarmOnly) return;
  }
  displayStr(s);
  delay(Volt_delay);
  return;
}

void Handler_Speed(){
  unsigned int x = speed_period;

  speed_period = 0;
  
  String s = "Tv=";
  s += x;
  displayStr("        ");
  displayStr(s);
  
  delay(500);
}

void Handler_Temp(int alarmOnly){
  //displayStr("        ");
  
  //todo: fix
/*
  displayStr("Eng 92 C");
  charTo(6,DEG);  
  delay(Temp_delay);
*/
  String s;

  unsigned v = analogRead(A2); 
  if ( v < ADC_IGNORE_THRESH){
    if (digitalRead(FAN_PIN)){
      s = "Eng ";
      digitalWrite(LED_PIN,1);
    }
    else {
      s = "FAN "; 
      digitalWrite(LED_PIN,1);
    }
    s+=v;
    s+=char(DEG);
    displayStr("        ");
    displayStr(s);
    delay(Temp_delay);    
  }

  v = analogRead(A3); 
  if ( v< ADC_IGNORE_THRESH){
    s = "A3:";
    s+=v;
    s+=char(DEG);
    displayStr("        ");
    displayStr(s);
    delay(Temp_delay);
  }

  v = analogRead(A4); 
  if ( v< ADC_IGNORE_THRESH){
    s = "A4:";
    s+=v;
    s+=char(DEG);
    displayStr("        ");
    displayStr(s);
    delay(Temp_delay);
  }
}

void setup() {
  for(int i=3;i<=19;i++) // 8 data + 5 addr + FL + (RES)
    pinMode(i,OUTPUT);  
  //pinMode(SERIAL_PIN, OUTPUT);

  pinMode(FL_PIN,OUTPUT);
  pinMode(RES_PIN,OUTPUT);

  digitalWrite(FL_PIN,1); 
  digitalWrite(RES_PIN,1); 
  setAddr(0x1F);
  digitalWrite(WR_PIN,1);
//  digitalWrite(OSC_SYNC_PIN,0);

  
  analogReference(DEFAULT);
  
  pinMode(RPM_PIN, INPUT_PULLUP);
  attachPCINT(digitalPinToPCINT(RPM_PIN), rpm_isr, FALLING);

  pinMode(SPEED_PIN, INPUT_PULLUP);
  attachPCINT(digitalPinToPCINT(SPEED_PIN), speed_isr, FALLING);

//  pinMode(FAN_PIN, INPUT_PULLUP);
//  attachPCINT(digitalPinToPCINT(FAN_PIN), fan_isr, CHANGE);
  pinMode(FAN_PIN, INPUT);
  
#ifdef LED_PIN
  pinMode(LED_PIN,OUTPUT);
#endif

#ifdef SERIAL_PIN
    ser.begin(9600);
#endif
}


void loop() {
  displayStr("          ");
  
  delay(500); 
  confDisplay(6);
  degree();
  
  reset();


  for(;;){ 
    String s;
    if (speed_period) {

      displayStr("DRIVE   ");
      delay(500);

      for(int i=0;i<10;i++)
        Handler_Speed();
      Handler_Voltage(1);  
      Handler_Temp(1);

    } else {
    
      Handler_Voltage(0);  
          
      for(int i=0;i<10;i++)
        Handler_Rpm();
      
      Handler_Temp(0);
      
      for(int i=0;i<10;i++)
        Handler_Rpm();
    }
  }
}
