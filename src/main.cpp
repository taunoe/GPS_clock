/**
 * 
 * Local date and time from GPS module
 * Started: 08.02.2025
 * Edited: 02.03.2025
 * Tauno Erik
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
#include <EEPROM.h>

// A struct to store date and time
struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

DateTime UTC_time;    // Instance for the UTC time
DateTime local_time;  // Instance for the local time

// A Struct to store settings
struct Settings {
  int time_zone_offset;
  bool is_summer_time; // or summer_time and wintter_time
};

// Create an instance of the Settings struct
Settings settings;

// Default settings values
// Time zone offset (in hours)
// Example: UTC+2 (Central European Time)
const Settings default_settings = {
  .time_zone_offset = 2,    // Default time zone offset (UTC)
  .is_summer_time = false // Default daylight saving (disabled)
};

enum USER_COMMANDS
{
  RAW = 0,
  CLOCK = 1,
  OFFSET = 2,
  DAYLIGHT = 3,
};

#define PRINT_DATE_TIME 0
#define PRINT_RAW_GPS   1

#define DOT_TOGGLE_TIME    500
#define CLOCK_UPDATE_TIME 1000

// 115200 bps: The default baud rate for most ESP8266
// 230400 bps: A good compromise between speed and reliability
// 460800 bps: Suitable for high-speed communication with minimal errors
// 921600 bps: The highest commonly used baud rate for reliable communication
static const int BAUD_RATE = 115200;

// Shift Register 74HC595 pins
static const int DATA_PIN  = D4;
static const int LATCH_PIN = D3;
static const int CLOCK_PIN = D2;

// GPS module pins
static const int  RX_PIN = D7;
static const int  TX_PIN = D8;

static const uint32_t GPSBaud = 9600;


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

/*
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

const int num_overlay_patterns = sizeof(overlay_patterns) / sizeof(overlay_patterns[0]);
*/

TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial GPS_Serial(RX_PIN, TX_PIN);

/**********************************************
 * Function prototypes
 **********************************************/
void print_date_time(const DateTime &dt);
bool update_date_time(DateTime &dt);
void local_date_time(DateTime &dt);
void write_to_display(uint32_t data);
void run_gps(int print);
void print_serial_cmds();

void load_settings();
void save_settings();
void print_settings();

int get_user_serial_input();

/*********************************************/
void setup() {
  Serial.begin(BAUD_RATE);
  GPS_Serial.begin(GPSBaud);

  // Initialize the shift register pins
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  // Initialize EEPROM with 4096 bytes (max size for ESP8266)
  EEPROM.begin(4096);

  // Load settings from EEPROM
  load_settings();
  print_settings();
}

void loop()
{
  unsigned long current_millis = millis();
  static unsigned long prev_millis = 0;
  static unsigned long prev_dot_millis = 0;

  static int user_cmd =  CLOCK; // User command to execute

  static uint8_t h1 = 0; // for displaying hours and minutes
  static uint8_t h2 = 0;
  static uint8_t m1 = 0;
  static uint8_t m2 = 0;

  // Check if data is available on the Serial port
  if (Serial.available() > 0)
  {
    user_cmd = get_user_serial_input();
  }


  // Select with data to serial print
  switch (user_cmd)
  {
    case RAW:
      run_gps(PRINT_RAW_GPS);
      break;
  
    case CLOCK:
    case OFFSET:
    case DAYLIGHT:
      run_gps(PRINT_DATE_TIME);
      break;

    default:
      break;
  }

  //////////
  /*
  if (current_millis - overlay_prev_millis >= overlay_delay) {
    overlay_prev_millis = current_millis;

    // merge
    //display_data = numbers_data & overlay_patterns[overlay_current_index];
    //write_to_display(display_data);
    write_to_display(numbers_data);

    overlay_current_index++;
    if (overlay_current_index >= num_overlay_patterns) {
      overlay_current_index = 0;
    }
  }
    */

  // Time to toggle the dot
  if (current_millis - prev_dot_millis >= DOT_TOGGLE_TIME)
  {
    prev_dot_millis = current_millis;
    numbers_data ^= dot_bitmask; // Toggle the dot
    write_to_display(numbers_data);
  }

  // Time to update the Clock
  if (current_millis - prev_millis >= CLOCK_UPDATE_TIME)
  {
    prev_millis = current_millis;
    // Update the struct with the current GPS UTC date and time
    bool no_time = update_date_time(UTC_time);
    no_time = update_date_time(local_time); // still UTC time

    if (no_time)
    {
      Serial.println("1 Waiting for valid GPS date and time");
    }

    local_date_time(local_time);

    // 8-bit numbers to display on the 7-segment display
    h1 = local_time.hour / 10;
    h2 = local_time.hour % 10;
    m1 = local_time.minute / 10;
    m2 = local_time.minute % 10;
    
    // 32-bit number to display on the 7-segment display
    numbers_data = digits[h1] << 24 | digits[h2] << 16 | digits[m1] << 8 | digits[m2];
    
    write_to_display(numbers_data);

    if (user_cmd != RAW)
    {
      Serial.print("UTC Time: ");
      print_date_time(UTC_time);
      Serial.print("My Time:  ");
      print_date_time(local_time);
    }
  }

} // loop end



/*******************************************************************
 * Function to read data from the GPS module
 * @param print: 1 - Print the raw GPS data
 ******************************************************************/
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
 * Function to print the date and time stored in the struct
 * @param dt: DateTime struct with the date and time
 */
void print_date_time(const DateTime &dt)
{
  char buffer[20];  // Buffer to store the formatted string
  // Make sure the buffer is large enough to hold the final string

  sprintf(buffer, "%02d:%02d:%02d %02d/%02d/%02d",
          dt.hour, dt.minute, dt.second, dt.day, dt.month, dt.year);

  Serial.println(buffer);
}


/**
 * Function to update the struct with the current GPS date and time
 * @param dt: DateTime struct to store the date and time
 * @return true if the date and time are valid; otherwise, false
 */
bool update_date_time(DateTime &dt)
{
  if (gps.date.isValid() && gps.time.isValid())
  {
    dt.year = gps.date.year();
    dt.month = gps.date.month();
    dt.day = gps.date.day();
    dt.hour = gps.time.hour();
    dt.minute = gps.time.minute();
    dt.second = gps.time.second();
  }
  else
  {
    return false;
  }

  return true;
}


/**
 * Function to calculate local date and time
 * @param dt: DateTime struct with UTC date and time
 */
void local_date_time(DateTime &dt)
{
  // Calculate local time by applying the time zone offset
  dt.hour += settings.time_zone_offset;

  // TODU: Handle daylight saving time
  // Get the current month and day
  // Suveaeg on vööndiajast ühe tunni võrra edasi nihutatud kellaaeg.
  // Suveajale minnakse Euroopas märtsi viimasel pühapäeval 
  // kell 01:00 UTC (Eestis 3:00) ja tagasi vööndiajale minnakse oktoobri 
  // viimasel pühapäeval kell 01:00 UTC

  if (settings.is_summer_time)
  {
    // Add 1 hour for daylight saving time
    dt.hour += 1;
  }

  // Handle overflow (e.g., if localHour >= 24)
  if (dt.hour >= 24)
  {
    dt.hour -= 24;
    dt.day += 1;  // Increment the day
  }
  else if (dt.hour < 0)
  {
    dt.hour += 24;
    dt.day -= 1;  // Decrement the day
  }
}


/**
 * Function to write data to the shift register
 * @param data: 32-bit data to write to the shift register
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
 * Function to print the available serial commands
 */
void print_serial_cmds()
{
  Serial.println("Available commands:");
  Serial.println("\tRAW: Print raw GPS data");
  Serial.println("\tCLOCK: Print GPS date and time");
  Serial.println("\tOFFSET: Set the time zone offset (e.g., OFFSET+2)");
  Serial.println("\tDAYLIGHTON: Enable daylight saving");
  Serial.println("\tDAYLIGHTOFF: Disable daylight saving");
}


/**
 * Function to load settings from EEPROM
 */
void load_settings()
{
  int address = 0; // Address in EEPROM
  // Read the settings from EEPROM
  EEPROM.get(address, settings); 
  // Check if the settings are valid (e.g., using a magic number or checksum)
  if (settings.time_zone_offset < -12 || settings.time_zone_offset > 14)
  {
    // If settings are invalid, use default values
    Serial.println("Invalid settings. Loading defaults.");
    settings = default_settings;
    save_settings(); // Save the default settings to EEPROM
  }
}

/**
 * Function to save settings to EEPROM
 */
void save_settings()
{
  // Write the settings to EEPROM
  EEPROM.put(0, settings); // Write to address 0
  EEPROM.commit(); // Commit changes to Flash
}

/**
 * Print the loaded settings
 */
void print_settings()
{
  Serial.println("Loaded Settings:");
  Serial.print("Time Zone Offset: ");
  Serial.println(settings.time_zone_offset);
  Serial.print("Daylight Saving: ");
  Serial.println(settings.is_summer_time ? "Enabled" : "Disabled");
}


/*******************************************************************
 * Get user input from the Serial port
 * @return the user command
 *******************************************************************/
int get_user_serial_input()
{
  String cmd_in = Serial.readStringUntil('\n');

  cmd_in.trim(); // Remove any extra whitespace

  if (cmd_in.equalsIgnoreCase("RAW"))
  {
    return RAW;
  }
  else if (cmd_in.equalsIgnoreCase("CLOCK"))
  {
    return CLOCK;
  }
  else if(cmd_in.startsWith("OFFSET")) // Example: OFFSET-2
  {
    // Extract the offset value (e.g., "+2" or "-3")
    String offset_str = cmd_in.substring(6); // Remove "OFFSET"
    int offset = offset_str.toInt();         // Convert to integer
    settings.time_zone_offset = offset;      // Update the time zone offset
    save_settings();                         // Save the settings to EEPROM
    return OFFSET;
  }
  else if(cmd_in.startsWith("DAYLIGHT"))
  {
    // Extract the daylight saving value (e.g., "ON" or "OFF")
    String daylight_str = cmd_in.substring(8); // Remove "DAYLIGHT"
    bool daylight = daylight_str.equalsIgnoreCase("ON");
    settings.is_summer_time = daylight;    // Update the daylight saving setting
    save_settings();                           // Save the settings to EEPROM
    return DAYLIGHT;
  }
  else
  {
    Serial.print("Unknown command: ");
    Serial.println(cmd_in);
    print_serial_cmds();
  }

  return -1;
}
