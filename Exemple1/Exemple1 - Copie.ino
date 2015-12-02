//************************************************************
// normal mode, sends one byte, std messages
//
//************************************************************

#include <MCP23X08.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <SPI.h>
#include <MCP2510.h>  // The CAN interface device
#include <Canutil.h>  // The CAN interface utilities

MCP2510  can_dev(9); // defines pb1 (arduino pin9) as the _CS pin for MCP2510
Canutil  canutil(can_dev);

// MCP2510 defines some basic access function to the CAN device like write register, read register and so on.
// For example, directly writing to the CANINTE register of the MCP2510 is:
//  can_dev.write(CANINTE,0x03);
//
// Canutil library defines "higher level" functions which manipulate more than one register and/or argument,
// like acceptance mask programming for example:
// canutil.setAcceptanceFilter(0x2AB, 2000, 0, 0);

// MCP2510 can be used standalone, Canutil must be used in conjunction with MCP2510


LiquidCrystal lcd(15, 0, 14, 4, 5, 6, 7);  //  4 bits without R/W pin
MCP23008 i2c_io(MCP23008_ADDR);         // Init MCP23008

uint8_t canstat = 0;
uint8_t opmode, txstatus;
uint8_t tosend[8];  // size  MUST be excatly 8
uint8_t recSize, recData[8];
uint8_t push = 1;
uint16_t msgID = 0x2AA;



void setup() {
  pinMode(3, INPUT); // switch SW4 initiates a transmission, jumper JMP4 must be in position A
  i2c_io.Write(IOCON, 0x04);   // makes I2C interrupt pin open-drain to allow sharing pin 3 with MCP23008 without conflict

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("simple TX");
  opmode = canutil.whichOpMode();
  lcd.print(" mode ");
  lcd.print(opmode, DEC);

  canutil.flashRxbf();  //just for fun!


  can_dev.write(CANINTE, 0x01); //disables all interrupts but RX0IE (received message in RX buffer 0)
  can_dev.write(CANINTF, 0x00);  // Clears all interrupts flags

  canutil.setClkoutMode(0, 0); // disables CLKOUT
  canutil.setTxnrtsPinMode(0, 0, 0); // all TXnRTS pins as all-purpose digital input

  canutil.setOpMode(4); // sets configuration mode
  // IMPORTANT NOTE: configuration mode is the ONLY mode where bit timing registers (CNF1, CNF2, CNF3), acceptance
  // filters and acceptance masks can be modified

  canutil.waitOpMode(4);  // waits configuration mode

  // Bit timing section
  //  setting the bit timing registers with Fosc = 16MHz -> Tosc = 62,5ns
  // data transfer = 125kHz -> bit time = 8us, we choose arbitrarily 8us = 16 TQ  (8 TQ <= bit time <= 25 TQ)
  // time quanta TQ = 2(BRP + 1) Tosc, so BRP =3
  // sync_seg = 1 TQ, we choose prop_seg = 2 TQ
  // Phase_seg1 = 7TQ yields a sampling point at 10 TQ (60% of bit length, recommended value)
  // phase_seg2 = 6 TQ SJSW <=4 TQ, SJSW = 1 TQ chosen
  can_dev.write(CNF1, 0x03); // SJW = 1, BRP = 3
  can_dev.write(CNF2, 0b10110001); //BLTMODE = 1, SAM = 0, PHSEG = 6, PRSEG = 1
  can_dev.write(CNF3, 0x05);  // WAKFIL = 0, PHSEG2 = 5

  canutil.setOpMode(0); // sets normal mode
  opmode = canutil.whichOpMode();
  lcd.setCursor(15, 0);
  lcd.print(opmode, DEC);


  canutil.setTxBufferDataLength(0, 1, 0); // TX normal data, 1 byte long, with buffer 0


  for (int i = 0; i < 8; i++) { // resets the table
    tosend[i] = 0;
  }

  delay(1000);

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("ready ");
}



void loop() {

  do {
    push = digitalRead(3);
    delay(50);
  }
  while (push == 1); // wait for an action on SW4 to start sending a message



  tosend[0]++;

  msgID++;
  if (msgID > 0x2AD) {
    msgID = 0x2AB;
  }  // cycles address through 2AB 2AC 2AD to check RX filter operation, 2AD will be rejected

  canutil.setTxBufferID(msgID, 2000, 0, 0); // sets the message ID, specifies standard message (i.e. short ID) with buffer 0

  canutil.setTxBufferDataField(tosend, 0);   // fills TX buffer

  canutil.messageTransmitRequest(0, 1, 3); // requests transmission of buffer 0 with highest priority


  do {
    txstatus = 0;
    txstatus = canutil.isTxError(0);  // checks tx error
    txstatus = txstatus + canutil.isArbitrationLoss(0);  // checks for arbitration loss
    txstatus = txstatus + canutil.isMessageAborted(0);  // ckecks for message abort
    txstatus = txstatus + canutil.isMessagePending(0);   // checks transmission
  }
  while (txstatus != 0); // waits for no error condition

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("ID= ");
  lcd.print(msgID, HEX);
  lcd.print(" sent ");
  lcd.print(tosend[0], HEX);

  do {
    push = digitalRead(3);
    delay(50);
  }
  while (push == 0);  // waits for SW4 release


}






//******************************************************************
//                     other routines
//******************************************************************



//************************************************
// routine attached to INT pin
//************************************************


