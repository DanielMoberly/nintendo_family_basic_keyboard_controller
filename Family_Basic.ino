/* 
Family Computer Basic Nintendo Keyboard convert
By Daniel Moberly

@see https://wiki.nesdev.com/w/index.php/Expansion_port
@see https://wiki.nesdev.com/w/index.php/Input_devices
@see https://wiki.nesdev.com/w/index.php/Family_BASIC_Keyboard

Hardware interface
Input ($4016 write)
7  bit  0
---- ----
xxxx xKCR
      |||
      ||+-- Reset the keyboard to the first row.
      |+--- Select column, row is incremented if this bit goes from high to low.
      +---- Enable keyboard matrix (if 0, all voltages inside the keyboard will be 5V, reading back as logical 0 always)
Incrementing the row from the (keyless) 10th row will cause it to wrap back to the first row.

Output ($4017 read)
7  bit  0
---- ----
xxxK KKKx
   | |||
   +-+++--- Receive key status of currently selected row/column.

$4016/$4017 to expansion port data:
One output port, 3 bits wide, accessible by writing the bottom 3 bits of $4016.
The values latched by $4016/write appear on the OUT0-OUT2 output pins of the 2A03/07, where OUT0 is routed to the controller ports and OUT0-OUT2 to the expansion port on the NES.
Two input ports, each 5 bits wide, accessible by reading the bottom 5 bits of $4016 and $4017. Reading $4016 and $4017 activates the /OE1 and /OE2 signals, respectively, which are routed to the controller ports and the expansion port.
On the original Famicom, the two ports differ: $4016 D0 and D2 and $4017 D0 are permanently connected to both controllers, while $4016 D1 and all of $4017's D0-D4 are connected to the expansion port.

Pinout
( Female DA-15 on peripheral )
             /\
            |   \
        GND | 01  \
            |   09 | /OE for joypad 2
      SOUND | 02   |
            |   10 | Out 2 ($4016.2)
        IRQ | 03   |
            |   11 | Out 1 ($4016.1)
Joypad 2 D4 | 04   |
            |   12 | Out 0 ($4016.0, Strobe)
Joypad 2 D3 | 05   |
            |   13 | Joypad 1 D1
Joypad 2 D2 | 06   |
            |   14 | /OE for Joypad 1
Joypad 2 D1 | 07   |
            |   15 | +5V
Joypad 2 D0 | 08  /
            |   /
             \/

Matrix
Column 0  Column 1
$4017 bit 4 3 2 1 4 3 2 1
Row 0 ] [ RETURN  F8  STOP  ¥ RSHIFT  KANA
Row 1  ;   :  @ F7  ^ - / _
Row 2 K L O F6  0 P , .
Row 3 J U I F5  8 9 N M
Row 4 H G Y F4  6 7 V B
Row 5 D R T F3  4 5 C F
Row 6 A S W F2  3 E Z X
Row 7 CTR Q ESC F1  2 1 GRPH  LSHIFT
Row 8 LEFT  RIGHT UP  CLR HOME  INS DEL SPACE DOWN

Usage
Family BASIC reads the keyboard state with the following procedure:
1) Write $05 to $4016 (reset to row 0, column 0)
2) Write $04 to $4016 (select column 0, next row if not just reset)
3) Read column 0 data from $4017
4) Write $06 to $4016 (select column 1)
5) Read column 1 data from $4017
6) Repeat steps 2-5 eight more times
7) Note that Family BASIC never writes to $4016 with bit 2 clear, there is no need to disable the keyboard matrix.
*/

int RXLED = 17;  // The RX LED has a defined Arduino pin
// The TX LED was not so lucky, we'll need to use pre-defined
// macros (TXLED1, TXLED0) to control that.
// (We could use the same macros for the RX LED too -- RXLED1,
//  and RXLED0.)

// Declare the input $4016 pins
int OUT0 = 7; // pin 7 wired to DB15 pin 12
int OUT1 = 8; // pin 8 wired to DB15 pin 11
int OUT2 = 9; // pin 9 wired to DB15 pin 10
// Declare the output $4017 pins
int D1 = 10; // pin 10 wired to DB15 pin 7
int D2 = 16; // pin 16 wired to DB15 pin 5
int D3 = 14; // pin 14 wired to DB15 pin 4
int D4 = 15; // pin 15 wired to DB15 pin 3
// pin VCC wired to DB15 pin 15
// pin GND wired to DB15 pin 1

// Declare variable to determine if shift is active.
bool shiftLeftActive = false;
bool shiftRightActive = false;
// Declare variable to determine if kana is active.
bool kanaActive = false;

// Declare the matrix of numberpad values - these are used for entering alt codes.
char numpad[11] = {
  '\352', // 0
  '\341', // 1
  '\342', // 2
  '\343', // 3
  '\344', // 4
  '\345', // 5
  '\346', // 6
  '\347', // 7
  '\350', // 8
  '\351', // 9
  '\337', // +
};

// Declare the matrix of key values.
char keyMatrix[][8] = {
  // Row 0:
  {
    // Column 0:
    ']', // ]
    '[',  // [
    0xB0, // RETURN
    0xC9, // F8
    // Column 1:
    0xB2, // STOP functioning as a backspace
    '¥', // ¥ functioning as a backslash
    NULL, // RSHIFT 
    NULL, // KANA
  },
  // Row 1:
  {
    // Column 0:
    ';', // ;
    ':', // :
    '@', // @
    0xC8, // F7
    // Column 1:
    '^', // ^
    '-', // -
    '/', // /
    '_', // _
  },
  // Row 2: 
  {
    // Column 0:
    'k', // k
    'l', // l
    'o', // o
    0xC7, // F6
    // Column 1:
    '0', // 0
    'p', // p
    ',', // ,
    '.', // .
  },
  // Row 3: 
  {
    // Column 0:
    'j', // j
    'u', // u
    'i', // i
    0xC6, // F5
    // Column 1:
    '8', // 8
    '9', // 9
    'n', // n
    'm', // m
  },
  // Row 4: 
  {
    // Column 0:
    'h', // h
    'g', // g
    'y', // y
    0xC5, // F4
    // Column 1:
    '6', // 6
    '7', // 7
    'v', // v
    'b', // b
  },
  // Row 5: 
  {
    // Column 0:
    'd', // d
    'r', // r
    't', // t
    0xC4, // F3
    // Column 1:
    '4', // 4
    '5', // 5
    'c' , // c
    'f', // f
  },
  // Row 6: A S W F2 3 E Z X
  {
    // Column 0:
    'a', // a
    's', // s
    'w', // w
    0xC3, // F2
    // Column 1:
    '3', // 3 
    'e', // e
    'z', // z
    'x', // x
  },
  // Row 7:
  {
    // Column 0:
    0x80, // CTR
    'q', // q
    0xB1, // ESC
    0xC2, // F1
    // Column 1:
    '2', // 2
    '1', // 1
    NULL, // GRPH functioning as an alt key
    NULL, // LSHIFT
  },
  // Row 8:
  {
    // Column 0:
    0xD8, // LEFTARROW
    0xD7, // RIGHTARROW
    0xDA, // UPARROW
    0xB3, // CLR_HOME functioning as a tab
    // Column 1:
    0xD1, // INS
    0xD4, // DEL
    ' ', // SPACE
    0xD9, // DOWNARROW
  },
};

// Declare the matrix of shifted key values.
char keyShiftMatrix[][8] = {
  // Row 0:
  {
    // Column 0:
    '}', // ]
    '{',  // [
    0xB0, // RETURN
    0xC9, // F8
    // Column 1:
    0xB2, // STOP functioning as a backspace
    '\\', // ¥ functioning as a backslash
    NULL, // RSHIFT 
    NULL, // KANA
  },
  // Row 1:
  {
    // Column 0:
    '+', // ;
    '*', // :
    '@', // @
    0xC8, // F7
    // Column 1:
    '^', // ^
    '=', // -
    '?', // /
    '_', // _
  },
  // Row 2: 
  {
    // Column 0:
    'K', // k
    'L', // l
    'O', // o
    0xC7, // F6
    // Column 1:
    '0', // 0
    'P', // p
    '<', // ,
    '>', // .
  },
  // Row 3: 
  {
    // Column 0:
    'J', // j
    'U', // u
    'I', // i
    0xC6, // F5
    // Column 1:
    '(', // 8
    ')', // 9
    'N', // n
    'M', // m
  },
  // Row 4: 
  {
    // Column 0:
    'H', // h
    'G', // g
    'Y', // y
    0xC5, // F4
    // Column 1:
    '&', // 6
    '\'', // 7
    'V', // v
    'B', // b
  },
  // Row 5: 
  {
    // Column 0:
    'D', // d
    'R', // r
    'T', // t
    0xC4, // F3
    // Column 1:
    '$', // 4
    '%', // 5
    'C' , // c
    'F', // f
  },
  // Row 6: A S W F2 3 E Z X
  {
    // Column 0:
    'A', // a
    'S', // s
    'W', // w
    0xC3, // F2
    // Column 1:
    '#', // 3 
    'E', // e
    'Z', // z
    'X', // x
  },
  // Row 7:
  {
    // Column 0:
    0x80, // CTR
    'Q', // q
    0xB1, // ESC
    0xC2, // F1
    // Column 1:
    '"', // 2
    '!', // 1
    NULL, // GRPH functioning as an alt key
    NULL, // LSHIFT
  },
  // Row 8:
  {
    // Column 0:
    0xD8, // LEFTARROW
    0xD7, // RIGHTARROW
    0xDA, // UPARROW
    0xC1, // CLR_HOME functioning as a caps lock
    // Column 1:
    0xD1, // INS
    0xD4, // DEL
    ' ', // SPACE
    0xD9, // DOWNARROW
  },
};

// FIXME - kana is not implemented.
// Declare the matrix of kana key values.
char keyKanaMatrix[][8] = {
  // Row 0:
  {
    // Column 0:
    '}', // ]
    '{',  // [
    0xB0, // RETURN
    0xC9, // F8
    // Column 1:
    0xB2, // STOP
    '|', // ¥
    NULL, // RSHIFT 
    NULL, // KANA
  },
  // Row 1:
  {
    // Column 0:
    '+', // ;
    '*', // :
    '@', // @
    0xC8, // F7
    // Column 1:
    '^', // ^
    '=', // -
    '?', // /
    '_', // _
  },
  // Row 2: 
  {
    // Column 0:
    'K', // k
    'L', // l
    'O', // o
    0xC7, // F6
    // Column 1:
    '0', // 0
    'P', // p
    '<', // ,
    '>', // .
  },
  // Row 3: 
  {
    // Column 0:
    'J', // j
    'U', // u
    'I', // i
    0xC6, // F5
    // Column 1:
    '(', // 8
    ')', // 9
    'N', // n
    'M', // m
  },
  // Row 4: 
  {
    // Column 0:
    'H', // h
    'G', // g
    'Y', // y
    0xC5, // F4
    // Column 1:
    '&', // 6
    '\'', // 7
    'V', // v
    'B', // b
  },
  // Row 5: 
  {
    // Column 0:
    'D', // d
    'R', // r
    'T', // t
    0xC4, // F3
    // Column 1:
    '$', // 4
    '%', // 5
    'C' , // c
    'F', // f
  },
  // Row 6: A S W F2 3 E Z X
  {
    // Column 0:
    'A', // a
    'S', // s
    'W', // w
    0xC3, // F2
    // Column 1:
    '3', // 3 
    'E', // e
    'Z', // z
    'X', // x
  },
  // Row 7:
  {
    // Column 0:
    0x80, // CTR
    'Q', // q
    0xB1, // ESC
    0xC2, // F1
    // Column 1:
    '"', // 2
    '!', // 1
    0x82, // GRPH
    NULL, // LSHIFT
  },
  // Row 8:
  {
    // Column 0:
    0xD8, // LEFTARROW
    0xD7, // RIGHTARROW
    0xDA, // UPARROW
    0xC1, // CLR_HOME
    // Column 1:
    0xD1, // INS
    0xD4, // DEL
    ' ', // SPACE
    0xD9, // DOWNARROW
  },
};

// Declare the matrix of key activity states.
int keyActivityState[][8] = {
  // Row 0
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 1
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 2
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 3
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 4
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 5
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 6
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 7
  {0, 0, 0, 0, 0, 0, 0, 0},
  // Row 8
  {0, 0, 0, 0, 0, 0, 0, 0},
};

/*
 * Initial setup.
 */
void setup()
{
  pinMode(RXLED, OUTPUT);  // Set RX LED as an output
  
  // Set the 3 $4016 pins as output.
  pinMode(OUT0, OUTPUT);
  pinMode(OUT1, OUTPUT);
  pinMode(OUT2, OUTPUT);
  // Set the 4 $4017 pins as input
  pinMode(D1, INPUT);
  pinMode(D2, INPUT);
  pinMode(D3, INPUT);
  pinMode(D4, INPUT);

  // Enable the keyboard.
  digitalWrite(OUT0, HIGH);
  digitalWrite(OUT1, HIGH);
  digitalWrite(OUT2, HIGH);

  // Slight delay to allow activation - per Tommy only 90 microseconds.
  delayMicroseconds(90);

  //Serial.begin(9600);
}

/*
 * Main program loop.
 */
void loop()
{
  // Turn off the lights.
  digitalWrite(RXLED, HIGH);
  TXLED1; 

  // Step 1: Write $05 to $4016 (reset to row 0, column 0)
  digitalWrite(OUT2, HIGH);
  digitalWrite(OUT1, LOW);
  digitalWrite(OUT0, HIGH);

  // Slight delay to allow activation - per Tommy only 90 microseconds.
  delayMicroseconds(90);

  // Poll all 8 rows of the keyboard for input.
  for (int row = 0; row <= 8; row++) {
    // Scan the row.
    keyboardPollRow(row);
  }
}

/*
 * Poll a row of the keyboard (rows 1 - 8)
 */
void keyboardPollRow(int rowNumber) {
  // Step 2: Write $04 to $4016 (select column 0, next row if not just reset)
  sendKeyboardData(HIGH, LOW, LOW);

  // Step 3: Read column 0 data from $4017
  readData(rowNumber, 0);

  // Step 4: Write $06 to $4016 (select column 1)
  sendKeyboardData(HIGH, HIGH, LOW);

  // Step 5: Read column 1 data from $4017
  readData(rowNumber, 4);
}

/*
 * Function to send data to keyboard.
 */
void sendKeyboardData(int output2, int output1, int output0) {
  // Write to $4016
  digitalWrite(OUT2, output2);
  digitalWrite(OUT1, output1);
  digitalWrite(OUT0, output0);

  // Slight delay to allow activation - per documentation only 90 microseconds.
  delayMicroseconds(90);
}

/*
 * Function to read the data from a row/column.
 */
void readData(int rowNumber, int startIndex) {
  // Read the values in this Row/Column.
  int values[4] = {
    digitalRead(D4),
    digitalRead(D3),
    digitalRead(D2),
    digitalRead(D1),
  };
  // Loop over the values.
  for (int bitIndex = 0; bitIndex <= 3; bitIndex++) {
    // Handle this key value.
    handleKeyValue(rowNumber, startIndex + bitIndex, values[bitIndex]);
  }
}

/*
 * Function to handle the current key value.
 */
void handleKeyValue(int rowNumber, int columnNumber, int keyState) {
    
  // If this is left or right shift.
  if (rowNumber == 7 && columnNumber == 7) {
    // Set shift value.
    shiftLeftActive = keyState;
  }
  if (rowNumber == 0 && columnNumber == 6) {
    // Set shift value.
    shiftRightActive = keyState;
  }
  if (rowNumber == 0 && columnNumber == 7) {
    // Set kana value.
    kanaActive = keyState;
    // FIXME - kana is not implemented.
  }
  if (rowNumber == 7 && columnNumber == 0) {
    // Press down control if its active.
    if (keyState) {
      Keyboard.press(KEY_LEFT_CTRL);
    }
    else {
      Keyboard.release(KEY_LEFT_CTRL);
    }
  }
  if (rowNumber == 7 && columnNumber == 6) {
    // Press down alt if its active.
    if (keyState) {
      Keyboard.press(KEY_LEFT_ALT);
    }
    else {
      Keyboard.release(KEY_LEFT_ALT);
    }
  }
    
  // If the key is depressed.
  if (keyState) {
    // Only output on initial key press, or if it has been held down for a while we output it repeatedly.
    if (keyActivityState[rowNumber][columnNumber] == 1 || ( keyActivityState[rowNumber][columnNumber] > 100 && (keyActivityState[rowNumber][columnNumber] % 20) == 0)) {
      char key;
      // If shift is active, need the shifted key.
      if (shiftLeftActive || shiftRightActive || kanaActive) {
        // Get the shiftted key value.
        key = keyShiftMatrix[rowNumber][columnNumber];
      }
      else {
        // Get the base key value.
        key = keyMatrix[rowNumber][columnNumber];
      }

      // Need special command for Yen.
      if (!shiftLeftActive && !shiftRightActive && !kanaActive && rowNumber == 0 && columnNumber == 5) {
        Keyboard.press(KEY_LEFT_ALT);
        Keyboard.write(numpad[1]);
        Keyboard.write(numpad[5]);
        Keyboard.write(numpad[7]);
        Keyboard.release(KEY_LEFT_ALT);
      }
      else {
        // Send the keyboard input.
        //Serial.print(key);
        Keyboard.write(key);
      }
    }
    
    // Set the activity state.
    keyActivityState[rowNumber][columnNumber]++;
  }
  else {
    // Set the activity state.
    keyActivityState[rowNumber][columnNumber] = 0;
  }
}

/* 
 *  Function for sending complicated keys not handled by normal ASCII.
 *  This is only accessible after making the sendReport function public in the file hardware\arduino\cores\arduino directory of your Arduino install
 *  @see https://www.sparkfun.com/tutorials/337
 */
void sendComplicatedKey(byte key, byte modifiers)
{
  KeyReport report = {0};  // Create an empty KeyReport
  
  /* First send a report with the keys and modifiers pressed */
  report.keys[0] = key;  // set the KeyReport to key
  report.modifiers = modifiers;  // set the KeyReport's modifiers
  report.reserved = 1;
  Keyboard.sendReport(&report);  // send the KeyReport
  
  /* Now we've got to send a report with nothing pressed */
  for (int i=0; i<6; i++)
    report.keys[i] = 0;  // clear out the keys
  report.modifiers = 0x00;  // clear out the modifires
  report.reserved = 0;
  Keyboard.sendReport(&report);  // send the empty key report
}
