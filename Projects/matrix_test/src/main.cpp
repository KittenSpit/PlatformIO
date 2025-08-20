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
float b=152.1234;



void loop(){
matrix.beginDraw();
while(Serial.available()) {
a= Serial.readString();// read the incoming data as string
}
a.trim();
//a = String(b,4);
//Serial.println(a);
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(100);

  // add the text
// const char text[] = " Hello World! ";
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print(a);
  matrix.endText(SCROLL_LEFT);

  matrix.endDraw();

  
}