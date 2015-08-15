//  This Arduino Sketch controls a kegerator by turning on and off the compressor using a relay
//  It also measures the beer left in 2 kegs by weighing the keg

//  Include the libraries below
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <Statistic.h>
#include "HX711.h"

//This is for the 2 scales one hx711 amplifier is used for each
#define calibration_factor1 -26980 //This value is obtained using the SparkFun_HX711_Calibration sketch
#define calibration_factor2 -24190 //This value is obtained using the SparkFun_HX711_Calibration sketch
#define DOUT1  A2
#define CLK1  A1
#define DOUT2  A3
#define CLK2 A4
HX711 scale1(DOUT1, CLK1);
HX711 scale2(DOUT2, CLK2);

// initialize the lcd library with the numbers of the interface pins
LiquidCrystal lcd(8, 7, 12, 11, 10, 9);

// Data wire for one wire sensors is on pin 4
#define ONE_WIRE_BUS 4
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
// Assign the addresses of your 1-Wire temp sensors.
// See the tutorial on how to obtain these addresses:
// http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html
DeviceAddress Temp1 = { 0x28, 0xA8, 0x4B, 0x22, 0x05, 0x00, 0x00, 0x38 };

//Define encoder pins
#define encoderPinA  2
#define encoderPinB  3

//define relay for control on analog pin 0 which will be used as digital
#define relayOut  A0

//setup button for turning on display backlight
//this is also used to reset minmax temps and averaging
//this is also used to tare the scales when held down long enough
#define displaySwitch 5
#define displayOnOff 6

Statistic myStats;//setup statistics library for average temp calculation

//define variables
int onCount;//used as a timer for the backlight
double tempSP = 33; // control point for relay
float maxTemp = -100; //stores max
float minTemp = 100; //stores min
int resetCount = 0; //used to reset min and max temps when the rotary button is held down
int compressorTimer = 0;//Compressor Min on/off time
int onTimer = 0;//counts how long the compressor has been running
int offTimer = 0;//counts how long the compressor has been off
float tempF;//temp in F
float tempC;//temp in C
int screenSwitch = 0;//Controls which screen is displayed at which time
//this is also controlled by the rotary encoder button
long time;//stores current time for sensor retrieval delay
int timeBetweenReadings = 5000;//time between temperature readings
int lastScreen;//store which scree was last displayed and deetermines if it should refresh
int lightFirst = 0; //keeps the screen from switching when the button is first pressed
float keg1;//weight of keg 1 which is scale 1
float keg2;//weight of keg 2 which is scale 2
float beerLeft1;//stores how many pints are left which is 1.04 pounds per 16 ounce pint for keg 1
float beerLeft2;//stores how many pints are left which is 1.04 pounds per 16 ounce pint for keg 2
float keg1Last;//stores the last weight measurement and is used to switch to the beerleft screen when pouring
float keg2Last;//stores the last weight measurement and is used to switch to the beerleft screen when pouring
int tareSelect;//selects the keg to tare using the rotary encoder
int tareCount;//enables taring of the scales when the rotary encoder button is held down long enough
int tareHold;//once a scale is selected this counts how long the button is held down and tares the scale
float keg1Tare = 7.5;//known tare weight of the keg1
float keg2Tare = 7;//known tare weight of the keg2



void setup(void)
{
	// Start up the sensor library
	sensors.begin();
	// set the sensor resolution to 10 bit or whatever you want 10 is enough
	sensors.setResolution(Temp1, 10);
	//Start LCD
	lcd.begin(16, 2);
	//Setup Button pins
	pinMode(displaySwitch, INPUT);
	pinMode(displayOnOff, OUTPUT);
	//setup encoder
	pinMode(encoderPinA, INPUT_PULLUP);
	pinMode(encoderPinB, INPUT_PULLUP);
	//Setup interupt for changing setpoint this is dettached when taring the kegs
	attachInterrupt(0, doEncoder, CHANGE);  // encoder pin on interrupt 0 - pin 2
	// Setup Relay pin
	pinMode(relayOut, OUTPUT);
	//clear stats to ensure it starts clean
	myStats.clear();
	//set calibration of the scales and tare scales to start at 0
	scale1.set_scale(calibration_factor1); //This value is obtained by using the SparkFun_HX711_Calibration sketch
	scale2.set_scale(calibration_factor2); //This value is obtained by using the SparkFun_HX711_Calibration sketch
	scale1.tare();
	scale2.tare();

}

void doEncoder()
{
	/* If pinA and pinB are both high or both low, it is spinning
	 * forward. If they're different, it's going backward.
	 * For more information on speeding up this process, see
	 * [Reference/PortManipulation], specifically the PIND register.
	 */
	if (digitalRead(encoderPinA) == digitalRead(encoderPinB))
	{
		tempSP = tempSP + 0.5;
	}
	else
	{
		tempSP = tempSP - 0.5;
	}
	digitalWrite(displayOnOff, HIGH);
	lcd.clear();
	lcd.print("SetPoint:");
	lcd.print(tempSP, 0);
	lcd.print(" F");
	onCount = 200;
}

void displayTemperature()
{
	lcd.clear();
	lcd.print("SetPoint=");
	lcd.print(tempSP, 0); //Display Setpoint
	lcd.print(" F");
	lcd.setCursor(0, 1); //Start at character 0 on line 1
	lcd.print("Act.Temp=");
	lcd.print(tempF, 2); //Display temperature reading with 2 decimal places
	lcd.print(" F");
}

void displayAverage()
{
	lcd.clear();
	lcd.print("Avg.Temp=");
	lcd.print(myStats.average(), 2); //Display avg temp
	lcd.print(" F");
	lcd.setCursor(0, 1); //Start at character 0 on line 1
	lcd.print("Act.Temp=");
	lcd.print(tempF, 2); //Display actual temp
	lcd.print(" F");
}

void displayMinMaxTemperature()
{
	lcd.clear();
	lcd.print("Min Temp=");
	lcd.print(minTemp, 2); //Display min temp
	lcd.print(" F");
	lcd.setCursor(0, 1); //Start at character 0 on line 1
	lcd.print("Max Temp=");
	lcd.print(maxTemp, 2); //Display max temp
	lcd.print(" F");
}

void displayBeerLeft()
{
	lcd.clear();
	lcd.print("Keg1=");
	lcd.print(beerLeft1, 1);
	lcd.print(" Pints");
	lcd.setCursor(0, 1); //Start at character 0 on line 1
	lcd.print("Keg2=");
	lcd.print(beerLeft2, 1);
	lcd.print(" Pints");
}

void loop(void)
{
	//if millis() has been reset this will correct the error
	if (time > millis())
	{
		time = millis();
	}

	//only read sensor if 5sec has passed
	if(millis() > time + timeBetweenReadings)
	{
		//get the temp from the sensor
		sensors.requestTemperatures();
		tempC = sensors.getTempC(Temp1);
		tempF = sensors.getTempF(Temp1);

		//test for sensor errors
		if (tempC == -127.00) // Measurement failed or no device found
		{
			lcd.clear();
			lcd.print("Temperature Error");
		}
		else
		{
			//calculate min and max
			if (tempF > maxTemp)
			{
				if(tempF < 100)
				{
					maxTemp = tempF;
				}
			}
			if (tempF < minTemp)
			{
				if(tempF > -50)
				{
					minTemp = tempF;
				}
			}
			//Control the Compressor relay
			if (tempF - 4 > tempSP && compressorTimer == 0)
			{
				if (onTimer < 100)
				{
					compressorTimer = 150;
				}
				else
				{
					compressorTimer = 5;
				}
			}
			if (tempF <= tempSP && compressorTimer == 0)
			{
				if (offTimer < 100)
				{
					compressorTimer = -150;
				}
				else
				{
					compressorTimer = -5;
				}
			}
			if (compressorTimer > 0)
			{
				digitalWrite(relayOut, HIGH);
				compressorTimer = compressorTimer - 1;
				onTimer++;
				offTimer = 0;
			}
			if (compressorTimer < 0)
			{
				digitalWrite(relayOut, LOW);
				compressorTimer = compressorTimer + 1;
				onTimer = 0;
				offTimer++;
			}
			//Now add reading to the average or reset average if a day has passed
			myStats.add(tempF);
			if (myStats.count() == 17280)
			{
				myStats.clear();
			}
		}
		time = millis();// set time to current so that delay works
		lastScreen = 99; // set last screen to random rumber so dispaly will refresh
	}




	//Now read the Scales and set display to beerleft if it changes
	keg1 = scale1.get_units(2) - keg1Tare;
	keg2 = scale2.get_units(2) - keg2Tare;

	//convert pounds to 16oz pints
	beerLeft1 = keg1 / 1.04;
	beerLeft2 = keg2 / 1.04;



	//Now turn on the backlight if the button is pressesd
	//Now rest min and max temps if button is held down
	//Now Tare Scales if button is held even longer
	if (digitalRead(displaySwitch) == HIGH)
	{
		onCount = 200; // This determines time that it will stay on
		// Remember delay also affects the time it is on
		resetCount++;
		if (resetCount == 40)
		{
			tareCount = 200;
		}
		if(screenSwitch < 4 && lightFirst == 1)
		{
			screenSwitch++;
		}
		if(screenSwitch == 4 && lightFirst == 1)
		{
			screenSwitch = 0;
		}
		lightFirst = 1;
	}
	else
	{
		resetCount = 0;
		if(onCount > 0)
		{
			onCount = onCount - 1;
		}
	}
	if(onCount > 0)
	{
		digitalWrite(displayOnOff, HIGH);
	}
	else
	{
		digitalWrite(displayOnOff, LOW);
		lightFirst = 0;
	}
	if(resetCount > 20)
	{
		maxTemp = -100;
		minTemp = 100;
		myStats.clear();
		keg1Last = 5000;
		keg2Last = 5000;
	}
	//Choose which screen to display
	//First Check for errors
	if(resetCount > 1 && resetCount < 20)
	{
		lcd.clear();
		lcd.print("Hold to Reset");
	}
	if(resetCount >= 20 && resetCount < 40)
	{
		lcd.clear();
		lcd.print("Reset Complete");
		lcd.setCursor(0, 1); //Start at character 0 on line 1
		lcd.print("Hold to Tare");
	}
	if(tareCount > 0)
	{
		while(tareCount > 1)
		{
			detachInterrupt(0);
			if (digitalRead(encoderPinA) == digitalRead(encoderPinB))
			{
				tareSelect = 1;
			}
			else
			{
				tareSelect = 0;
			}

			lcd.clear();
			lcd.print("Select Keg");
			lcd.setCursor(0, 1); //Start at character 0 on line 1
			if(tareSelect == 1)
			{
				lcd.print("Keg1");
			}
			else
			{
				lcd.print("Keg2");
			}
			if (digitalRead(displaySwitch) == HIGH)
			{
				tareHold++;
			}
			else
			{
				tareHold = 0;
			}
			if (tareSelect == 1 && tareHold > 20)
			{
				scale1.tare();
				lcd.clear();
				lcd.print("Keg 1 Torn!");
				delay(5000);
			}
			tareCount --;
			if (tareSelect == 0 && tareHold > 20)
			{
				scale2.tare();
				lcd.clear();
				lcd.print("Keg 2 Torn!");
				delay(5000);
			}
			delay(200);
		}
		attachInterrupt(0, doEncoder, CHANGE);  // encoder pin on interrupt 0 - pin 2
	}

	//Now select the screen to be displayed if nothing above is using the screen
	if(lastScreen != screenSwitch && resetCount < 2)
	{
		if (tempC == -127.00) // Measurement failed or no device found
		{
			lcd.clear();
			lcd.print("Temperature Error");
		}
		else
		{
			if(screenSwitch == 0)
			{
				displayTemperature();
			}
			if(screenSwitch == 1)
			{
				displayAverage();
			}
			if(screenSwitch == 2)
			{
				displayMinMaxTemperature();
			}
			if(screenSwitch == 3)
			{
				displayBeerLeft();
			}
		}
		lastScreen = screenSwitch;
	}
	
}