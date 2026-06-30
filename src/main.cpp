#pragma GCC optimize ("Ofast")
#include <M5Unified.h>
#include <ESP32Encoder.h>
#include <PatternMath.h>
#include "ossm/OssmClient.h"
#include "ossm/OssmDiscovery.h"
#include "OneButton.h"          //For Button Debounce and Longpress
#include "config.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <Wire.h>
#include <lvgl.h>
#include <SPI.h>
#include "ui/ui.h"
#include "main.h"
#include "Preferences.h"      //EEPROM replacement function

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <esp_timer.h>

constexpr int32_t HOR_RES=320;
constexpr int32_t VER_RES=240;
constexpr uint32_t OSSM_START_BUTTON_COLOR = 0x2E7D32;
constexpr uint32_t OSSM_STOP_BUTTON_COLOR = 0xC62828;

///////////////////////////////////////////
////
////  To Debug or not to Debug
////
///////////////////////////////////////////

// Uncomment the following line if you wish to print DEBUG info
#define DEBUG 

#ifdef DEBUG
#define LogDebug(...) Serial.println(__VA_ARGS__)
#define LogDebugFormatted(...) Serial.printf(__VA_ARGS__)
#else
#define LogDebug(...) ((void)0)
#define LogDebugFormatted(...) ((void)0)
#endif

// Screens 

#define ST_UI_START 0
#define ST_UI_HOME 1

#define ST_UI_MENUE 10
#define ST_UI_PATTERN 11
#define ST_UI_Torqe 12
#define ST_UI_EJECTSETTINGS 13

#define ST_UI_SETTINGS 20

int st_screens = ST_UI_START;



// Menü States

#define CONNECT 0
#define HOME 1
#define MENUE 2
#define MENUE2 3
#define TORQE 4
#define PATTERN_MENUE 5
#define PATTERN_MENUE2 6
#define PATTERN_MENUE3 7
#define CUM_MENUE 20

int menuestatus = CONNECT;

// EEPROM replacement function using Non-volatie memory (NVS)
Preferences m5prf; //initiate an instance of the Preferences library

bool eject_status = false;
bool dark_mode = false;
bool vibrate_mode = true;
bool touch_home = false;
bool touch_disabled = false;

// Command States
#define CONN 0
#define SPEED 1
#define DEPTH 2
#define STROKE 3
#define SENSATION 4
#define PATTERN 5
#define TORQE_F 6
#define TORQE_R 7
#define OFF 10
#define ON  11
#define SETUP_D_I 12
#define SETUP_D_I_F 13
#define REBOOT 14

#define CUMSPEED 20
#define CUMTIME 21
#define CUMSIZE   22
#define CUMACCEL  23

#define CONNECT 88
#define HEARTBEAT 99

int displaywidth;
int displayheight;
int progheight = 30;
int distheight = 10;
int S1Pos;
int S2Pos;
int S3Pos;
int S4Pos;
bool rstate = false;
int pattern = 2;
char patternstr[OssmClient::kPatternNameCapacity];
bool onoff = false;


long speedenc = 0;
long depthenc = 0;
long strokeenc = 0;
long sensationenc = 0;
long torqe_f_enc = 0;
long torqe_r_enc = 0;
long cum_t_enc = 0;
long cum_si_enc =0;
long cum_s_enc = 0;
long cum_a_enc = 0;
long encoder4_enc = 0;

extern float maxdepthinmm = 100.0; // Temporary compatibility for generated UI; Home controls are percent based.
extern float speedlimit = 100;
int speedscale = -5;

float speed = 0.0;
float depth = 10.0;
float stroke = 10.0;
float sensation = 50.0;
float torqe_f = 100.0;
float torqe_r = -180.0;
float cum_time = 0.0;
float cum_speed = 0.0;
float cum_size = 0.0;
float cum_accel = 0.0;

unsigned long nowMs;
int  rampMs;
bool rampEnabled = true;
int rampValue;
int rampTime = 75;
int maxRamp = 8;
int encId;
int activeEncId;

bool dynamicStroke = false;

ESP32Encoder encoder1;
ESP32Encoder encoder2;
ESP32Encoder encoder3;
ESP32Encoder encoder4;

OssmClient ossmClient;
OssmDiscovery ossmDiscovery;

bool ossmClientInitialized = false;
bool ossmConnectedScreenShown = false;

constexpr uint32_t OSSM_SCAN_DURATION_MS = 5000;

constexpr size_t PATTERN_OPTIONS_CAPACITY =
  OssmClient::kMaxPatternCount * (OssmClient::kPatternNameCapacity + 1);
OssmClient::PatternList activePatternList;
char patternOptions[PATTERN_OPTIONS_CAPACITY];

// Bool

bool EJECT_On = false;
bool OSSM_On = false;

void setOssmOn(bool isOn){
  OSSM_On = isOn;
  if(ui_HomeButtonMText != nullptr){
    lv_label_set_text(ui_HomeButtonMText, OSSM_On ? "Stop" : "Start");
  }
  if(ui_HomeButtonM != nullptr){
    lv_obj_set_style_bg_color(ui_HomeButtonM,
                              lv_color_hex(OSSM_On ? OSSM_STOP_BUTTON_COLOR : OSSM_START_BUTTON_COLOR),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}

uint32_t vibrationEndAt = 0;

bool connectbtn(); //Handels Connectbtn
int64_t touchmenue();

// Makes vibration motor go Brrrrr
void vibrate(int vbr_Intensity = 200, int vbr_Length = 100){
    if(lv_obj_has_state(ui_vibrate, LV_STATE_CHECKED) == 1){
      M5.Power.setVibration(vbr_Intensity);
      vibrationEndAt = millis() + vbr_Length;
    }
}

void updateVibration(){
  if(vibrationEndAt != 0 && millis() >= vibrationEndAt){
    M5.Power.setVibration(0);
    vibrationEndAt = 0;
  }
}

void mxclick();
bool mxclick_short_waspressed = false;
void mxlong();
bool mxclick_long_waspressed = false;
void click2();
bool click2_short_waspressed = false;
void click3();
bool click3_short_waspressed = false;
void c3long();
bool click3_long_waspressed = false;
void c3double();
bool click3_double_waspressed = false;


lv_display_t *display;
lv_indev_t *indev;

static lv_draw_buf_t *draw_buf1;
static lv_draw_buf_t *draw_buf2;

// Display flushing
void my_display_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  lv_draw_sw_rgb565_swap(px_map, w*h);
  M5.Display.pushImageDMA<uint16_t>(area->x1, area->y1, w, h, (uint16_t *)px_map);
  lv_disp_flush_ready(disp);
}

uint32_t my_tick_function() {
  return (esp_timer_get_time() / 1000LL);
}

void my_touchpad_read(lv_indev_t * drv, lv_indev_data_t * data) {
  M5.update();
  auto count = M5.Touch.getCount();

  if(touch_disabled != true){
    if ( count == 0 ) {
      data->state = LV_INDEV_STATE_RELEASED;
    } else {
      auto touch = M5.Touch.getDetail(0);
      data->state = LV_INDEV_STATE_PRESSED; 
      data->point.x = touch.x;
      data->point.y = touch.y;
    }
}
}

static void event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *label = reinterpret_cast<lv_obj_t *>(lv_event_get_user_data(e));

  switch (code)
  {
  case LV_EVENT_PRESSED:
    lv_label_set_text(label, "The last button event:\nLV_EVENT_PRESSED");
    break;
  case LV_EVENT_CLICKED:
    lv_label_set_text(label, "The last button event:\nLV_EVENT_CLICKED");
    break;
  case LV_EVENT_LONG_PRESSED:
    lv_label_set_text(label, "The last button event:\nLV_EVENT_LONG_PRESSED");
    break;
  case LV_EVENT_LONG_PRESSED_REPEAT:
    lv_label_set_text(label, "The last button event:\nLV_EVENT_LONG_PRESSED_REPEAT");
    break;
  default:
    break;
  }
}



constexpr int HOME_CONTROL_MIN = 0;
constexpr int HOME_CONTROL_MAX = 100;
constexpr int HOME_SENSATION_NEUTRAL = 50;

int clampHomePercent(float value){
  if(value < 0.0f){
    return HOME_CONTROL_MIN;
  }
  if(value > 100.0f){
    return HOME_CONTROL_MAX;
  }
  return static_cast<int>(value + 0.5f);
}

int patternRowForId(int patternId){
  for(size_t i = 0; i < activePatternList.count; ++i){
    if(activePatternList.patterns[i].id == patternId){
      return static_cast<int>(i);
    }
  }

  return -1;
}

void applyPatternListToUi(const OssmClient::PatternList& patterns){
  if(patterns.count == 0){
    return;
  }

  activePatternList = patterns;
  patternOptions[0] = '\0';
  size_t used = 0;
  for(size_t i = 0; i < activePatternList.count && used < sizeof(patternOptions) - 1; ++i){
    const int written = snprintf(patternOptions + used, sizeof(patternOptions) - used,
                                 "%s%s", i > 0 ? "\n" : "", activePatternList.patterns[i].name);
    if(written <= 0){
      break;
    }

    const size_t appended = static_cast<size_t>(written);
    if(appended >= sizeof(patternOptions) - used){
      used = sizeof(patternOptions) - 1;
      break;
    }

    used += appended;
  }

  if(patternOptions[0] != '\0'){
    lv_roller_set_options(ui_PatternS, patternOptions, LV_ROLLER_MODE_NORMAL);
  }

  const int row = patternRowForId(pattern);
  if(row >= 0){
    lv_roller_set_selected(ui_PatternS, row, LV_ANIM_OFF);
    lv_label_set_text(ui_HomePatternLabel, activePatternList.patterns[row].name);
  }
}

void syncObservedPatternToUi(int observedPattern){
  pattern = observedPattern;
  const int row = patternRowForId(pattern);
  if(row >= 0){
    lv_label_set_text(ui_HomePatternLabel, activePatternList.patterns[row].name);
  }else{
    snprintf(patternstr, sizeof(patternstr), "Pattern %d", pattern);
    lv_label_set_text(ui_HomePatternLabel, patternstr);
  }

  if(lv_scr_act() != ui_Pattern && row >= 0){
    lv_roller_set_selected(ui_PatternS, row, LV_ANIM_OFF);
  }
}

void clampStrokeWindow(){
  depth = clampHomePercent(depth);
  stroke = clampHomePercent(stroke);
  if(stroke > depth){
    stroke = depth;
  }
}

void setHomeControlRanges(){
  lv_slider_set_range(ui_homespeedslider, HOME_CONTROL_MIN, HOME_CONTROL_MAX);
  lv_slider_set_range(ui_homedepthslider, HOME_CONTROL_MIN, HOME_CONTROL_MAX);
  lv_slider_set_range(ui_homestrokeslider, HOME_CONTROL_MIN, HOME_CONTROL_MAX);
  lv_slider_set_range(ui_homesensationslider, HOME_CONTROL_MIN, HOME_CONTROL_MAX);
  lv_slider_set_mode(ui_homesensationslider, LV_SLIDER_MODE_NORMAL);
}

const char* ossmModeStateText(OssmClient::ModeState modeState){
  switch(modeState){
    case OssmClient::ModeState::Idle:
      return "Connected";
    case OssmClient::ModeState::Entering:
      return "Mode Change";
    case OssmClient::ModeState::SpeedKnobBlocked:
      return "Lower Knob";
    case OssmClient::ModeState::Ready:
      return "Ready";
    case OssmClient::ModeState::Failed:
      return "Error";
  }
  return "";
}

void pollOssmConnection(){
  ossmDiscovery.update();

  if(ossmDiscovery.isScanning()){
    DiscoveredOssm discovered;
    if(ossmDiscovery.deviceAt(0, discovered)){
      ossmDiscovery.stopScan();

      if(ossmClient.connect(discovered.address)){
        setOssmOn(false);
        ossmClient.setSpeed(0);
        ossmClient.setDepth(clampHomePercent(depth));
        ossmClient.setStroke(clampHomePercent(stroke));
        ossmClient.setSensation(clampHomePercent(sensation));
        ossmClient.setPattern(pattern);
      }
    }
  }

  const OssmClient::ConnectionState connectionState = ossmClient.connectionState();
  const bool connectionBusy = ossmDiscovery.isScanning() ||
                              connectionState == OssmClient::ConnectionState::Connecting ||
                              connectionState == OssmClient::ConnectionState::Disconnecting;
  lv_label_set_text(ui_StartButtonLText, connectionBusy ? "Connecting..." : "Connect");

  if(connectionState == OssmClient::ConnectionState::Connected){
    OssmClient::ModeState modeState = ossmClient.modeState();
    if(modeState == OssmClient::ModeState::Idle && ossmClient.enterStrokeEngine()){
      modeState = ossmClient.modeState();
    }
    lv_label_set_text(ui_connect, ossmModeStateText(modeState));
    if(!ossmConnectedScreenShown){
      OssmClient::PatternList patterns;
      if(!ossmClient.patternList(patterns)){
        Serial.println("Unable to get pattern list");
      }
      applyPatternListToUi(patterns);
      ossmConnectedScreenShown = true;
      lv_scr_load_anim(ui_Home, LV_SCR_LOAD_ANIM_FADE_ON,20,0,false);
    }
  }else if(connectionState == OssmClient::ConnectionState::Disconnected &&
           !ossmDiscovery.isScanning()){
    const bool homeWasShown = ossmConnectedScreenShown;
    setOssmOn(false);
    ossmConnectedScreenShown = false;

    if(homeWasShown){
      lv_label_set_text(ui_connect, "Conn Lost");
    }
  }

  if(!ossmClient.isReady()){
    setOssmOn(false);
  }

  OssmClient::ObservedState observed;
  if(ossmClient.latestObservedState(observed)){
    syncObservedPatternToUi(observed.pattern);
  }
}

// Compatibility adapter for the old command-based UI code.
bool SendCommand(int Command, float Value, int Target){
  if(Target != OSSM_ID){
    switch(Command){
      case CUMSPEED:
      case CUMTIME:
      case CUMSIZE:
      case CUMACCEL:
        // Unsupported
        return true;
      default:
        return false;
    }
  }

  if(!ossmClientInitialized){
    return false;
  }

  switch(Command){
    case SPEED:
      if(OSSM_On){
        if(!ossmClient.setSpeed(clampHomePercent(Value))){
          setOssmOn(false);
          return false;
        }
      }
      return true;
    case DEPTH:
      ossmClient.setDepth(clampHomePercent(Value));
      return true;
    case STROKE:
      ossmClient.setStroke(clampHomePercent(Value));
      return true;
    case SENSATION:
      ossmClient.setSensation(clampHomePercent(Value));
      return true;
    case ON:
      if(!ossmClient.isReady() || !ossmClient.setSpeed(clampHomePercent(speed))){
        setOssmOn(false);
        return false;
      }
      setOssmOn(true);
      return true;
    case OFF:
      ossmClient.stop();
      setOssmOn(false);
      return true;
    case PATTERN:
      ossmClient.setPattern(Value < 0.0f ? 0 : static_cast<int>(Value + 0.5f));
      return true;
    case TORQE_F:
    case TORQE_R:
      // Unsupported
      return true;
    case SETUP_D_I:
    case SETUP_D_I_F:
      // Unsupported
      return true;
    case REBOOT:
      // TODO: Reimplement?
      return true;
    case HEARTBEAT:
    case CONN:
      // TODO: Replace legacy connection commands with richer BLE status handling.
      return true;
    default:
      return false;
  }
}

void connectbutton(lv_event_t * e)
{
    if(ossmDiscovery.isScanning() ||
       ossmClient.connectionState() != OssmClient::ConnectionState::Disconnected){
      return;
    }

    if(!ossmClientInitialized){
      return;
    }

    if(ossmDiscovery.startScan(OSSM_SCAN_DURATION_MS)){
      lv_label_set_text(ui_StartButtonLText, "Connecting...");
    }
}

void savesettings(lv_event_t * e)
{

  m5prf.begin("m5-ctnr", false); //open NVS-storage container/session. False means that it's used it in read+write mode. Set true to open or create the namespace in read-only mode.

  if(lv_obj_has_state(ui_vibrate, LV_STATE_CHECKED) == 1){
    m5prf.putBool("Vibrate", true); //NSV-storage write true to key "Vibrate"
	}else if(lv_obj_has_state(ui_vibrate, LV_STATE_CHECKED) == 0){
    m5prf.putBool("Vibrate", false);
	}

  if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
    m5prf.putBool("Lefty", true); // ui_lefty in SL-Studio code is actually Touch-enable toggle
	}else if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 0){
    m5prf.putBool("Lefty", false);
	}

	if(lv_obj_has_state(ui_ejectaddon, LV_STATE_CHECKED) == 1){
    m5prf.putBool("ejectAddon", true);  
	}else if(lv_obj_has_state(ui_ejectaddon, LV_STATE_CHECKED) == 0){
    m5prf.putBool("ejectAddon", false);
	}

  //read darkmode saved setting to force reboot for theme change
  bool theme_Change_Previous = false;
  bool theme_Change_New = false;
  theme_Change_Previous = m5prf.getBool("Darkmode", true);

  if(lv_obj_has_state(ui_darkmode, LV_STATE_CHECKED) == 1){
    theme_Change_New = true;
    m5prf.putBool("Darkmode", true);
	}else if(lv_obj_has_state(ui_darkmode, LV_STATE_CHECKED) == 0){
    theme_Change_New = false;
    m5prf.putBool("Darkmode", false);
	}

  m5prf.end(); //close storage container/session.
  delay(100);

  if(theme_Change_Previous != theme_Change_New){
    vibrate(225,75);
    ESP.restart(); //reboot is only required to change themes, you don't need to restart for settings to save with NVS.
  }else{
    vibrate(225,75);
  }
}

void screenmachine(lv_event_t * e)
{
  if (lv_scr_act() == ui_Start){
    st_screens = ST_UI_START;
  } else if (lv_scr_act() == ui_Home){
    st_screens = ST_UI_HOME;
    setHomeControlRanges();
    speed = lv_slider_get_value(ui_homespeedslider);
    LogDebug(speedenc);
    LogDebug(speed);

            
  } else if (lv_scr_act() == ui_Menue){
    st_screens = ST_UI_MENUE;
  } else if (lv_scr_act() == ui_Pattern){
    st_screens = ST_UI_PATTERN;
    const int row = patternRowForId(pattern);
    if(row >= 0){
      lv_roller_set_selected(ui_PatternS, row, LV_ANIM_OFF);
    }
  } else if (lv_scr_act() == ui_Torqe){
    st_screens = ST_UI_Torqe;
    torqe_f = lv_slider_get_value(ui_outtroqeslider);
    torqe_f_enc = fscale(50, 200, 0, Encoder_MAP, torqe_f, 0);
    encoder1.setCount(torqe_f_enc);

    torqe_r = lv_slider_get_value(ui_introqeslider);
    torqe_r_enc = fscale(20, 200, 0, Encoder_MAP, torqe_r, 0);
    encoder4.setCount(torqe_r_enc);

  } else if (lv_scr_act() == ui_EJECTSettings){
    st_screens = ST_UI_EJECTSETTINGS;
  } else if (lv_scr_act() == ui_Settings){
    st_screens = ST_UI_SETTINGS;
  }
}

void ejectcreampie(lv_event_t * e){
  if(EJECT_On == true){
    lv_obj_clear_state(ui_HomeButtonL, LV_STATE_CHECKED);
    EJECT_On = false;
  } else if(EJECT_On == false){
    lv_obj_clear_state(ui_HomeButtonL, LV_STATE_CHECKED);
    depth = 0;
    speed = 0;
    stroke = 0;
    SendCommand(SETUP_D_I, 0.0, OSSM_ID);
    SendCommand(DEPTH, depth, OSSM_ID);
    screenmachine(e);
    EJECT_On = true;
  } 
}

void savepattern(lv_event_t * e){
  const int selectedRow = lv_roller_get_selected(ui_PatternS);
  pattern = selectedRow;
  if(selectedRow >= 0 && static_cast<size_t>(selectedRow) < activePatternList.count){
    pattern = activePatternList.patterns[selectedRow].id;
    lv_label_set_text(ui_HomePatternLabel, activePatternList.patterns[selectedRow].name);
  }else{
    lv_roller_get_selected_str(ui_PatternS, patternstr, sizeof(patternstr));
    lv_label_set_text(ui_HomePatternLabel, patternstr);
  }
  LogDebug(pattern);
  SendCommand(PATTERN, pattern, OSSM_ID);
}

void homebuttonmevent(lv_event_t * e){
  LogDebug("HomeButton");
  if(OSSM_On == false){
    SendCommand(ON, 0.0, OSSM_ID);
  } else if(OSSM_On == true){
    SendCommand(OFF, 0.0, OSSM_ID);
  }
}

void setupDepthInter(lv_event_t * e){
    SendCommand(SETUP_D_I, 0.0, OSSM_ID);
}

void setupdepthF(lv_event_t * e){
    SendCommand(SETUP_D_I_F, 0.0, OSSM_ID);
}

void setup(){
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  Serial.println("Serial diagnostics ready");

  m5prf.begin("m5-ctnr", false); 
    // Loads these settings at boot
    eject_status = m5prf.getBool("ejectAddon", false); //boolean here is used if key does not exist yet
    dark_mode = m5prf.getBool("Darkmode", true);       // ^ (basically first boot defaults, saving settings surives a re-flash!)
    vibrate_mode = m5prf.getBool("Vibrate", true);
    touch_home= m5prf.getBool("Lefty", false);       // = touchcreen. There apears to be no actual lefthanded mode anywhere
  m5prf.end();

  M5.Power.setChargeCurrent(BATTERY_CHARGE_CURRENT);
  LogDebug("\n Starting");      // Start LogDebug

  NimBLEDevice::init("OSSM M5 Remote");
  ossmClientInitialized = ossmClient.begin();
  if(!ossmClientInitialized){
    Serial.printf("OSSM client init failed: %d\n", ossmClient.lastError());
  }
  if(!ossmDiscovery.begin()){
    Serial.println("OSSM discovery init failed");
  }
  delay(200);
  
  encoder1.attachHalfQuad(ENC_1_CLK, ENC_1_DT);
  encoder2.attachHalfQuad(ENC_2_CLK, ENC_2_DT);
  encoder3.attachHalfQuad(ENC_3_CLK, ENC_3_DT);
  encoder4.attachHalfQuad(ENC_4_CLK, ENC_4_DT);
  Button1.attachClick(mxclick);
  Button1.attachLongPressStart(mxlong);
  Button2.attachClick(click2);
  Button3.attachClick(click3);
  Button3.attachLongPressStart(c3long);
  Button3.attachDoubleClick(c3double);

  // Initialize `disp_buf` display buffer with the buffer(s).
  // lv_draw_buf_init(&draw_buf, LV_HOR_RES_MAX, LV_VER_RES_MAX);
  M5.Display.setEpdMode(epd_mode_t::epd_fastest); // fastest but very-low quality.
  if (M5.Display.width() < M5.Display.height())
  { /// Landscape mode.
  M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }
  
  lv_init();
  lv_tick_set_cb(my_tick_function);

  display = lv_display_create(HOR_RES, VER_RES);
  lv_display_set_flush_cb(display, my_display_flush);

  static lv_color_t buf1[HOR_RES * 15]; 
  lv_display_set_buffers(display, buf1, nullptr, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
  ui_init();
  setOssmOn(false);
  setHomeControlRanges();
  lv_slider_set_value(ui_homesensationslider, sensation, LV_ANIM_OFF);

  if(eject_status == true){
  lv_obj_add_state(ui_ejectaddon, LV_STATE_CHECKED);
  lv_obj_clear_state(ui_EJECTSettingButton, LV_STATE_DISABLED);
  lv_obj_clear_state(ui_HomeButtonL, LV_STATE_DISABLED);
  }
  if(dark_mode == true){
  lv_obj_add_state(ui_darkmode, LV_STATE_CHECKED);
  }
  if(vibrate_mode == true){
  lv_obj_add_state(ui_vibrate, LV_STATE_CHECKED);
  }
  if(touch_home == true){
  lv_obj_add_state(ui_lefty, LV_STATE_CHECKED);
  }
  lv_roller_set_selected(ui_PatternS, pattern, LV_ANIM_OFF);
  lv_roller_get_selected_str(ui_PatternS, patternstr, sizeof(patternstr));
  lv_label_set_text(ui_HomePatternLabel, patternstr);


}

void loop()
{
     bool changed=false;
     const int BatteryLevel = M5.Power.getBatteryLevel();
     String BatteryValue = (String(BatteryLevel, DEC) + "%");
     const char *battVal = BatteryValue.c_str();
     lv_bar_set_value(ui_Battery, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue, battVal);
     lv_bar_set_value(ui_Battery1, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue1, battVal);
     lv_bar_set_value(ui_Battery2, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue2, battVal);
     lv_bar_set_value(ui_Battery3, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue3, battVal);
     lv_bar_set_value(ui_Battery4, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue4, battVal);
     lv_bar_set_value(ui_Battery5, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue5, battVal);

     M5.update();
     updateVibration();
     lv_task_handler();
     Button1.tick();
     Button2.tick();
     Button3.tick();
     pollOssmConnection();

     switch(st_screens){
      
     case ST_UI_START: //Menu With logo after boot
      {
        if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
          touch_disabled = true;
        }

        if(click2_short_waspressed == true){
         lv_obj_send_event(ui_StartButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_StartButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_obj_send_event(ui_StartButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

      case ST_UI_HOME: //Menu with OSSM control sliders
      {
        if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
          touch_disabled = true;
        }

          //RampHelper
          nowMs = millis();
          if (nowMs - rampMs <= rampTime && rampEnabled == true){ 
              if (rampValue <= maxRamp && encId == activeEncId){
            				++rampValue;
              }
          }else{
          rampValue = 1;
          activeEncId = encId;
          }

        //
        // Encoder 1 Speed 
        //
        if(lv_slider_is_dragged(ui_homespeedslider) == false){ //if knob gets rotated
          changed = false;
          lv_slider_set_value(ui_homespeedslider, speed, LV_ANIM_OFF);

          if (encoder1.getCount() >= 2){      //speed up
            changed = true;
            speed += rampValue;
            encoder1.setCount(0);
            rampMs = millis();
            encId = 1;
		      }else if (encoder1.getCount() <= -2){      //speed down
            changed = true;
            speed -=rampValue;
            encoder1.setCount(0);
            rampMs = millis();
            encId = 1;
          }

          //speed min-max bounds
          if (speed < HOME_CONTROL_MIN){
            changed = true;
            speed = HOME_CONTROL_MIN;
          }
          if (speed > HOME_CONTROL_MAX){
            changed = true;
            speed = HOME_CONTROL_MAX;
          }
          
          //send speed
          if (changed) {
            SendCommand(SPEED, speed, OSSM_ID);
          }
        
        
        }else if (lv_slider_get_value(ui_homespeedslider) != speed){ //if slider moved
            speed = lv_slider_get_value(ui_homespeedslider);
            SendCommand(SPEED, speed, OSSM_ID);
        }
        char speed_v[7];
        dtostrf(speed, 6, 0, speed_v);
        lv_label_set_text(ui_homespeedvalue, speed_v);

        //
        // Encoder 2 Depth 
        //
        if(lv_slider_is_dragged(ui_homedepthslider) == false){ //if knob gets rotated
          changed = false;
          lv_slider_set_value(ui_homedepthslider, depth, LV_ANIM_OFF);

		      if (encoder2.getCount() >= 2){      //depth up
            changed = true;
            depth += rampValue;
            if (dynamicStroke == true){
              stroke += rampValue;
            }
            encoder2.setCount(0);
            rampMs = millis();
            encId = 2;
		      }else if (encoder2.getCount() <= -2){      //depth down
            changed = true;
            depth -=rampValue;
            if (dynamicStroke == true){
              stroke -= rampValue;
              if(stroke >= depth){
                stroke = depth;
              }
            }
            encoder2.setCount(0);
            rampMs = millis();
            encId = 2;
          }

          clampStrokeWindow();
          
          //send depth
          if (changed) {
            SendCommand(DEPTH, depth, OSSM_ID);
            SendCommand(STROKE, stroke, OSSM_ID);
          }
        }else if(lv_slider_get_value(ui_homedepthslider) != depth){
            depth = lv_slider_get_value(ui_homedepthslider);
            clampStrokeWindow();
            SendCommand(DEPTH, depth, OSSM_ID);
            SendCommand(STROKE, stroke, OSSM_ID);
        }
        char depth_v[7];
        dtostrf(depth, 6, 0, depth_v);
        lv_label_set_text(ui_homedepthvalue, depth_v);
        
        //
        // Encoder 3 Stroke 
        //
        if(lv_slider_is_dragged(ui_homestrokeslider) == false){ //if knob gets rotated
          changed = false;
          lv_bar_set_start_value(ui_homestrokeslider, depth - stroke, LV_ANIM_OFF);
          lv_slider_set_value(ui_homestrokeslider, depth, LV_ANIM_OFF);


		      if (encoder3.getCount() >= 2){      //Stroke up
            changed = true;
            stroke -= rampValue;
            encoder3.setCount(0);
            rampMs = millis();
            encId = 3;
		      }else if (encoder3.getCount() <= -2){      //Stroke down
            changed = true;
            stroke += rampValue;
            encoder3.setCount(0);
            rampMs = millis();
            encId = 3;
          }

          clampStrokeWindow();
          
          //send stroke
          if (changed) {
            SendCommand(STROKE, stroke, OSSM_ID);
            SendCommand(DEPTH, depth, OSSM_ID);
          }

        } else if(lv_slider_get_left_value(ui_homestrokeslider) != depth - stroke){
            stroke = depth - lv_slider_get_left_value(ui_homestrokeslider);
            clampStrokeWindow();
            SendCommand(STROKE, stroke, OSSM_ID);
        } else if(lv_slider_get_value(ui_homestrokeslider) != depth){
            depth = lv_slider_get_value(ui_homestrokeslider);
            clampStrokeWindow();
            SendCommand(DEPTH, depth, OSSM_ID);
            SendCommand(STROKE, stroke, OSSM_ID);
        }

        char stroke_v[7];
        dtostrf(stroke, 6, 0, stroke_v);
        lv_label_set_text(ui_homestrokevalue, stroke_v);  //was lv_label_set_text(ui_homestrokevalue, stroke_v);

        //
        // Encoder4 Sensation
        //
        if(lv_slider_is_dragged(ui_homesensationslider) == false){
          changed = false;
          lv_slider_set_value(ui_homesensationslider, sensation, LV_ANIM_OFF);

		      if (encoder4.getCount() >= 2){      //Stroke up
            changed = true;
            sensation += 2;
            encoder4.setCount(0);
            rampMs = millis();
            encId = 4;
		      }else if (encoder4.getCount() <= -2){      //Stroke down
            changed = true;
            sensation -= 2;
            encoder4.setCount(0);
            rampMs = millis();
            encId = 4;
            
          }

          //Stoke min-max bounds
          if (sensation < HOME_CONTROL_MIN){
            changed = true;
            sensation = HOME_CONTROL_MIN;
          }
          if (sensation > HOME_CONTROL_MAX){
            changed = true;
            sensation = HOME_CONTROL_MAX;
          }

          if (changed) {
            SendCommand(SENSATION, sensation, OSSM_ID);
          }          
        } else if(lv_slider_get_value(ui_homesensationslider) != sensation){
            sensation = lv_slider_get_value(ui_homesensationslider);
            SendCommand(SENSATION, sensation, OSSM_ID);
        }

        if(click2_short_waspressed == true){
         lv_obj_send_event(ui_HomeButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_HomeButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_obj_send_event(ui_HomeButtonR, LV_EVENT_CLICKED, NULL);
        } else if(click3_long_waspressed == true){
          sensation = HOME_SENSATION_NEUTRAL;        //reset sensation to neutral
          SendCommand(SENSATION, sensation, OSSM_ID);
        }else if(click3_double_waspressed == true){
          if (dynamicStroke == false){
            dynamicStroke = true;;            /// dynamicStroke = !dynamicStroke; crashes M5 for some reason
          }else{
            dynamicStroke = false;;
          }
          if (stroke >= depth){
            stroke = depth;
          }
          clampStrokeWindow();
        }
      }
      break;

      case ST_UI_MENUE:
      {
        if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
          touch_disabled = true;
        }
        if(encoder4.getCount() > encoder4_enc + 2){
          LogDebug("next");
          lv_group_focus_next(ui_g_menue);
          encoder4_enc = encoder4.getCount();
        } else if(encoder4.getCount() < encoder4_enc -2){
          lv_group_focus_prev(ui_g_menue);
          LogDebug("Preview");
          encoder4_enc = encoder4.getCount();
        }

        if(click2_short_waspressed == true){
         lv_obj_send_event(ui_MenueButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_MenueButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_obj_send_event(lv_group_get_focused(ui_g_menue), LV_EVENT_CLICKED, NULL);
        } else if(click3_long_waspressed == true){
         SendCommand(REBOOT, 0, OSSM_ID);
        } 
      }
      break;

      case ST_UI_PATTERN:
      {
        if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
          touch_disabled = true;
        }
        if(encoder4.getCount() > encoder4_enc + 2){
          LogDebug("next");
          uint32_t t = LV_KEY_DOWN;
          lv_obj_send_event(ui_PatternS, LV_EVENT_KEY, &t);
          encoder4_enc = encoder4.getCount();
        } else if(encoder4.getCount() < encoder4_enc -2){
          uint32_t t = LV_KEY_UP;
          lv_obj_send_event(ui_PatternS, LV_EVENT_KEY, &t);
          LogDebug("Preview");
          encoder4_enc = encoder4.getCount();
        }
         if(click2_short_waspressed == true){
         lv_obj_send_event(ui_PatternButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_PatternButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_obj_send_event(ui_PatternButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

      case ST_UI_Torqe:
      {
        if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
          touch_disabled = true;
        }
        // Encoder 1 Torqe Out
        if(lv_slider_is_dragged(ui_outtroqeslider) == false){
          if (encoder1.getCount() != torqe_f_enc){
            lv_slider_set_value(ui_outtroqeslider, torqe_f, LV_ANIM_OFF);
            if(encoder1.getCount() <= 0){
              encoder1.setCount(0);
            } else if (encoder1.getCount() >= Encoder_MAP){
              encoder1.setCount(Encoder_MAP);
            } 
            torqe_f_enc = encoder1.getCount();
            torqe_f = fscale(0, Encoder_MAP, 50, 200, torqe_f_enc, 0);
            SendCommand(TORQE_F, torqe_f, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_outtroqeslider) != torqe_f){
            torqe_f_enc = fscale(50, 200, 0, Encoder_MAP, torqe_f, 0);
            encoder1.setCount(torqe_f_enc);
            torqe_f = lv_slider_get_value(ui_outtroqeslider);
            SendCommand(TORQE_F, torqe_f, OSSM_ID);
        }
        char torqe_f_v[7];
        dtostrf((torqe_f*-1), 6, 0, torqe_f_v);
        lv_label_set_text(ui_outtroqevalue, torqe_f_v);

        // Encoder 4 Torqe IN
        if(lv_slider_is_dragged(ui_introqeslider) == false){
          if (encoder4.getCount() != torqe_r_enc){
            lv_slider_set_value(ui_introqeslider, torqe_r, LV_ANIM_OFF);
            if(encoder4.getCount() <= 0){
              encoder4.setCount(0);
            } else if (encoder4.getCount() >= Encoder_MAP){
              encoder4.setCount(Encoder_MAP);
            } 
            torqe_r_enc = encoder4.getCount();
            torqe_r = fscale(0, Encoder_MAP, 20, 200, torqe_r_enc, 0);
            SendCommand(TORQE_R, torqe_r, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_introqeslider) != torqe_r){
            torqe_r_enc = fscale(20, 200, 0, Encoder_MAP, torqe_r, 0);
            encoder4.setCount(torqe_r_enc);
            torqe_r = lv_slider_get_value(ui_introqeslider);
            SendCommand(TORQE_R, torqe_r, OSSM_ID);
        }
        char torqe_r_v[7];
        dtostrf(torqe_r, 6, 0, torqe_r_v);
        lv_label_set_text(ui_introqevalue, torqe_r_v);

         if(click2_short_waspressed == true){
         lv_obj_send_event(ui_TorqeButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_TorqeButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_obj_send_event(ui_TorqeButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

      case ST_UI_EJECTSETTINGS:
      {
        if(lv_obj_has_state(ui_lefty, LV_STATE_CHECKED) == 1){
          touch_disabled = true;
        }
        
         if(click2_short_waspressed == true){
         lv_obj_send_event(ui_EJECTButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_EJECTButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         
        }
      }
      break;

      case ST_UI_SETTINGS: //Settings Menu
      {
        touch_disabled = false;

        if(encoder4.getCount() > encoder4_enc + 2){
          LogDebug("next");
          lv_group_focus_next(ui_g_settings);
          encoder4_enc = encoder4.getCount();
        } else if(encoder4.getCount() < encoder4_enc -2){
          lv_group_focus_prev(ui_g_settings);
          LogDebug("Preview");
          encoder4_enc = encoder4.getCount();
        }

        if(click2_short_waspressed == true){
         lv_obj_send_event(ui_MenueButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_obj_send_event(ui_MenueButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_obj_send_event(ui_EJECTButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

     }
     mxclick_long_waspressed = false;
     mxclick_short_waspressed = false;
     click2_short_waspressed = false;
     click3_short_waspressed = false;
     click3_long_waspressed = false;
     click3_double_waspressed = false;

  delay(5);
}

/*
void cumscreentask(void *pvParameters)
{
  for(;;)
  {
    M5.Lcd.setTextColor(FrontColor);
    if(encoder1.getCount() != cum_s_enc)
    {
    cum_s_enc = encoder1.getCount();
    cum_speed = map(constrain(cum_s_enc,0,Encoder_MAP),0,Encoder_MAP,1000,30000);
    M5.Lcd.fillRect(199,S1Pos,85,30,BgColor);
    M5.Lcd.setCursor(200,S1Pos+progheight-5);
    M5.Lcd.print(cum_speed);
    SendCommand(CUMSPEED, cum_speed, CUM);
    }

  if(encoder2.getCount() != cum_t_enc)
    {
    cum_t_enc = encoder2.getCount();
    cum_time = map(constrain(cum_t_enc,0,Encoder_MAP),0,Encoder_MAP,0,60);
    M5.Lcd.fillRect(199,S2Pos,85,30,BgColor);
    M5.Lcd.setCursor(200,S2Pos+progheight-5);
    M5.Lcd.print(cum_time);
    SendCommand(CUMTIME, cum_time, CUM);
    }

   if(encoder3.getCount() != cum_si_enc)
    {
    cum_si_enc = encoder3.getCount();
    cum_size = map(constrain(cum_si_enc,0,Encoder_MAP),0,Encoder_MAP,0,40);
    M5.Lcd.fillRect(199,S3Pos,85,30,BgColor);
    M5.Lcd.setCursor(200,S3Pos+progheight-5);
    M5.Lcd.print(cum_size);
    SendCommand(CUMSIZE, cum_size, CUM);
    }

   if(encoder4.getCount() != cum_a_enc)
    {
    cum_a_enc = encoder4.getCount();
    cum_accel = map(constrain(cum_a_enc,0,Encoder_MAP),0,Encoder_MAP,0,20);
    M5.Lcd.fillRect(199,S4Pos,85,30,BgColor);
    M5.Lcd.setCursor(200,S4Pos+progheight-5);
    M5.Lcd.print(cum_accel);
    SendCommand(CUMACCEL, cum_accel, CUM);
    }
  vTaskDelay(100);
  }
}
*/


void mxclick() {
  vibrate();
  mxclick_short_waspressed = true;
} 

void mxlong(){
  vibrate(200, 200);
  mxclick_long_waspressed = true;
} 

void click2() {
  vibrate();
  click2_short_waspressed = true;
} // click2

void click3() {
  vibrate();
  click3_short_waspressed = true;
} // click3

void c3long() {
    vibrate();
  click3_long_waspressed = true;
}

void c3double() {
    vibrate();
  click3_double_waspressed = true;
}
