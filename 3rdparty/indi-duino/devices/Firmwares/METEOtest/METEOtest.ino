/* Induino METEOSTATION test firmware.
Usefull to test if all the hardware work as
expected prior to go further
NACHO MAS 2013 

This firmware is independent of indiduino
drivers. Just load on your board and look 
at serial monitor.
*/

#include "i2cmaster.h"
#include <Wire.h>
#include "Adafruit_BMP085.h"

#include "dht.h"

#define DHTPIN 2     // what pin we're connected to
//#define DHTTYPE DHT22   // DHT 22  (AM2302)

//DHT dht(DHTPIN, DHTTYPE);
dht DHT;
Adafruit_BMP085 bmp;

float P,HR,IR,Tp,Thr,Tir;

void setupMeteoStation(){
	//i2c_init(); //Initialise the i2c bus
        //dht.begin();
        if (!bmp.begin()) {
	      Serial.println("Could not find a valid BMP085 sensor, check wiring!");
	      while (1) {}
        }
}

void runMeteoStation() {
    double tempData = 0x0000; // zero out the data
    
    int dev = 0x5A<<1;
    int data_low = 0;
    int data_high = 0;
    int pec = 0;
    
    i2c_start_wait(dev+I2C_WRITE);
    i2c_write(0x07);
    
    // read
    i2c_rep_start(dev+I2C_READ);
    data_low = i2c_readAck(); //Read 1 byte and then send ack
    data_high = i2c_readAck(); //Read 1 byte and then send ack
    pec = i2c_readNak();
    i2c_stop();
    
    //This converts high and low bytes together and processes temperature, MSB is a error bit and is ignored for temps

    int frac; // data past the decimal point
    
    // This masks off the error bit of the high byte, then moves it left 8 bits and adds the low byte.
    tempData = (double)(((data_high & 0x007F) << 8) + data_low);
    tempData = (tempData /50);
    
    IR = tempData - 273.15;

    //Serial.print("Fahrenheit: ");
    //Serial.println(fahrenheit);

    i2c_start_wait(dev+I2C_WRITE);
    i2c_write(0x06);
    
    // read
    i2c_rep_start(dev+I2C_READ);
    data_low = i2c_readAck(); //Read 1 byte and then send ack
    data_high = i2c_readAck(); //Read 1 byte and then send ack
    pec = i2c_readNak();
    i2c_stop();
  
    
    // This masks off the error bit of the high byte, then moves it left 8 bits and adds the low byte.
    tempData = (double)(((data_high & 0x007F) << 8) + data_low);
    tempData = (tempData /50);
    
    Tir = tempData - 273.15;
    int chk = DHT.read22(DHTPIN);
  switch (chk)
  {
    case DHTLIB_OK:  
                //Serial.print("OK,\t"); 
                break;
    case DHTLIB_ERROR_CHECKSUM: 
                Serial.print("Checksum error,\t"); 
                break;
    case DHTLIB_ERROR_TIMEOUT: 
                Serial.print("Time out error,\t"); 
                break;
    default: 
                Serial.print("Unknown error,\t"); 
                break;
  }
   HR=DHT.humidity;  
   Thr=DHT.temperature;  
//    HR = dht.readHumidity();
//    Thr = dht.readTemperature();
    Tp=bmp.readTemperature();
    P=bmp.readPressure();
}

// dewPoint function NOAA
// reference: http://wahiduddin.net/calc/density_algorithms.htm 
double dewPoint(double celsius, double humidity)
{
        double A0= 373.15/(273.15 + celsius);
        double SUM = -7.90298 * (A0-1);
        SUM += 5.02808 * log10(A0);
        SUM += -1.3816e-7 * (pow(10, (11.344*(1-1/A0)))-1) ;
        SUM += 8.1328e-3 * (pow(10,(-3.49149*(A0-1)))-1) ;
        SUM += log10(1013.246);
        double VP = pow(10, SUM-3) * humidity;
        double T = log(VP/0.61078);   // temp var
        return (241.88 * T) / (17.558-T);
}

// delta max = 0.6544 wrt dewPoint()
// 5x faster than dewPoint()
// reference: http://en.wikipedia.org/wiki/Dew_point
double dewPointFast(double celsius, double humidity)
{
        double a = 17.271;
        double b = 237.7;
        double temp = (a * celsius) / (b + celsius) + log(humidity/100);
        double Td = (b * temp) / (a - temp);
        return Td;
}

void setup(){
	Serial.begin(9600);
	Serial.println("Setup...");
	setupMeteoStation();
}

void loop(){

    runMeteoStation();
    Serial.print("T:");
    Serial.print(Thr);
    Serial.print(" IR:");
    Serial.print(IR);
    Serial.print(" P:");
    Serial.print(P);
    Serial.print(" HR:");
    Serial.print(HR);
    Serial.print(" DEW:");
    Serial.print(dewPoint(Thr,HR));
    Serial.print(" Tir:");
    Serial.print(Tir);
    Serial.print(" Tp:");
    Serial.println(Tp);
    delay(1000); // wait a second before printing again
}
