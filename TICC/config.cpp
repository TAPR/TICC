// config.cpp -- set/read/write configuration

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Portions Copyright David McQuate WA8YWQ 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <ctype.h>
#include <EEPROM.h>           // read/write EEPROM
#include <SPI.h>

#include "misc.h"             // random functions
#include "config.h"           // config and eeprom
#include "board.h"            // Arduino pin definitions
#include "tdc7200.h"          // TDC registers and structures

extern const char SW_VERSION[17];
extern const char BOARD_ID[17];

#define inputLineIndexMax 125
char inputLine[128];    // The above define is less than the declared size to ensure against overflow
int inputLineIndex = 0;
int inputLineReadIndex = 0;

char getChar()
{
  char c;
  while ( ! Serial.available())      /*    wait for a character   */     ;
  c = Serial.read();
  Serial.print(c);                      // echo the character receivednp
  return c;
}

// get a character
// if  charSourceIsLine != 0   get the next character from   inputLine
// inputLineReadIndex should be set to 0 after filling  inputLine and
// before calling getChar
char getChar(int charSourceIsLine)
{
  if (charSourceIsLine == 0)
  { return getChar();
  }
  else if (inputLineReadIndex < inputLineIndex)
  {
    Serial.print(inputLine[inputLineReadIndex]);    // Add this as a debug aid
    return inputLine[inputLineReadIndex++];
  }
  else return getChar();
}

// Get a string of characters from Serial input.  Return when buffer inputLine is full or when a newline character is received.
// The newline is  put into inputLine. In addition, inputLine is always null-terminated.
// ToDo:  
// (1) allow use of Backspace  DONE
// (2) allow use of Home, End, Delete;  
// (3) allow insertion of characters
void getLine()
{
  char newChar;
  for ( inputLineIndex = 0; ; )
  {
      newChar = getChar();
      if ( (inputLineIndex > inputLineIndexMax) || (newChar == '\n')  || (newChar == '\r') )
      {
        inputLine[inputLineIndex++] = '\r';
        inputLine[inputLineIndex] = '\0';
        if (inputLineIndex > 1) return;
      }
      else if (newChar == '\b')
      {
        Serial.print(' ');
        Serial.print(newChar);
        if (inputLineIndex > 0) --inputLineIndex;
      }
      else inputLine[inputLineIndex++] = newChar;
    }
}


// Convert character to int64.
// The 64-bit integer part is placed in result.
// Handles any optionally-signed floating point format number, as long as it fits in [64 bit].[64 bit]
// In principle, that could mean up to 9,223,372,036,854,775,807
// No, maybe half that?  4,611,686,018,427,387,903 ?
// The values below are made available so that the fractional part may be used.
// The value of the fraction is    mantissaFractionPart / mantissaFractionPartPower
// The number of fraction digits is limited to 18.
// Algorithm based on http://krashan.ppa.pl/articles/stringtofloat/

  int getInt64new = 0;    // set to 1 if a new value has been parsed
  int64_t mantissa = 0;
  int64_t mantissaIntegerPart = 0;
  int64_t mantissaFractionPart = 0;
  int64_t mantissaFractionPartPower = 1;
// if 'source' is 0, characters are obtained from the Serial port;
// if source is 1, characters come from inputLine, using inputLineIndex
// parse Serial input and return value in result.  Result is unchanged if only a newline is received.  In this case getInt64new remains 0.
void getInt64(int64_t *result, int source)   
{
  int negative = 0;
  int exponentNegative = 0;
  int exponent = 0;
  int exponentInteger = 0;
  mantissa = 0;;
  mantissaIntegerPart = 0;
  mantissaFractionPart = 0;
  mantissaFractionPartPower = 1;
  getInt64new = 0;          // If we are successful in building a number, this will be set to one.
  char newChar;

  while ( isspace(newChar = getChar(source) )  ) /* ignore leading whitespace */ ;
  
  if (newChar == '+') newChar = getChar(source);    // allow leading plus
  else if (newChar == '-')                                      // handle leading minus
  {
    negative = 1;
    newChar = getChar(source);
  }
    while (newChar == '0')
    { newChar = getChar(source);   // ignore mantissa leading zeros     
      getInt64new = 1;                  // but note any zero entered
    }
    
  if (isdigit(newChar))    // get mantissa integer part
  {
    while (isdigit(newChar))
    {
      getInt64new = 1;
      mantissa *= 10;
      mantissa += (newChar - '0');
      newChar = getChar(source);
    }
  }
  else if (newChar == '.')          // No integer part; begin mantissa fractional part
  { newChar = getChar(source);
    while (newChar == '0')
    {
      --exponent;
      newChar = getChar(source);
    }
  }
  
    // newChar should be '.' or digit
    if (newChar == '.') newChar = getChar(source);
    while (isdigit(newChar))      // fractional part of mantissa
    {
      getInt64new = 1;
      mantissa *= 10;
      mantissa += (newChar - '0');
      --exponent;
      newChar = getChar(source);
    }
    // optional exponent
    if ( (newChar == 'e') || (newChar == 'E') )
    {
      newChar = getChar(source);
      if (newChar == '+') newChar = getChar(source);
     else if (newChar == '-') 
      {
        exponentNegative = 1;
        newChar = getChar(source);
      }
      while (newChar == '0') newChar = getChar(source);   // bypass exponent leading zeros
      while(isdigit(newChar))
      {
        exponentInteger *= 10;
        exponentInteger += (newChar - '0');
        newChar = getChar(source);
      }
      if (exponentNegative) exponentInteger = -exponentInteger;
      exponent += exponentInteger;
    }

    if (getInt64new == 0) return;   // no mantissa found

    // scale the mantissa according to the exponent, and separate into integer and fractional parts
    
    mantissaIntegerPart = mantissa;
    if (exponent < 0)  
    { 
      for (int i = exponent; i < 0; ++i)  mantissaFractionPartPower *= 10;
      mantissaFractionPart = mantissaIntegerPart % mantissaFractionPartPower;
      mantissaIntegerPart /= mantissaFractionPartPower;
    }
    if (exponent > 0)  for (int i = 0; i < exponent; ++i)   mantissaIntegerPart *= 10;
    if (negative) 
    { mantissaIntegerPart = -mantissaIntegerPart;
      mantissaFractionPart = -mantissaFractionPart;
    }
    *result = mantissaIntegerPart;
}

void printHzAsMHz(int64_t x)
{
  char str[128];
  int64_t MHz = x / 1000000;
  int64_t Hz = MHz * 1000000;
  int64_t fract = x - Hz;
  sprintf(str, "%ld.", (int32_t)MHz), Serial.print(str);
  sprintf(str,"%06ld", (int32_t)fract), Serial.print(str);
}

void measurementMode(struct config_t *pConfigInfo)
{
  int valid = 1;
Serial.println("Setup mode.  Valid single-letter commands are:"), Serial.println();
  Serial.println("   T     Timestamp mode");
  Serial.println("   P     Period mode");
  Serial.println("   I     time Interval A->B mode");
  Serial.println("   L     TimeLab interval mode");
  Serial.println("   D     Debug mode");
  Serial.println(),  Serial.print("Enter mode: ");

  do
  {
    valid = 1;
   switch( toupper(getChar()))
  {
    case 'T': pConfigInfo->MODE = Timestamp;    break;
    case 'P': pConfigInfo->MODE = Period;   break;
    case 'I': pConfigInfo->MODE = Interval;   break;
    case 'L': pConfigInfo->MODE = timeLab;    break;
    case 'D': pConfigInfo->MODE = Debug;    break;
      default:  valid = 0;
      Serial.println("? Please enter T P I L or D");
    break;
  }
  } while (valid == 0);
}

void clockSpeed(struct config_t *pConfigInfo)
{ int64_t clock;
  Serial.print("Clock speed "), printHzAsMHz(pConfigInfo->CLOCK_HZ), Serial.println(" MHz  (1 to 16 MHz, default 10 MHz)");
  Serial.println("Enter new value (in MHz) and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&clock, 1);
  if (( clock >= 1) && (clock <= 16) ) pConfigInfo->CLOCK_HZ = clock * 1000000 + mantissaFractionPart * 1000000 / mantissaFractionPartPower;    
}

void coarseClockRate(struct config_t *pConfigInfo)
{
  int64_t clock;
  Serial.print("Coarse Clock Rate "), printHzAsMHz(pConfigInfo->PICTICK_PS), Serial.println(" us.  Default 100 us");
  Serial.println("Enter new value (in us) and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&clock, 1);
  if (clock > 0) pConfigInfo->PICTICK_PS = clock * 1000000 + mantissaFractionPart * 1000000 / mantissaFractionPartPower;  // value entered in us, but stored in ps
}

void calibrationPeriods(struct config_t *pConfigInfo)
{
  int64_t periods;
  Serial.print("Calibration periods "), Serial.print((int32_t)pConfigInfo->CAL_PERIODS), Serial.println(" Choose one of 2, 10, 20, or 40.  Default 20");  // casting to int32_t discards upper 32 bits
  Serial.println("Enter new value and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&periods, 1);
  if (getInt64new)
          if ( (periods == 2) || (periods == 10) || (periods == 20) || (periods == 40) ) pConfigInfo->CAL_PERIODS = periods;
}

void timeout(struct config_t *pConfigInfo)
{
  int64_t timeout;
  Serial.print("Timeout "), Serial.print((int32_t)pConfigInfo->TIMEOUT), Serial.println(" Choose a value in the range 0 to 255.  Default 5");  // casting to int32_t discards upper 32 bits
  Serial.println("Enter new value and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&timeout, 1);
  if ( (getInt64new) && (timeout >= 0) && (timeout <= 255) ) pConfigInfo->TIMEOUT = timeout;   
}

void masterSlave(struct config_t *pConfigInfo)
{
  Serial.print("Master or Slave "), Serial.print(pConfigInfo->SYNC_MODE), Serial.println("   Choose M or S.  Default is M");
  char c = toupper(getChar());
  if ( (c == 'M') || (c == 'S') ) pConfigInfo->SYNC_MODE = c;
}

void triggerEdge(struct config_t *pConfigInfo)
{
  Serial.println("Channel      Edge");
  Serial.print   ("   A             "), Serial.println(pConfigInfo->START_EDGE[0]);
  Serial.print   ("   B             "); Serial.println(pConfigInfo->START_EDGE[1]);
  Serial.println(" Enter channel {space} edge: R (rising) or F (falling).  Default is R");
  getLine();
  
  int i = 0;
  int index = -1;
  while (inputLine[i] == ' ') ++i;    // bypass leading spaces
  char c = toupper(inputLine[i]);
  if (c == 'A') index = 0;
  if (c == 'B') index = 1;
  if (index < 0) return;
  ++i;
  while(inputLine[i] == ' ') ++i;   // bypass separating spaces
  c = toupper(inputLine[i]);
  if ( (c == 'R') || (c == 'F') ) pConfigInfo->START_EDGE[index] = c;
}

void timeDilation(struct config_t *pConfigInfo)
{
  int index = -1; 
  int64_t dilation;
  Serial.println("Channel       Time Dilation Factor");
  Serial.print  ("   A              "), Serial.println((int32_t)pConfigInfo->TIME_DILATION[0]);
  Serial.print  ("   B              "), Serial.println((int32_t)pConfigInfo->TIME_DILATION[1]);

  for ( ; ; )
  {Serial.println("Enter channel  (A or B)");
  char c = toupper(getChar());
  if (c == 'A') index = 0;
  if (c == 'B') index = 1;
  if (index >= 0) break;
  }
  Serial.println("  Enter time Dilation.  (Default is 2500)");

  getLine();
  inputLineReadIndex = 0;
  getInt64(&dilation, 1);
  if (getInt64new) pConfigInfo->TIME_DILATION[index] = dilation;
}

void fixedTime2(struct  config_t *pConfigInfo)
{
  int index = -1;
  int64_t fixed;
  Serial.println("Channel      Fixed Time2");
  Serial.print   ("   A            "), Serial.println((int32_t)pConfigInfo->FIXED_TIME2[0]);
  Serial.print   ("   B            "), Serial.println((int32_t)pConfigInfo->FIXED_TIME2[1]);

  for ( ; ; )
  {Serial.println("Enter channel  (A or B)");
  char c = toupper(getChar());

  if (c == 'A') index = 0;
  if (c == 'B') index = 1;
  if (index >= 0) break;
  }
  Serial.println("  Enter Fixed Time2.  (Default is 0)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&fixed, 1);
  if (getInt64new) pConfigInfo->FIXED_TIME2[index] = fixed;
}

void fudge0(struct config_t *pConfigInfo)
{
  int index = -1;
  int64_t fudge;
  Serial.println("Channel       Fudge0");
  Serial.print   ("    A           "), Serial.println((int32_t)pConfigInfo->FUDGE0[0]);
  Serial.print   ("    B           "), Serial.println((int32_t)pConfigInfo->FUDGE0[1]);
 
  for ( ; ; )
  {Serial.println("Enter channel  (A or B)");
  char c = toupper(getChar());
  if (c == 'A') index = 0;
  if (c == 'B') index = 1;
  if (index >= 0) break;
  }
  Serial.println("  Enter Fudge0.  (Default is 0)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&fudge, 1);
  if (getInt64new) pConfigInfo->FUDGE0[index] = fudge;
}

char modeToChar(unsigned char mode)
{
		switch (mode)
		{
			case Timestamp: return 'T';
			case Interval:  return 'I';
			case Period:    return 'P';
			case timeLab:   return 'L';
			case Debug:     return 'D';
		}
   return '?';
}

struct config_t defaultConfig() {
  struct config_t x;
  x.VERSION == EEPROM_VERSION;
  strncpy(x.SW_VERSION,SW_VERSION,sizeof(SW_VERSION));
  x.BOARD_REV = BOARD_REVISION;
  strncpy(x.BOARD_ID,BOARD_ID,sizeof(BOARD_ID));
  x.MODE = MEASUREMENT_MODE;
  x.CLOCK_HZ = CLOCK_HERTZ;
  x.PICTICK_PS = PICTICK_PICOS;
  x.CAL_PERIODS = CAL_PER;
  x.TIMEOUT = TIMEOUT_HEX;
  x.SYNC_MODE = SYNC;
  x.START_EDGE[0] = START_EDGE_0;
  x.START_EDGE[1] = START_EDGE_1;
  x.TIME_DILATION[0] = TIME_DILATION_0;
  x.TIME_DILATION[1] = TIME_DILATION_1;
  x.FIXED_TIME2[0] = FIXED_TIME2_0;
  x.FIXED_TIME2[1] = FIXED_TIME2_1;
  x.FUDGE0[0] = FUDGE0_0;
  x.FUDGE0[1] = FUDGE0_1;
  return x;
}

void initializeConfig(struct config_t *x)
{
  x->MODE = Timestamp; // MODE
  x->CLOCK_HZ = 10000000; // 10 MHz
  x->PICTICK_PS = 100000000; // 100us
  x->CAL_PERIODS = 20; // CAL_PERIODS (2, 10, 20, 40)
  x->TIMEOUT = 0x05; // measurement timeout
  x->SYNC_MODE = 'M';
  x->START_EDGE[0] = 'R';
  x->START_EDGE[1] = 'R';
  x->TIME_DILATION[0] = 2500;  // SWAG that seems to work
  x->TIME_DILATION[1] = 2500;  // SWAG that seems to work
  x->FIXED_TIME2[0] = 0;
  x->FIXED_TIME2[1] = 0;
  x->FUDGE0[0] = 0;
  x->FUDGE0[1] = 0;
}
void doSetupMenu(struct config_t *pConfigInfo)      // also display the default values ----------------
{
  char response;
  for ( ; ; )
  {
    Serial.println(), Serial.println();
  Serial.print("M   Measurement Mode        "); Serial.print( modeToChar(pConfigInfo->MODE)); Serial.println("               default T");   // enum MeasureMode, default Timestamp
  Serial.print("S   clock Speed  (MHz)      "); printHzAsMHz(pConfigInfo->CLOCK_HZ), Serial.println("       default 10");           // int_64, default 1e7
  Serial.print("C   Coarse Clock Rate (us)  "); printHzAsMHz(pConfigInfo->PICTICK_PS), Serial.println("      default 100");         // int_64, default 1e8
  Serial.print("P   calibration Periods     "); Serial.print((int32_t)pConfigInfo->CAL_PERIODS); Serial.println("              default 20");// int_16, choices are 2, 10, 20, 40; default 20
  Serial.print("T   Timeout                 "); Serial.print((int32_t)pConfigInfo->TIMEOUT); Serial.println("               default 5");             // int_16, default 5
  Serial.print("Y   sync:  master / slave   "); Serial.print(pConfigInfo->SYNC_MODE); Serial.println("               default M");                // M (default) or S
    
  Serial.print("E   trigger Edge            ");     // R(ising) or F(alling)
    	    Serial.print(pConfigInfo->START_EDGE[0]);
    	    Serial.print(' ');
    	    Serial.println(pConfigInfo->START_EDGE[1]);
    	
  Serial.print("D   time Dilation           ");       // int_64, default 2500
    	    Serial.print((int32_t)pConfigInfo->TIME_DILATION[0]);
    	    Serial.print(' ');
        Serial.print((int32_t)pConfigInfo->TIME_DILATION[1]);
        Serial.println("       default 2500");
    	
  Serial.print("F   Fixed Time2             ");   // int_64, default 0
        Serial.print((int32_t)pConfigInfo->FIXED_TIME2[0]);
        Serial.print(' ');
      Serial.print((int32_t)pConfigInfo->FIXED_TIME2[1]);
      Serial.println("          default 0");
     
  Serial.print("G   fudge0                  ");   // int_64, default 0
    	    Serial.print((int32_t)pConfigInfo->FUDGE0[0]);
    	    Serial.print(' ');
        Serial.print((int32_t)pConfigInfo->FUDGE0[1]);
        Serial.println("             default 0");
    	
  Serial.println();
  Serial.println("R   Reset all to default values");
  Serial.println("W   Write changes and exit setup");
    Serial.println("Z   Discard changes and exit setup");
    Serial.println("choose one: ");
    
    response = toupper(getChar());    // wait for a character
  
    switch(response)   
    {
      case 'M':  measurementMode(pConfigInfo);
    		break;
      case 'S':  clockSpeed(pConfigInfo);
    		break;
    		case 'C':	 coarseClockRate(pConfigInfo);
    		break;
      case 'P':  calibrationPeriods(pConfigInfo);
    		break;
      case 'T':  timeout(pConfigInfo);
    		break;
      case 'Y':  Serial.println("Y selected");
      masterSlave(pConfigInfo); // (sync mode)
    		break;
      case 'E':  triggerEdge(pConfigInfo);
    		break;
      case 'D':  timeDilation(pConfigInfo);
    		break;
      case 'F':  fixedTime2(pConfigInfo);
    		break;
      case 'G':  fudge0(pConfigInfo);
    		break;
      case 'R': initializeConfig(pConfigInfo);
      break;
      case 'W':  // write changes and exit
                      EEPROM_writeAnything(CONFIG_START, *pConfigInfo); // save change to config
                      return;
    		break;
    		case 'Z':	// discard changes and exit
                        return;
    		break;
      default:  Serial.println("???");  // 'bad selection'
    		break;
    }   // switch
  }   // for 
}


void UserConfig(struct config_t *pConfigInfo) 
{
    char c;
    while ( ! Serial )   /* wait until Serial port is open */ ;

    Serial.println("Type any character to make changes to TICC configuration.");
    bool configRequested = 0;
    for (int i = 6; i >= 0; --i)  // wait ~6 sec so user can type something
    { 
      delay(250);   Serial.print('.');      if (Serial.available())       { configRequested = 1;        break;      }
      delay(250);   Serial.print('.');      if (Serial.available())       { configRequested = 1;        break;      }
      delay(250);   Serial.print('.');      if (Serial.available())       { configRequested = 1;        break;      }
      delay(250);   Serial.print(i);        if (Serial.available())       { configRequested = 1;        break;      }
    }
    while (Serial.available()) c = Serial.read();   // eat any characters entered before we start  doSetupMenu()
    if (configRequested) doSetupMenu(pConfigInfo); 
}
  



// Pretty-print mode
void print_MeasureMode(MeasureMode x) {
  switch (x) {
    case Timestamp:
      Serial.println("Timestamp");
      break;
    case Period:
      Serial.println("Period");
      break;
    case Interval:
      Serial.println("Time Interval A->B");
      break;
    case timeLab:
      Serial.println("TimeLab 3-channel");     
    case Debug:
      Serial.println("Debug");
  }  
}

void eeprom_write_config_default (uint16_t offset) {
  struct config_t x = defaultConfig();
  EEPROM_writeAnything(offset,x);
}

void print_config (config_t x) {
  char tmpbuf[8];
  Serial.print("Measurement Mode: ");print_MeasureMode(MeasureMode(x.MODE));
  Serial.print("EEPROM Version: ");Serial.print(EEPROM.read(CONFIG_START)); 
  Serial.print(", Board Version: ");Serial.println(x.BOARD_REV);
  Serial.print("Software Version: ");Serial.println(x.SW_VERSION);
  Serial.print("Board Serial Number: ");Serial.println(x.BOARD_ID); 
  Serial.print("Clock Speed: ");Serial.println((uint32_t)x.CLOCK_HZ);
  Serial.print("Coarse tick (ps): ");Serial.println((uint32_t)x.PICTICK_PS);
  Serial.print("Cal Periods: ");Serial.println(x.CAL_PERIODS);
  Serial.print("SyncMode: ");Serial.println(x.SYNC_MODE);
  Serial.print("Timeout: ");
  sprintf(tmpbuf,"0x%.2X",x.TIMEOUT);Serial.println(tmpbuf);
  Serial.print("Time Dilation: ");Serial.print((int32_t)x.TIME_DILATION[0]);
  Serial.print(" (chA), ");Serial.print((int32_t)x.TIME_DILATION[1]);Serial.println(" (chB)");
  Serial.print("FIXED_TIME2: ");Serial.print((int32_t)x.FIXED_TIME2[0]);
  Serial.print(" (chA), ");Serial.print((int32_t)x.FIXED_TIME2[1]);Serial.println(" (chB)");
  Serial.print("FUDGE0: ");Serial.print((int32_t)x.FUDGE0[0]);
  Serial.print(" (chA), ");Serial.print((int32_t)x.FUDGE0[1]);Serial.println(" (chB)");
}


