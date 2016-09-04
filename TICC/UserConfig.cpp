#include "UserConfig.h"

MeasureMode UserConfig() {
////////////////////////////////////////////////////////////////////////////
// Test for operator request to go to setup mode.
// If setup mode not entered, or invalid command, defaults to Timestamp mode.
// Commands:
// T    - Timestamp mode
// P    - Period mode
// I    - time Interval (A-B) mode
// L    - Dual Channel TI mode for TimeLab
//
// BUGBUG - This is buggy right now

  char inputline[128];
  char modechar;

  Serial.println("Type a few characters to go into setup mode. ");

  delay(10000);

  int j = 0;

  while (Serial.available())
    inputline[j++] = Serial.read();

  inputline[j] = '/0';
  
// Serial.print("# chars received = "),Serial.println(j);
// Serial.print("Received input chars: ");

 for(int i=0; i<j; i++) {
   Serial.print(inputline[i]);
 }

 
  if(j==0) {
    Serial.println("No setup command received, Default to timestamp mode");
    return(timestamp);
  }

  Serial.println("Setup mode.  Valid single-letter commands are:"), Serial.println();
  Serial.println("   T     (T)imestamp mode");
  Serial.println("   P     (P)eriod mode");
  Serial.println("   I     time (I)nterval A->B mode");
  Serial.println("   L     Time(L)ab interval mode");
  Serial.println(),Serial.print("Enter mode: ");

  while (!Serial.available())
    ;

  modechar = toupper(Serial.read());
  
  Serial.print(modechar),Serial.println();Serial.println();

  MeasureMode m;

  switch (modechar) {
    case 'T':
      Serial.println("Timestamp mode");
      m = timestamp;
      return(m);  // Timestamp mode
      break;
    case 'P':
      Serial.println("Period mode");
      m = period;
      return(m);  // Period mode
      break;
    case 'I':
      Serial.println("time Interval A->B mode");
      m = interval;
      return(m);  // time Interval mode
      break;
    case 'L':
      Serial.println("TimeLab time interval mode");
      m = timelab;
      return(m); // TimeLab mode
      break;
    default:
      Serial.println("Invalid command. Defaulting to Timestamp mode");
      m = timestamp;
      return(m);
      break;
    
  }
  
//
//////////////////////////////////////////////////////////////////////////////
}


