#include <Arduino.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
// david edit
ArduinoLEDMatrix matrix;



void setup() {
  Serial.begin(9600);
  matrix.begin();
  matrix.textFont(Font_5x7);
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(100);
}

String a;

void loop(){
//  matrix.beginDraw();
while(Serial.available()) {
a= Serial.readString();// read the incoming data as string
Serial.println(a);
}
 
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(100);

  // add the text
// const char text[] = " Hello World! ";
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(a);
  matrix.endText(SCROLL_LEFT);

  matrix.endDraw();

  
}