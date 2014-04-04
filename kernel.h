// Outputcommands
#define IDLE            0
#define ACK             1
#define DUMP            2
#define SENDTEMPERATURE 3
#define SENDSTATE       4
#define SENDEVENT       5

// Inputbuffer State
#define EMPTY      0
#if !defined(NOINCOMINGPACKETS)
#define READING    1
#endif
#define READY      2
#define PROCESSING 3
#define GARBAGE    4
#define OUTPUT     5
#define FORWARD    6
#define ANY_OUT    254
#define ANY_IN     255


// Reading Interrupthandler States
#define HEADER 0
#define DROP   1
#define BUFFER 2

#define SENDSIZE 2

#define SEND_ACK 255

#if defined(ACKONBYTE6)
#define PAYLOADSTART 7
#else
#define PAYLOADSTART 6
#endif

#define PACKETTYPE(paketid) packets[((paketid)*2)]
#define PACKETSIZE(paketid) packets[((paketid)*2)+1]
