#include <SPI.h>
#include <SparkFunDS3234RTC.h>                                        // include libs, adafruit pcd lib is modified
#include <Adafruit_PCD8544.h>
#include <Adafruit_GFX.h>
#include <avr/sleep.h>

const byte RTC_CS = 9, RTC_INT = 2;                                   //ds3234 spi rtc pin definitions

Adafruit_PCD8544 display = Adafruit_PCD8544(6, 8, 7);
const byte lcdpwr = 4, lcdled = 10;

const byte b1 = A4, b2 = A3, b3 = A2, b4 = A1, b5 = A0, bint = 3;     //5 buttons & button interrupt
const byte bven = A5, bvhalf = A6;                                    //battery voltage managment pins
const byte bz = 5;                                                    //buzzer pin

//-------------------------------------------------------------------------------------------------------------------
//code data variables
byte pressed;                 //most recently pressed button
bool usdate = true;           //USA date format
byte ui = 1;                  
//current UI window selected by user, 1 = home ui, 2 = time set ui, 3 = date set ui, 4 = general set ui, 5 = confirm changes, 
//6 = timer ui 7 = stopwatch ui, 8 = general timers ui, 9 = timer set ui
byte prevui;                  //previous ui window, same id values as above
bool firstex = true;          //a window was just opened, run first execution code
byte menhoriz;                //horizontal position in a menu
float bv;                     //last read battery voltage
bool ifsleep = false;         //if the mcu was previously in the sleep ui
byte timerprev[3];            //previous timer value, tmp1 = [0] tmp2 = [1] tmp3 = [2]
byte timermain[3];            //current timer countdown, same data storage as before
bool countdown = false;       //if the timer is currently counting down
bool active;                  //if the button code in loop() should be run
bool sqwex;                   //sqw pin just been interrupted
bool countup = false;         //if the stopwatch is currently counting up (stopwatch)
byte stopmain[3];             //the time currently held in the stopwatch
byte alarmct = 0;             //number of alarms
byte alarmsdata[80];        
//data which stores the data for the 20 configurable alarms. 4 bytes are allotted to each alarm,
//[0] = hours, [1] = minutes, [2] = day(s) of the week, [3] is if it repeats or not. [2] stores the days of the week with

//temporary data variables
byte tmp1, tmp2, tmp3, tmp4;  //temporary data storage bytes
int intt1;                    //temporary data storage int
bool boolt1;                  //temporary data storage bools

//-------------------------------------------------------------------------------------------------------------------
//icon definitions

const byte PROGMEM back_i[] {
  0x20, 0x60, 0xfc, 0x66, 0x22, 0x06, 0x7c              //7x7 px
};

const byte PROGMEM timer_i[] {
  0xfc, 0x48, 0x48, 0x30, 0x48, 0x48, 0xfc              //6x7px
};

const byte PROGMEM clock_i[] {
  0x38, 0x44, 0x92, 0x92, 0x8a, 0x44, 0x38              //7x7 px
};

const byte PROGMEM alarm_i[] {
  0x10, 0x38, 0x44, 0x44, 0x44, 0xfe, 0x38              //7x7 px
};

const byte PROGMEM sleep_i[] {
  0x70, 0xe0, 0xc0, 0xe4, 0xfc, 0x78                    //6x6 px
};

const byte PROGMEM calc_i[] {
  0xff, 0x81, 0xa1, 0xf7, 0xa1, 0x81, 0xff              //8x7 px
};

const byte PROGMEM up_a[] {
  0x10, 0x38, 0x7c, 0xd6, 0x92, 0x10, 0x10              //7x7 px
};

const byte PROGMEM down_a[] {
  0x10, 0x10, 0x92, 0xd6, 0x7c, 0x38, 0x10              //7x7 px
};

const byte PROGMEM right_a[] {
  0x30, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x30              //7x7 px
};

const byte PROGMEM left_a[] {
  0x18, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x18              //7x7 px
};

const byte PROGMEM play_pause[] {
  0x8a, 0xca, 0xea, 0xca, 0x8a                          //7x5 px
};

const byte PROGMEM redo_i[] {
  0x3a, 0x66, 0xce, 0x80, 0xc4, 0x6c, 0x38              //7x7 px
};

const byte PROGMEM menu_i[] {
  0xbc, 0x00, 0xbc, 0x00, 0xbc                          //6x5 px
};

//-------------------------------------------------------------------------------------------------------------------
//function which initiates when the sqw pin goes low once per second (pps)

void sqw() {
  sqwex = true;                               //set bool to true so some code can be run in loop()
}

//-------------------------------------------------------------------------------------------------------------------
//interrupt function if a button press is read

void button() {
  if (digitalRead(b1) == 1) {       // if button 1 is pressed
    pressed = 1;
  }
  else if (digitalRead(b2) == 1) {  // if button 2 is pressed
    pressed = 2;
  }
  else if (digitalRead(b3) == 1) {  // if button 3 is pressed
    pressed = 3;
  }
  else if (digitalRead(b4) == 1) {  // if button 4 is pressed
    pressed = 4;
  }
  else if (digitalRead(b5) == 1) {  // if button 5 is pressed
    pressed = 5;
  }
  active = true;                    //set bool to true to do code in loop()
}

//-------------------------------------------------------------------------------------------------------------------
//ui to choose between timer and stopwatch

void choosetimer(){
  if (firstex == true) {        // choose between time set and date set
    ui = 8;
    firstex = false;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Choose Timers");
    display.setCursor(42, 10);
    display.print("Timer >");
    display.setCursor(18, 25);
    display.print("Stopwatch >");
    display.drawBitmap(0, 41, back_i, 7, 7, 1);     //back icon
    display.display();
    pressed = NULL;
  }
  else if (pressed == 2) {      // go to appropriate ui when corresponding button is pressed
    prevui = 8;
    firstex = true;
    timerui();
  }
  else if (pressed == 3) {
    prevui = 8;
    firstex = true;
    stopwatch();
  }
}

//-------------------------------------------------------------------------------------------------------------------
//stopwatch function

/*void alarmset(){
  if (firstex == true){
    ui = 9;
    byte sheesh = 
  }
}
*/
//-------------------------------------------------------------------------------------------------------------------
//stopwatch function

void stopwatch() {
  if (firstex == true) {
    ui = 7;
    menhoriz = 1;
    stopmain[0] = 0;
    stopmain[1] = 0;
    stopmain[2] = 0;
    countup = false;
    firstex = false;
  }
  else if (pressed == 2) {
    stopmain[0] = 0;
    stopmain[1] = 0;
    stopmain[2] = 0;
    tmp2 = 0;
    countup = false;
  }
  else if (pressed == 3) {
    countup = !countup;
  }
  pressed = NULL;                 // ensure buttons don't get toggled again when the function is called for timer countown

  display.clearDisplay();
  display.setTextSize(1);         // display "Stopwatch" at the top of screen
  display.setCursor(0, 0);

  display.print("Stopwatch");
  display.setTextSize(2);
  display.setCursor(0, 10);
  if (stopmain[0] < 10) {
    display.print('0');         // add zeros if needed, then print hours
  }
  display.print(String(stopmain[0]) + ':');
  if (stopmain[1] < 10) {
    display.print('0');         // add zeros if needed, then print minutes
  }
  display.print(String(stopmain[1]) + ':');
  display.setCursor(0, 25);
  if (stopmain[2] < 10) {
    display.print('0');         // add zeros if needed, then print seconds
  }
  display.print(stopmain[2]);
  display.drawBitmap(77, 12, redo_i, 7, 7, 1);     //redo icon
  display.drawBitmap(77, 27, play_pause, 7, 5, 1);  //play pause
  display.drawBitmap(0, 41, back_i, 7, 7, 1);       //back icon
  if (countup == true) {
    display.drawBitmap(55, 29, timer_i, 7, 7, 1);   //timer icon
  }
  display.display();
}

//-------------------------------------------------------------------------------------------------------------------
//the timer function on the watch

void timerui() {
  if (firstex == true) {            // if running for the first time
    ui = 6;
    firstex = false;
    if (countdown == true) {        // if timer is running in the background
      menhoriz = 4;                 // go straight to the timer screen
    }
    else {
      menhoriz = 1;                 // otherwise go to set timer
    }
  }

  else if (menhoriz == 3 and pressed == 1 and (timermain[0] + timermain[1] + timermain[2]) > 0) {
    timerprev[0] = timermain[0];    // if transitioning to the timer screen and there is a value in the timer
    timerprev[1] = timermain[1];    // log current timer length for the redo function
    timerprev[2] = timermain[2];
    countdown = true;               // start the timer
    menhoriz = 4;
  }
  else if (pressed == 2 and menhoriz == 4) {
    countdown = false;              // if leaving the timer screen
    timermain[0] = timerprev[0];    // use previous timer length
    timermain[1] = timerprev[1];
    timermain[2] = timerprev[2];
    menhoriz --;
  }
  else if (pressed == 1) {        // if right arrow is pressed
    if (menhoriz == 3) {}
    else {                        // change current window
      menhoriz++;
    }
  }
  else if (pressed == 2) {        // if left arrow is pressed
    if (menhoriz == 1 or menhoriz == 4) {}
    else {                        // change current window
      menhoriz --;
    }
  }
  else if (pressed == 3) {        // if play pause / up is pressed
    if (menhoriz == 1) {
      if (timermain[0] == 23) {   // if window is on hours
        timermain[0] = 0;
      }                           // increment hours
      else {
        timermain[0]++;
      }
    }

    else if (menhoriz == 2) {     // if window is on minutes
      if (timermain[1] == 59) {
        timermain[1] = 0;
      }                           // increment minutes
      else {
        timermain[1]++;
      }
    }
    else if (menhoriz == 3) {     // if window is on seconds
      if (timermain[2] == 59) {
        timermain[2] = 0;
      }                           // increment seconds
      else {
        timermain[2]++;
      }
    }
    else if (menhoriz == 4) {     // if window is on timer screen
      countdown = !countdown;     // toggle timer countdown
    }
  }
  else if (pressed == 4) {        // if down / restart is pressed
    if (menhoriz == 1) {          // if window is on hours
      if (timermain[0] == 0) {
        timermain[0] = 23;
      }                           // decrement hours
      else {
        timermain[0]--;
      }
    }
    else if (menhoriz == 2) {     // if window is on minutes
      if (timermain[1] == 0) {
        timermain[1] = 59;
      }                           // decrement minutes
      else {
        timermain[1]--;
      }
    }
    else if (menhoriz == 3) {     // if window is on seconds
      if (timermain[2] == 0) {
        timermain[2] = 59;
      }                           // decrement seconds
      else {
        timermain[2]--;
      }
    }
    else if (menhoriz == 4) {     // if window is on timer screen, restart timer using previous timer variables
      timermain[0] = timerprev[0];
      timermain[1] = timerprev[1];
      timermain[2] = timerprev[2];
    }
  }
  pressed = NULL;                 // ensure buttons don't get toggled again when the function is called for timer countown

  display.clearDisplay();
  display.setTextSize(1);         // display "timer menu" at the top of screen
  display.setCursor(0, 0);
  display.print("Timer Menu");

  if (menhoriz == 4) {            // if in timer menu
    display.setTextSize(2);
    display.setCursor(0, 10);
    if (timermain[0] < 10) {
      display.print('0');         // add zeros if needed, then print hours
    }
    display.print(String(timermain[0]) + ':');
    if (timermain[1] < 10) {
      display.print('0');         // add zeros if needed, then print minutes
    }
    display.print(String(timermain[1]) + ':');
    display.setCursor(0, 25);
    if (timermain[2] < 10) {
      display.print('0');         // add zeros if needed, then print seconds
    }
    display.print(timermain[2]);
    display.drawBitmap(77, 14, left_a, 7, 7, 1);      //left arrow
    display.drawBitmap(77, 27, play_pause, 7, 5, 1);  //play pause
    display.drawBitmap(77, 41, redo_i, 7, 7, 1);      //redo icon
    display.drawBitmap(0, 41, back_i, 7, 7, 1);       //back icon
    if (countdown == true) {
      display.drawBitmap(55, 28, timer_i, 7, 7, 1);   //timer icon
    }
    display.display();

    if (timermain[0] == 0 and timermain[1] == 0 and timermain[2] == 0) {
      countdown = false;          // if the timer has counted down all the way
      timermain[0] = timerprev[0];
      timermain[1] = timerprev[1];// reset timer
      timermain[2] = timerprev[2];
      while (digitalRead(bint) == 0) {
        tone(bz, 2400, 400);      // beep on buzzer indefinetly until someone comes around and presses something
        delay(800);
      }
      noTone(bz);                 // turn of buzzer
    }
  }

  else {
    display.setCursor(3, 16);     // if the window is on one of the set screens
    display.print("Timer Set:");  // print "timer set"
    display.setCursor(63, 21);
    display.print('>');
    display.setCursor(3, 26);
    if (timermain[0] < 10) {
      display.print('0');         // add zeros if needed, then print hours
    }
    display.print(String(timermain[0]) + ':');
    if (timermain[1] < 10) {
      display.print('0');         // add zeros if needed, then print minutes
    }
    display.print(String(timermain[1]) + ':');
    if (timermain[2] < 10) {
      display.print('0');         // add zeros if needed, then print seconds
    }
    display.print(timermain[2]);  // add square for astetics
    display.drawRect(0, 13, 60, 24, BLACK);

    display.setTextSize(1);
    if (menhoriz == 1) {
      display.setCursor(6, 9);    // put arrow to indicate which variable you are setting
      display.print('^');
    }
    else if (menhoriz == 2) {
      display.setCursor(22, 9);
      display.print('^');
    }
    else if (menhoriz == 3) {
      display.setCursor(43, 9);
      display.print('^');
    }
    arrows();
    display.display();          // show buttons
  }
}

//-------------------------------------------------------------------------------------------------------------------
//intermediary ui where you choose what settings to change

void chooseset() {
  if (firstex == true) {        // choose between time set and date set
    ui = 4;
    firstex = false;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Time/Date Set");
    display.setCursor(20, 10);
    display.print("Time Set >");
    display.setCursor(20, 25);
    display.print("Date Set >");
    display.drawBitmap(0, 41, back_i, 7, 7, 1);     //back icon
    display.display();
  }
  else if (pressed == 2) {      // go to appropriate ui when corresponding button is pressed
    prevui = 4;
    firstex = true;
    timeset();
  }
  else if (pressed == 3) {
    prevui = 4;
    firstex = true;
    dateset();
  }
}
//-------------------------------------------------------------------------------------------------------------------
// function for the home screen of the watch
void homeui() {
  if (firstex == true) {
    boolt1 = false;
    firstex = false;
  }
  ui = 1;
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(rtc.dayStr());                  // print day of week

  if (usdate == false) {
    display.print(String(rtc.date()) + "/" + String(rtc.month()) + "/");  // Print month and day
  }
  else {
    display.print(String(rtc.month()) + "/" + String(rtc.date()) + "/");  // Print month and day
  }
  display.print(String(rtc.year()));

  display.setTextSize(2);
  display.setCursor(0, 9);
  display.print(String(rtc.hour()) + ":");                                // Print hour
  if (rtc.minute() < 10) {
    display.print('0');
  }
  else if (rtc.minute() % 10 == 0) {
    if (boolt1 == false) {
      readbv();
      boolt1 = true;
    }
  }
  else {
    boolt1 = false;
  }

  display.print(String(rtc.minute()) + ":");                              // Print minute
  display.setCursor(0, 25);
  if (rtc.second() < 10) {
    display.print('0');
  }
  display.print(String(rtc.second()));                                    // Print second

  if (rtc.is12Hour()) {
    if (rtc.pm()) {
      display.print(" PM");
    }                                                                     // Print am/pm
    else {
      display.print(" AM");
    }
  }
  rtc.update();

  display.setCursor(12, 40);
  display.setTextSize(1);
  display.print(String(bv) + "v,");
  display.print(String(rtc.temperature(), 0) + 'C');

  display.drawBitmap(77, 0, clock_i, 7, 7, 1);    //clock icon
  display.drawBitmap(77, 14, timer_i, 7, 7, 1);   //timer icon
  display.drawBitmap(77, 27, alarm_i, 7, 7, 1);   //alarm icon
  display.drawBitmap(76, 41, calc_i, 8, 7, 1);    //calculator icon
  display.drawBitmap(0, 42, sleep_i, 6, 6, 1);    //sleep icon
  display.display();
}

//-------------------------------------------------------------------------------------------------------------------
//sub-function to read the battery voltage

void readbv() {
  digitalWrite(bven, 1);                //open gate of MOSFET to the voltage divider
  bv = analogRead(bvhalf);            //read analog data of bv/2
  digitalWrite(bven, 0);
  bv = map(bv, 0, 1024, 0, 660);    //map this data value into a short between 0 and 660
  bv = bv + 5;                      //account for voltage drop across regulator
  bv = bv / 100;                    //divide by 100 to get final voltage value
}

//-------------------------------------------------------------------------------------------------------------------
// the function for the time set window

void timeset() {
  if (firstex == true) {      // if the window is just opened
    ui = 2;
    tmp1 = rtc.minute();      // put surrent rtc times in temp variables
    tmp2 = rtc.hour();
    if (rtc.pm()) {
      boolt1 = true;
    }
    else {                    // put am/pm value into temp variable
      boolt1 = false;
    }
    rtc.update();
    menhoriz = 1;
    firstex = false;
  }
  else if (pressed == 3) {    // if up arrow is pressed
    if (menhoriz == 1) {      // if window is on minutes
      if (tmp1 == 59) {
        tmp1 = 0;
      }                       // increment minutes
      else {
        tmp1++;
      }
    }
    else if (menhoriz == 2) { // if window is on hours
      if (tmp2 == 12) {
        tmp2 = 1;
      }                       // increment hours
      else {
        tmp2++;
      }
    }
    else if (menhoriz == 3) { // if window is on am/pm
      boolt1 = !boolt1;
    }
  }
  else if (pressed == 4) {    // if down arrow is pressed
    if (menhoriz == 1) {      // if window is on minutes
      if (tmp1 == 0) {
        tmp1 = 59;
      }                       // decrement minutes
      else {
        tmp1--;
      }
    }
    else if (menhoriz == 2) { // if window is on hours
      if (tmp2 == 1) {
        tmp2 = 12;
      }                       // decrement hours
      else {
        tmp2--;
      }
    }
    else if (menhoriz == 3) { // if window is on am/pm
      boolt1 = !boolt1;
    }
  }
  else if (pressed == 1) {    // if right arrow is pressed
    if (menhoriz == 3) {}
    else {                    // change current window
      menhoriz++;
    }
  }
  else if (pressed == 2) {    // if left arrow is pressed
    if (menhoriz == 1) {}
    else {                    // change current window
      menhoriz --;
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);    // print time set at top
  display.print("Time Set");

  if (menhoriz == 1) {        // if window is on minutes value
    display.setCursor(4, 12);
    display.print("Minutes");
    display.setCursor(48, 21);
    display.print('>');       // window details
    display.setCursor(4, 22);
    display.setTextSize(2);
    if (tmp1 < 10) {
      display.print('0');
    }
    display.print(tmp1);
    display.drawRect(0, 9, 48, 30, BLACK);
  }
  else if (menhoriz == 2) {   // if window is on hours value
    display.setCursor(9, 12);
    display.print("Hours");
    display.setCursor(0, 21);
    display.print('<');
    display.setCursor(40, 21);
    display.print('>');       // window details
    display.setTextSize(2);
    display.setCursor(9, 22);
    display.print(tmp2);
    display.drawRect(6, 9, 34, 30, BLACK);
  }
  else if (menhoriz == 3) {   // if window is on am/pm
    display.setCursor(9, 12);
    display.print("AM/PM");
    display.setCursor(0, 21);
    display.print('<');       // window details
    display.setTextSize(2);
    display.setCursor(9, 22);
    if (boolt1 == true) {
      display.print("PM");
    }
    else {
      display.print("AM");
    }
    display.drawRect(6, 9, 35, 30, BLACK);
  }

  arrows();                 // other icons and display displaying
  display.display();
}

//-------------------------------------------------------------------------------------------------------------------
//confirm chages ui

void confirm() {
  if (firstex == true) {
    ui = 5;
    display.clearDisplay();       //make bit text saying "set time?"
    display.setTextSize(2);
    display.setCursor(7, 5);
    display.print("Set");
    display.setCursor(5, 25);
    if (prevui == 2) {
      display.print("Time?");
    }
    else if (prevui == 3) {
      display.print("Date?");
    }

    display.setTextSize(1);       //put Y and N next to the corresponding buttons
    display.setCursor(77, 14);
    display.print("Y");
    display.setCursor(77, 27);
    display.print("N");
    display.drawRect(3, 1, 63, 41, BLACK);

    display.display();            //wrap up
    firstex = false;
  }
  else if (pressed == 2) {        // if yes is pressed
    if (prevui == 2) {            // if previous window is time set
      if (boolt1 == true) {
        tmp2 = tmp2 + 12;         // am/pm calculation
      }
      rtc.setSecond(0);           //put the temp values into the rtc for timekeeping
      rtc.setMinute(tmp1);
      rtc.setHour(tmp2);
      rtc.set12Hour();
    }
    else if (prevui == 3) {       //if previous window is date set
      rtc.setDay(tmp1);
      rtc.setDate(tmp2);          //put the dates into the rtc
      rtc.setMonth(tmp3);
      rtc.setYear(intt1);
    }
    rtc.update();
    firstex = true;               //wrap up
    prevui = 5;
    homeui();
  }
  else if (pressed == 3) {
    firstex = true;               // if no pressed
    prevui = 5;
    homeui();                     // quit out
  }
}

//-------------------------------------------------------------------------------------------------------------------
//sub-function to be called when a certain arrangement of arrows is needed

void arrows() {
  display.drawBitmap(77, 0, right_a, 7, 7, 1);    //right arrow
  display.drawBitmap(77, 14, left_a, 7, 7, 1);    //left arrow
  display.drawBitmap(77, 27, up_a, 7, 7, 1);      //up arrow
  display.drawBitmap(77, 41, down_a, 7, 7, 1);    //down arrow
  display.drawBitmap(0, 41, back_i, 7, 7, 1);     //back icon
}

//-------------------------------------------------------------------------------------------------------------------
// subfunction to decrement date when month is decremented (code is executed twice in function code)

void datesub1() {
  if (tmp2 > 27) {
    if (tmp3 == 2) {        //if it is a leap year, and febuary, and it is the day value is greater than 29
      if (intt1 % 4 == 0) {
        if (tmp2 > 28) {
          tmp2 = 29;        //reset value to 29
        }
      }
      else {                //if its not a leap year and same condition
        tmp2 = 28;          //reset valuse to 28
      }
    }
    else if (tmp3 < 8 && tmp3 % 2 == 1 or tmp3 > 7 && tmp3 % 2 == 0) {
      if (tmp2 > 30) {      //if it is a month with 31 days
        tmp2 = 31;          //reset value to 31
      }
    }
    else {
      if (tmp2 > 29) {      // if the month has 30 days reset to 30
        tmp2 = 30;
      }
    }
  }
}

//-------------------------------------------------------------------------------------------------------------------
// the function for the date set window

void dateset() {
  if (firstex == true) {      // if the window is just opened
    ui = 3;
    tmp1 = rtc.day();         // put current rtc times in temp variables
    tmp2 = rtc.date();
    tmp3 = rtc.month();
    intt1 = rtc.year();
    rtc.update();
    menhoriz = 1;             // finish up
    firstex = false;
  }
  else if (pressed == 1) {    // if right arrow is pressed
    if (menhoriz == 4) {}
    else {                    // change current window
      menhoriz++;
    }
  }
  else if (pressed == 2) {    // if left arrow is pressed
    if (menhoriz == 1) {}
    else {                    // change current window
      menhoriz --;
    }
  }
  else if (pressed == 3) {    // if up arrow is pressed
    if (menhoriz == 1) {      // if window is on day of week
      if (tmp1 == 7) {
        tmp1 = 1;
      }                       // increment day of week
      else {
        tmp1++;
      }
    }
    else if (menhoriz == 3) { // if window is on days
      if (tmp2 >= 28) {       // if day is greater than or equal to 28
        if (tmp3 == 2) {      // if it is febuary
          if (tmp2 == 28 && intt1 % 4 == 0) {
            tmp2++;           // if it is a leap year increment
          }
          else {
            tmp2 = 1;       // otherwise, reset to day 1
          }
        }
        else if (tmp3 < 8 && tmp3 % 2 == 1 or tmp3 > 7 && tmp3 % 2 == 0) {
          if (tmp2 == 31) { //if there are 31 days in the current month
            tmp2 = 1;       //reset
          }
          else {            //otherwise, increment
            tmp2++;
          }
        }
        else {
          if (tmp2 == 30) { // if there are not 30 days in the current month
            tmp2 = 1;        // reset
          }
          else {
            tmp2++;          //otherwise, increment
          }
        }
      }
      else {
        tmp2++;
      }
    }
    else if (menhoriz == 2) { // if window is on month
      if (tmp3 == 12) {
        tmp3 = 1;
      }
      else {
        tmp3++;               //increment month
      }
      datesub1();             //decrement if impossible values occur
    }
    else if (menhoriz == 4) { //if window is on year
      if (intt1 == 99) {
        intt1 = 1;            //increment year
      }
      else {
        intt1++;
      }
      if (intt1 % 4 != 0 && tmp3 == 2) {
        if (tmp2 > 27) {      //if its not a leap year
          tmp2 = 28;          //make sure there is no leap day
        }
      }
    }
  }
  else if (pressed == 4) {
    if (menhoriz == 1) {      // if window is on day of week
      if (tmp1 == 1) {
        tmp1 = 7;
      }                       // decrement day of week
      else {
        tmp1--;
      }
    }
    else if (menhoriz == 3) { // if window is on days
      if (tmp2 == 1) {        // if day is greater than or equal to 28
        if (tmp3 == 2) {      // if it is febuary
          if (tmp2 == 1 && intt1 % 4 == 0) {
            tmp2 = 29;        // if it is a leap year, increment
          }
          else {
            tmp2 = 28;       // otherwise reset to day 1
          }
        }
        else if (tmp3 < 8 && tmp3 % 2 == 1 or tmp3 > 7 && tmp3 % 2 == 0) {
          if (tmp2 == 1) {  //if there are 31 days in the current month
            tmp2 = 31;      //if its day 31, reset to 1
          }
          else {
            tmp2--;         //decrement the month if its not day 31
          }
        }
        else {              // if there are 30 days in this month
          tmp2 = 30;        //if its day 30, reset to 1
        }
      }
      else {
        tmp2--;             //otherwise decrement
      }
    }
    else if (menhoriz == 2) {
      if (tmp3 == 1) {      //if window is on month
        tmp3 = 12;
      }
      else {
        tmp3--;             //increment month
      }
      datesub1();           //decrement if impossible values occur
    }
    else if (menhoriz == 4) {
      if (intt1 == 99) {    //if window is on year
        intt1 = 0;
      }
      else {
        intt1 --;           //decrement year
      }
      if (intt1 % 4 != 0 && tmp3 == 2) {
        if (tmp2 > 27) {    //if its not a leap year
          tmp2 = 28;        //make sure its not a leap day
        }
      }
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);    // print time set at top
  display.print("Date Set");

  if (menhoriz == 1) {
    display.setCursor(4, 12);
    display.print("Day (week)");
    display.setCursor(66, 21);
    display.print('>');       // window details
    display.setCursor(4, 22);
    display.setTextSize(2);
    display.print(dayIntToStr[tmp1 - 1]);
    display.drawRect(0, 9, 65, 30, BLACK);
  }
  else {
    display.setCursor(10, 16); //if you're reading this i'm tired of writing comments
    display.print("Date:");
    display.setCursor(0, 21);
    display.print('<');
    display.setCursor(10, 26);
    if (tmp3 < 10) {
      display.print('0');   // add zeros if needed
    }
    display.print(String(tmp3) + '/');
    if (tmp2 < 10) {
      display.print('0');   //put everything in date format
    }
    display.print(String(tmp2) + '/');
    if (intt1 < 10) {
      display.print('0');
    }
    display.print(intt1);
    display.drawRect(7, 13, 57, 24, BLACK);

    display.setTextSize(1);
    if (menhoriz == 2) {
      display.setCursor(13, 9); //put arrow to indicate which variable you are setting
      display.print('^');
    }
    else if (menhoriz == 3) {
      display.setCursor(29, 9);
      display.print('^');
    }
    else if (menhoriz == 4) {
      display.setCursor(50, 9);
      display.print('^');
    }
  }
  arrows();                   //display arrows and finish up
  display.display();
}

//-------------------------------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(57600);

  pinMode(b1, INPUT); pinMode(b2, INPUT); pinMode(b3, INPUT); pinMode(b4, INPUT); pinMode(b5, INPUT); pinMode(bint, INPUT);
  attachInterrupt(digitalPinToInterrupt(bint), button, RISING);   //define button pins and interrupt
  pinMode(bven, OUTPUT); pinMode(bvhalf, INPUT);    //define battery managment pins
  pinMode(bz, OUTPUT);

  rtc.begin(RTC_CS);        //ds2324 initialization
  pinMode(RTC_INT, INPUT);
  attachInterrupt(digitalPinToInterrupt(RTC_INT), sqw, FALLING);
  rtc.writeSQW(SQW_SQUARE_1);
  rtc.autoTime();
  rtc.set12Hour();
  rtc.setAlarm1(rtc.second() + 1);
  rtc.update();

  pinMode(lcdpwr, OUTPUT); pinMode(lcdled, OUTPUT);
  digitalWrite(lcdpwr, 1);
  display.begin();          //nokia 5110 lcd initialization
  display.setContrast(60);
  display.setTextColor(BLACK);

  sleep_enable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  readbv();
  tone(bz, 3000);      //make noise on buzzer
  delay(250);
  noTone(bz);
}

//-------------------------------------------------------------------------------------------------------------------

void loop() {
  if (sqwex == true) {
    if (ui == 1) {                              // update clock if the ui variable is 1
      homeui();
    }
    if (countdown == true) {                    // if timer is counting down
      if (timermain[2] == 0) {
        if (timermain[1] != 0) {
          timermain[1]--;
          timermain[2] = 59;                    //decrement hours, then minutes, then seconds
        }
        if (timermain[1] == 0) {
          if (timermain[0] != 0) {
            timermain[0]--;
            timermain[1] = 59;
          }
        }
      }
      else {
        timermain[2]--;
      }
      if (ui == 6 or timermain[0] == 0 and timermain[1] == 0 and timermain[2] == 0) {
        ui = 6;
        timerui();                            // if the timer is up go to timer screen
      }
    }
    if (countup == true) {
      if (stopmain[2] == 59) {
        stopmain[2] = 0;
        if (stopmain[1] == 59) {
          stopmain[1] == 0;
          if (stopmain[0] == 23) {
            countup = false;
          }
          else {
            stopmain[0]++;
          }
        }
        else {
          stopmain[1]++;
        }
      }
      else {
        stopmain[2]++;
      }
      stopwatch();
    }
    sqwex = false;
  }

  if (active == true) {
    if (ifsleep == true) {
      attachInterrupt(digitalPinToInterrupt(RTC_INT), sqw, FALLING);
      firstex = true;
      ifsleep = false;
      homeui();
    }
    else if (pressed == 1) {          // if button 1 is pressed
      if (ui == 1) {
        firstex = true;               // go to time set menu
        prevui = 1;
        chooseset();
      }
      else if (ui == 2) {
        timeset();
      }
      else if (ui == 3) {
        dateset();
      }
      else if (ui == 6) {
        timerui();
      }
    }
    else if (pressed == 2) {  // if button 2 is pressed
      if (ui == 1) {
        firstex = true;
        prevui = 1;
        choosetimer();
      }
      else if (ui == 2) {
        timeset();
      }
      else if (ui == 3) {
        dateset();
      }
      else if (ui == 4) {
        chooseset();
      }
      else if (ui == 5) {
        confirm();
      }
      else if (ui == 6) {
        timerui();
      }
      else if (ui == 7) {
        stopwatch();
      }
      else if (ui == 8) {
        choosetimer();
      }
    }
    else if (pressed == 3) {  // if button 3 is pressed
      if (ui == 2) {
        timeset();
      }
      else if (ui == 3) {
        dateset();
      }
      else if (ui == 4) {
        chooseset();
      }
      else if (ui == 5) {
        confirm();
      }
      else if (ui == 6) {
        timerui();
      }
      else if (ui == 7) {
        stopwatch();
      }
      else if (ui == 8){
        choosetimer();
      }
    }
    else if (pressed == 4) {  // if button 4 is pressed
      if (ui == 2) {
        timeset();
      }
      else if (ui == 3) {
        dateset();
      }
      else if (ui == 6) {
        timerui();
      }
    }
    else if (pressed == 5) { // if button 5 is pressed
      if (ui == 1) {
        ifsleep = true;
        display.clearDisplay();
        display.display();
        detachInterrupt(digitalPinToInterrupt(RTC_INT));
      }
      else if (ui == 2) {
        prevui = 2;
        firstex = true;
        confirm();
      }
      else if (ui == 3) {
        prevui = 3;
        firstex = true;
        confirm();
      }
      else if (ui == 4) {
        prevui = 4;
        firstex = true;
        homeui();
      }
      else if (ui == 6) {
        prevui = 6;
        firstex = true;
        homeui();
      }
      else if (ui == 7) {
        countup = false;
        prevui = 7;
        firstex = true;
        homeui();
      }
      else if (ui == 8) {
        countup = false;
        prevui = 8;
        firstex = true;
        homeui();
      }
    }
    active = false;
  }
  sleep_cpu();            // put mcu to sleep to save power
}
