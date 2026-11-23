#include <LiquidCrystal_I2C.h>

//-------Pins-----//
#define up 18              //Up button
#define down 19           //Down button

LiquidCrystal_I2C lcd(0x27, 16, 2);

//Counter to change positions of pages

int page_counter = 1 ;       //To move beetwen pages

//---------Storage debounce function-----//
bool current_up = LOW;
bool last_up=LOW;
bool last_down = LOW;
bool current_down = LOW;
        

//---- De-bouncing function for all buttons----//
bool debounce(bool last, int pin) {
  bool current = digitalRead(pin);
  
  if (current != last) {
    delay(5);
    current = digitalRead(pin);
    }
  
  return current;
}

// ========== SETUP ==========
void setup() {
  lcd.init();
  lcd.backlight();

  pinMode(up, INPUT_PULLDOWN);
  pinMode(down, INPUT_PULLDOWN);
}

// ========== LOOP ==========
void loop() {
  current_up = debounce(last_up, up);         //Debounce for Up button
  current_down = debounce(last_down, down);   //Debounce for Down button

  //----Page counter function to move pages----//
  //Page Up
  if (last_up == LOW && current_up == HIGH) {  //When up button is pressed
    lcd.clear();                               //When page is changed, lcd clear to print new page  
      
    if(page_counter < 3) {       //Page counter never higher than 3(total of pages)
      page_counter++;            //Page up
      delay(150);
    } else {
      page_counter= 3;  
    }
  }
  
  last_up = current_up;

  //Page Down
  if (last_down == LOW && current_down == HIGH){ //When down button is pressed
    lcd.clear();                                 //When page is changed, lcd clear to print new page    
    
    if (page_counter > 1) {       //Page counter never lower than 1 (total of pages)
      page_counter--;             //Page down
      delay(150);
    } else {
      page_counter= 1;  
    }
  }
    
  last_down = current_down;

//------- Switch function to write and show what you want---// 
  switch (page_counter) {
    case 1:
      lcd.setCursor(5,0);
      lcd.print("This is");
      lcd.setCursor(3,1);
      lcd.print(" Home Page");
      break;
    case 2:
      lcd.setCursor(5,0);
      lcd.print("This is");
      lcd.setCursor(5,1);
      lcd.print("Page 2");
      break;
    case 3:
      lcd.setCursor(1,0);
      lcd.print("You are now on");
      lcd.setCursor(4,1);
      lcd.print("Page 3");
      break;
  } //switch end
} //loop end