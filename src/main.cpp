/**
 * 
 * Local date and time from GPS module
 * Started: 08.02.2025
 * Tauno Erik
 * Edited: 23.02.2025
 * 
 * PPS -
 * RXD -
 * TXD -
 * GND - GND
 * VCC - 3.3V
 * 
 * SDA - GPIO4 (ESP8266 - D2)
 * SCL - GPIO5 (ESP8266 - D1)
 * 
 * 
 */
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>    // https://github.com/mikalhart/TinyGPSPlus/tree/master/examples

#define PRINT_RAW_GPS   1
#define PRINT_DATE_TIME 0

#ifdef ESP8266
  // Shift Register 74HC595 pins
  static const int DATA_PIN  = D4;
  static const int LATCH_PIN = D3;
  static const int CLOCK_PIN = D2;

  // GPS module pins
  static const int  RX_PIN = D7;
  static const int  TX_PIN = D8;
#elif defined(ARDUINO_AVR_NANO)
  // GPS module pins
  static const int  RX_PIN = 5;
  static const int  TX_PIN = 4;
  // Shift Register 74HC595 pins
  static const int DATA_PIN  = 8;
  static const int LATCH_PIN = 9;
  static const int CLOCK_PIN = 10;
#endif


static const uint32_t GPSBaud = 9600;

// Time zone offset (in hours)
// Example: UTC+2 (Central European Time)
int time_zone_offset = 2;

TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial GPS_Serial(RX_PIN, TX_PIN);


// A struct to store date and time
struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

enum USER_COMMANDS
{
  RAW = 0,
  CLOCK = 1,
  OFFSET = 2,
};

// Lookup table for digits 0-9
// MSBFIRST
// 0 - ON, 1 - OFF
const uint8_t digits[10] = {
  0b00000011, // 0
  0b10011111, // 1
  0b00100101, // 2
  0b00001101, // 3
  0b10011001, // 4
  0b01001001, // 5
  0b11000001, // 6
  0b00011111, // 7
  0b00000001, // 8
  0b00001001  // 9
};

uint32_t numbers_data = 0;
uint32_t display_data = 0; // liidetud

uint8_t hour_minut_dot_pos = 16;
uint32_t dot_bitmask = 1 << hour_minut_dot_pos;

// 7-segment led bits
const uint8_t  a = 0b01111111;
const uint8_t  b = 0b10111111;
const uint8_t  c = 0b11011111;
const uint8_t  d = 0b11101111;
const uint8_t  e = 0b11110111;
const uint8_t  f = 0b11111011;
const uint8_t  g = 0b11111101;
const uint8_t dp = 0b11111110;

//
int overlay_delay = 100; // Pausi aeg millisekundites
unsigned long overlay_prev_millis = 0; // Eelmise mustri kuvamise aeg
int overlay_current_index = 0; // Praeguse mustri indeks

const uint32_t overlay_patterns[] = {
    0b11111111111111111111111101111111,
    0b11111111111111110111111111111111,
    0b11111111011111111111111111111111,
    0b01111111111111111111111111111111,
    0b11111011111111111111111111111111,
    0b11110111111111111111111111111111,
    0b11101111111111111111111111111111,
    0b11111111111011111111111111111111,
    0b11111111111111111110111111111111,
    0b11111111111111111111111111101111,
    0b11111111111111111111111111011111,
    0b11111111111111111111111110111111
};

const int num_patterns = sizeof(overlay_patterns) / sizeof(overlay_patterns[0]);

/**********************************************
 * Function prototypes
 **********************************************/
void print_raw_gps();
void print_date_time(const DateTime &dt);
bool update_date_time(DateTime &dt);
void local_date_time(DateTime &dt);
void write_to_display(uint32_t data);
void process_user_cmd(int cmd);
void run_gps(int print);
void print_serial_cmds();

void print_local_time();
void display_gps_info();


/*********************************************/
void setup() {
  Serial.begin(9600);
  GPS_Serial.begin(GPSBaud);

  // Initialize the shift register pins
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
}

void loop()
{
  DateTime UTC_time;
  DateTime local_time;

  unsigned long current_millis = millis();
  static unsigned long prev_millis = 0;
  static unsigned long prev_dot_millis = 0;

  static int user_cmd =  CLOCK; // User command to execute (serial print)

  static uint8_t h1 = 0; // for displaying hours and minutes
  static uint8_t h2 = 0;
  static uint8_t m1 = 0;
  static uint8_t m2 = 0;


  // Check if data is available on the Serial port
  if (Serial.available() > 0)
  {
    String cmd_in = Serial.readStringUntil('\n');

    cmd_in.trim(); // Remove any extra whitespace

    if (cmd_in.equalsIgnoreCase("RAW"))
    {
      user_cmd = RAW;
    }
    else if (cmd_in.equalsIgnoreCase("CLOCK"))
    {
      user_cmd = CLOCK;
    }
    else if(cmd_in.startsWith("OFFSET")) // Example: OFFSET-2
    {
      //char offset_sign = cmd_in.substring(6, 7).charAt(0); // Extract the 6th character
      //int offset_num = cmd_in.substring(7, 8).toInt(); // Extract the 8th character
      // Extract the offset value (e.g., "+2" or "-3")
      String offsetStr = cmd_in.substring(6); // Remove "OFFSET"
      int offset = offsetStr.toInt(); // Convert to integer
      time_zone_offset = offset; // Update the time zone offset

      user_cmd = OFFSET;
    }
    else
    {
      Serial.print("Unknown command: ");
      Serial.println(cmd_in);
      print_serial_cmds();
    }
  }


  // Select with data to serial print
  switch (user_cmd)
  {
    case RAW:
      run_gps(PRINT_RAW_GPS);
      break;
  
    case CLOCK:
      run_gps(PRINT_DATE_TIME);
      break;
    
    case OFFSET:
      // TODO:
      run_gps(PRINT_DATE_TIME);
      break;

    default:
      break;
  }

  //////////
  if (current_millis - overlay_prev_millis >= overlay_delay) {
    overlay_prev_millis = current_millis;

    // merge
    //display_data = numbers_data & overlay_patterns[overlay_current_index];
    //write_to_display(display_data);
    write_to_display(numbers_data);

    overlay_current_index++;
    if (overlay_current_index >= num_patterns) {
      overlay_current_index = 0;
    }
  }

  if (current_millis - prev_dot_millis >= 500)
  {
    prev_dot_millis = current_millis;
    //Serial.println("-----------------------------Toggle dot");
    numbers_data ^= dot_bitmask; // Toggle the dot

    //display_data = numbers_data & overlay_patterns[overlay_current_index];
    //write_to_display(display_data);
    write_to_display(numbers_data);
  }

  if (current_millis - prev_millis >= 1000)
  {
    prev_millis = current_millis;
     // Update the struct with the current GPS UTC date and time
    bool no_time = update_date_time(UTC_time);
    no_time = update_date_time(local_time); // still UTC time
    local_date_time(local_time);
    h1 = local_time.hour / 10;
    h2 = local_time.hour % 10;
    m1 = local_time.minute / 10;
    m2 = local_time.minute % 10;
    
    numbers_data = digits[h1] << 24 | digits[h2] << 16 | digits[m1] << 8 | digits[m2];
    //numbers_data ^= dot_bitmask; // Toggle the dot
    
    //display_data = overlay_patterns[overlay_current_index];
    write_to_display(numbers_data);


    if (user_cmd == CLOCK || user_cmd == OFFSET)
    {
      Serial.print("UTC Time: ");
      print_date_time(UTC_time);
      Serial.print("My Time:  ");
      print_date_time(local_time);
    }
  }


} // loop end



/*********************************************/

/**
 * Print a byte of data from GPS
 */
void print_raw_gps()
{
  while (GPS_Serial.available() > 0)
  {
    uint8_t gps_data = GPS_Serial.read();
    gps.encode(gps_data); // ? test ?
    Serial.write(gps_data);
  }
}

void run_gps(int print = 0)
{
  while (GPS_Serial.available())
  {
    uint8_t gps_data = GPS_Serial.read();
    gps.encode(gps_data);
    if (print == PRINT_RAW_GPS)
    {
      Serial.write(gps_data);
    }
    
  }
}

/**
 * 
 */
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





/**
 * 
 */
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
    int localHour = utc_hour + time_zone_offset;

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
    Serial.println("2 Waiting for valid GPS time...");
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
bool update_date_time(DateTime &dt) {
  if (gps.date.isValid() && gps.time.isValid())
  {
    dt.year = gps.date.year();
    dt.month = gps.date.month();
    dt.day = gps.date.day();
    dt.hour = gps.time.hour();
    dt.minute = gps.time.minute();
    dt.second = gps.time.second();
  } else {
    Serial.println("1 Waiting for valid GPS date and time");
    return false;
  }
  return true;
}


/**
 * Function to calculate local date and time
 */
void local_date_time(DateTime &dt) {
  // Calculate local time by applying the time zone offset
  dt.hour += time_zone_offset;

  // Handle overflow (e.g., if localHour >= 24)
  if (dt.hour >= 24) {
    dt.hour -= 24;
    dt.day += 1;  // Increment the day
  } else if (dt.hour < 0) {
    dt.hour += 24;
    dt.day -= 1;  // Decrement the day
  }

}


/**
 * Function to write data to the shift register
 */
void write_to_display(uint32_t data)
{
  uint8_t bit_order = MSBFIRST; // MSBFIRST; // Most significant bit first
  int size = 32; // 8 bits * 4 shift registers

  digitalWrite(LATCH_PIN, LOW);

  for (int i = 0; i < size; i++)
  {
    if (bit_order == LSBFIRST)
    {
      digitalWrite(DATA_PIN, data & 1); // Send the least significant bit
      data >>= 1; // Shift right to get the next bit
    }
    else // MSBFIRST
    {
      digitalWrite(DATA_PIN, (data & 0x80000000) ? HIGH : LOW); // Send the most significant bit
      data <<= 1; // Shift left to get the next bit
    }

    // Pulse the clock pin to shift the bit into the 74HC595
    digitalWrite(CLOCK_PIN, HIGH);
    digitalWrite(CLOCK_PIN, LOW);
  }

  digitalWrite(LATCH_PIN, HIGH);
}


/**
 * 
 */
void print_serial_cmds()
{
  Serial.println("Available commands:");
  Serial.println("\tRAW: Print raw GPS data");
  Serial.println("\tCLOCK: Print GPS date and time");
  Serial.println("\tOFFSET: Set the time zone offset (e.g., OFFSET+2)");
}

