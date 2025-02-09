/**
 * 
 * Local date and time from GPS module
 * Started: 08.02.2025
 * Tauno Erik
 * Edited: 09.02.2025
 * 
 * PPS -
 * RXD -
 * TXD -
 * GND - GND
 * VCC - 3.3V
 * 
 * 
 */
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>  // https://github.com/mikalhart/TinyGPSPlus/tree/master/examples

int cmd =  0;

static const int  RX_PIN  = D7;
static const int  TX_PIN = D8;
static const uint32_t GPSBaud = 9600;
// Time zone offset (in hours)
const int TIME_ZONE_OFFSET = 2;  // Example: UTC+2 (Central European Time)

// The TinyGPS++ object
TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial GPS_Serial(RX_PIN, TX_PIN);

String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

// A struct to store date and time
struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

/**********************************************
 * Function prototypes
 **********************************************/
void print_raw_gps();
void print_date_time(const DateTime &dt);
void update_date_time(DateTime &dt);
void local_date_time(DateTime &dt);

void print_local_time();
void display_gps_info();
static void smartDelay(unsigned long ms);
static void printFloat(float val, bool valid, int len, int prec);
static void printInt(unsigned long val, bool valid, int len);
static void printDateTime(TinyGPSDate &d, TinyGPSTime &t);


/*********************************************/
void setup() {
  Serial.begin(9600);
  GPS_Serial.begin(GPSBaud);


}

void loop() {
  // An instance of the DateTime struct
  DateTime currentDateTime;

  // Check if data is available on the Serial port
  if (Serial.available() > 0)
  {
    // Read the incoming command
    String command = Serial.readStringUntil('\n'); // Read until newline
    command.trim(); // Remove any extra whitespace
    //Serial.print("IN: ");
    //Serial.println(command);
    if (command.equalsIgnoreCase("RAW"))
    {
      cmd = 0;
    } 
    else if (command.equalsIgnoreCase("CLOCK"))
    {
      cmd = 1;
    }
    else
    {
      Serial.println("Unknown command: " + command);
    }
  }

  switch (cmd)
  {
    case 0:
      print_raw_gps();
      break;
  
    case 1:
      smartDelay(1000);

      // Update the struct with the current GPS UTC date and time
      update_date_time(currentDateTime);

      // Print the UTC date and time stored in the struct
      Serial.print("UTC Time: ");
      print_date_time(currentDateTime);

      // Calculate local time
      local_date_time(currentDateTime);
      Serial.print("My Time: ");
      print_date_time(currentDateTime);
      break;

    default:
      break;
  }

} // loop end



/*********************************************/

void print_raw_gps()
{
  while (GPS_Serial.available() > 0){
    // get the byte data from the GPS
    byte gpsData = GPS_Serial.read();
    Serial.write(gpsData);
  }
}

void display_gps_info()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    // Check if there is data available from the GPS module
    while (GPS_Serial.available())
    {
      // Pass the data to the TinyGPS++ object
      gps.encode(GPS_Serial.read());
    }
  } while (millis() - start < ms);
}


static void printFloat(float val, bool valid, int len, int prec)
{
  if (!valid)
  {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len)
{
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}


static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
  if (!d.isValid())
  {
    Serial.print(F("********** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }
  
  if (!t.isValid())
  {
    Serial.print(F("******** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}




// Function to calculate local time
void print_local_time() {
  if (gps.time.isValid() && gps.date.isValid())
  {
    // Get UTC time from GPS
    int utc_hour = gps.time.hour();
    int utc_minute = gps.time.minute();
    int utc_second = gps.time.second();
    int day = gps.date.day();
    int month = gps.date.month();
    int year = gps.date.year();

    // Calculate local time by applying the time zone offset
    int localHour = utc_hour + TIME_ZONE_OFFSET;

    // Handle overflow (e.g., if localHour >= 24)
    if (localHour >= 24) {
      localHour -= 24;
      day += 1;  // Increment the day
    } else if (localHour < 0) {
      localHour += 24;
      day -= 1;  // Decrement the day
    }

    // Print local time
    Serial.print("Local Time: ");
    Serial.print(year);
    Serial.print("-");
    Serial.print(month);
    Serial.print("-");
    Serial.print(day);
    Serial.print(" ");
    Serial.print(localHour);
    Serial.print(":");
    Serial.print(utc_minute);
    Serial.print(":");
    Serial.println(utc_second);
  } else {
    Serial.println("Waiting for valid GPS time...");
  }
}

/**
 * Function to print the date and time stored in the struct
 */
void print_date_time(const DateTime &dt) {
  char buffer[20];  // Buffer to store the formatted string
  // Make sure the buffer is large enough to hold the final string

  sprintf(buffer, "%02d:%02d:%02d %02d/%02d/%02d",
          dt.hour, dt.minute, dt.second, dt.day, dt.month, dt.year);
      
  Serial.println(buffer);
}

/**
 * Function to update the struct with the current GPS date and time
 */
void update_date_time(DateTime &dt) {
  if (gps.date.isValid() && gps.time.isValid())
  {
    dt.year = gps.date.year();
    dt.month = gps.date.month();
    dt.day = gps.date.day();
    dt.hour = gps.time.hour();
    dt.minute = gps.time.minute();
    dt.second = gps.time.second();
  } else {
    Serial.println("Waiting for valid GPS date and time...");
  }
}

/**
 * Function to calculate local date and time
 */
void local_date_time(DateTime &dt) {
  // Calculate local time by applying the time zone offset
  dt.hour += TIME_ZONE_OFFSET;

  // Handle overflow (e.g., if localHour >= 24)
  if (dt.hour >= 24) {
    dt.hour -= 24;
    dt.day += 1;  // Increment the day
  } else if (dt.hour < 0) {
    dt.hour += 24;
    dt.day -= 1;  // Decrement the day
  }

}