

#include <RF24Network.h>
#include <RF24.h>
#include <coap.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
byte mac[] = {0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00 }; //adres MAC arduino
// Struktura przechowujca dane obserwatora zasobu
struct Observer
{

  uint8_t observer_token[2]; //token zasobu
  uint8_t observer_tokenlen = 0; // dlugosc tokena
  IPAddress observer_clientip; // IP klienta obserwujacego zasob
  int observer_clientport = 0; // port klienta obserwujacego zasob
  String observer_url = ""; // nazwa obserwowanego zasobu
  uint8_t format = 0;
};

Observer observers[1]; // tablica obserwatorow; zaimplementowany max 1
int obscount = 0; // liczba aktualnych obserwatorow
EthernetUDP Udp; // Obiekt UDP
Coap coap(Udp); // Obiekt serwera CoAP
int buzzTone = 0;// aktualny ton buzzera
int prevBuzzTone = 0; // poprzedni ton buzzera, uzywany do sprawdzania czy ton zostal zmieniony
char pressedKey = '\0'; // ostatnio wcisniety klawisz klawiatury
char prevPressedKey = '\0'; // poprzednio wcisniety klawisz klawiatury uzywany do sprawdzania czy wcsnieto inny klawisz niz ostatnio
static unsigned long timestamp1 = 0;
static unsigned long timestamp2 = 0;
const char *wrongformat = "wrong_format";
char  *w = "</sound/buzzer>;rt=\"buzzer freq\",</input/keyboard>;rt=\"keyboard key pressed\",</metrics/rtt>;rt=\"round trip time\"";

// enumerator uzywany w komunikacji z node'm
typedef enum
{
  GET = 1,
  PUT = 2,
  GETResponse = 3,
  PUTResponse = 4,
} TYPE;
// // enumerator uzywany w komunikacji z node'm
typedef enum
{
  BUZZER = 1,
  KEYPAD = 2,
} DEVICE;
//struktura uzywana do komunikacji z node'm w celu zmiany lub pobrania stanu zasobu
struct request {
  TYPE type; // typ wiadomosci (GET,PUT)
  DEVICE device;// zadane urzadzenie (klawiatura, glosniczek)
  int buzzerTone;// czestotliwosc glosniczka w wypadku wiadomosci PUT(null dla wiadomosci innego typu)
};
//struktura uzywana do komunikacji z node'm w celu zmiany lub pobrania stanu zasobu
struct response {
  TYPE type; // typ wiadomosci (GETResponse,PUTResponse)
  DEVICE device;// zadane urzadzenie (klawiatura, glosniczek)
  int buzzerTone;// czestotliwosc glosniczka w wypadku wiadomosci GET(null dla wiadomosci innego typu)
  char keyPressed;// ostatnio wcisniety klawisz klawiatury
};


// funkcja  wysyajca zadanie do Arduino MINI
void sendRequest(TYPE typ, DEVICE dev, int buzT, char keyPres, RF24Network &net)
{
  struct response payload;
  payload.type = typ;
  payload.device = dev;
  payload.buzzerTone = buzT;
  payload.keyPressed = keyPres;
  RF24NetworkHeader header(0); // identyfikator adresata
  net.write(header, &payload, sizeof(payload));
}


RF24 radio(7, 8); // Obiekt komunikacji radiowej
RF24Network network(radio); // Obiekt sieci radiowej

// funkcja callback wywolywana gdy przyjdzie wiadomosc CoAP dotyczaca zasobu buzzer

void callback_buzz(CoapPacket &packet, IPAddress ip, int port)
{


  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;

  String message(p); // payload zadania

  if (packet.code == 1) // jesli GET
  {
    //  Serial.println("GET");
    int acceptNum = -1;
    //sprawdzanie czy jest opcja  ACCEPT
    for (int i = 0; i < packet.optionnum; i++) {

      if (packet.options[i].number == COAP_ACCEPT)
      {
        acceptNum = i;
        // Serial.println(*(packet.options[acceptNum].buffer), DEC);
      }
    }
    char char_array[9]; // payload odpowiedzi

    String x = String(buzzTone) + " Hz";
    x.toCharArray(char_array, 9);


    if (acceptNum == -1) { //nie ma opcji Accept
      coap.sendResponse(ip, port, rand(), char_array, strlen(char_array), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen );//odpowiedz

    }
    else {

      if (packet.options[acceptNum].length == 0) { //wysylanie w formacie text plain

        coap.sendResponse(ip, port, rand(), char_array, strlen(char_array), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen); // wyslanie odpowiedzi ze stanem zasobu

      } else {
        coap.sendResponse(ip, port, rand(), wrongformat, strlen(wrongformat), COAP_NOT_ACCEPTABLE, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);//nieobslugiwany format

      }


    }

  }
  if (packet.code == 3) // jesli PUT
  {

    buzzTone = atoi(p); // przypisanie payloadu do tonu buzzera



  }

}

// funkcja callback wywolywana gdy przyjdzie wiadomosc CoAP dotyczaca zasobu keyboard
void callback_keyboard(CoapPacket &packet, IPAddress ip, int port)
{
  String json = "{\"keyboard\":\"" + String(pressedKey) + "\"}"; //reprezentacja zasobu w formacie json
  int str_len = json.length() + 1;
  char char_array_json[str_len];


  json.toCharArray(char_array_json, str_len);
  char char_array_txt[] = {pressedKey, '\0'};//reprezentacja zasobu w formacie tekstowym


  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen); //ladowanie payloadu do bufora
  p[packet.payloadlen] = NULL;

  String message(p); // payload zadnia

  if (packet.code == 1) // jesli GET
  {

    int num = -1;
    int acceptNum = -1;
    //sprawdzanie czy jest opcja OBSERVE i ACCEPT
    for (int i = 0; i < packet.optionnum; i++) {
      if (packet.options[i].number == COAP_OBSERVE) {

        num = i; // numer opcji OBSERVE
      }
      if (packet.options[i].number == COAP_ACCEPT)
      {
        acceptNum = i; // numer opcji ACCEPT
      }
    }

    // jesli -1 to nie ma OBSERVE, wyslij zwykla odpowiedz ze ostatnio wcisnietym klawiszem klawiatury
    if (num == -1) {

      if (acceptNum == -1) {
        coap.sendResponse(ip, port, rand(), char_array_txt, strlen(char_array_txt), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen );

      }
      else {
        if (*(packet.options[acceptNum].buffer) == COAP_APPLICATION_JSON && packet.options[acceptNum].length > 0) // jesli jest opcja ACCEPT json
        {
          coap.sendResponse(ip, port, rand(), char_array_json, strlen(char_array_json), COAP_CONTENT, COAP_APPLICATION_JSON, packet.token, packet.tokenlen );//wyslij w formacie json

        } else if (packet.options[acceptNum].length == 0) { //jesli jest ACCEPT plain text

          coap.sendResponse(ip, port, rand(), char_array_txt, strlen(char_array_txt), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen); // wyslanie odpowiedzi ze stanem zasobu
        }
        else { // jesli inne wyslij odpowiedz "zly format"
          coap.sendResponse(ip, port, rand(), wrongformat, strlen(wrongformat), COAP_NOT_ACCEPTABLE, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);

        }
      }
    }
    else //jesli jest observe
    {

      if (*(packet.options[num].buffer) == 1) // jesli wartosc opcji OBSERVE = 1 to usun obserwatora
      {
        for (int i = 0; i < 5; i++)
        {
          if (observers[i].observer_clientip == ip) // jesli istnieje taki obserwator z takim IP
          {
            observers[obscount].observer_token[0] = 0 ;
            observers[obscount].observer_token[1] = 0 ; // zerowanie wszystkich pol struktury tego obserwatora
            observers[i].observer_tokenlen = 0;
            observers[i].observer_clientip = (0, 0, 0, 0);
            observers[i].observer_clientport = 0;
            observers[i].observer_url = "";


            break;
          }
        }
      }
      else // jesli OBSERVE != -1
      {
        int number = 0; // zmienna pomocnicza potrzebna do sprawdzenia czy ten klient juz istnieje w tablicy obserwatorow
        for (int i = 0; i < 10; i++)
        {
          if (observers[i].observer_clientip == ip)
          {
            number++;
          }
        }
        if (number == 0)// jesli 0 to nie istnieje ten klient w tablicy obserwatorow, dodaj go i wysij odpowiedz
        {


          observers[obscount].observer_token[0] = packet.token[0];
          observers[obscount].observer_token[1] = packet.token[1];


          // token klienta

          observers[obscount].observer_tokenlen = packet.tokenlen; // dlugosc tokenu klienta
          observers[obscount].observer_clientip = ip; // ip klienta
          observers[obscount].observer_clientport = port; // port klienta
          observers[obscount].observer_url = "keyboard"; // nazwa obserwowanego zasobu

          if (acceptNum == -1 ) {//jesli nie bylo opcji ACCEPT

            coap.sendNotification(ip, port, rand(), char_array_txt, strlen(char_array_txt), COAP_CONTENT, COAP_TEXT_PLAIN, observers[obscount].observer_token, packet.tokenlen); // wyslanie odpowiedzi ze stanem zasobu

          }
          else { // Jeslli byla
            if (*(packet.options[acceptNum].buffer) == COAP_APPLICATION_JSON && packet.options[acceptNum].length > 0) { //jesli JSON
              observers[obscount].format = 1;
              coap.sendNotification(ip, port, rand(), char_array_json, strlen(char_array_json), COAP_CONTENT, COAP_APPLICATION_JSON, packet.token, packet.tokenlen); // wyslanie odpowiedzi ze stanem zasobu

            } else if (packet.options[acceptNum].length == 0) { // jesli plain text
              observers[obscount].format = 0;
              coap.sendNotification(ip, port, rand(), char_array_txt, strlen(char_array_txt), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen); // wyslanie odpowiedzi ze stanem zasobu
              //   Serial.println("text/plain");
            }
            else {
              coap.sendResponse(ip, port, rand(), wrongformat, strlen(wrongformat), COAP_NOT_ACCEPTABLE, COAP_TEXT_PLAIN, packet.token, packet.tokenlen); // jesli inny format nie zapisuj observatora
              observers[obscount].observer_token[0] = 0 ;
              observers[obscount].observer_token[1] = 0 ; // zerowanie wszystkich pol struktury tego obserwatora
              observers[obscount].observer_tokenlen = 0;
              observers[obscount].observer_clientip = (0, 0, 0, 0);
              observers[obscount].observer_clientport = 0;
              observers[obscount].observer_url = "";
              observers[obscount].format = 0;

            }
          }
        }
      }

    }

  }

}


void notification() // funkcja wysylajaca powiadomienie o zmianie stanu zasobu do kazdego obserwatora
{
  String json = "{\"keyboard\":\"" + String(pressedKey) + "\"}"; // payload/reprezentacja zasobu w formacie Json
  int str_len = json.length() + 1;
  char char_array_json[str_len];


  json.toCharArray(char_array_json, str_len);
  char char_array[] = {pressedKey, '\0'}; // payload w formie tekstowej
  for (int i = 0; i < 5; i++)
  {
    if (observers[i].observer_url == "keyboard") // sprawdzenie czy obserwator obserwuje stan klawiatury
    {
      if (observers[i].format == 1)
      {
        //wyslanie powiadomienia w postaci JSON
        coap.sendNotification(observers[i].observer_clientip, observers[i].observer_clientport, rand(), char_array_json, strlen(char_array_json), COAP_CONTENT, COAP_APPLICATION_JSON, observers[i].observer_token, observers[i].observer_tokenlen); // wyslanie powiadomienia
      }
      else {
        //wyslanie powiadomienia w plain text
        coap.sendNotification(observers[i].observer_clientip, observers[i].observer_clientport, rand(), char_array, strlen(char_array), COAP_CONTENT, COAP_TEXT_PLAIN, observers[i].observer_token, observers[i].observer_tokenlen); // wyslanie powiadomienia
      }
    }
  }
}

// funkcja wysylajaca odpowiedz na zadanie odkrycia zasobow

void callback_wn(CoapPacket & packet, IPAddress ip, int port) {

  //CoRE Link Format



  int BlockNum = -1;
  int Size2num = -1;
  //sprawdzanie czy jest opcja block2 i size2
  for (int i = 0; i < packet.optionnum; i++) {
    if (packet.options[i].number == COAP_BLOCK_2) {

      BlockNum = i; // numer opcji BLOCK_2
    }
    if (packet.options[i].number == COAP_SIZE_2) {

      Size2num = i; // numer opcji SIZE_2
    }
  }
  

  
  if (packet.options[BlockNum].length > 0) {

    uint8_t NUM = packet.options[BlockNum].buffer[0] >> 4; // wartosc NUM pierwsze bity  bajtu opcji
    if (NUM == 0)
    {
      timestamp1 = millis(); // timestamp potrzebny do obliczenia RTT (pierwszy blok)
    }
    else if (NUM == 1)
    {
      timestamp2 = millis();// timestamp potrzebny do obliczenia RTT (drugi blok)

    }



   
    uint8_t  SZX = packet.options[BlockNum].buffer[0] & 7; // wartosc SZX (trzy ostatnie bity bajtu opcji)
    
    if (Size2num == -1) //jesli nie ma opcji Size2
      coap.sendBlockResponse(ip, port, rand(), w, strlen(w), COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen, SZX, NUM, 0);
    else
    {
      if (SZX < 8) // jesli 0<=SZX<=7
        coap.sendBlockResponse(ip, port, rand(), w, strlen(w), COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen, SZX, NUM, 1);
      else //jesli SZX > 7 wysylamy z SZX = 1
        coap.sendBlockResponse(ip, port, rand(), w, strlen(w), COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen, 1, NUM, 1);
    }
  }
  else // jeslii wartosc opcji jest pusta wysylamy z SZX = 0 
  {
    if (Size2num == -1) 
      coap.sendBlockResponse(ip, port, rand(), w, strlen(w), COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen, 0, 0, 0);
    else
      coap.sendBlockResponse(ip, port, rand(), w, strlen(w), COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen, 0, 0, 1);
  }


}
//funkcja wysylajaca odpowiedz na zadanie dotyczace zasobu metryk
void callback_metric(CoapPacket & packet, IPAddress ip, int port)
{
  String x = "Round trip time : " + String((int)(timestamp2 - timestamp1)) + " ms"; //reprezentacja zasobu
  int str_len = x.length() + 1;
  char char_array[str_len];


  x.toCharArray(char_array, str_len);
  if (packet.code == 1) // jesli GET
  {
    
    int acceptNum = -1;
    //sprawdzanie czy jest opcja  ACCEPT
    for (int i = 0; i < packet.optionnum; i++) {

      if (packet.options[i].number == COAP_ACCEPT)
      {
        acceptNum = i; //numer opcji ACCEPT
        
      }
    }

    if (acceptNum == -1) // jesli nie ma ACCEPT
    {
      coap.sendResponse(ip, port, rand(), char_array, strlen(char_array), COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen );
    } else {
        //Jesli jest to odpowiadamy wiadomoscia NOT_ACCEPTABLE

      coap.sendResponse(ip, port, rand(), wrongformat, strlen(wrongformat), COAP_NOT_ACCEPTABLE, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);
      

    }

  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // Serial
  SPI.begin(); //SPI
  radio.begin();
  network.begin(90, 1);
  Ethernet.begin(mac);


  
  for (byte thisByte = 0; thisByte < 4; thisByte++) { 
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  } // wyswietlenia uzyskanego z DHCP adresu IP

  coap.server(callback_buzz, "sound/buzzer"); // dodanie zasobu buzzera do obiektu serwera CoAP
  coap.server(callback_wn, ".well-known/core");// dodanie zasobu .well-known/core do obiektu serwera CoAP
  coap.server(callback_keyboard, "input/keyboard");// dodanie zasobu klawiatury do obiektu serwera CoAP
  coap.server(callback_metric, "metrics/rtt"); // dodanie zasobu klawiatury do obiektu serwera CoAP
  coap.start(); // wystartowanie serwera CoAP

}

void loop() {
  // put your main code here, to run repeatedly:
  network.update();

  while (network.available()) { // jesli node wyslal wiadomosc przez radio
    struct response res; // odpowiedz
    RF24NetworkHeader header; // naglowek wiadomosci
    network.read(header, &res, sizeof(res)); // parsuj wiadomosc
    pressedKey = res.keyPressed; // ustawienie ostatnio wcisnietego klawisza
  };
  if (buzzTone != prevBuzzTone) // jesli klient CoAP ustawil wyslal wiadomosc PUT  z nowym stanem glosnika
  {

    sendRequest(PUT, BUZZER, buzzTone, '\0', network); // wyslanie zadania do wezla by ustawil zadana wartosc
    //  sendRequest(GET, BUZZER, 0, '\0', network); // zadanie wyslania potwierdzenia ze zostala ustawiona wartosc
    prevBuzzTone = buzzTone;
  }
  if (pressedKey != prevPressedKey) // sprawdzenie czy wcisnieto nowyklawisz
  {
    notification();// jesli tak to wyslij powiadmoienie obserwatorom
    prevPressedKey = pressedKey;
  }

  coap.loop(); // parsuj nowe zadnie od klienta CoAP
  //delay(500);
}
