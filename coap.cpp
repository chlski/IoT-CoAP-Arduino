#include "coap.h"
#include "Arduino.h"

#define LOGGING

static uint8_t obsstate=10;
Coap::Coap(
    UDP& udp
) {
    this->_udp = &udp;
}

bool Coap::start() {
    this->start(COAP_DEFAULT_PORT);
    return true;
}

bool Coap::start(int port) {
    this->_udp->begin(port);
    return true;
}

uint16_t Coap::sendPacket(CoapPacket &packet, IPAddress ip) {
    return this->sendPacket(packet, ip, COAP_DEFAULT_PORT);
}

uint16_t Coap::sendPacket(CoapPacket &packet, IPAddress ip, int port) {
    uint8_t buffer[BUF_MAX_SIZE];
    uint8_t *p = buffer; //wskaznik na poczatek bufora
    uint16_t running_delta = 0; //zmienna pozwalajaca liczenie option delta - numer poprzedniej opcji
    uint16_t packetSize = 0; //wielkosc pakietu

    // tworzenie base headera
    *p = 0x01 << 6; //numer wersji wiadomosci CoAP
    *p |= (packet.type & 0x03) << 4; //typ wiadomosci CON lub NON
    *p++ |= (packet.tokenlen & 0x0F); //dlugosc tokena
    *p++ = packet.code;	// kod - 1 bajt
    *p++ = (packet.messageid >> 8); // 8 pierwszych bitow Message ID
    *p++ = (packet.messageid & 0xFF); // 8 kolejnych bitow MID
    p = buffer + COAP_HEADER_SIZE;
    packetSize += 4; //zwiekszenie wielkosci pakietu o base header

    //	tworzenie tokena
    if (packet.token != NULL && packet.tokenlen <= 0x0F) {
        memcpy(p, packet.token, packet.tokenlen); //ladowanie headera i tokena do bufora
        p += packet.tokenlen;
        packetSize += packet.tokenlen; //zwiekszenie wielkosci pakietu o token
    }

    // tworzenie option header
    for (int i = 0; i < packet.optionnum; i++)  {
        uint32_t optdelta;
        uint8_t len, delta;

        if (packetSize + 5 + packet.options[i].length >= BUF_MAX_SIZE) {
            return 0;
        }
        optdelta = packet.options[i].number - running_delta;
        COAP_OPTION_DELTA(optdelta, &delta);
        COAP_OPTION_DELTA((uint32_t)packet.options[i].length, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13) {
            *p++ = (optdelta - 13);
            packetSize++;	//kolejny bajt opcji
        } else if (delta == 14) {
            *p++ = ((optdelta - 269) >> 8);
            *p++ = (0xFF & (optdelta - 269));
            packetSize+=2;	 //kolejne dwa bajty opcji
        } if (len == 13) {
            *p++ = (packet.options[i].length - 13);
            packetSize++;
        } else if (len == 14) {
            *p++ = (packet.options[i].length >> 8);
            *p++ = (0xFF & (packet.options[i].length - 269));
            packetSize+=2;
        }

        memcpy(p, packet.options[i].buffer, packet.options[i].length); //ladowanie opcji do bufora
        p += packet.options[i].length;
        packetSize += packet.options[i].length + 1; //zwiekszenie wielkosci pakietu o opcje
        running_delta = packet.options[i].number;
    }

       
    // tworzenie payloada
    if (packet.payloadlen > 0) {
        if ((packetSize + 1 + packet.payloadlen)  >= BUF_MAX_SIZE) { //jesli caly pakiet jest wiekszy od maksymalnej wielkosci bufora wtedy odrzucamy
            return 0;
        }
       
        

        
        *p++ = 0xFF; //payload marker
        memcpy(p, packet.payload, packet.payloadlen); //ladowanie payloada do bufora
        packetSize += 1 + packet.payloadlen; //zwieksz rozmiar pakietu o dlugosc payloada i payload marker
    }


    _udp->beginPacket(ip, port); //rozpoczecie wysylania na danym ip i porcie
    _udp->write(buffer, packetSize); //wysylanie zawartosci bufora
    _udp->endPacket(); //zakonczenie wysylania

    return packet.messageid; // zwroc MID
}

//uint16_t Coap::get(IPAddress ip, int port, char *url) {
  //  return this->send(ip, port, url, COAP_CON, COAP_GET, NULL, 0, NULL, 0);
//}

//uint16_t Coap::put(IPAddress ip, int port, char *url, char *payload) {
  //  return this->send(ip, port, url, COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)payload, strlen(payload));
//}

//uint16_t Coap::put(IPAddress ip, int port, char *url, char *payload, int payloadlen) {
  //  return this->send(ip, port, url, COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)payload, payloadlen);


/*uint16_t Coap::send(IPAddress ip, int port, char *url, COAP_TYPE type, COAP_METHOD method, uint8_t *token, uint8_t tokenlen, uint8_t *payload, uint32_t payloadlen) {

    // make packet
    CoapPacket packet;

    packet.type = type;
    packet.code = method;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = rand();

    // use URI_HOST UIR_PATH
    String ipaddress = String(ip[0]) + String(".") + String(ip[1]) + String(".") + String(ip[2]) + String(".") + String(ip[3]);
    packet.options[packet.optionnum].buffer = (uint8_t *)ipaddress.c_str();
    packet.options[packet.optionnum].length = ipaddress.length();
    packet.options[packet.optionnum].number = COAP_URI_HOST;
    packet.optionnum++;

    // parse url
    int idx = 0;
    for (int i = 0; i < strlen(url); i++) {
        if (url[i] == '/') {
            packet.options[packet.optionnum].buffer = (uint8_t *)(url + idx);
            packet.options[packet.optionnum].length = i - idx;
            packet.options[packet.optionnum].number = COAP_URI_PATH;
            packet.optionnum++;
            idx = i + 1;
        }
    }

    if (idx <= strlen(url)) {
        packet.options[packet.optionnum].buffer = (uint8_t *)(url + idx);
        packet.options[packet.optionnum].length = strlen(url) - idx;
        packet.options[packet.optionnum].number = COAP_URI_PATH;
        packet.optionnum++;
    }

    // send packet
    return this->sendPacket(packet, ip, port);
}*/

int Coap::parseOption(CoapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen) {
    uint8_t *p = *buf; //wskaznik na poczatek opcji
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen) return -1; //jesli nie ma opcji

    delta = (p[0] & 0xF0) >> 4; //option delta, 4 pierwsze bity
    len = p[0] & 0x0F; //option length, kolejne 4 bity

    if (delta == 13) {
        headlen++; //jest kolejny bajt opcji
        if (buflen < headlen) return -1;
        delta = p[1] + 13; //wartosc delty
        p++;
    } else if (delta == 14) {
        headlen += 2; //kolejne 2 bajty opcji
        if (buflen < headlen) return -1;
        delta = ((p[1] << 8) | p[2]) + 269; //sklejamy dwa kolejne bajty
        p+=2;
    } else if (delta == 15) return -1;

    if (len == 13) {
        headlen++;
        if (buflen < headlen) return -1;
        len = p[1] + 13;
        p++;
    } else if (len == 14) {
        headlen += 2;
        if (buflen < headlen) return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    } else if (len == 15) //15 zarezerwowane dla payload marker
        return -1;

    if ((p + 1 + len) > (*buf + buflen))  return -1;
    option->number = delta + *running_delta; //iteracyjnie przekazywana wartosc running delta
    option->buffer = p+1;
    option->length = len; //dlugosc opcji
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

bool Coap::loop() {

    uint8_t buffer[BUF_MAX_SIZE]; // bufor pakietu
    int32_t packetlen = _udp->parsePacket(); // dlugosc pakietu

    while (packetlen > 0) {
        packetlen = _udp->read(buffer, packetlen >= BUF_MAX_SIZE ? BUF_MAX_SIZE : packetlen); //  obcinamy wczytany pakiet do wielkosci bufora

        CoapPacket packet; // tworzymy pakiet

        // parse coap packet header
        if (packetlen < COAP_HEADER_SIZE || (((buffer[0] & 0xC0) >> 6) != 1)) { // wersja rozna od jeden lub pakiet krotszy od dl naglowka
            packetlen = _udp->parsePacket(); // bierzemy nastepny pakiet
            continue;
        }

        packet.type = (buffer[0] & 0x30) >> 4; // typ (CON,NON)
        packet.tokenlen = buffer[0] & 0x0F; // okreslenie dl tokena
        packet.code = buffer[1]; // kod (cale 8 bitow)
        packet.messageid = 0xFF00 & (buffer[2] << 8); // pierwsze 8 bitow MID
        packet.messageid |= 0x00FF & buffer[3]; // druie 8 bitow MID

        if (packet.tokenlen == 0)  packet.token = NULL;
        else if (packet.tokenlen <= 8)  packet.token = buffer + 4; // jesli token mniejszy od 8 bajtow ,przypisz do packet.token
        else {
            packetlen = _udp->parsePacket(); // jesli nie to wczytaj nowy pakiet udp bo poprzedni jest nieprawidlowy
            continue;
        }

        // parse packet options/payload
        if (COAP_HEADER_SIZE + packet.tokenlen < packetlen) { //jesli jest cos jeszzcze  opricz naglowka i tokena
            int optionIndex = 0;
            uint16_t delta = 0;
            uint8_t *end = buffer + packetlen; // wskaznik na koniec pakietu
            uint8_t *p = buffer + COAP_HEADER_SIZE + packet.tokenlen; // wskaznik na pierwszy bajt opcji
            while(optionIndex < MAX_OPTION_NUM && *p != 0xFF && p < end) { // jesli wskaznik p jest rozny od payload marker i nie jest koncem pakietu
                packet.options[optionIndex];
                if (0 != parseOption(&packet.options[optionIndex], &delta, &p, end-p))
                    return false;
                optionIndex++;
            }
            packet.optionnum = optionIndex; // ilosc opcji

            if (p+1 < end && *p == 0xFF) { // jest cos jeszcze ?? (paylaod)
                packet.payload = p+1; // ustawiamy wskaznik na payload
                packet.payloadlen = end-(p+1); // odejmujemy  koniec od aktualnej pozycji wskaznika i otrzymujemty dlugosc payloadu w bajtach
            } else {
                packet.payload = NULL;
                packet.payloadlen= 0;
            }
        }

 /*       if (packet.type == COAP_ACK) {
            // call response function
            resp(packet, _udp->remoteIP(), _udp->remotePort());

        } */

            String url = "";
            // call endpoint url function
            for (int i = 0; i < packet.optionnum; i++) {
                if (packet.options[i].number == COAP_URI_PATH && packet.options[i].length > 0) { // sprawdzamy opcje URI_PATH i zczytujemy z niej sciezke do zasobu
                    char urlname[packet.options[i].length + 1];
                    memcpy(urlname, packet.options[i].buffer, packet.options[i].length);
                    urlname[packet.options[i].length] = NULL;
                    if(url.length() > 0)
                      url += "/";
                    url += urlname; // odczytalismy sciezke do zasobu
                }
            }


            if (!uri.find(url)) {
                sendResponse(_udp->remoteIP(), _udp->remotePort(), rand(), "nie ma takiego zasobu", strlen("nie ma takiego zasobu"),
                        COAP_NOT_FOUNT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen); // jesli nie ma takiego zasobu, NOT_FOUND wsyslamy
            } else {
                uri.find(url)(packet, _udp->remoteIP(), _udp->remotePort()); // wywolujemy funkcje callback dla danego zasobu i przekazujemy adres i port klienta
            }
        

        
        // next packet
        packetlen = _udp->parsePacket();
    }

    return true;
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageid) {
    this->sendResponse(ip, port, messageid, NULL, 0, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageid, char *payload) {
    this->sendResponse(ip, port, messageid, payload, strlen(payload), COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}
uint16_t Coap::sendResponseWellKnown(IPAddress ip, int port, uint16_t messageid, char *payload) {
    this->sendResponse(ip, port, messageid, payload, strlen(payload), COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, NULL, 0);
}
uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageid, char *payload, int payloadlen) {
    this->sendResponse(ip, port, messageid, payload, payloadlen, COAP_CONTENT, COAP_TEXT_PLAIN, NULL, 0);
}


uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageid, char *payload, int payloadlen,
                COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, uint8_t *token, int tokenlen) {
    // tworzenie pakietu
    CoapPacket packet;

    packet.type = COAP_NONCON;
    packet.code = code;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = (uint8_t *)payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = messageid;

    // czy jest wiecej opcji?
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)type & 0x00FF) ;
    packet.options[packet.optionnum].buffer = (uint8_t *)optionBuffer;
    packet.options[packet.optionnum].length = 2;
    packet.options[packet.optionnum].number = COAP_CONTENT_FORMAT;
    packet.optionnum++;

    return this->sendPacket(packet, ip, port);
}

uint16_t Coap::sendNotification(IPAddress ip, int port, uint16_t messageid, char *payload, int payloadlen, COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, uint8_t *token, int tokenlen){

			CoapPacket packet;

    packet.type = COAP_NONCON;
    packet.code = code;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = (uint8_t *)payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = messageid;

			obsstate=obsstate+1;

			packet.options[packet.optionnum].buffer =&obsstate;
			packet.options[packet.optionnum].length = 1;
			packet.options[packet.optionnum].number = COAP_OBSERVE;
			packet.optionnum++;


			char optionBuffer2[2];
			optionBuffer2[0] = ((uint16_t)type & 0xFF00) >> 8;
			optionBuffer2[1] = ((uint16_t)type & 0x00FF) ;
			packet.options[packet.optionnum].buffer = (uint8_t *)optionBuffer2;
			packet.options[packet.optionnum].length = 2;
			packet.options[packet.optionnum].number = COAP_CONTENT_FORMAT;
			packet.optionnum++;
	return this->sendPacket(packet, ip, port);

}


uint16_t Coap::sendBlockResponse(IPAddress ip, int port, uint16_t messageid, char *payload, int payloadlen,
                COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE type, uint8_t *token, int tokenlen, uint8_t szx, uint8_t num,int size2) {

		uint8_t NUM =num;
		uint8_t M=1;
		uint8_t SZX= szx;
		uint8_t maxlen = (uint8_t) payloadlen;
	if((NUM + 1) * (int)(pow(2,(SZX+4))+1) >= payloadlen) // czy aktualny block jest ostatni ?
	{
	 M=0; // jesli tak to nie ma wiecej blokow
	}
	
    CoapPacket packet;
	
    packet.type = COAP_NONCON;
    packet.code = code;
    packet.token = token;
    packet.tokenlen = tokenlen;
	for(int j=(0);j<(int)pow(2,SZX+4)+1;j++){
	if(NUM*((int)pow(2,SZX+4)+1)+j>=maxlen)// jezeli ostatni blok to przerwij przypisywanie 
    		break;// przypisywanie do payloadu pakietu w zaleznosci od numeru bloku1
	else 
	packet.payload[j]= payload[NUM*((int)pow(2,SZX+4)+1)+j];
	 
	
	}
	if(M==1)
    	packet.payloadlen = (int)pow(2,SZX+4)+1; // jezeli jest wiecej blokow to dlugosc payloadu jest rowna dlugosci bloku
	else
	packet.payloadlen = payloadlen - (NUM*(int)pow(2,SZX+4)+1); // jesli nie to obliczamy dlugosc ostatniego bloku
    packet.optionnum = 0;
    packet.messageid = messageid;
		
	

    // dodajemy opcje content format
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8; // tworzymy 16 bitowa wartosc opcji przekazana w argumencie funkcji
    optionBuffer[1] = ((uint16_t)type & 0x00FF) ;
    packet.options[packet.optionnum].buffer = (uint8_t *)optionBuffer;
    packet.options[packet.optionnum].length = 2;
    packet.options[packet.optionnum].number = COAP_CONTENT_FORMAT;
    packet.optionnum++;
		// dodajemy opcje block 2
		char optionBuffer1[1];
		optionBuffer1[0]=SZX | M <<3 | NUM << 4; // nr bloku/czy zostalo wiecej?/ wielkosc bloku
		
		packet.options[packet.optionnum].buffer = (uint8_t *)optionBuffer1;
			packet.options[packet.optionnum].length = 1;
			packet.options[packet.optionnum].number = COAP_BLOCK_2;
			packet.optionnum++;
	// dodajemy fuzkcje size2 jesli zostala przekazana w argumencie wartos 1 czyli, klient wymaga tej opcji
	if(size2==1)
	{		
			packet.options[packet.optionnum].buffer =&maxlen;
			packet.options[packet.optionnum].length = 1;
			packet.options[packet.optionnum].number = COAP_SIZE_2;
			packet.optionnum++;
	}

    return this->sendPacket(packet, ip, port); // wysylamy pakiet
}


