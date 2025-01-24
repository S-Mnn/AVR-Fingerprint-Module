#define F_CPU 4000000 /* Using default clock 4MHz */
#define LCD PORTA
#define EN 7
#define RW 6
#define RS 5

#define USART0_BAUD_RATE(BAUD_RATE) ( (float) (F_CPU * 64 / (16 * (float)BAUD_RATE)) + 0.5)

#include <avr/io.h>
#include <util/delay.h>



///////////////////////////////////////////////////////////////////////////////////////////////////LCD INITIALIZATION
void lcdcmd(unsigned char cmd) {
    LCD.OUT = cmd & 0xF0;           // Pins copy upper 4 bits
    PORTD.OUT &= ~(1 << RS);        // RS = 0 for command
    PORTD.OUT &= ~(1 << RW);        // RW = 0 for write
    PORTD.OUT |= (1 << EN);         // EN high
    _delay_ms(20);
   PORTD.OUT &= ~(1 << EN);         // EN low
   LCD.OUT = cmd << 4;              // Pins copy next 4 bits
  PORTD.OUT |= (1 << EN);           // EN high
   _delay_ms(20);
    PORTD.OUT &= ~(1 << EN);        // EN low
    _delay_ms(3);

}

void lcddata(unsigned char data) {
    LCD.OUT =  data & 0xF0;
    PORTD.OUT |= (1 << RS);         // RS = 1 for data
    PORTD.OUT &= ~(1 << RW);        // RW = 0 for write
    PORTD.OUT |= (1 << EN);         // EN high
    _delay_ms(10);                          // LETTER PRINTING T/2
    PORTD.OUT &= ~(1 << EN);        // EN low

    LCD.OUT = data << 4;
   PORTD.OUT |= (1 << EN);          // EN high
   _delay_ms(20);
    PORTD.OUT &= ~(1 << EN);        // EN low
    _delay_ms(10);                          // LETTER PRINTING T/2

}

void lcd_init() {
    PORTA.DIRSET = 0xF0;            // Set A0 to A7 as output (DATA)
    PORTD.DIRSET = 0b11100000;            // Set D0 to D7 as output (CTRL)
    PORTD.OUT &= ~(1 << EN);        // Initialize EN low
    _delay_ms(20);                  // Hold for LCD



    lcdcmd(0x33);                   // Following commands initialize 4bit mode
    lcdcmd(0x32); 
    lcdcmd(0x28);
    lcdcmd(0x0E);                   // Cursor on
    lcdcmd(0x01);                   // Clear display
    _delay_ms(2);
}

void lcd_print(const char* str) {
    unsigned int i = 0;
    while(str[i] != '\0') 
    {
        lcddata(str[i]);
        i++;
    }
}


/////////////////////////////////////////////////////////////////////////////////////////FINGERPRINT INITIALIZATION

/*
 * File:   finalcode.c
 * Author: sudee
 *
 * Created on March 12, 2024, 6:55 AM
 */

//Baud Rate Calculations and Defines
/*---------------------------------------------------------------------------*/

#define FB_connect	1
#define F_find		2
#define F_im1		3
#define F_im2		4
#define F_cretModl	5
#define F_store		6
#define F_delete	7
#define FP_empty	8
#define F_search	9

#define success		0
#define fail		1
#define again		2

//Led defines
#define output1 PORTA.DIRSET = PIN2_bm; //set pin 2 (green led) as output
#define GreenledOff PORTA.OUT &= ~PIN2_bm;
#define GreenledOn PORTA.OUT |= PIN2_bm;

#define output2 PORTD.DIRSET = PIN1_bm; //set pinD 1 (red led) as output
#define RedledOff PORTD.OUT &= ~PIN1_bm;
#define RedledOn PORTD.OUT |= PIN1_bm;

#define output3 PORTA.DIRSET = PIN3_bm; //set pin 3 (blue led as output)
#define BlueledOff PORTA.OUT &= ~PIN3_bm;
#define BlueledOn PORTA.OUT |= PIN3_bm;

#define input1 PORTD.DIRCLR = PIN2_bm; //button 1
#define input2 PORTD.DIRCLR = PIN3_bm; //button 2

// End of Baud Rate Calculations and Defines
/*----------------------------------------------------------------------------*/

// Includes
/*---------------------------------------------------------------------------*/

#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <avr/interrupt.h>

// End of Includes
/*---------------------------------------------------------------------------*/

// Global Variables
/*---------------------------------------------------------------------------*/
// ISR variables
volatile char cont;
volatile char rcvData[15];

//First three Packages
const char Header[]			= {0xEF, 0x1};
const char Address[]		= {0xFF, 0xFF, 0xFF, 0xFF};
const char Command[]		= {0x1, 0x00};

//commands Packages
const char PassVfy[]		= {0x7, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B};
const char f_detect[]		= {0x3, 0x1, 0x0, 0x5}; //genimage
const char f_imz2ch1[]		= {0x4, 0x2, 0x1, 0x0, 0x8};
const char f_imz2ch2[]		= {0x4, 0x2, 0x2, 0x0, 0x9};
const char f_createModel[]	= {0x3, 0x5, 0x0, 0x9};
char f_storeModel[]			= {0x6, 0x6, 0x1, 0x0, 0x1, 0x0, 0xE};
const char f_search[]		= {0x8, 0x4, 0x1, 0x0, 0x0, 0x0, 0xA3, 0x0, 0xb1};
char f_delete[]				= {0x7, 0xC, 0x0, 0x0, 0x0, 0x1, 0x0, 0x15};
char fp_empty[]				= {0x3,0xd,0x0,0x11};

// End of Global Variables
/*---------------------------------------------------------------------------*/

// Function Declerations
/*---------------------------------------------------------------------------*/

char sendcmd2fb(unsigned char order);
char GetID();
void enroll();
void Search();

//End of function Declerations
/*---------------------------------------------------------------------------*/



// USART Functions
/*-------------------------------------------------------------------------*/
ISR(USART0_RXC_vect) {
    // Read the data from buffer
    rcvData[cont++] = USART0.RXDATAL;
    // Clear the interrupt flag by reading the flag register
    USART0.STATUS |= (1<<USART_RXCIF_bp);
}

void clearArray(unsigned char *str)
{
  while(*str)
  {
    *str = 0;
    *str++;
  }
}

void USART0_init(void)
{
    PORTA.DIR &= ~PIN1_bm; //set PA1 as reciever
    PORTA.DIR |= PIN0_bm; //set PA0 as output transmitter

    USART0.BAUD = (uint16_t)USART0_BAUD_RATE(57600); //set baud rate to desired value
    USART0.CTRLB |= USART_TXEN_bm; // enable transmitter
    USART0.CTRLB |= USART_RXEN_bm; //enable reciever
}

void USART0_InterrptInit(void) {
    //set baud rate to desired value
    USART0.BAUD = (uint16_t)USART0_BAUD_RATE(57600);
    // Set PA1 as receiver
    PORTA.DIR &= ~PIN1_bm;
    // Set PA0 as transmitter
    PORTA.DIR |= PIN0_bm;
    // Enable transmitter and receiver
    USART0.CTRLB |= USART_TXEN_bm | USART_RXEN_bm;
    // Enable receive complete interrupt
    USART0.CTRLA |= USART_RXCIE_bm;
    // Enable global interrupts
    sei();
}

// UART Receive function for AVR128DB28
unsigned char USART0_Receive() {
    // Wait till data is received
    while (!(USART0.STATUS & (1 << USART_RXCIF_bp)));
    // Return the byte
    return USART0.RXDATAL;
}

void USART0_Transmit(char data)
{ // Check to see previous transmission is completed by checking STATUS register
    while(!(USART0.STATUS & USART_DREIF_bm))
    {
        ;
    }
    USART0.TXDATAL = data; //Transmitting character
}

void USART0_Array(const char *data, int size){
    for(int i=0; i<size; i++){
        USART0_Transmit(*data);
        *data++;
    }
}

void USART0_sendString(const char *str)
{
    for(size_t i = 0; i < strlen(str); i++)
    {
        USART0_Transmit(str[i]);
    }
}

// End of USART functions
/*--------------------------------------------------------------------------*/

// Led test functions
/*--------------------------------------------------------------------------*/

void ToggleGreen(){
    //output1;
    GreenledOn;
     _delay_ms(50);
    GreenledOff;
}

void ToggleRed(){
    //output2;
    RedledOn;
    _delay_ms(50);
    RedledOff;
}

void ToggleBlue(){
    BlueledOn;
    _delay_ms(50);
    BlueledOff;
}

// End of led test functions
/*--------------------------------------------------------------------------*/

// Start of Finger Print Program Functions
/*--------------------------------------------------------------------------*/

char sendcmd2fb(unsigned char order)
{
    unsigned char successed = again;

    while(successed == again)
    {
       /* Stage 1 LCD code will go here
        switch(order)...
        * 
        */


     /* Second stage commands to module*/
    //First three Packages (Common between any command)
    USART0_Array(&Header ,2);
  USART0_Array(&Address,4);
  USART0_Array(&Command,2);

    int arr_size;
    unsigned char ID;

    // Choosing which command to send to FP
    switch(order)
    {
        case FB_connect:
            arr_size = (sizeof(PassVfy)) / sizeof((PassVfy)[0]);
            USART0_Array(&PassVfy,arr_size);
            break;

        case F_find:
            arr_size = (sizeof(f_detect)) / sizeof((f_detect)[0]);
            USART0_Array(&f_detect,arr_size);
            break;

        case F_im1:
            arr_size = (sizeof(f_imz2ch1)) / sizeof((f_imz2ch1)[0]);
            USART0_Array(&f_imz2ch1,arr_size);
            break;

         case F_im2:
            arr_size = (sizeof(f_imz2ch2)) / sizeof((f_imz2ch2)[0]);
            USART0_Array(&f_imz2ch2,arr_size);
            break;

         case F_cretModl:
            arr_size = (sizeof(f_createModel)) / sizeof((f_createModel)[0]);
            USART0_Array(&f_createModel,arr_size);
            break;

         case FP_empty:
            arr_size = (sizeof(fp_empty)) / sizeof((fp_empty)[0]);
            USART0_Array(&fp_empty,arr_size);
            break;

        case F_search:
            arr_size = (sizeof(f_search)) / sizeof((f_search)[0]);
            USART0_Array(&f_search,arr_size);
            break;

        case F_store:
            ID = GetID();
            f_storeModel[4] = ID;
            f_storeModel[6] = (0xE + ID);
            arr_size = (sizeof(f_storeModel)) / sizeof((f_storeModel)[0]);
            //USART0_Array(&Header ,2);
            //USART0_Array(&Address,4);
            //USART0_Array(&Command,2);
            USART0_Array(&f_storeModel,arr_size);
            break;

        case F_delete:
            ID = GetID();
            f_delete[3] = ID;
            f_delete[7] = (0x15 + ID);
            arr_size = (sizeof(f_delete)) / sizeof((f_delete)[0]);
            USART0_Array(&Header ,2);
            USART0_Array(&Address,4);
            USART0_Array(&Command,2);
            USART0_Array(&f_delete,arr_size);
            break; 
    }
    _delay_ms(1000);

    /*Third stage: From module 
     If Failed print on lcd screen*/

    //Feedback (Acknowledge) from fingerprint module
    if(cont>1)
    {
        if( rcvData[6] == 0x07 && (rcvData[8] == 0x03 || rcvData[8] == 0x07) )
        {
            //go to lcd second row
            if(rcvData[9]==0)
                successed = success;
            else
            {
                successed = fail;
                if(rcvData[9] == 0x01){ // rcv packet error
                  lcdcmd(0x01);
                    lcd_print("rcv packet error");
                    ToggleRed();}
                else if(rcvData[9] == 0x04){ // fail try again
                  lcdcmd(0x01);
                    lcd_print("fail try again");
                    ToggleRed();}
                else if (rcvData[9] == 0x05){
                  lcdcmd(0x01);
                    lcd_print("fail clean and try");// fail clean and try
                    ToggleRed();}
                else if (rcvData[9]== 0x06){
                  lcdcmd(0x01);
                  lcd_print("failed generated ");// failed generated 
                    ToggleRed();}
                else if (rcvData[9]== 0x07){
                  lcdcmd(0x01);
                  lcd_print("small finger area");// small finger area
                    ToggleRed();}
                else if (rcvData[9] == 0x09){
                  lcdcmd(0x01);
                  lcd_print("ID not found");// ID not found
                    ToggleRed();}
                else if (rcvData[9] == 0x0b){
                  lcdcmd(0x01);
                  lcd_print("ID overload");// ID overload
                    ToggleRed();}
                else if (rcvData[9] == 0x18){
                  lcdcmd(0x01);
                  lcd_print("flash writing error");//flash writing error
                    ToggleRed();}
                else if(rcvData[9] == 0x0a){
                  lcdcmd(0x01);
                  lcd_print("failed modeling");//failed modeling
                    ToggleRed();}
                else if(rcvData[9] == 0x13){
                  lcdcmd(0x01);
                  lcd_print("password incorrect");//password incorrect
                    ToggleRed();}
                else if(rcvData[9] == 0x21){
                  lcdcmd(0x01);
                  lcd_print("must verify pass");//must verify pass
                    ToggleRed();}
                else
                {
                    if(rcvData[9] == 0x02){
                      lcdcmd(0x01);
                      lcd_print("Waiting");
                      lcdcmd(0xC0);
                        lcd_print("for finger");//cant detect finger
                        ToggleRed();}
                    else if(rcvData[9] == 0x03){
                      lcdcmd(0x01);
                      lcd_print("fail to connect");// fail to connect
                        ToggleRed();}
                    else {
                      lcdcmd(0x01);
                      lcd_print("undefined error");//not defined error
                        ToggleRed();}
                    successed = again;
                } //continue from here
            }
        }
        else
        {
          lcdcmd(0x01);
          lcd_print("fp module err");//fp module connection error
            ToggleRed();
        }

    }
    else
    {
        lcdcmd(0x01);
        lcd_print("fp not responding");
        //fp not found or not responding
        ToggleRed();
        _delay_ms(1000);
    }

    if(successed == success)
    {
        switch(order)
        {
            case FB_connect:
              lcdcmd(0x01);
              lcd_print("Module found");//module found
                ToggleGreen();
                break;
            case F_find:
              lcdcmd(0x01);
              lcd_print("Finger found");
              lcdcmd(0xC0);
              lcd_print("Starting scan");//finger captured
                ToggleGreen();
                ToggleBlue();
                break;
            case F_im1:
              lcdcmd(0x01);
              lcd_print("Scanning...");// done
               ToggleGreen();
               ToggleBlue();
               break;
            case F_im2:
              lcdcmd(0x01);
              lcd_print("Scanning...");// done
               ToggleGreen();
               ToggleBlue();
               break;
            case F_cretModl:
              lcdcmd(0x01);
              lcd_print("Model created");//model created
               ToggleGreen();
               ToggleBlue();
               break;
            case FP_empty:
              lcdcmd(0x01);
              lcd_print("memory erased");// memory erased
               ToggleGreen();
               break;
            case F_search:
              lcdcmd(0x01);
              lcd_print("Found ID");// found id
               ToggleGreen();
               break;
            case F_store:
              lcdcmd(0x01);
              lcd_print("Save successful");// saved successful
               ToggleGreen();
               _delay_ms(250);
               break;
            case F_delete:
              lcdcmd(0x01);
              lcd_print("delete successful");// delete successful
               ToggleGreen();
               _delay_ms(700);
               break;
        }
    }
    clearArray(&rcvData);
    cont = 0;
    _delay_ms(700);

  }//end of while
    return successed;
}//end of sendcmd2fb

char GetID()
{
   //clear lcd
    static unsigned char ids = 1;
    //increment id value each time function is called 
    //return current id
    return ids++;

}//end of GetID

void enroll()
{
    loop:
    if(sendcmd2fb(F_find)) goto loop;
    if(sendcmd2fb(F_im1)) goto loop;
    if(sendcmd2fb(F_find)) goto loop;
    if(sendcmd2fb(F_im2)) goto loop;
    if(sendcmd2fb(F_cretModl)) goto loop;
    if(sendcmd2fb(F_store)) goto loop;
} //end of enroll

void Search()
{
    loop:
    if(sendcmd2fb(F_find)) goto loop;
    if(sendcmd2fb(F_im1)) goto loop;
    if(sendcmd2fb(F_search)) goto loop;

}


//End of Finger Print Program Functions
/*--------------------------------------------------------------------------*/


int main(void) {
    int enroll_state = 1;
    int search_state = 0;
    output1;
    output2;
    output3;
    input1;
    input2;

    lcd_init();

    USART0_InterrptInit();
     _delay_ms(500);
    sendcmd2fb(FB_connect);
    _delay_ms(500);
    //enroll();
    /* Replace with your application code */
    while (1) {
        //if((PORTD.IN & PIN2_bm))
        //{
        //    Search();
        //}
        if((PORTD.IN & PIN3_bm) && (enroll_state == 1))
        {
            lcdcmd(0x01);
            lcd_print("Starting enroll");
            enroll();
            enroll_state = 0;
            search_state = 1;
        }
        if((PORTD.IN & PIN2_bm) && (search_state == 1))
        {
            Search();
        }

    } //end of while
} //end of main

/*
 * int main(void) {
    lcd_init();
    lcd_print("User_Name");
    lcdcmd(0xC0);                   // Set cursor to next line
    lcd_print("Access Granted");
    _delay_ms(10);
    while (1) {
        //(0x1C);             // Activate scrolling
        //_delay_ms(50);            // Scrolling period
    }
    return 0;}
 */