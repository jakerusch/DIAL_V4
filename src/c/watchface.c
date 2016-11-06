////////////////////////////////////////////
// Written by Jacob Rusch
// 10/3/2016
// code for analog watch dial
// only for Dorite
////////////////////////////////////////////

#include <pebble.h>

///////////////////////
// weather variables //
///////////////////////
#define KEY_TEMP
#define KEY_ICON

////////////////////
// font variables //
////////////////////
#define WORD_FONT RESOURCE_ID_ULTRALIGHT_14
#define NUMBER_FONT RESOURCE_ID_ARCON_FONT_14

/////////////////////
// color variables //
/////////////////////
#define BACKGROUND_COLOR GColorBlack
#define FOREGROUND_COLOR GColorWhite

static Window *s_main_window;
static Layer *s_dial_layer, *s_hands_layer, *s_temp_circle, *s_battery_circle, *s_health_circle;
static TextLayer *s_temp_layer, *s_health_layer, *s_day_text_layer, *s_date_text_layer;
static GBitmap *s_weather_bitmap, *s_health_bitmap, *s_bluetooth_bitmap, *s_charging_bitmap, *s_bluetooth_bitmap;
static BitmapLayer *s_weather_bitmap_layer, *s_health_bitmap_layer, *s_bluetooth_bitmap_layer, *s_charging_bitmap_layer, *s_bluetooth_bitmap_layer;
static int buf=24, battery_percent, step_goal=10000;
static GFont s_word_font, s_number_font;
static char icon_buf[64];
static double step_count;
static char *char_current_steps;
static bool charging;

// //////////////////////
// // hide clock hands //
// //////////////////////
// static void hide_hands() {
//   layer_set_hidden(s_hands_layer, true); 
// }

// //////////////////////
// // show clock hands //
// //////////////////////
// static void show_hands() {
//   layer_set_hidden(s_hands_layer, false);
// }

// static void tap_handler(AccelAxisType axis, int32_t direction) {
//   if(direction > 0) {
//     hide_hands();
//     app_timer_register(5000, show_hands, NULL);
//   } else {
//     show_hands();
//   }
// }

/////////////////////////
// draws dial on watch //
/////////////////////////
static void dial_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds); 
  
  // draw dial
  graphics_context_set_fill_color(ctx, BACKGROUND_COLOR);
  graphics_fill_circle(ctx, center, (bounds.size.w+buf)/2);
  
  // set number of tickmarks
  int tick_marks_number = 60;

  // tick mark lengths
  int tick_length_end = (bounds.size.w+buf)/2; 
  int tick_length_start;
  
  // set colors
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_color(ctx, FOREGROUND_COLOR);
  graphics_context_set_stroke_width(ctx, 6);
  
  for(int i=0; i<tick_marks_number; i++) {
    // if number is divisible by 5, make large mark
    if(i%5==0) {
      graphics_context_set_stroke_width(ctx, 4);
      tick_length_start = tick_length_end-9;
    } else {
      graphics_context_set_stroke_width(ctx, 1);
      tick_length_start = tick_length_end-4;
    }

    int angle = TRIG_MAX_ANGLE * i/tick_marks_number;

    GPoint tick_mark_start = {
      .x = (int)(sin_lookup(angle) * (int)tick_length_start / TRIG_MAX_RATIO) + center.x,
      .y = (int)(-cos_lookup(angle) * (int)tick_length_start / TRIG_MAX_RATIO) + center.y,
    };
    
    GPoint tick_mark_end = {
      .x = (int)(sin_lookup(angle) * (int)tick_length_end / TRIG_MAX_RATIO) + center.x,
      .y = (int)(-cos_lookup(angle) * (int)tick_length_end / TRIG_MAX_RATIO) + center.y,
    };      
    
    graphics_draw_line(ctx, tick_mark_end, tick_mark_start);  
  } // end of loop 
  
  // draw box for day and date on right of watch
  GRect temp_rect = GRect(89, 76, 53, 16);
  graphics_draw_round_rect(ctx, temp_rect, 3);
  
  // dividing line in date round rectange
  GPoint start_temp_line = GPoint(123, 77);
  GPoint end_temp_line = GPoint(123, 91);
  graphics_draw_line(ctx, start_temp_line, end_temp_line);    
}

////////////////////////
// update temperature //
////////////////////////
static void temp_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, FOREGROUND_COLOR);
  GPoint center = GPoint(144/2, 36);
  graphics_context_set_fill_color(ctx, FOREGROUND_COLOR);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, 40/2);
}

///////////////////////////
// update battery status //
///////////////////////////
static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = GRect(4, 64, 40, 40);
  graphics_context_set_fill_color(ctx, FOREGROUND_COLOR);
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, 2, DEG_TO_TRIGANGLE(360-(battery_percent*3.6)), DEG_TO_TRIGANGLE(360));
  
  // draw vertical battery
  graphics_context_set_stroke_color(ctx, FOREGROUND_COLOR);
  graphics_draw_round_rect(ctx, GRect(21, 77, 7, 14), 1);
  int batt = battery_percent/10;
  graphics_fill_rect(ctx, GRect(23, 89-batt, 3, batt), 1, GCornerNone);
  graphics_fill_rect(ctx, GRect(23, 76, 3, 1), 0, GCornerNone);  
  
  // set visibility of charging icon
  layer_set_hidden(bitmap_layer_get_layer(s_charging_bitmap_layer), !charging);
}

//////////////////////////
// update health status //
//////////////////////////
static void health_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = GRect(50, 110, 40, 40);
  graphics_context_set_fill_color(ctx, FOREGROUND_COLOR);
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, 2, DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE((step_count/step_goal)*360));
}

/////////////////////////////////
// draw hands and update ticks //
/////////////////////////////////
static void ticks_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds); 
    
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  int hand_point_end = ((bounds.size.h)/2)-10;
  int hand_point_start = hand_point_end - 60;
  
  int filler_point_end = 40;
  int filler_point_start = filler_point_end-15;
  
  float minute_angle = TRIG_MAX_ANGLE * t->tm_min / 60;
  GPoint minute_hand_start = {
    .x = (int)(sin_lookup(minute_angle) * (int)hand_point_start / TRIG_MAX_RATIO) + center.x,
    .y = (int)(-cos_lookup(minute_angle) * (int)hand_point_start / TRIG_MAX_RATIO) + center.y,
  };
  
  GPoint minute_hand_end = {
    .x = (int)(sin_lookup(minute_angle) * (int)hand_point_end / TRIG_MAX_RATIO) + center.x,
    .y = (int)(-cos_lookup(minute_angle) * (int)hand_point_end / TRIG_MAX_RATIO) + center.y,
  };    
  
  float hour_angle = TRIG_MAX_ANGLE * ((((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6);
  GPoint hour_hand_start = {
    .x = (int)(sin_lookup(hour_angle) * (int)hand_point_start / TRIG_MAX_RATIO) + center.x,
    .y = (int)(-cos_lookup(hour_angle) * (int)hand_point_start / TRIG_MAX_RATIO) + center.y,
  };
  
  GPoint hour_hand_end = {
    .x = (int)(sin_lookup(hour_angle) * (int)(hand_point_end-25) / TRIG_MAX_RATIO) + center.x,
    .y = (int)(-cos_lookup(hour_angle) * (int)(hand_point_end-25) / TRIG_MAX_RATIO) + center.y,
  };   
  
  GPoint filler_start = {
    .x = (int)(sin_lookup(hour_angle) * (int)filler_point_start / TRIG_MAX_RATIO) + center.x,
    .y = (int)(-cos_lookup(hour_angle) * (int)filler_point_start / TRIG_MAX_RATIO) + center.y,
  };
  
  GPoint filler_end = {
    .x = (int)(sin_lookup(hour_angle) * (int)filler_point_end / TRIG_MAX_RATIO) + center.x,
    .y = (int)(-cos_lookup(hour_angle) * (int)filler_point_end / TRIG_MAX_RATIO) + center.y,
  }; 
  
  // set colors
  graphics_context_set_antialiased(ctx, true);
   
  // draw wide part of minute hand in background color for shadow
  graphics_context_set_stroke_color(ctx, BACKGROUND_COLOR);  
  graphics_context_set_stroke_width(ctx, 8);  
  graphics_draw_line(ctx, minute_hand_start, minute_hand_end);  
  
  // draw minute line
  graphics_context_set_stroke_color(ctx, FOREGROUND_COLOR);  
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, center, minute_hand_start);  
  
  // draw wide part of minute hand
   graphics_context_set_stroke_color(ctx, FOREGROUND_COLOR);
  graphics_context_set_stroke_width(ctx, 6);  
  graphics_draw_line(ctx, minute_hand_start, minute_hand_end);   
  
  // draw wide part of hour hand in background color for shadow
  graphics_context_set_stroke_color(ctx, BACKGROUND_COLOR);  
  graphics_context_set_stroke_width(ctx, 8);
  graphics_draw_line(ctx, hour_hand_start, hour_hand_end);  
  
  // draw small hour line
  graphics_context_set_stroke_color(ctx, FOREGROUND_COLOR); 
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, center, hour_hand_start);   
  
  // draw wide part of hour hand
  graphics_context_set_stroke_color(ctx, FOREGROUND_COLOR);  
  graphics_context_set_stroke_width(ctx, 6);
  graphics_draw_line(ctx, hour_hand_start, hour_hand_end);
  
  // draw inner hour line
  graphics_context_set_stroke_color(ctx, BACKGROUND_COLOR);  
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, filler_start, filler_end);    
  
  // circle overlay
  // draw circle in middle 
  graphics_context_set_fill_color(ctx, FOREGROUND_COLOR);  
  graphics_fill_circle(ctx, center, 3);
  
  // dot in the middle
  graphics_context_set_fill_color(ctx, BACKGROUND_COLOR);
  graphics_fill_circle(ctx, center, 1);    
}

//////////////////////
// load main window //
//////////////////////
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // set background color
  window_set_background_color(s_main_window, BACKGROUND_COLOR); // default GColorWhite
  
  // register button clicks
//   window_set_click_config_provider(window, click_config_provider);
  
  s_word_font = fonts_load_custom_font(resource_get_handle(WORD_FONT));
  s_number_font = fonts_load_custom_font(resource_get_handle(NUMBER_FONT));

  // create canvas layer for dial
  s_dial_layer = layer_create(bounds);
  layer_set_update_proc(s_dial_layer, dial_update_proc);
  layer_add_child(window_layer, s_dial_layer);  
  
  // create temp circle
  s_temp_circle = layer_create(bounds);
  layer_set_update_proc(s_temp_circle, temp_update_proc);
  layer_add_child(s_dial_layer, s_temp_circle);
  
  // create temp text
  s_temp_layer = text_layer_create(GRect(60, 19, 24, 16));
  text_layer_set_text_color(s_temp_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_temp_layer, GColorClear);
  text_layer_set_text_alignment(s_temp_layer, GTextAlignmentCenter);
  text_layer_set_font(s_temp_layer, s_number_font);
  layer_add_child(s_dial_layer, text_layer_get_layer(s_temp_layer));
  
  // create battery layer
  s_battery_circle = layer_create(bounds);
  layer_set_update_proc(s_battery_circle, battery_update_proc);
  layer_add_child(s_dial_layer, s_battery_circle);
  
  // charging icon
  s_charging_bitmap = gbitmap_create_with_resource(RESOURCE_ID_LIGHTENING_WHITE_ICON);
  s_charging_bitmap_layer = bitmap_layer_create(GRect(26, 76, 14, 14));
  bitmap_layer_set_compositing_mode(s_charging_bitmap_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_charging_bitmap_layer, s_charging_bitmap); 
  layer_add_child(s_dial_layer, bitmap_layer_get_layer(s_charging_bitmap_layer));    
  
  // bluetooth disconnected icon
  s_bluetooth_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_DISCONNECTED_WHITE_ICON);
  s_bluetooth_bitmap_layer = bitmap_layer_create(GRect(8, 76, 14, 14));
  bitmap_layer_set_compositing_mode(s_bluetooth_bitmap_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_bluetooth_bitmap_layer, s_bluetooth_bitmap); 
  layer_add_child(s_dial_layer, bitmap_layer_get_layer(s_bluetooth_bitmap_layer));       
  
  // create health layer text
  s_health_layer = text_layer_create(GRect(54, 115, 36, 16));
  text_layer_set_text_color(s_health_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_health_layer, GColorClear);
  text_layer_set_text_alignment(s_health_layer, GTextAlignmentCenter);
  text_layer_set_font(s_health_layer, s_number_font);
  layer_add_child(s_dial_layer, text_layer_get_layer(s_health_layer));  
  
  // create health layer circle
  s_health_circle = layer_create(bounds);
  layer_set_update_proc(s_health_circle, health_update_proc);
  layer_add_child(s_dial_layer, s_health_circle);
    
  // create shoe icon
  s_health_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SHOE_WHITE_ICON);
  s_health_bitmap_layer = bitmap_layer_create(GRect(60, 131, 24, 16));
  bitmap_layer_set_compositing_mode(s_health_bitmap_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_health_bitmap_layer, s_health_bitmap); 
  layer_add_child(s_dial_layer, bitmap_layer_get_layer(s_health_bitmap_layer));
  
  // Day Text
  s_day_text_layer = text_layer_create(GRect(90, 75, 34, 14));
  text_layer_set_text_color(s_day_text_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_day_text_layer, GColorClear);
  text_layer_set_text_alignment(s_day_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_day_text_layer, s_word_font);
  layer_add_child(s_dial_layer, text_layer_get_layer(s_day_text_layer));
  
  // Date text
  s_date_text_layer = text_layer_create(GRect(124, 75, 17, 14));
  text_layer_set_text_color(s_date_text_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_date_text_layer, GColorClear);
  text_layer_set_text_alignment(s_date_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_date_text_layer, s_number_font);
  layer_add_child(s_dial_layer, text_layer_get_layer(s_date_text_layer));  
  
  // create canvas layer for hands
  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, ticks_update_proc);
  layer_add_child(window_layer, s_hands_layer);
}

///////////////////////
// update clock time //
///////////////////////
static void update_time() {
  // get a tm strucutre
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // write date to buffer
  static char date_buffer[32];
  strftime(date_buffer, sizeof(date_buffer), "%e", tick_time);
  
  // write day to buffer
  static char day_buffer[32];
  char *weekday = day_buffer;
  strftime(day_buffer, sizeof(day_buffer), "%w", tick_time);
  
  // make date all caps
  if(strcmp(day_buffer, "0")==0) {
    weekday = "SUN";
  } else if(strcmp(day_buffer, "1")==0) {
    weekday = "MON";
  } else if(strcmp(day_buffer, "2")==0) {
    weekday = "TUE";
  } else if(strcmp(day_buffer, "3")==0) {
    weekday = "WED";
  } else if(strcmp(day_buffer, "4")==0) {
    weekday = "THU";
  } else if(strcmp(day_buffer, "5")==0) {
    weekday = "FRI";
  } else if(strcmp(day_buffer, "6")==0) {
    weekday = "SAT";
  } else {
    weekday = "";
  }
  
  // display this time on the text layer
  text_layer_set_text(s_date_text_layer, date_buffer+(('0' == date_buffer[0]?1:0))); // remove padding
  text_layer_set_text(s_day_text_layer, weekday); 
}

//////////////////
// handle ticks //
//////////////////
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_hands_layer);
  update_time();
  
  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) {
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
  
    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);
  
    // Send the message!
    app_message_outbox_send();
  }  
}

/////////////////////////////////////
// registers battery update events //
/////////////////////////////////////
static void battery_handler(BatteryChargeState charge_state) {
  battery_percent = charge_state.charge_percent;
  if(charge_state.is_charging || charge_state.is_plugged) {
    charging = true;
  } else {
    charging = false;
  }
  // force update to circle
  layer_mark_dirty(s_battery_circle);
}

/////////////////////////////
// manage bluetooth status //
/////////////////////////////
static void bluetooth_callback(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(s_bluetooth_bitmap_layer), connected);
  if(!connected) {
    vibes_double_pulse();
  }
}

// registers health update events
static void health_handler(HealthEventType event, void *context) {
  if(event==HealthEventMovementUpdate) {
    step_count = (double)health_service_sum_today(HealthMetricStepCount);
    // write to char_current_steps variable
    static char health_buf[32];
    if(step_count>=1000) {
      double s_c = step_count/1000;
      snprintf(health_buf, sizeof(health_buf), "%dk", (int)s_c);
    } else {
      snprintf(health_buf, sizeof(health_buf), "%d", (int)step_count);
    }
    
    char_current_steps = health_buf;
    text_layer_set_text(s_health_layer, char_current_steps);
    
    // force update to circle
    layer_mark_dirty(s_health_circle);
    
    APP_LOG(APP_LOG_LEVEL_INFO, "health_handler completed");
  }
}

///////////////////
// unload window //
///////////////////
static void main_window_unload(Window *window) {
  layer_destroy(s_dial_layer);
  layer_destroy(s_hands_layer);
  layer_destroy(s_temp_circle);
  layer_destroy(s_battery_circle);
  layer_destroy(s_health_circle);
  
  text_layer_destroy(s_temp_layer);
  text_layer_destroy(s_health_layer);
  text_layer_destroy(s_day_text_layer);
  text_layer_destroy(s_date_text_layer);  
  
  gbitmap_destroy(s_weather_bitmap);
  gbitmap_destroy(s_health_bitmap);
  gbitmap_destroy(s_bluetooth_bitmap);
  gbitmap_destroy(s_charging_bitmap);
  
  bitmap_layer_destroy(s_weather_bitmap_layer);
  bitmap_layer_destroy(s_health_bitmap_layer);
  bitmap_layer_destroy(s_bluetooth_bitmap_layer);
  bitmap_layer_destroy(s_charging_bitmap_layer);
}

//////////////////////////////////////
// display appropriate weather icon //
//////////////////////////////////////
static void load_icons() {
  // populate icon variable
    if(strcmp(icon_buf, "clear-day")==0 || 
       strcmp(icon_buf, "01d")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLEAR_SKY_DAY_WHITE_ICON);  
    } else if(strcmp(icon_buf, "clear-night")==0 || 
              strcmp(icon_buf, "01n")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLEAR_SKY_NIGHT_WHITE_ICON);
    }else if(strcmp(icon_buf, "rain")==0 ||
             strcmp(icon_buf, "09d")==0 || 
             strcmp(icon_buf, "09n")==0 || 
             strcmp(icon_buf, "10d")==0 || 
             strcmp(icon_buf, "10n")==0 || 
             strcmp(icon_buf, "50d")==0 || 
             strcmp(icon_buf, "50n")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_RAIN_WHITE_ICON);
    } else if(strcmp(icon_buf, "snow")==0 || 
              strcmp(icon_buf, "11d")==0 || 
              strcmp(icon_buf, "11n")==0 || 
              strcmp(icon_buf, "13d")==0 || 
              strcmp(icon_buf, "13n")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SNOW_WHITE_ICON);
    } else if(strcmp(icon_buf, "sleet")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SLEET_WHITE_ICON);
    } else if(strcmp(icon_buf, "wind")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WIND_WHITE_ICON);
    } else if(strcmp(icon_buf, "fog")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_FOG_WHITE_ICON);
    } else if(strcmp(icon_buf, "cloudy")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLOUDY_WHITE_ICON);
    } else if(strcmp(icon_buf, "partly-cloudy-day")==0 || 
              strcmp(icon_buf, "02d")==0 || 
              strcmp(icon_buf, "03d")==0 || 
              strcmp(icon_buf, "04d")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PARTLY_CLOUDY_DAY_WHITE_ICON);
    } else if(strcmp(icon_buf, "partly-cloudy-night")==0 || 
              strcmp(icon_buf, "02n")==0 || 
              strcmp(icon_buf, "03n")==0 || 
              strcmp(icon_buf, "04n")==0) {
      s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PARTLY_CLOUDY_NIGHT_WHITE_ICON);
    }  
//   // populate icon variable
//     if(strcmp(icon_buf, "clear-day")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLEAR_SKY_DAY_WHITE_ICON);  
//     } else if(strcmp(icon_buf, "clear-night")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLEAR_SKY_NIGHT_WHITE_ICON);
//     }else if(strcmp(icon_buf, "rain")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_RAIN_WHITE_ICON);
//     } else if(strcmp(icon_buf, "snow")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SNOW_WHITE_ICON);
//     } else if(strcmp(icon_buf, "sleet")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_SLEET_WHITE_ICON);
//     } else if(strcmp(icon_buf, "wind")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WIND_WHITE_ICON);
//     } else if(strcmp(icon_buf, "fog")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_FOG_WHITE_ICON);
//     } else if(strcmp(icon_buf, "cloudy")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_CLOUDY_WHITE_ICON);
//     } else if(strcmp(icon_buf, "partly-cloudy-day")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PARTLY_CLOUDY_DAY_WHITE_ICON);
//     } else if(strcmp(icon_buf, "partly-cloudy-night")==0) {
//       s_weather_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PARTLY_CLOUDY_NIGHT_WHITE_ICON);
//     }
  // populate weather icon
  if(s_weather_bitmap_layer) {
    bitmap_layer_destroy(s_weather_bitmap_layer);
  }
  s_weather_bitmap_layer = bitmap_layer_create(GRect(60, 35, 24, 16));
  bitmap_layer_set_compositing_mode(s_weather_bitmap_layer, GCompOpSet);  
  bitmap_layer_set_bitmap(s_weather_bitmap_layer, s_weather_bitmap); 
  layer_add_child(s_dial_layer, bitmap_layer_get_layer(s_weather_bitmap_layer)); 
}

///////////////////
// weather calls //
///////////////////
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temp_buf[64];

  // Read tuples for data
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_KEY_TEMP);
  Tuple *icon_tuple = dict_find(iterator, MESSAGE_KEY_KEY_ICON);  

  // If all data is available, use it
  if(temp_tuple && icon_tuple) {
    
    // temp
    snprintf(temp_buf, sizeof(temp_buf), "%d°", (int)temp_tuple->value->int32);
    text_layer_set_text(s_temp_layer, temp_buf);

    // icon
    snprintf(icon_buf, sizeof(icon_buf), "%s", icon_tuple->value->cstring);
  }  
  
  load_icons();
  APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_callback");
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

////////////////////
// initialize app //
////////////////////
static void init() {
  s_main_window = window_create();
  
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  // show window on the watch with animated=true
  window_stack_push(s_main_window, true);
  
  // subscribe to time events
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Make sure the time is displayed from the start
  update_time();
  
//   // init hand paths
//   s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
//   s_minute_filler = gpath_create(&MINUTE_HAND_FILLER);
//   s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);
//   s_hour_filler = gpath_create(&HOUR_HAND_FILLER);
    
  // subscribe to health events 
  health_service_events_subscribe(health_handler, NULL); 
  // force initial update
  health_handler(HealthEventMovementUpdate, NULL);   
    
  // register with Battery State Service
  battery_state_service_subscribe(battery_handler);
  // force initial update
  battery_handler(battery_state_service_peek());      
  
  // register with bluetooth state service
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  bluetooth_callback(connection_service_peek_pebble_app_connection());  
  
  // register for taps
//   accel_tap_service_subscribe(tap_handler);
  
  // Register weather callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);  
  
  // Open AppMessage for weather callbacks
  const int inbox_size = 128;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Clock show_clock_window");  
}

///////////////////////
// de-initialize app //
///////////////////////
static void deinit() {
  window_destroy(s_main_window);
}

/////////////
// run app //
/////////////
int main(void) {
  init();
  app_event_loop();
  deinit();
}