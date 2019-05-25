


#include <Keypad.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

// enumerator typu wiadomosci
typedef enum
{
  GET=1,
  PUT=2,
  GETResponse=3,
  PUTResponse=4,
}TYPE;
//enumerator typu urzadzenia 
typedef enum
{
  BUZZER=1,
  KEYPAD=2,
}DEVICE;
// matryca klawiatury
char keys[4][3] = {
 {'*','0','#'},
 {'7','8','9'},
 {'4','5','6'},
 {'1','2','3'}
};
byte rowPins[4] = {5, 4, 3, 2}; // row pinouts of the keypad
byte colPins[3] = {A2, A1, 6}; //column pinouts of the keypad

Keypad keypad=Keypad(makeKeymap(keys), rowPins, colPins, 4, 3); //obekt klawiaury

RF24 radio(7, 8); // radio

RF24Network network(radio); // siec radiowa
//unsigned long x=0;

int buzzTone=0; // aktualny ton buzzera
char pressedKey='\0'; // ostatnio wcisniety klawisz

struct request {
 TYPE type; // typ wiadomosci (GET,PUT)
 DEVICE device;// zadane urzadzenie (klawiatura, glosniczek)
 int buzzerTone;// czestotliwosc glosniczka w wypadku wiadomosci PUT(null dla wiadomosci innego typu)
};

struct response{
  TYPE type; // typ wiadomosci (GETResponse,PUTResponse)
  DEVICE device;// zadane urzadzenie (klawiatura, glosniczek)
 int buzzerTone;// czestotliwosc glosniczka w wypadku wiadomosci GET(null dla wiadomosci innego typu)
 char keyPressed;// ostatnio wcisniety klawisz klawiatury
};


// funkcja wysylajaca odpowiedz na zadanie Arduino UNO
void sendResponse(TYPE typ, DEVICE dev, int buzT, char keyPres,RF24Network &net)
{
  struct response payload;
        payload.type = typ;
        payload.device = dev;
        payload.buzzerTone=buzT;
        payload.keyPressed = keyPres;
        RF24NetworkHeader header(1); // identyfikator adresata
        net.write(header, &payload, sizeof(payload));
        Serial.println("sent packet");
}


void setup() {
  // put your setup code here, to run once:
  SPI.begin();
radio.begin();
network.begin(90, 0);
Serial.begin(115200);
pinMode(9,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  network.update();
  
  char key = keypad.getKey(); // wcisniety klawisz
    
    if (key != NO_KEY){
        Serial.print("key pressed : ");
        Serial.println(key);
        pressedKey=key;
        sendResponse(PUT,KEYPAD,'\0',pressedKey,network);
        
  }
    while(network.available()){
  
        RF24NetworkHeader header;
        struct request req; 
        network.read(header, &req, sizeof(req)); // parsowanie zadania
        Serial.print("received request ");
        Serial.print(req.type,DEC);
        Serial.print(" ");
        Serial.print(req.device,DEC);
        Serial.print(" ");
        Serial.println(req.buzzerTone,DEC);
        if(req.type==GET) //jelsi GET
          {
            if(req.device==BUZZER)
            {
              sendResponse(GETResponse,BUZZER,buzzTone,'\0',network); // aktualny ton buzzera
            }
            else if(req.device==KEYPAD)
              {
                 sendResponse(GETResponse,KEYPAD,'\0',pressedKey,network); // wcisniety klawisz klawiatry
              }
             
          }
          else if(req.type==PUT) // jesli PUT
          {
            if(req.device==BUZZER)
            {
              buzzTone=req.buzzerTone; // przypisz buzzerowi nowy ton podany w zadaniu
              if(buzzTone!=0)
              tone(9,buzzTone); // "graj" ton
              else
              noTone(9); // wycisz buzzer
            }
          }
        }
 };
