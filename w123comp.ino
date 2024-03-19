// Dashboard:
// Gnd
// Vcc
// speed (int@D3)
// ?? fuel (A6)
// ?? temp (A5)

// ext:
// rpm (int@D2)
// out temp (A6) 
// temp (A5)

#include <stdlib.h>
#include <string.h>

#define DEG 0x80  // degree symbol (custom)
#define Volt_min 125
#define Volt_max 145
#define Volt_delay 1500

char buf[9];

#define Speed_delay 500
#define Rpm_delay 500
#define Temp_delay 1500

unsigned long rpm_internal = 0;
volatile unsigned char rpm_period = 0;

unsigned long speed_internal = 0;
volatile unsigned int speed_period = 0;



void setData(int data){
//  for(int i=2,mask=0x80;i<10;i++,mask>>=1)
  for(int i=4,mask=0x20;i<10;i++,mask>>=1) // need to release D2 & D3 for ISR
    digitalWrite(i,((data&mask) == mask));  
  for(int i=17,mask=0x80;i<19;i++,mask>>=1) // need to release D2 & D3 for ISR
    digitalWrite(i,((data&mask) == mask));  
    
}

void setAddr(int addr){
  for(int i=10,mask=1;i<16;i++,mask<<=1){
    digitalWrite(i,((addr&mask)==mask));  
  }
}

void wr(){
  delay(1);
  digitalWrite(16,LOW);
  delay(1);
  digitalWrite(16,HIGH);  
   delay(1); 
}

void charTo(int addr, int data){
  setData(data);
  setAddr(addr | 0x38);
  wr();
}

void degree(){
  setAddr(0x20);  // UDC AR
  setData(DEG);  // char to draw
  wr();

  setAddr(0x28);// upper
  setData(0x1C);
  wr();
  setAddr(0x29);
  setData(0x14);
  wr();
  setAddr(0x2A);
  setData(0x1C);
  wr();
  
  for(int i=3;i<7;i++){
    setAddr(0x28+i);  // UDC AR
    setData(0);
    wr();
  }  
}

void displayStr(String s){
  int l = s.length();
  if(l>=7) l = 7;

  for(int i=0;i<=l;i++)
    charTo(i,s[i]);
}

void flash(int mask){
  int m = mask;
  for(int a=7; a>=0; a--, m>>=1){
    setAddr(a);
    setData(m);
    wr();
  }
}

void confDisplay(){
  setAddr(0x30);
  setData(0x06); //0x1E
  wr();

  degree();
}

void rpm_isr(){
    unsigned long m = millis();
    if (rpm_internal) {
      unsigned long diff = m-rpm_internal;
      rpm_period = diff&0xff;
    }
    rpm_internal = m;
}

void speed_isr(){
    unsigned long m = millis();
    if (speed_internal) {
      unsigned long diff = m-speed_internal;
      speed_period = diff;
    }
    speed_internal = m;
}

void Handler_Rpm(){
  unsigned char x = rpm_period;
  if (x) {
    unsigned rpm = 60000/x;
    rpm_period = 0;
    itoa(rpm,buf,10);
  } else {
    return; 
    
    // buf[0] = '0';
    // buf[1] = '*';
  }
  
  String s;
  int pos = 0;
      
  for(int i=0;i<8;i++){
    if((buf[i]<'0')||(buf[i]>'9')){
      buf[i] = 0;
      pos = i;
      break;
    }
  }
  
  while(pos<4){
    s += " ";
    pos++;
  }
  s += buf;
  s += " rpm";
    
  displayStr(s);
  delay(Rpm_delay);
}

void Handler_Voltage(int alarmOnly){
  String s="  ";
  String tail = "V      ";
  unsigned int v = analogRead(A7);
  v = v*10/62;
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
    displayStr(s);
    delay(Volt_delay);
    return;
  } else {
    displayStr(s);
    flash(0xF0);
    delay(Volt_delay);
    flash(0);
    return;
  }
}

void Handler_Speed(){
  // displayStr("123 km/h");

  unsigned int x = speed_period;
  for(int i=0;i<8;i++)
    buf[i] = ' ';
  itoa(x,buf,10);

  speed_period = 0;
  
  String s = buf;
  displayStr(s);
  
  delay(1000);
}

void Handler_Temp(int alarmOnly){
  displayStr("Temp 92 ");
  charTo(7,DEG);  
  delay(Temp_delay);
}



void setup() {
  for(int i=4;i<19;i++)
    pinMode(i,OUTPUT);

  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), rpm_isr, FALLING);

  pinMode(3, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(3), speed_isr, FALLING);
  
  delay(1000);
}

void loop() {
  confDisplay(); 

  if (speed_period) {
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
