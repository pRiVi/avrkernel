#include <avr/interrupt.h>
#include "../main.h"
#include "kernel.h"

// #define sendFlashString(string, c) for(c=0;c<sizeof(string);c++) TransmitByte(pgm_read_byte(&string[c]));
// #define bufFlashString(buffer, pre, string, c) for(c=0;c<sizeof(string);c++) buffer[c+pre] = pgm_read_byte(&string[c]); pre += (c-1);

//#define TRANSBUFSIZECHECK 1
//#define WURGAROUND 1

//volatile char sentbytes = 0;

//#define NOHEADER 1
//#define TRANSBUFSIZECHECK 1


/*Global Variables*/
volatile unsigned char transbuf[TRANSBUFSIZE];
volatile unsigned char packets[QUESIZE];
volatile unsigned char action = HEADER;
volatile unsigned char packetPos = 0;
#ifdef TRANSBUFSIZECHECK
volatile unsigned char badheaderbytes = 0;
#endif
volatile unsigned char outpos = 0;
volatile unsigned char currentCommandLength = 0;
volatile unsigned char headerbuf[HEADERSIZE];
volatile unsigned char * outbuf;

#if defined(ATMEGA644)
#define USART_CAN_SEND_AUS UCSR0B = UCSR0B & 0b11011111;
#else
#define USART_CAN_SEND_AUS UCSRB  = UCSRB  & 0b11011111;
#endif

#if defined(ATMEGA644)
#define USART_CAN_SEND_AN UCSR0B = UCSR0B | 0b00100000;
#else
#define USART_CAN_SEND_AN UCSRB  = UCSRB  | 0b00100000;
#endif

#if defined(ATMEGA644)
#define CUR_UDR UDR0
#else
#define CUR_UDR UDR
#endif

/* Initialize UART */
void InitUART (unsigned char baudrate){
        /* Set the baud rate */
#if defined(ATMEGA644)
        UBRR0L
#else
                UBRRL
#endif
                = baudrate;

        /* Enable UART receiver and transmitter and interrupt*/
#if defined(ATMEGA644)
        UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0); 
#else
        UCSRB = (1 << RXEN) | (1 << TXEN)| (1 << RXCIE); 
#endif

        /* set to 8 data bits, 1 stop bit */
#if defined(ATMEGA644)
        UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
#else
        UCSRC = (1 << UCSZ1) | (1 << UCSZ0)
#if defined(ATMEGA8)
                | (1 << URSEL)
#endif
                ;
#endif
}

void WAITFORSEND(void){
        while(currentCommandLength != 0);
}

void sendPacket(unsigned char size) {
        while(currentCommandLength != 0);
        currentCommandLength = HEADERSIZE + size;
        USART_CAN_SEND_AN;
}

/* void TransmitByte (unsigned char data){
// Wait for empty transmit buffer
while (!(UCSRA & (1 << UDRE)));
// Start transmittion
CUR_UDR = data;
}

void TransmitChar(unsigned char x, unsigned char * buf, unsigned char pre){
unsigned char einer, zehner, hunderter;
hunderter = x /100;
x=x-(hunderter*100);
zehner = x / 10;
x=x-(zehner*10);
einer = x;
if ((buf) && (pre)) {
buf[pre++] = (hunderter+48);
buf[pre++] = (zehner+48);
buf[pre++] = (einer+48);
} else {
TransmitByte(hunderter+48);
TransmitByte(zehner+48);
TransmitByte(einer+48);
}
}*/

void removeTransbufData(unsigned char start, unsigned char length) {
        if (length == 0) return;
        unsigned char tmp = 0;
        unsigned char size = 0;
        for (tmp = 0; tmp < MAXPACKETS; tmp++) size += PACKETSIZE(tmp);
        for (tmp = start; tmp < (size-length); tmp++) transbuf[tmp] = transbuf[tmp+length];
}

unsigned char getPacket(unsigned char* curpacket, unsigned char type, unsigned char len) {
        unsigned char bufpos = 0;
        unsigned char packetid = 0;
        unsigned char tmp = 0;
        unsigned char curbufpos = 0;
        unsigned char matched = 0;
        unsigned char flashstring = 0;
        while (packetid < MAXPACKETS) {
                if (PACKETTYPE(packetid) == GARBAGE) {
                        if (packetid < (MAXPACKETS-1)) {
                                removeTransbufData(bufpos, PACKETSIZE(packetid));
                                for (tmp = packetid; tmp < (MAXPACKETS-1); tmp++) {
                                        PACKETTYPE(tmp) = PACKETTYPE(tmp+1);
                                        PACKETSIZE(tmp) = PACKETSIZE(tmp+1);
                                }
                        }
                        PACKETTYPE(MAXPACKETS-1) = EMPTY;
                        PACKETSIZE(MAXPACKETS-1) = 0;
                        continue;
                }
                bufpos += PACKETSIZE(packetid);
                packetid++;
        }
        bufpos = 0;
        for (packetid = 0; packetid < MAXPACKETS; packetid++) {
                *curpacket = packetid;
                if ((PACKETTYPE(packetid) == type) || (((type == ANY_IN ) && (
#if !defined(NOINCOMINGPACKETS)
                                                                (PACKETTYPE(packetid) == READING) || 
#endif
                                                                (PACKETTYPE(packetid) == FORWARD))) ||
                                                       ((type == ANY_OUT) && ((PACKETTYPE(packetid) == OUTPUT ) || (PACKETTYPE(packetid) == FORWARD))))) {
                        matched = 1;
                        break;
                }
                bufpos += PACKETSIZE(packetid);
        }
        if (!matched) return 255;
        if ((bufpos+len) >= TRANSBUFSIZE) {
#ifdef TRANSBUFSIZECHECK
                CUR_UDR = 'F';
#endif
                return 255;
        }
        return bufpos;
}

unsigned char bufferNewPacket(unsigned char type) {
        unsigned char c;
        unsigned char curpacket = 0;
        unsigned char bufpos = 0;
        if (headerbuf[0] < HEADERSIZE) return DROP;
        bufpos = getPacket(&curpacket, EMPTY, HEADERSIZE);
        if (bufpos == 255) return DROP; // Paket nicht pufferbar!
        PACKETTYPE(curpacket) = type;
        for(c=0; c<HEADERSIZE; c++) {
                transbuf[bufpos+c] = headerbuf[c];
        }
        if (type == FORWARD) {
                PACKETSIZE(curpacket) = HEADERSIZE;
                USART_CAN_SEND_AN;
        } else {
                PACKETSIZE(curpacket) = headerbuf[0];
        }
        return BUFFER;
}

void processHeader(unsigned char curbyte) {
        unsigned char c = 0;
        unsigned char i = 0;
        for(c=0; c < (HEADERSIZE-1); c++) { headerbuf[c] = headerbuf[c+1]; }
        headerbuf[(HEADERSIZE-1)] = curbyte;
        if((((unsigned char)(
                     headerbuf[0] +
                     headerbuf[1] +
                     headerbuf[2] +
                     headerbuf[3] +
                     headerbuf[4])) ==
            headerbuf[5]) &&
           (headerbuf[0] != 0)) { // checksum richtig
#ifdef TRANSBUFSIZECHECK
                if (badheaderbytes > 5) CUR_UDR = 'D';
                badheaderbytes = 0;
#endif
                packetPos = HEADERSIZE;
                if((headerbuf[3] == ADDRESS_H) && //Paket ist an dieses Bauteil adressiert
                   (headerbuf[4] == ADDRESS_L)) {
                        action =
#if defined(NOINCOMINGPACKETS)
                                DROP;
#else
                        bufferNewPacket(READING);
#endif
                } else {
                        action = bufferNewPacket(FORWARD);
                }
        }
#ifdef TRANSBUFSIZECHECK
        else {
                if (badheaderbytes != 255) badheaderbytes++;
        }
#endif
}

void processStream(unsigned char curbyte) {
        unsigned char c;
        unsigned char bufpos=0;
        unsigned char curpacket = 0;
        packetPos++;
        if (action == BUFFER) {
                if ((bufpos = getPacket(&curpacket, ANY_IN, 0)) == 255) {
                        //sendFlashString(notfoundstr, c);
#ifdef TRANSBUFSIZECHECK
                        CUR_UDR = 'N';
                        CUR_UDR = 'F';
#endif
                        PACKETTYPE(curpacket) = GARBAGE;
                        action = DROP;
                } else {
                        if (PACKETTYPE(curpacket) == FORWARD) {
                                if ((bufpos + PACKETSIZE(curpacket) + 1) >= TRANSBUFSIZE) {
                                        action = DROP;
                                        PACKETTYPE(curpacket) = GARBAGE;
                                } else {
#ifdef TRANSBUFSIZECHECK
                                        if (((unsigned char)(bufpos+PACKETSIZE(curpacket))) >= TRANSBUFSIZE) CUR_UDR = 'I';
#endif
                                        transbuf[bufpos+PACKETSIZE(curpacket)] = curbyte;
                                        PACKETSIZE(curpacket)++;
                                        if (packetPos >= headerbuf[0]) PACKETTYPE(curpacket) = OUTPUT;
                                        USART_CAN_SEND_AN;
                                }
                        } else {
#ifdef TRANSBUFSIZECHECK
                                if (((unsigned char)(bufpos+(packetPos-1))) >= TRANSBUFSIZE) CUR_UDR = 'Q';
#endif
                                transbuf[bufpos+(packetPos-1)] = curbyte;
                                if (packetPos >= headerbuf[0]) PACKETTYPE(curpacket) = READY;
                        }
                }
        }
        if (packetPos >= headerbuf[0]){ //Paket komplett
                for (c = 0; c< HEADERSIZE; c++) { headerbuf[c] = 0; }
                packetPos = 0;
                action = HEADER;
        }
}

/*Interrupt Handler*/
ISR(
#if defined (ATMEGA644)
        USART0_UDRE_vect
#else
        USART_UDRE_vect
#endif
        ){ //USART Data Register Empty
        unsigned char bufpos, curpacket, tmp;
        if ((outpos == 0) && ((bufpos = getPacket(&curpacket, ANY_OUT, 0)) != 255)) {
                if (PACKETSIZE(curpacket) == 0) {
                        if (PACKETTYPE(curpacket) == FORWARD) {
                                USART_CAN_SEND_AUS;
                        } else {
                                PACKETTYPE(curpacket) = GARBAGE;
                        }
                } else {
                        CUR_UDR = transbuf[bufpos];
                        //sentbytes++;
                        removeTransbufData(bufpos, 1);
                        PACKETSIZE(curpacket)--;
                }
                return;
        }
        if (currentCommandLength != 0) {
                //tmp = onBufferFreeForUserspace();
                // Ändern in: return pgm_read_byte(&tosend[(HEADERSIZE+2)-currentCommandLength]);
                if      (outpos == 0) { tmp = currentCommandLength; }
                //else if ((outpos > 0) && (outpos < 5)) { tmp = pgm_read_byte(&header[outpos-1]);}
                else if (outpos == 1) { tmp = ADDRESS_H; }
                else if (outpos == 2) { tmp = ADDRESS_L; }
                else if ((outpos == 3) || (outpos == 4)) { tmp = 0xFF; }
                else if (outpos == 5) { tmp = (currentCommandLength+ADDRESS_H+ADDRESS_L+0xFF+0xFF); }
                else { tmp = outbuf[outpos-HEADERSIZE]; }
                CUR_UDR = tmp;
                if (++outpos >= currentCommandLength) {
                        currentCommandLength = 0;
                        outpos = 0;
                }
        } else {
                USART_CAN_SEND_AUS;
        }
}

#if !defined(NOINCOMINGPACKETS)
unsigned char* getTransbuf(unsigned char* curpacket, unsigned char type, unsigned char len) {
        unsigned char bufpos;
        if ((bufpos = getPacket(curpacket, type, len)) != 255) {
                return (unsigned char *) (&transbuf + bufpos);
        } else {
                return 0;
        }
}

void setPacketState(unsigned char packet, unsigned char state, unsigned char* transbuf) {
        if (state == GARBAGE) {
                sei();
                //UCSRB  = UCSRB  | 0b00100000; //USART Sende-Interrupt einschalten
                WAITFORSEND;
                outbuf[0] = SEND_ACK;
                outbuf[1] =
#if defined(ACKONBYTE6)
                        transbuf[6];
#else
                transbuf[7];
#endif
        }
        PACKETTYPE(packet) = state;
        if (state == GARBAGE) {
                sendPacket(2);
        }
}
#endif

void init(unsigned char speed, unsigned char* realoutbuf) {
        outbuf = realoutbuf;
/* unsigned char a;
   for (a = 0; a< HEADERSIZE; a++)   { headerbuf[a] = 0; }
   for (a = 0; a< QUESIZE; a++)      { packets[a] = 0; }
   for (a = 0; a< TRANSBUFSIZE; a++) { transbuf[a] = 0; } */
        InitUART(speed);
        action = HEADER;
}

ISR(
#if defined(ATMEGA644)
        USART0_RX_vect
#elif defined(ATMEGA8)
        USART_RXC_vect
#else
        USART_RX_vect
#endif
        ){ //Interrupt, wenn was über's RS232 reinkommt
        //if ((UCSR0A & 0b00001000) > 0) { CUR_UDR = 'E'; } <- hier hat ein Überlauf am Eingang stattgefunden
        //while ((UCSRA & 0b00001000) > 0) { //Data OverRun
        while ((
#if defined(ATMEGA644)
                       UCSR0A
#else
                       UCSRA
#endif
                       & 0b10000000) > 0) { //unread data in receive buffer
                if (action == HEADER) {
                        processHeader(CUR_UDR);
                } else {
                        processStream(CUR_UDR);
                }
        }
}
