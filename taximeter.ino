//libraries
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>

//pin definition
#define RESET A1
#define ODOMETER 10
#define REPORT 11
#define END 12
#define START 13
#define SPEED_SENSOR 2
#define LCDPOWER A2

//constants - can be changed
#define STARTING_FARE 50
#define FARE 40
#define FARE_NIGHT 50
#define WAITING_FARE 10 //fare addition for 5 minute wait

/*
 addr 0  : Total distance
 addr 1  : Total hire distance
 addr 3  : Total income
 addr 4  : Day start income
 addr 5  : Day start distance
 addr 6  : Month start income
 addr 7  : Month start distance
 addr 8  : Day
 addr 9  : Month
 addr 10 : Last Reset Day
 addr 11 : Last Reset Month
 addr 12 : Last Reset Century
 addr 13 : Last Reset Year
 addr 14 : Last Reset Hour
 addr 15 : Last Reset Minute
*/

LiquidCrystal lcd(9, 8, 7, 6, 5, 4);

//variable definition
short pulseCounter; //pulses from the hall sensor for a second
unsigned int vehicleSpeed;

unsigned short meters;
float totalKm;
float hireTotalKm;
float hireDistance;

long income;
unsigned int fare;

long time1;
long time2;
int waitingTime;
long waitingClock;
long menuShowTime; //time that a menu is shown
tmElements_t tm;


bool commandSent;
bool onHire;
bool waiting;
bool working;
int inputWait;
int longPress;


//Setup
void setup() {
  Serial.begin(9600);
  
  lcd.begin(16, 2);
  lcd.clear();
  
  pinMode(SPEED_SENSOR, INPUT);
  pinMode(RESET, INPUT);
  pinMode(START, INPUT);
  pinMode(END, INPUT);
  pinMode(REPORT, INPUT);
  pinMode(ODOMETER, INPUT);
  pinMode(LCDPOWER, OUTPUT);
  
  attachInterrupt(0, pulseCalculator, RISING);
  
  pulseCounter = 0;
  meters = 0;
  hireDistance = 0;
  fare = STARTING_FARE;
  vehicleSpeed = 0;
  onHire = false;
  waiting = false;
  time1 = millis();
  working = false;
  waitingTime = 0;
  menuShowTime = 0;
}

//ISR 0
void pulseCalculator(){
  ++pulseCounter;
  ++meters;
}

void boot(){  
  digitalWrite(LCDPOWER, HIGH);
  
  lcd.setCursor(4, 0);
  lcd.print("WELCOME!!");
  
  delay(3000);
  lcd.clear();
  
  working = true;
  totalKm=readFloat(0);
  hireTotalKm = readFloat(1);
  income = readLong(3);
    
  if (RTC.read(tm)) {
    if(tm.Day != readLong(8)){
      saveData(4, income);
      saveData(5, hireTotalKm);
      saveData(8, (long)tm.Day);
    }
    else if(tm.Month != readLong(9)){
      saveData(6, income);
      saveData(7, hireTotalKm);
      saveData(9, (long)tm.Month);
    }
  }
  
}

void shutDown(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Shutting down...");
  
  saveData(0, totalKm);
  saveData(1, hireTotalKm);
  saveData(3, income);
  
  delay(3000);
  digitalWrite(LCDPOWER, LOW);
  lcd.clear();
  
  working = false;
}

void startButton(){
    hireDistance = 0;
    fare = STARTING_FARE;
    onHire = true;
    income += 50;
}

void endButton(){
  delay(50);
  if(!isLongPressed(END)){
    if(onHire){
      onHire = false;
      saveData(1, hireTotalKm);
      saveData(3, income);
    }
  }
  else{
    if(onHire){
      onHire = false;
      saveData(1, hireTotalKm);
      saveData(3, income);
    }
    shutDown();
  }
}

void reset(){
  commandSent = false;
  inputWait = 3000; //wait for 3 seconds
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press button");
  lcd.setCursor(0, 1);
  lcd.print("to reset");
  
  while(!commandSent){
    if((digitalRead(START) == LOW) && onHire){
      resetHire();
    }
    
    if(!commandSent){
      --inputWait;
    }
    
    if(inputWait <= 0){
      commandSent = true;
    }
    lcd.clear();
  }
}

void resetHire(){
  menuShowTime = millis();
  
  long day_LR,month_LR,year1_LR, year2_LR, hour_LR, minute_LR;
  onHire = false;
  if(RTC.read(tm)){
    day_LR = tm.Day;
    month_LR = tm.Month;
    year1_LR = tmYearToCalendar(tm.Year);
    hour_LR = tm.Hour;
    minute_LR = tm.Minute;
    
    year2_LR = year1_LR % 100;
    year1_LR /= 100; 
  
    saveData(10, day_LR);
    saveData(11, month_LR);
    saveData(12, year1_LR);
    saveData(13, year2_LR);
    saveData(14, hour_LR);
    saveData(15, minute_LR);
  }
        
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hire Reset");
  lcd.setCursor(0, 1);
  lcd.print("Complete");
  while((millis() - menuShowTime) < 1000);
}

void showLastResetTime(){
  menuShowTime = millis();
  long day_LR,month_LR,year1_LR, year2_LR, hour_LR, minute_LR;
  
  day_LR = readLong(10);
  month_LR = readLong(11);
  year1_LR = readLong(12);
  year2_LR = readLong(13);
  hour_LR = readLong(14);
  minute_LR = readLong(15);
  
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print(hour_LR);
  lcd.print(":");
  lcd.print(minute_LR);
  
  lcd.setCursor(0, 1);
  lcd.print(day_LR);
  lcd.print("-");
  lcd.print(month_LR);
  lcd.print("-");
  lcd.print(year1_LR);
  lcd.print(year2_LR);
  while((millis() - menuShowTime) < 5000);
}

void reportButton(){
  delay(50);
  if(!isLongPressed(REPORT)){
    showDailyReport();
  }
  else{
    showMonthlyReport();
  }
}

void odometerPressed(){
  delay(50);
  if(!isLongPressed(ODOMETER)){
    showTotalDistance();
  }
  else{
    clearDistance();
  }
}

void showDailyReport(){
  menuShowTime = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(income-readLong(4));
  lcd.print(" LKR");
  lcd.setCursor(0, 1);
  lcd.print(hireTotalKm-readFloat(5));
  lcd.print(" km");
  while(digitalRead(RESET) == HIGH && millis() - menuShowTime < 5000);
  delay(600);
  lcd.clear();
}

void showMonthlyReport(){
  menuShowTime = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(income-readLong(6));
  lcd.print(" LKR");
  lcd.setCursor(0, 1);
  lcd.print(hireTotalKm-readFloat(7));
  lcd.print(" km");
  while(digitalRead(RESET) == HIGH && millis() - menuShowTime < 5000);
  delay(600);
  lcd.clear();
}

void showTotalDistance(){
  menuShowTime = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Total distance");
  lcd.setCursor(0, 1);
  lcd.print(totalKm);
  lcd.print(" km");
  while(digitalRead(RESET) == HIGH && millis() - menuShowTime < 5000);
  delay(600);
  lcd.clear();
}

void clearDistance(){
  saveData(2,totalKm);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reset Suceeded!");
  delay(2000);
  lcd.clear();
}

void loop() {
  
  //power up taxi meter
  while(!working){
    if(digitalRead(START) == LOW){
      digitalWrite(LCDPOWER, HIGH);
      delay(50);
      if(isLongPressed(START)){
        boot();
      }
      else
        digitalWrite(LCDPOWER, LOW);
    }
  }
  
  time2 = millis()-time1;
  
  if(time2 >= 1000){
    vehicleSpeed = pulseCounter*3600/time2;
    if(vehicleSpeed == 0){
      waiting = true;
      waitingClock = millis();
    }
    else
      waiting = false;
      
    time1 = millis();
    pulseCounter = 0;    
  }
  
  if(meters >= 100){
    meters = 0;
    totalKm += 0.1;
    
    if(onHire){
      hireDistance += 0.1;
      hireTotalKm += 0.1;
    }
    
    if(hireDistance > 1){
      RTC.read(tm);
      if(tm.Hour > 21 && tm.Hour < 5){
        fare += FARE_NIGHT/10;
        income += FARE_NIGHT/10;
      }
      else{
        fare += FARE/10;
        income += FARE/10;
      }
    }
  }
  
  lcd.setCursor(0, 0);
  
  if(onHire){
    lcd.print(hireDistance);
    lcd.print(" km ");
  
    lcd.print(fare);
    lcd.print("LKR      ");  
    
  }
  else{
    lcd.clear();
    
    if (RTC.read(tm)){
      //print day
      lcd.setCursor(0, 1);
      lcd.print(tm.Day);
      lcd.print("-");
      lcd.print(tm.Month);
      lcd.print("-");
      lcd.print(tmYearToCalendar(tm.Year));    
    
      //print time
      lcd.setCursor(0, 0);      
      RTC.read(tm);
      if (tm.Hour >= 0 && tm.Hour < 10)
        lcd.print("0");
      lcd.print(tm.Hour);
      lcd.print(":");
      
      if (tm.Minute >= 0 && tm.Minute < 10)
        lcd.print("0");
      lcd.print(tm.Minute);
      lcd.print(":");
      
      if (tm.Second >= 0 && tm
      .Second < 10)
        lcd.print("0");
      lcd.print(tm.Second); 
    
      delay(200);
    }
  }
  
  lcd.setCursor(0, 1);
  if(onHire && waiting){
    if(millis() - waitingClock >= 30000){
      fare += WAITING_FARE/10;
      income += WAITING_FARE/10;
      waitingClock = millis();
    }
    lcd.print("Waiting...");
  }
  else{
    lcd.print(vehicleSpeed);
    lcd.print("km/h        ");
  }
  
  //option buttons
  if(!onHire){
    if(digitalRead(START) == LOW)
      startButton();
    else if(digitalRead(REPORT) == LOW)
      reportButton();
    else if(digitalRead(ODOMETER) == LOW)
      odometerPressed();
  }
  if(digitalRead(RESET) == LOW){
    if(!isLongPressed(RESET))
      reset();
    else
      showLastResetTime();
  }
  
  if(digitalRead(END) == LOW)
    endButton();
}

bool isLongPressed(int pin){
  longPress = 300;
  while(digitalRead(pin) == LOW){
    Serial.println(longPress);
    --longPress;
    if(longPress < 0)
      longPress = 0;
  }
  if(longPress > 100)
    return false;
  else
    return true;
}

void saveData(int addr, float data){
  saveData(addr,(long)data*10);
}

void saveData(int addr, long data){
  for(int i = addr*4; i < addr*4 + 4; ++i){
    EEPROM.write(i, data & 0xFF);
    data = data >> 8;
  }
}

long readLong(int addr){
  long result = 0;  
  for(int i = addr*4 + 3; i >= addr*4; --i){
    result = (result << 8) + EEPROM.read(i);
  }  
  return result;
}

float readFloat(int addr){
  return readLong(addr)/10.0;
}
