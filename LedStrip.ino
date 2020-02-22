#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>
#include <MemoryFree.h>

#ifdef __AVR__
#include <avr/power.h>
#endif

#define BAUDRATE 115200
#define PACKETMAXSIZE 92
#define LEDCOUNT 29
#define PIN 7
#define FIRSTBYTE 0
#define LASTBYTE 255
#define RXPIN 7
#define TXPIN 8
#define DEBUG 1

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDCOUNT, PIN, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.


SoftwareSerial mySerial(RXPIN, TXPIN); // RX, TX
bool reading = false;
byte index = 0;

byte size = 0;
byte currentPacket[PACKETMAXSIZE];

void readPacket(void(*f)(byte*));
void callback(byte * data);


void setup() {
	// This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
	if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
	// End of trinket special code
	if(DEBUG)
		mySerial.begin(BAUDRATE);
	Serial.begin(BAUDRATE);
	strip.begin();
	strip.show(); // Initialize all pixels to 'off'

	clearBuffer();
	Serial.flush();
}

void loop() {
	// Lecture du packet
	readPacket(callback);
}

void clearBuffer() {
	while (Serial.available())
		Serial.read();
}

void readPacket(void(*f)(byte*)) {
	// Some example procedures showing how to display to the pixels:
	byte current = 0;

	while (Serial.available())
	{
		if (Serial.available() > 0)
			if (!reading)
			{
				Serial.flush();
			}
	
		current = Serial.read();  //gets one byte from serial buffer

		if (!reading)
		{
			if (current == FIRSTBYTE)
			{
				// On initialise la trame si le premier octet est à 0
				reading = true;
				//currentPacket = 0;
				index++;
			}
			else
			{
				// Le premier octet n'est pas à 1
				// On annule la lecture et on envoie une erreur.
				reading = false;
				if (DEBUG) {
					mySerial.print(current, DEC);
					mySerial.println(" -> erreur, premier octet pas a 0");
				}
				Serial.flush();
				index = 0;
				size = 0;

				sendAck(B11111111);
			}
		}
		else
		{
			if (index == 1)
			{
				// Lecture de la size de la trame
				size = current;

				if (DEBUG) {
					mySerial.println();
					mySerial.print("Taille de la trame: ");
					mySerial.print(size);
					mySerial.println(" octets.");
					//mySerial.println(freeMemory());
				}
				index++;
			}
			else if (index < size + 2)
			{
				// Lecture de la payload
				currentPacket[index - 2] = current;
				index++;
			}
			else
			{
				if (current == LASTBYTE)
				{
					// Lecture d'une trame terminee

					// Appel du callback
					(*f)(currentPacket);

					reading = false;
					Serial.flush();
					index = 0;
					size = 0;

					sendAck(B00000000);
				}
				else
				{
					// Le dernier octet n'est pas à 1
					// On annule la lecture et on envoie une erreur.
					reading = false;
					if(DEBUG)
						mySerial.println("Erreur, dernier octet pas a 1");

					sendAck(B11111111);

					Serial.flush();
					index = 0;
					size = 0;
				}
			}
		}
	}
}

// Fonction appelée à la fin d'une lecture de trame complète.
void callback(byte * data) 
{
	// Afficher le contenu de la trame
	if (DEBUG) {
		for (int i = 0; i < size; i++)
		{
			mySerial.print(data[i], DEC);
			mySerial.print(" ");
		}
		mySerial.println("-> Trame OK (callback)");
	}

	// Interpreteur
	switch (data[0])
	{
	case 0:
		break;
	case 1:
		break;
	case 2:
		setLed(data[1], data[2], data[3], data[4]);
		break;
	case 3:
		setRangeLed(data[1], data[2], data[3], data[4], data[5]);
		break;
	case 4:
		setAllLeds(data[1], data[2], data[3]);
		break;
	case 5:
		updateLeds(data);
		break;
	default:
		sendAck(2);
		break;
	}
}

void updateLeds(byte * data) {
	for (int i = 1; i < LEDCOUNT + 1; i++) {
		strip.setPixelColor(i, strip.Color(data[3 * i - 2], data[3 * i - 1], data[3 * i]));
		strip.show();
	}
	
}

void setLed(byte pixel, byte r, byte g, byte b)
{
	strip.setPixelColor(pixel, strip.Color(r, g, b));
	strip.show();
}

void setRangeLed(byte start, byte end, byte r, byte g, byte b)
{
	for (int i = start; i <= end; i++)
	{
		strip.setPixelColor(i, strip.Color(r, g, b));
	}
	strip.show();
}

void setAllLeds(byte r, byte g, byte b)
{
	for (int i = 0; i <= LEDCOUNT; i++)
	{
		strip.setPixelColor(i, strip.Color(r, g, b));
	}
	strip.show();
}

// Fonction envoyant un packet
void packetSender(byte * data) {
	Serial.write("a");
}

void sendAck(byte ack) {
	Serial.write(ack);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
	for (uint16_t i = 0; i < strip.numPixels(); i++) {
		strip.setPixelColor(i, c);
		strip.show();
		delay(wait);
	}
}

void rainbow(uint8_t wait) {
	uint16_t i, j;

	for (j = 0; j < 256; j++) {
		for (i = 0; i < strip.numPixels(); i++) {
			strip.setPixelColor(i, Wheel((i + j) & 255));
		}
		strip.show();
		delay(wait);
	}
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
	uint16_t i, j;

	for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
		for (i = 0; i < strip.numPixels(); i++) {
			strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
		}
		strip.show();
		delay(wait);
	}
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
	for (int j = 0; j < 10; j++) {  //do 10 cycles of chasing
		for (int q = 0; q < 3; q++) {
			for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
				strip.setPixelColor(i + q, c);    //turn every third pixel on
			}
			strip.show();

			delay(wait);

			for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
				strip.setPixelColor(i + q, 0);        //turn every third pixel off
			}
		}
	}
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
	for (int j = 0; j < 256; j++) {     // cycle all 256 colors in the wheel
		for (int q = 0; q < 3; q++) {
			for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
				strip.setPixelColor(i + q, Wheel((i + j) % 255));    //turn every third pixel on
			}
			strip.show();

			delay(wait);

			for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
				strip.setPixelColor(i + q, 0);        //turn every third pixel off
			}
		}
	}
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
	WheelPos = 255 - WheelPos;
	if (WheelPos < 85) {
		return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
	}
	if (WheelPos < 170) {
		WheelPos -= 85;
		return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
	}
	WheelPos -= 170;
	return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}