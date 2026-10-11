#include "configuration.h"
#include <ArduinoJson.h>
#include <FS.h>

SettingsStruct settings;

// Prototypes of some private helper functions.
// These functions will copy a value out of a JSON dictionary, or fail silently.
void cmpFromJSON(JsonObject* json, const char* key, char* destination, size_t maxLength);
void intFromJSON(JsonObject* json, const char* key, int8_t* destination);
void intFromJSON(JsonObject* json, const char* key, uint8_t* destination);
void intFromJSON(JsonObject* json, const char* key, uint16_t* destination);
void RGBFromJSON(JsonObject* json, const char* key, CRGB* destination);


void loadConfigurationFile(SettingsStruct& destination) {
  SettingsStruct settingsFromFile;
  
  if (!SPIFFS.exists(CONFIG_FILE_PATH)) {
    saveConfigurationFile(settingsFromFile);
    return;
  } 

  //file exists, reading and loading
  Serial.println("reading config file");
  File configFile = SPIFFS.open(CONFIG_FILE_PATH, "r");
  if (configFile) {
    Serial.println("opened config file");
    size_t size = configFile.size();
    // ArduinoJson 5 reads until a NUL. The file itself has none.
    std::unique_ptr<char[]> buf(new char[size + 1]);

    configFile.readBytes(buf.get(), size);
    buf[size] = '\0';
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());
    json.printTo(Serial);
    if (json.success()) {
        // http_username
        intFromJSON(&json, "numLeds",       &settingsFromFile.numLeds);
        cmpFromJSON(&json, "hostname",      settingsFromFile.hostname, sizeof(settings.hostname));
        cmpFromJSON(&json, "http_username", settingsFromFile.http_username, sizeof(settings.http_username));
        cmpFromJSON(&json, "http_password", settingsFromFile.http_password, sizeof(settings.http_password));       
        cmpFromJSON(&json, "mqtt_server",   settingsFromFile.mqtt_server,   sizeof(settings.mqtt_server));        
        cmpFromJSON(&json, "mqtt_username", settingsFromFile.mqtt_username, sizeof(settings.mqtt_username));        
        cmpFromJSON(&json, "mqtt_password", settingsFromFile.mqtt_password, sizeof(settings.mqtt_password));        
        cmpFromJSON(&json, "mqtt_topic",    settingsFromFile.mqtt_topic,    sizeof(settings.mqtt_topic));        
        intFromJSON(&json, "mode",          (uint8_t*)&settingsFromFile.mode);
        intFromJSON(&json, "last_mode",     (uint8_t*)&settingsFromFile.last_mode);
        intFromJSON(&json, "fps",           &settingsFromFile.fps);
        intFromJSON(&json, "overall_brightness",    &settingsFromFile.overall_brightness);
        intFromJSON(&json, "show_length",           &settingsFromFile.show_length);
        intFromJSON(&json, "ftb_speed",             &settingsFromFile.ftb_speed);
        intFromJSON(&json, "glitter_density",       &settingsFromFile.glitter_density);
        intFromJSON(&json, "glitter_on",            (uint8_t*)&settingsFromFile.glitter_on);
        intFromJSON(&json, "glitter_wipe_on",       (uint8_t*)&settingsFromFile.glitter_wipe_on);
        RGBFromJSON(&json, "main_color",            &settingsFromFile.main_color); 
        RGBFromJSON(&json, "glitter_color",         &settingsFromFile.glitter_color); 
        intFromJSON(&json, "effect_brightness",     &settingsFromFile.effect_brightness);
        intFromJSON(&json, "confetti_dens",         &settingsFromFile.confetti_dens);
        intFromJSON(&json, "palette_ndx",           &settingsFromFile.palette_ndx);
        intFromJSON(&json, "fixed_region_enabled",  (uint8_t*)&settingsFromFile.fixed_region_enabled);
        intFromJSON(&json, "fixed_region_start",    &settingsFromFile.fixed_region_start);
        intFromJSON(&json, "fixed_region_length",   &settingsFromFile.fixed_region_length);
        destination = settingsFromFile;
    } else {
      Serial.println("failed to load json config");
    }
  }

}

void loadConfigurationDefaults() {
  // Create a clean copy of the setting struct and save it to the working area.
  SettingsStruct defaults;
  settings = defaults;
}

void renderConfigurationAsJson(JsonObject& json, SettingsStruct& settingsToRender) {
  json["numLeds"]       = settingsToRender.numLeds;  
  json["hostname"]      = settingsToRender.hostname;
  json["http_username"] = settingsToRender.http_username;
  json["http_password"] = settingsToRender.http_password;
  json["mqtt_server"]   = settingsToRender.mqtt_server;
  json["mqtt_username"] = settingsToRender.mqtt_username;
  json["mqtt_password"] = settingsToRender.mqtt_password;
  json["mqtt_topic"]    = settingsToRender.mqtt_topic;
  json["mode"]          = (int)settingsToRender.mode;
  json["last_mode"]     = (int)settingsToRender.last_mode;
  json["fps"]           = settingsToRender.fps;
  json["overall_brightness"] = settingsToRender.overall_brightness;
  json["show_length"]        = settingsToRender.show_length;
  json["ftb_speed"]          = settingsToRender.ftb_speed;
  json["glitter_density"]    = settingsToRender.glitter_density;
  json["glitter_on"]         = settingsToRender.glitter_on;
  json["glitter_wipe_on"]    = settingsToRender.glitter_wipe_on;
  json.createNestedArray("main_color").copyFrom(settingsToRender.main_color.raw);
  json.createNestedArray("glitter_color").copyFrom(settingsToRender.glitter_color.raw);
  json["effect_brightness"]  = settingsToRender.effect_brightness;
  json["confetti_dens"]      = settingsToRender.confetti_dens;
  json["palette_ndx"]        = settingsToRender.palette_ndx;
  json["fixed_region_enabled"] = settingsToRender.fixed_region_enabled;
  json["fixed_region_start"]  = settingsToRender.fixed_region_start;
  json["fixed_region_length"] = settingsToRender.fixed_region_length;
}

void printConfiguration(SettingsStruct& settingsToPrint) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  renderConfigurationAsJson(json, settingsToPrint);
  json.printTo(Serial);
  Serial.print("\n");
}

void saveConfigurationFile(SettingsStruct& settingsToSave) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  renderConfigurationAsJson(json, settingsToSave);
  
  File configFile = SPIFFS.open(CONFIG_FILE_PATH, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  // Print to the console
  Serial.print("Saving to " CONFIG_FILE_PATH ": ");
  json.printTo(Serial);
  Serial.print("\n");

  // Print to the file
  json.prettyPrintTo(configFile);
  configFile.close();     
}

//***** HELPER FUNCTIONS
void cmpFromJSON(JsonObject* json, const char* key, char* destination, size_t maxLength) {
  Serial.printf("\t cmpFromJSON %s (%d)\n", key, maxLength);
  const char* value = (*json)[key].as<char*>();
  if (value) {
    Serial.printf("\t\t found value in JSON %s\n", value);
    strncpy(destination, value, maxLength);
    Serial.printf("\t\t %s is now %s\n", key, destination);
    return;
  }
}

void intFromJSON(JsonObject* json, const char* key, uint8_t* destination) {
  Serial.printf("\t intFromJSON %s\n", key);
  JsonVariant valueObj = (*json)[key];
  if (valueObj.success()) {
    Serial.printf("\t\t found value in JSON %d\n", valueObj.as<uint8_t>());
     *destination = valueObj.as<uint8_t>();
     Serial.printf("\t\t %s is now %d\n", key, *destination);
     return;
  }
}
void intFromJSON(JsonObject* json, const char* key, int8_t* destination) {
  Serial.printf("\t intFromJSON %s\n", key);
  JsonVariant valueObj = (*json)[key];
  if (valueObj.success()) {
    Serial.printf("\t\t found value in JSON %d\n", valueObj.as<int8_t>());
     *destination = valueObj.as<int8_t>();
     Serial.printf("\t\t %s is now %d\n", key, *destination);
     return;
  }
}

void intFromJSON(JsonObject* json, const char* key, uint16_t* destination) {
  Serial.printf("\t intFromJSON %s\n", key);
  JsonVariant valueObj = (*json)[key];
  if (valueObj.success()) {
    Serial.printf("\t\t found value in JSON %d\n", valueObj.as<uint16_t>());
     *destination = valueObj.as<uint16_t>();
     Serial.printf("\t\t %s is now %d\n", key, *destination);
     return;
  }
}
void RGBFromJSON(JsonObject* json, const char* key, CRGB* destination) {
  Serial.printf("\t RGBFromJSON %s\n", key);
  CRGB color;
  int ledCount = (*json)[key].asArray().copyTo(color.raw);
  if (ledCount == 3) {
    *destination = color;
    Serial.printf("\t\t %s value is now (%d,%d,%d)\n", key, color.r, color.g, color.b);
    return;
  }
}

