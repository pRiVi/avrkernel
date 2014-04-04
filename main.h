// Beispiel einer Main.h eines Projektes

// Adresse
#define ADDRESS_H 003
#define ADDRESS_L 007

// Inputbuffer Allocation
#define HEADERSIZE   6
#define MAXPACKETS   4
#define QUESIZE      MAXPACKETS*2
#define TRANSBUFSIZE 70-QUESIZE-HEADERSIZE

//#define ATMEGA644 1
//#define ATMEGA8 1
//#define NOINCOMINGPACKETS 1
//#define ACKONBYTE6 1
