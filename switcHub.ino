//switcHub
//Avi Kazen
//3.12.2017

//EEPROM layout: 
//Adress 0: int holding number of switches
//address 161 to 25*161: members of class Outlet holding all  user data for switches

//enhancement ideas:
//add "profile" mode
//toggle timer active/disabled
//oscillator mode

#include <Wire.h>
#include <RCSwitch.h>
#include <TimeLib.h>
#define eeprom1 0x50    //Address of 24LC256 eeprom chip


RCSwitch mySwitch = RCSwitch();
RCSwitch myReceiver = RCSwitch();
int numSwitches;//number of switches currently loaded
const int maxSwitches=100;//max number of switches
const int numSlots=5; //max number of slots we can add. Limited by memory.
boolean debug=false;
boolean onFlag[maxSwitches];

class Outlet
{
   public:
   char Name[10];
   unsigned long on_code; 
   unsigned long off_code;
   int bitLength;
   int pulseWidth;
   int protocol;
   byte slotTimes[7][numSlots][2][2]; 
   
   Outlet(){ //the constructor
     return;
   }
   
   void On(){ //a function
     mySwitch.setPulseLength(pulseWidth);
     mySwitch.send(on_code,bitLength);
     return;
   }
    void Off(){ //a function
     mySwitch.setPulseLength(pulseWidth);
     mySwitch.send(off_code,bitLength);
     return;
   }

   //the next two functions are for manageing "timer slots". Each switch has five slots for each day of the week. Each slot consists of one on time and one off time, and stores an hour and a minute.
   void addTimeSlot(int D,int onH,int onM,int offH,int offM){
    for(byte i = 0; i < numSlots; i++){
      if (slotTimes[D][i][0][0]==255){ //find next unused slot. 255 is unused
          slotTimes[D][i][0][1]=onH;
          slotTimes[D][i][0][0]=onM;
          slotTimes[D][i][1][1]=offH;
          slotTimes[D][i][1][0]=offM;
          return;
        }
        else if(i==numSlots-1){
          Serial.println(F("All slots taken. Please delete a timer to add another."));
        }
    }
    return;
  }

  void deleteTimeSlot(int D,int slotNumber){ //255 is unused slot
    slotTimes[D][slotNumber][0][1]=255;
    slotTimes[D][slotNumber][0][0]=255;
    slotTimes[D][slotNumber][1][1]=255;
    slotTimes[D][slotNumber][1][0]=255;
    return;
  }
};

//Outlet outlets[1]; //outlets stored  on EEPROM!!
Outlet currentOutlet; //temp outlet loaded from EEPROM
void setup() {
  Serial.begin(9600);
  
  //Serial.print("size: ");
  //Serial.println(sizeof(currentOutlet)); //uncomment to see size of outlet data structure
  
  Wire.begin();  
  mySwitch.enableTransmit(10);
  myReceiver.enableReceive(0); //digital pin 2
  //mySwitch.setRepeatTransmit(15);
  setTime(0, 0, 0, 0, 0, 1999);

  delay(100);
  mainScreen();
  EEPROM_readAnything(eeprom1, 0, numSwitches); //load the number of switches from address zero
  if (numSwitches>maxSwitches){
    numSwitches=0;
  }
}

void loop() {
 bluetoothInterface();
 if (millis()%60000<5){ //prevent repeated sending by triggering only once a minute
 scanSwitches(); //scans and flips switches using time library
 }
}

void  scanSwitches(){ //cannot set timer for 00:00 midnight exactly!
 for (int switchNum=0;switchNum<numSwitches;switchNum++){
   currentOutlet=loadOutlet(switchNum);
   for (byte slotNum=0; slotNum<numSlots; slotNum++){
     if (currentOutlet.slotTimes[weekday()-1][slotNum][0][1]==hour()){
        if (currentOutlet.slotTimes[weekday()-1][slotNum][0][0]==minute()){
            currentOutlet.On();
            onFlag[switchNum]=true;
         }
      }
      if (currentOutlet.slotTimes[weekday()-1][slotNum][1][1]==hour()){
         if (currentOutlet.slotTimes[weekday()-1][slotNum][1][0]==minute()){
            currentOutlet.Off();
            onFlag[switchNum]=false;
         }
      }
    }
  }
}

void bluetoothInterface(){
bool aTimerSet=false;
bool timerFlag=false;  
byte mode;
if(Serial.available()){
 mode=Serial.read();
switch (mode) {
    case '1'://Toggle switches?
      toggleSwitchScreen();
      break;
    case '2'://Set timers?
      setTimers();
      break;
    case '3'://delete timers?
     deleteTimers();
      break;
    case '4'://delete timers?
     setSystemTime();
      break;
    case '5':
      Serial.println("Loading...");
      for (byte D=0; D<7; D++){
        for(byte j = 0; j < numSwitches; j++){
          timerFlag=spillSlots(D,j);
          if (timerFlag){aTimerSet=true;} //a second timer set flag is required because we want this to be true if ANY iterations return true
          }
        }
      if (!aTimerSet){
        Serial.println();
        Serial.println(F("No timers currently set"));
      }
      mainScreen();
    break;
    case '6':
      learningMode();
    break;
    case '7':
    {
     Serial.println(F("This will clear all of your current switch information."));
     Serial.println(F("Enter R to proceed, anything else to cancel."));
     long checkPoint=millis();
     while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
       if(Serial.available()){
        int confirmation=Serial.read();
        if (confirmation=='R'||confirmation=='r'){
          reset();
          Serial.println(F("Reset complete. \n"));
        }
        backToMain(checkPoint);
      }
     }
    }
    break;
    default: 
      // if invalid input, print the instructions
      Serial.println(F("Invalid selection"));
      mainScreen();
    break;
   }
  }
}
void toggleSwitchScreen(){
  char mode2;
  long checkPoint=millis();
  printNames();
  if(numSwitches==0){
    mainScreen();
    return;
  }
  Serial.println(F("Enter number for switch to toggle"));
  Serial.println(F("Enter A for all switches on"));
  Serial.println(F("Enter B for all switches off"));
  Serial.println(F("Enter Z for main menu"));
  while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
    if(Serial.available()){
      mode2=Serial.read();
      Serial.println(mode2);
      if ((mode2>=48)&&(mode2<48+numSwitches)){
        int switchNum=mode2-48;
        currentOutlet=loadOutlet(switchNum);
         if (onFlag[switchNum]==true){ //subtract 48 to convert from ascii numbers to actual numbers
            currentOutlet.Off();
            onFlag[switchNum]=false;
            Serial.print(F("Outlet "));
            Serial.print(mode2-48);
            Serial.println(F(" off"));
            //checkPoint=millis(); //re-extend 30 second timout timer due to activity
            backToMain(checkPoint);
          }
          else {
            currentOutlet.On();
            onFlag[switchNum]=true;
            Serial.print(F("Outlet "));
            Serial.print(mode2-48);
            Serial.println(F(" on"));
            //checkPoint=millis(); //re-extend 30 second timout timer due to activity
            backToMain(checkPoint);
          }
      }
      else if(mode2=='A'||mode2=='a'){//all switches on
          for (int switchNum=0;switchNum<numSwitches;switchNum++){
            currentOutlet=loadOutlet(switchNum);
            currentOutlet.On();
            onFlag[switchNum]=true;
          }
          Serial.println(F("All on"));
          backToMain(checkPoint);
      }
      else if(mode2=='B'||mode2=='b'){//all switches off
         for (int switchNum=0;switchNum<numSwitches;switchNum++){
            currentOutlet=loadOutlet(switchNum);
            currentOutlet.Off();
            onFlag[switchNum]=false;
          }
          Serial.println(F("All off"));
          backToMain(checkPoint);
      }
       else if(mode2=='Z'||mode2=='z'){//back to main
          backToMain(checkPoint);
       }
      else{
            // if invalid input, print the instructions
            Serial.println(F("Invalid selection"));
            Serial.println(F("Enter number for switch to toggle"));
            Serial.println(F("Enter A for all switches on"));
            Serial.println(F("Enter B for all switches off"));
            Serial.println(F("Enter Z for main menu"));
      }
    }
  }
}

void setTimers(){
  int switchNums[numSwitches];
  int numToSet;
  int DoW;
  byte hourOn;
  byte hourOff;
  byte minuteOn;
  byte minuteOff;
  byte stage=1; //keep track of ehre the user is at
  long checkPoint=millis();
  printNames();
  if(numSwitches==0){
    mainScreen();
    return;
  }

  Serial.println(F("Enter number for switch to set"));
  Serial.println("Enter Z at any time to quit");

  while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
    switch (stage) {
    case 1:
      numToSet=getSwitchNumber(checkPoint,stage,switchNums);
    break;
    case 2:      
      DoW=getDay(checkPoint,stage);
    break;
    case 3:      
      hourOn=getOnHour(checkPoint,stage);
    break;
    case 4:      
      minuteOn=getOnMinute(checkPoint,stage);
    break;
    case 5:      
      hourOff=getOffHour(checkPoint,stage);
    break;
    case 6:      
       minuteOff=getOffMinute(checkPoint,stage);
    break;
    case 7:  
    {
          String minuteOnStr=String(minuteOn);
          String minuteOffStr=String(minuteOff);
          if (minuteOn<10){
            minuteOnStr=String("0")+minuteOn;
          }
          if (minuteOff<10){
            minuteOffStr=String("0")+minuteOff;
          }
          String confirmationMessage;
          //confirmationMessage.reserve(70);
          confirmationMessage="Switch(es) ";
              for (int j=0; j<numToSet; j++){
                if (switchNums[j]>=0&&switchNums[j]<numSwitches){
                  confirmationMessage=confirmationMessage+switchNums[j]+", ";
                }
              }
          confirmationMessage=confirmationMessage+"\nset to turn on at "+hourOn+":"+minuteOnStr+" and set to turn off at "+hourOff+":"+minuteOffStr; 
          Serial.print(confirmationMessage);
          if (DoW=='A'||DoW=='a'){
              Serial.println(" everyday");
            }
            else if(DoW=='W'||DoW=='w'){//back to main
              Serial.println(" on weekdays");
            }
            else if(DoW=='E'||DoW=='e'){//back to main
              Serial.println(" on weekends");
            }
            else{//back to main
              Serial.print(" on ");
              Serial.print(dayStr(DoW+1));
              Serial.println("s");
            }
          Serial.println(F("Enter 1 to confirm. Enter anything else to cancel."));
          stage++;
    }
    case 8:
      {
       if(Serial.available()){
          int confirmation=Serial.read();
          if (confirmation=='1'){
             Serial.println(F("Please wait...\n"));
            for (int j=0; j<numToSet; j++){
              if (switchNums[j]>=0&&switchNums[j]<numSwitches){
                currentOutlet=loadOutlet(switchNums[j]);
                if (DoW=='A'||DoW=='a'){
                  for (int D=0;D<7;D++){
                    currentOutlet.addTimeSlot(D,hourOn,minuteOn,hourOff,minuteOff);
                  }
                }
                else if(DoW=='W'||DoW=='w'){//back to main
                  for (int D=1;D<6;D++){
                    currentOutlet.addTimeSlot(D,hourOn,minuteOn,hourOff,minuteOff);
                  }
                }
                else if(DoW=='E'||DoW=='e'){//back to main
                   currentOutlet.addTimeSlot(0,hourOn,minuteOn,hourOff,minuteOff);
                   currentOutlet.addTimeSlot(6,hourOn,minuteOn,hourOff,minuteOff);
                }
                else{
                  currentOutlet.addTimeSlot(DoW,hourOn,minuteOn,hourOff,minuteOff);
                }
                saveOutlet(switchNums[j],currentOutlet);
              }
            }
            backToMain(checkPoint);
          }
         else{backToMain(checkPoint);}//back to main
       }
     }
    break;
    default:
      backToMain(checkPoint);
    break;
    }
  }
}
    
int getSwitchNumber(long &checkPoint, byte &stage, int switchNums[]){
    int switchRead;
    int i=0;
    String inString;
    String copyString;
    boolean continueFlag=false; //flag to trigger moving on before numSwitches reached
    checkPoint=millis();
    while ((millis()-checkPoint)<30000){
      if(Serial.available()){
        inString=Serial.readString();
        copyString=inString;
        switchRead=inString.toInt();
        checkPoint=millis(); //re-extend 30 second timout timer due to activity
        if (inString=="Y"||copyString=="y"){
          if (i>0){
            continueFlag=true;
          }
          else{
            Serial.println("Must enter at least one switch.");
          }
        }
        else if (inString=="Z"||copyString=="z"){backToMain(checkPoint);}
        else if ((switchRead>=0)&&(switchRead<numSwitches)&&(switchRead!=switchNums[i-1])&&(i!=numSwitches)){
         switchNums[i]=switchRead;
         Serial.print(F("Switch "));
         Serial.println(switchNums[i]);
         i++; //i is always one more than valid switch entries
         if (i!=numSwitches){
          Serial.println(F("Enter an additional switch to set, or enter Y to continue \n"));
          
         }
        }
        else if (switchRead==switchNums[i-1]){
          Serial.println(F("Must enter a different switch, or enter Y to continue \n"));
        }
        else{
            // if invalid input, print the instructions
            Serial.println(F("Invalid selection"));
            Serial.println(F("Enter number for switch to set"));
            if(i>0){
              Serial.println(F("Or enter Y to continue"));
            }
            Serial.println(F("Enter Z for main menu"));
         }
         if (i==numSwitches||continueFlag){
            if (switchNums[0]>=0){
              if (i>1){
              Serial.print(F("Switch numbers "));
              }
              else{
                Serial.print(F("Switch number "));
              }
              for (int j=0; j<i; j++){
                Serial.print(switchNums[j]);
                Serial.print(", ");
              }
              Serial.println();
              Serial.println(F("Enter day of week (1-7 for sun-sat)"));
              Serial.println(F("Enter A for all"));
              Serial.println(F("Enter W for weekdays"));
              Serial.println(F("Enter E for weekdends"));
              Serial.println(F("Enter Z for main menu"));
              stage++;
              return i;
          }
        }
     }
  }
}
int getDay(long &checkPoint, byte &stage){
    if(Serial.available()){ 
      int DoW=Serial.read();
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((DoW>=49)&&(DoW<49+8)){ //verify number
          DoW-=49;
          Serial.println(dayStr(DoW+1));
          stage++;
          Serial.println(F("Enter hour of day to turn on(0-23)"));
          return DoW;
      }
      else if(DoW=='A'||DoW=='a'){//back to main
        Serial.println(F("Everyday"));
          stage++;
          Serial.println(F("Enter hour of day to turn on(0-23)"));
          return DoW;
      }
      else if(DoW=='W'||DoW=='w'){//back to main
        Serial.println(F("Weekdays"));
          stage++;
          Serial.println(F("Enter hour of day to turn on(0-23)"));
          return DoW;
      }
      else if(DoW=='E'||DoW=='e'){//back to main
        Serial.println(F("Weekends"));
          stage++;
          Serial.println(F("Enter hour of day to turn on(0-23)"));
          return DoW;
      }
      else if(DoW=='Z'||DoW=='z'){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.println(F("Invalid selection"));
          Serial.println(F("Enter day of week (1-7 for sun-sat)"));
          Serial.println(F("Enter A for all;"));
          Serial.println(F("Enter W for weekdays;"));
          Serial.println(F("Enter E for weekdends"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }

  int getOnHour(long &checkPoint, byte &stage){
    String inString = "";
    if(Serial.available()){
         inString=Serial.readString();
      int hourOn=inString.toInt(); //convert string to ints
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((hourOn>=0)&&(hourOn<25)){ //verify number
          Serial.println(hourOn);
          stage++;
          Serial.println(F("Enter minute of hour to turn on (0-59)"));
          return hourOn;
      }
      else if(inString=="Z"||inString=="z"){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.print(F("Invalid selection: "));
          Serial.println(hourOn);
          Serial.println(F("Enter hour of day to turn on(0-23)"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }

  int getOnMinute(long &checkPoint, byte &stage){
    String inString = "";
    if(Serial.available()){
         inString=Serial.readString();
      int minuteOn=inString.toInt(); //convert string to ints
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((minuteOn>=0)&&(minuteOn<60)){ //verify number
          Serial.println(minuteOn);
          stage++;
          Serial.println(F("Enter of hour of day to turn off (0-23)"));
          return minuteOn;
      }
      else if(inString=="Z"||inString=="z"){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.print(F("Invalid selection: "));
          Serial.println(minuteOn);
          Serial.println(F("Enter minute of hour to turn on (0-59)"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }

  int getOffHour(long &checkPoint, byte &stage){
    String inString = "";
    if(Serial.available()){
         inString=Serial.readString();
      int hourOff=inString.toInt(); //convert string to ints
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((hourOff>=0)&&(hourOff<25)){ //verify number
          Serial.println(hourOff);
          stage++;
          Serial.println(F("Enter minute of hour to turn off (0-59)"));
          return hourOff;
      }
      else if(inString=="Z"||inString=="z"){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.print(F("Invalid selection: "));
          Serial.println(hourOff);
          Serial.println(F("Enter hour of day to turn off(0-23)"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }

  int getOffMinute(long &checkPoint, byte &stage){
    String inString = "";
    if(Serial.available()){
      inString=Serial.readString();
      int minuteOff=inString.toInt(); //convert string to ints
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((minuteOff>=0)&&(minuteOff<60)){ //verify number
          Serial.println(minuteOff);
          stage++;
          return minuteOff;
      }
      else if(inString=="Z"||inString=="z"){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.print(F("Invalid selection: "));
          Serial.println(minuteOff);
          Serial.println(F("Enter minute of hour to turn Off (0-59)"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }
  
void backToMain(long &checkPoint){ //breaks from submodes back to main
          checkPoint=millis()-30001; //break 30 second timer
          mainScreen();
}

void mainScreen(){
  Serial.println();
  printSystemTime();
  Serial.println(F("1-Toggle switches"));
  Serial.println(F("2-Set timers"));
  Serial.println(F("3-Delete timers"));
  Serial.println(F("4-Set system time"));
  Serial.println(F("5-Show current timers"));
  Serial.println(F("6-Add new switch")); 
  Serial.println(F("7-Full Reset")); 
}

void printSystemTime(){
  Serial.print(F("Current system clock: "));
  Serial.print(hour());
  Serial.print(":");
  if (minute()<10){
    Serial.print(0);
  }
  Serial.print(minute());
  Serial.print(" ");
  Serial.println(dayStr(weekday()));
}



bool spillSlots(int DoW, int switchNum){
  bool aTimerSet=false; //a flag for determining if no timers are set
  currentOutlet=loadOutlet(switchNum);
        for(byte i = 0; i < numSlots; i++){
          if (currentOutlet.slotTimes[DoW][i][0][0]!=255){ //find next unused slot.
            Serial.println();
            Serial.print(F("Day: "));
            Serial.println(dayStr(DoW+1));
            Serial.print(F("  Switch: "));
            Serial.println(switchNum);
            Serial.print(F("  Slot number:  "));
            Serial.println(i);
            Serial.print(F("    On at: "));
            Serial.print(currentOutlet.slotTimes[DoW][i][0][1]);
            Serial.print(F(":"));
              if (currentOutlet.slotTimes[DoW][i][0][0]<10){     //2/2/2018 - display bug fix
              Serial.print(0);
            }
            Serial.println(currentOutlet.slotTimes[DoW][i][0][0]);
            Serial.print(F("    Off at:"));
            Serial.print(currentOutlet.slotTimes[DoW][i][1][1]);
            Serial.print(F(":"));
            if (currentOutlet.slotTimes[DoW][i][1][0]<10){
              Serial.print(0);
            }
            Serial.println(currentOutlet.slotTimes[DoW][i][1][0]);
            aTimerSet=true;
    }
  }
 return aTimerSet;
}



void deleteTimers(){
  bool aTimerSet;
  int DoW;
  int slot;
  int switchNum = 0;
  long checkPoint=millis();
  printNames();
  if(numSwitches==0){
    mainScreen();
    return;
  }
  Serial.println(F("Enter number of switch the timer is on"));
  Serial.println(F("Enter 'A' to clear all timers"));
  Serial.println(F("Enter 'Z' to go back"));
  while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
    if(Serial.available()){
      switchNum=Serial.read();
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if (switchNum=='A'||switchNum=='a'){ //verify number
          clearAllTimers();
          backToMain(checkPoint);
      }
      else if(switchNum=='Z'||switchNum=='z'){//back to main
        backToMain(checkPoint);
       }
      else if((switchNum>=48)&&(switchNum<48+numSwitches)){
          switchNum-=48;
          checkPoint=millis(); //re-extend 30 second timout timer due to activity
          Serial.println(F("Enter number for day of week to remove timer (1-7 for sun-sat)"));
          Serial.println(F("Enter A to clear all slots on the current switch"));
          Serial.println(F("Enter 'Z' to go back"));
          while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
            if(Serial.available()){
              DoW=Serial.read();
              checkPoint=millis(); //re-extend 30 second timout timer due to activity
              if((DoW>=49)&&(DoW<49+7)){
                DoW-=49;
                 aTimerSet=spillSlots(DoW,switchNum);
                 if (!aTimerSet){
                    Serial.println();
                    Serial.println(F("No timers currently set for that day"));
                    Serial.println(F("Enter number for day of week to remove timer (1-7 for sun-sat)"));
                    Serial.println(F("Enter 'Z' to go back"));
                  }
                  else{
                    Serial.println(F("Enter the slot number from the above timers to delete"));
                    Serial.println(F("Enter 'Z' to go back"));
                    while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
                        if(Serial.available()){
                          slot=Serial.read();
                          checkPoint=millis(); //re-extend 30 second timout timer due to activity
                          if((slot>=48)&&(slot<48+numSlots)){
                            slot-=48;
                            currentOutlet=loadOutlet(switchNum);
                            currentOutlet.deleteTimeSlot(DoW,slot);
                            saveOutlet(switchNum,currentOutlet); 
                            Serial.println(F("Timer deleted"));
                            backToMain(checkPoint);
                          }
                          else if (slot=='Z'||slot=='z'){
                               backToMain(checkPoint);
                          }
                          else{
                           Serial.println(F("Invalid selection"));
                           Serial.println(F("Enter the slot number from the above timers to delete"));
                           Serial.println(F("Enter 'Z' to go back"));
                          }
                       }   
                  }
                }
              }
              
              else if (DoW=='A'||DoW=='a'){
                Serial.println(F("Please wait..."));
                currentOutlet=loadOutlet(switchNum);
                for (byte D=0; D<7; D++){
                  for(byte i = 0; i < numSlots; i++){
                    currentOutlet.deleteTimeSlot(D,i);
                  }
                }
                saveOutlet(switchNum,currentOutlet);
                Serial.println(F("Timers deleted"));
                backToMain(checkPoint);
              }    
              
              else if (DoW=='Z'||DoW=='z'){
                backToMain(checkPoint);
              }
              else{
                Serial.println(F("Invalid Selection"));
                Serial.println(F("Enter number for day of week to remove timer (1-7 for sun-sat)"));
                Serial.println(F("Enter A to clear all slots on the current switch"));
                Serial.println(F("Enter 'Z' to go back")); 
              }
            }
          }
       }
       else{
          // if invalid input, print the instructions
          Serial.println(F("Invalid selection: "));
          Serial.println(F("Enter number of switch the timer is on"));
          Serial.println(F("Enter 'A' to clear all timers"));
          Serial.println(F("Enter 'Z' to go back"));
      }
    }
  }
}


void setSystemTime(){
  int DoW;
  byte h;
  byte m;
  byte stage=1; //keep track of where the user is at
  long checkPoint=millis();
  Serial.println(F("Enter Day of week (1-7 for sun-sat)"));
  Serial.println(F("Enter Z for main menu"));
  while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
    switch (stage) {
    case 1:       
      DoW=getSystemDay(checkPoint,stage);
    break;
    case 2:      
      h=getSystemHour(checkPoint,stage);
    break;
    case 3:      
      m=getSystemMinute(checkPoint,stage);
    break;
    case 4:  
    {
          String minuteOnStr=String(m);
          if (m<10){
            minuteOnStr=String("0")+m;
          }
          String confirmationMessage;
          confirmationMessage.reserve(70);
          confirmationMessage=String("System time set to ")+dayStr(DoW+1)+" at "+h+":"+minuteOnStr;
          Serial.println(confirmationMessage);
          Serial.println(F("Enter 1 to confirm. Enter anything else to cancel."));
          stage++;
    }
    case 5:
      {
       if(Serial.available()){
          int confirmation=Serial.read();
          if (confirmation=='1'){
            setTime(h, m, 0, 10+DoW, 1, 1999);
            backToMain(checkPoint);
          }
          else{backToMain(checkPoint);}
       }
      }
    break;
    default:
      backToMain(checkPoint);
    break;
    }
  }
}

int getSystemDay(long &checkPoint, byte &stage){
    if(Serial.available()){ 
      int DoW=Serial.read();
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((DoW>=49)&&(DoW<49+8)){ //verify number
          DoW-=49;
          Serial.println(dayStr(DoW+1));
          stage++;
          Serial.println(F("Enter hour of day (0-23)"));
          return DoW;
      }
      else if(DoW=='Z'||DoW=='z'){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.println(F("Invalid selection"));
          Serial.println(F("Enter day of week (1-7 for sun-sat)"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }

  int getSystemHour(long &checkPoint, byte &stage){
    String inString = "";
    if(Serial.available()){
         inString=Serial.readString();
      int hourOn=inString.toInt(); //convert string to ints
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((hourOn>=0)&&(hourOn<25)){ //verify number
          Serial.println(hourOn);
          stage++;
          Serial.println(F("Enter minute of hour (0-59)"));
          return hourOn;
      }
      else if(inString=="Z"||inString=="z"){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.print(F("Invalid selection: "));
          Serial.println(hourOn);
          Serial.println(F("Enter hour of day (0-23)"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }

  int getSystemMinute(long &checkPoint, byte &stage){
    String inString = "";
    if(Serial.available()){
         inString=Serial.readString();
      int minuteOn=inString.toInt(); //convert string to ints
      checkPoint=millis(); //re-extend 30 second timout timer due to activity
      if ((minuteOn>=0)&&(minuteOn<60)){ //verify number
          Serial.println(minuteOn);
          stage++;
          return minuteOn;
      }
      else if(inString=="Z"||inString=="z"){//back to main
        stage=99;
       }
       else{
          // if invalid input, print the instructions
          Serial.print(F("Invalid selection: "));
          Serial.println(minuteOn);
          Serial.println(F("Enter minute of hour (0-59)"));
          Serial.println(F("Enter Z for main menu"));
      }
    }
  }

  void learningMode(){
  int switchNum;
  int back;
  printNames();
  long checkPoint=millis(); //re-extend 30 second timout timer due to activity
   if (numSwitches>1){
     Serial.print(F("Enter number for the switch to overwrite(0-"));
     Serial.print(numSwitches-1);
     Serial.println(")");
     Serial.print(F("Or enter "));
     Serial.print(numSwitches);
     Serial.println(" to add a new switch.");
     Serial.println(F("Enter 'Z' to go back"));
   }
   else if (numSwitches>0){
     Serial.print(F("Enter 0 to overwrite existing switch ")); 
     Serial.print(F("or enter "));
     Serial.print(numSwitches);
     Serial.println(" to add a new switch.");
     Serial.println(F("Enter 'Z' to go back"));
   }
   
  while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
    if(Serial.available()||numSwitches==0){
      if (numSwitches>0){
        String inString=Serial.readString();
        switchNum=inString.toInt();
        if(inString=="Z"||inString=="z"){//back to main
          backToMain(checkPoint);
        }
        else if((switchNum>=0)&&(switchNum<=numSwitches)){
          checkPoint=millis(); //re-extend 30 second timout timer due to activity
          getName(checkPoint,switchNum);
        }
        else{
          if (numSwitches>1){
             Serial.print(F("Enter number for the switch to overwrite(0-"));
             Serial.print(numSwitches-1);
             Serial.println(")");
             Serial.print(F("Or enter "));
             Serial.print(numSwitches);
             Serial.println(" to add a new switch.");
           }
           else if (numSwitches=1){
             Serial.print(F("Enter 0 to overwrite existing switch ")); 
             Serial.print(F("or enter "));
             Serial.print(numSwitches);
             Serial.println(" to add a new switch.");
           }
           else{
             Serial.print(F("Enter "));
             Serial.print(numSwitches);
             Serial.println(" to add a new switch.");
          }
       } 
      }
      else{
        switchNum=0;
        checkPoint=millis(); //re-extend 30 second timout timer due to activity
        getName(checkPoint,switchNum);
      }
    }
   }
 }

 void getName(long &checkPoint,int switchNum){
  Serial.println(F("Enter name for this switch"));
  while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
    if(Serial.available()){
        char Name[10];
        int i;
        for (i=0;i<10;i++){
          if(Serial.available()){
            Name[i]=Serial.read();}
          else{Name[i]=' ';}
          delay(10);
        }
        for (i=0;i<10;i++){
            currentOutlet.Name[i]=Name[i];
        }

        saveOutlet(switchNum,currentOutlet);
        //currentOutlet=loadOutlet(1);
        //currentOutlet=loadOutlet(0);
        delay(100);
        
        Serial.print("Switch named ");
        for (i=0;i<10;i++){
          Serial.print(currentOutlet.Name[i]);
        }
        saveOutlet(switchNum,currentOutlet);
        Serial.println();
        receiveCode(checkPoint,switchNum); 
      }
   }
}
  

 void receiveCode(long &checkPoint,int switchNum){
  currentOutlet=loadOutlet(switchNum);
  mySwitch.resetAvailable();//clear the buffer
  String state="on";
  checkPoint=millis(); //re-extend 30 second timout timer due to activity
  Serial.print(F("Press the "));
  Serial.print(state);
  Serial.println(F(" button on your remote now."));
  Serial.println(F("Enter 'Z' to go back"));
  while ((millis()-checkPoint)<30000){ //timeout after 30 seconds
    if (myReceiver.available()) {
      long value=myReceiver.getReceivedValue();
      if (value == 0) {
        Serial.println(F("Unknown encoding, might not work with this switch. Please try again."));
      } 
      else{
        if (state=="off"){
          currentOutlet.off_code=value;
          Serial.print(F("Received off value: "));
          Serial.println( value );
          delay(100);
          saveOutlet(switchNum,currentOutlet);
          delay(100);
          backToMain(checkPoint);
          if(switchNum==numSwitches){
            numSwitches++;
            EEPROM_writeAnything(eeprom1, 0, numSwitches);
          }
        }
        else{
          currentOutlet.on_code=value; 
          currentOutlet.bitLength=mySwitch.getReceivedBitlength();
          currentOutlet.pulseWidth=mySwitch.getReceivedDelay();
          currentOutlet.protocol=mySwitch.getReceivedProtocol();
          state="off";
          //checkPoint=millis(); //reset 30 second timer
          Serial.print(F("Received on value: "));
          Serial.println( value );
          delay(1000);
          Serial.println();
          Serial.print(F("Press the "));
          Serial.print(state);
          Serial.println(F(" button on your remote now."));
          Serial.println(F("Enter 'Z' to go back"));
        } 
     }
     delay(300);
     mySwitch.resetAvailable();
  }
  else if(Serial.available()){
      int back=Serial.read();
      if (back=='Z'||back=='z'){
          backToMain(checkPoint);
      }
      else{
          Serial.print(F("Press the "));
          Serial.print(state);
          Serial.println(F(" button on your remote now."));
          Serial.println(F("Enter 'Z' to go back"));
      }
    }
   }
}

void printNames(){
  Serial.println();
  for (unsigned int switchNum=0;switchNum<numSwitches;switchNum++){
    currentOutlet=loadOutlet(switchNum);
    Serial.print(switchNum);
    Serial.print(" - ");
    for (int i=0;i<10;i++){ //print char array
      if (currentOutlet.Name[i]<123&&currentOutlet.Name[i]>47){
        Serial.print(currentOutlet.Name[i]);
      }
    }
    Serial.println();
  }
  if (numSwitches==0){
    Serial.println("No switches currently loaded.");
  }
  Serial.println();
}

void clearAllTimers(){
    Serial.println(F("Please wait..."));
    for(byte switchNum = 0; switchNum < numSwitches; switchNum++){
      currentOutlet=loadOutlet(switchNum);
      for (byte D=0; D<7; D++){
        for(byte i = 0; i < numSlots; i++){
           currentOutlet.deleteTimeSlot(D,i);
         }
      }
      saveOutlet(switchNum,currentOutlet);
    }
    Serial.println(F("All timers cleared"));
}

void reset(){
    clearAllTimers();
    numSwitches=0;
    EEPROM_writeAnything(eeprom1, 0, numSwitches);
}

Outlet loadOutlet(unsigned int outletNumber){ //outlets are 164 bytes
  Outlet load;
  EEPROM_readAnything(eeprom1, sizeof(currentOutlet)*(outletNumber+1), load);
  delay(50);
  return load;
}

void saveOutlet(unsigned int outletNumber,Outlet save){ //outlets are 164 bytes
  EEPROM_writeAnything(eeprom1, sizeof(currentOutlet)*(outletNumber+1), save);
  delay(50);
}

template <class T> int EEPROM_writeAnything(int deviceaddress, int eeaddress, const T& value){ //EEPROM_writeAnything(eeprom1, sizeof(currentOutlet)*(outletNumber+1), save)

    const byte* p = (const byte*)(const void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++){
        Wire.beginTransmission(deviceaddress);
        Wire.write((int)(eeaddress >> 8));   // MSB
        Wire.write((int)(eeaddress & 0xFF)); // LSB
        Wire.write(*p++);
        Wire.endTransmission();
        delay(5);
        eeaddress++;
        //EEPROM.update(ee++, *p++);
    };
}

template <class T> int EEPROM_readAnything(int deviceaddress, int eeaddress, T& value){// EEPROM_readAnything(eeprom1, sizeof(currentOutlet)*(outletNumber+1), load);

    byte* p = (byte*)(void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++){

      Wire.beginTransmission(deviceaddress);
      Wire.write((int)(eeaddress >> 8));   // MSB
      Wire.write((int)(eeaddress & 0xFF)); // LSB
      Wire.endTransmission();
      Wire.requestFrom(deviceaddress,1);
      if (Wire.available()) *p++ = Wire.read();
      eeaddress++;
    }
}
