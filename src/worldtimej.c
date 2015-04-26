#include <pebble.h>
#include "PWTimeKeys.h"

static GFont big_bold_font;
static GFont med_bold_font;
static GFont small_bold_font;

static AppSync sync;
static uint8_t sync_buffer[512];

#define MAX_WATCH_FACES		3               // How many timezones do we support?
#define MAX_TEMPERATURE_LEN 16

#define MINUTES_BETWEEN_WEATHER_UPDATES 30  // How often (in minutes) do we ask for a weather update?

typedef enum DayOffset {
    PREVDAY,
    SAMEDAY,
    NEXTDAY
} DayOffsetType;

typedef struct {
    Window      *window;
	int          background;
	int          display;
	int32_t      gmt_sec_offset;
	int          last_day;
	int          last_month;
	int          last_year;
	time_t       last_weather_update;
	int          temp;
	int          hi_temp[MAX_WEATHER_DAYS];
	int          lo_temp[MAX_WEATHER_DAYS];
	uint8_t      sunrise_hour;
	uint8_t      sunrise_min;
	uint8_t      sunset_hour;
	uint8_t      sunset_min;
	TextLayer   *main_time_layer;
	TextLayer   *text_time_layer;
	char         time_text[10];
	TextLayer   *main_city_layer;
	TextLayer   *text_city_layer;
	TextLayer   *text_date_layer;
	char         date_text[18];
	char         time_format[12];
	BitmapLayer *bitmap_weather_layer[MAX_WEATHER_DAYS];
	TextLayer   *text_temp_layer[MAX_WEATHER_DAYS];
    char         temps[MAX_WEATHER_DAYS][MAX_TEMPERATURE_LEN];
} WatchFace;

typedef struct {
    TextLayer   *version;
    char         version_text[24];
    TextLayer   *batterystatus;
    char         batt_text[20];
    TextLayer   *weatherupdate[MAX_WATCH_FACES];
    char         last_text[MAX_WATCH_FACES][20];
} Status;

static Window     *mainwindow;
static WatchFace   watchfaces[MAX_WATCH_FACES];
static Window     *statuswindow;
static Status      status;
static GBitmap    *conditions[MAX_WEATHER_CONDITIONS];

AppTimer          *statuswindow_timer;

int current_window = 0;
int temp_display   = 0;
static char time_12h_format[9] = "%I:%M %p";
static char time_24h_format[6] = "%Hh%M";

uint8_t weather[] = {(uint8_t)WEATHER_UNKNOWN, (uint8_t)WEATHER_UNKNOWN, (uint8_t)WEATHER_UNKNOWN,
                     (uint8_t)-99, (uint8_t)-10, (uint8_t)0, (uint8_t)100,
                                   (uint8_t)-11, (uint8_t)1, (uint8_t)101, 
                     (uint8_t)0, (uint8_t)0, (uint8_t)12, (uint8_t)0 };

static void request_update_from_phone(void) {
    Tuplet value = TupletInteger(1, 1);
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    if (iter == NULL) {
        return;
    }
    dict_write_tuplet(iter, &value);
    dict_write_end(iter);
    app_message_outbox_send();
}

bool is_sunrise (struct tm *local_time, WatchFace *wf) {
    return ((local_time->tm_hour == wf->sunrise_hour ) &&
            (local_time->tm_min  == wf->sunrise_min  )    );
}

bool is_sunset (struct tm *local_time, WatchFace *wf) {
    return ((local_time->tm_hour == wf->sunset_hour  ) &&
            (local_time->tm_min  == wf->sunset_min   )    );
}

bool sunisup ( struct tm *local_time, int sunrise_hour, int sunrise_min,
               int sunset_hour, int sunset_min ) {
    return (((local_time->tm_hour > sunrise_hour)   ||
            ((local_time->tm_hour == sunrise_hour) &&
             (local_time->tm_min >= sunrise_min)      ) )   
           && 
            ((local_time->tm_hour < sunset_hour)   ||
            ((local_time->tm_hour == sunset_hour) &&
             (local_time->tm_min < sunset_min)       )  )  );
}

void update_temps(WatchFace *wf) {
    static char now_single_temp_format[10]   = "%d\nNow";
    static char high_single_temp_format[10]  = "%d\nHigh";
    static char low_single_temp_format[10]   = "%d\nLow";
    static char dual_temp_format[10]         = "%d\n%d";

    for (int i=0; i<MAX_WEATHER_DAYS; i++) {
        if (i == 0) {
            switch(temp_display) {
                case 0:
                default:
                    snprintf(wf->temps[i], MAX_TEMPERATURE_LEN, now_single_temp_format,
                             wf->temp);
                    break;
                case 1:
                    snprintf(wf->temps[i], MAX_TEMPERATURE_LEN, high_single_temp_format,
                             wf->hi_temp[i]);
                    break;
                case 2:
                    snprintf(wf->temps[i], MAX_TEMPERATURE_LEN, low_single_temp_format,
                             wf->lo_temp[i]);
                    break;
            }
        } else {
            snprintf(wf->temps[i], MAX_TEMPERATURE_LEN, dual_temp_format,
                     wf->hi_temp[i], wf->lo_temp[i]);
        }
        layer_mark_dirty((Layer *)wf->text_temp_layer[i]);
    }
}

void update_background(WatchFace *wf, int32_t local_gmt_offset) {

    GColor text_color;
    GColor bg_color;
    
    time_t time_in_secs = time(NULL);
    time_in_secs = (time_in_secs - local_gmt_offset) + wf->gmt_sec_offset;
    struct tm *local_time = localtime(&time_in_secs);

    switch (wf->background) {
        case BACKGROUND_DARK:
#ifdef PBL_COLOR
            text_color = GColorYellow;
            bg_color = GColorBlue;
#else
            text_color = GColorWhite;
            bg_color = GColorBlack;
#endif
            break;
        case BACKGROUND_LIGHT:
#ifdef PBL_COLOR
            text_color = GColorBlue;
            bg_color = GColorYellow;
#else
            text_color = GColorBlack;
            bg_color = GColorWhite;
#endif
            break;
        case BACKGROUND_SUNS:
            if (sunisup(local_time, wf->sunrise_hour, wf->sunrise_min,
                        wf->sunset_hour, wf->sunset_min)) {
#ifdef PBL_COLOR
                text_color = GColorBlue;
                bg_color = GColorYellow;
#else
                text_color = GColorBlack;
                bg_color = GColorWhite;
#endif
            } else {
#ifdef PBL_COLOR
                text_color = GColorYellow;
                bg_color = GColorBlue;
#else
                text_color = GColorWhite;
                bg_color = GColorBlack;
#endif
            }
            break;
        default:
#ifdef PBL_COLOR
            text_color = GColorYellow;
            bg_color = GColorBlue;
#else
            text_color = GColorWhite;
            bg_color = GColorBlack;
#endif
            break;
    }
  
    text_layer_set_text_color(wf->main_time_layer, text_color);
    text_layer_set_background_color(wf->main_time_layer, bg_color);
    text_layer_set_text_color(wf->main_city_layer, text_color);
    text_layer_set_background_color(wf->main_city_layer, bg_color);
    
    window_set_background_color(wf->window, bg_color);
    text_layer_set_text_color(wf->text_time_layer, text_color);
    text_layer_set_text_color(wf->text_date_layer, text_color);
    text_layer_set_text_color(wf->text_city_layer, text_color);
    
    // update each of the weather icon spaces
    for ( int j = 0; j < MAX_WEATHER_DAYS; j++ ) {
        if (GColorEq(bg_color, GColorWhite)) {
            bitmap_layer_set_compositing_mode(wf->bitmap_weather_layer[j], GCompOpAssign);
        } else {
            bitmap_layer_set_compositing_mode(wf->bitmap_weather_layer[j], GCompOpAssignInverted);
        }
        layer_mark_dirty((Layer *)wf->bitmap_weather_layer[j]);
        text_layer_set_text_color(wf->text_temp_layer[j], text_color);
        layer_mark_dirty((Layer *)wf->text_temp_layer[j]);
    }
    layer_mark_dirty((Layer *)wf->main_time_layer);
    layer_mark_dirty((Layer *)wf->main_city_layer);
    layer_mark_dirty((Layer *)wf->text_time_layer);
    layer_mark_dirty((Layer *)wf->text_date_layer);
    layer_mark_dirty((Layer *)wf->text_city_layer);
}

void update_time(WatchFace *wf, int32_t local_gmt_offset) {

    static char date_format[14] = "%a %b %e, %Y";

    time_t time_in_secs = time(NULL);
    time_in_secs = (time_in_secs - local_gmt_offset) + wf->gmt_sec_offset;
    struct tm *local_time = localtime(&time_in_secs);

    strftime(wf->time_text, sizeof(wf->time_text), wf->time_format, local_time);
    if ((!clock_is_24h_style()) &&
        (wf->display != DISPLAY_24_HOUR_TIME) &&
        (wf->time_text[0] == '0')) {
        int pos = strlen(wf->time_text);
        for (int k = 0; k < pos; k++) {
            wf->time_text[k] = wf->time_text[k+1];
        }
    } 
    layer_mark_dirty((Layer *)wf->main_time_layer);
    layer_mark_dirty((Layer *)wf->text_time_layer);

    // Update date only if needed
    if ((wf->last_day   != local_time->tm_mday)  ||
        (wf->last_month != local_time->tm_mon)   ||
        (wf->last_year  != local_time->tm_year))    {
        strftime (wf->date_text, sizeof(wf->date_text), date_format, local_time);
        wf->last_day   = local_time->tm_mday;
        wf->last_month = local_time->tm_mon;
        wf->last_year  = local_time->tm_year;
        layer_mark_dirty((Layer *)wf->text_date_layer);
    }
    if (is_sunrise(local_time, wf) || is_sunset(local_time, wf)) {
        update_background(wf, local_gmt_offset);
    }
}

void update_watches() {
    for (int i=0; i<MAX_WATCH_FACES; i++) {
        update_time(&watchfaces[i], watchfaces[0].gmt_sec_offset);
    }
}

void handle_minute_tick(struct tm *t, TimeUnits units_changed) {
    static int minutes_since_last_update = 0;
    update_watches();
    
    // Every 30 minutes (MINUTES_BETWEEN_WEATHER_UPDATES) ask for a weather refresh
    if (minutes_since_last_update >= MINUTES_BETWEEN_WEATHER_UPDATES ) {
        request_update_from_phone();
        minutes_since_last_update = 0;
    } else {
        minutes_since_last_update++;
    }
}

// TODO: Error handling
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
    (void) dict_error;
    (void) app_message_error;
    (void) context;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "sync_error_callback, %d", app_message_error); 
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple,
                                        const Tuple* old_tuple, void* context) {
                                        
    bool temps_changed = false;
    bool suns_changed = false;
    uint32_t watch_num = key / KEYS_PER_WATCH;
    uint32_t function = key % KEYS_PER_WATCH;
    
    watchfaces[watch_num].last_weather_update = time(NULL);

    switch (function) {
        case PBCOMM_GMT_SEC_OFFSET_KEY:
            if (new_tuple->value->int32 != watchfaces[watch_num].gmt_sec_offset) {
                watchfaces[watch_num].gmt_sec_offset = new_tuple->value->int32;
                update_time(&watchfaces[watch_num], watchfaces[0].gmt_sec_offset);
                update_background(&watchfaces[watch_num], watchfaces[0].gmt_sec_offset);
            }
            if (watch_num == 0) {
                update_time(&watchfaces[1], watchfaces[0].gmt_sec_offset);
                update_background(&watchfaces[1], watchfaces[0].gmt_sec_offset);
                update_time(&watchfaces[2], watchfaces[0].gmt_sec_offset);
                update_background(&watchfaces[2], watchfaces[0].gmt_sec_offset);
            }
            break;
        case PBCOMM_CITY_KEY:
//          if (strcmp(new_tuple->value->cstring, old_tuple->value->cstring) != 0) {
                text_layer_set_text(watchfaces[watch_num].text_city_layer, new_tuple->value->cstring);
                text_layer_set_text(watchfaces[watch_num].main_city_layer, new_tuple->value->cstring);
                layer_mark_dirty((Layer *)watchfaces[watch_num].text_city_layer);
                layer_mark_dirty((Layer *)watchfaces[watch_num].main_city_layer);
//          }
            break;
        case PBCOMM_BACKGROUND_KEY:
            if (new_tuple->value->int8 != watchfaces[watch_num].background) {
                watchfaces[watch_num].background = new_tuple->value->int8;
                update_background(&watchfaces[watch_num], watchfaces[0].gmt_sec_offset);
            }
            break;
        case PBCOMM_12_24_DISPLAY_KEY:
            if (new_tuple->value->int8 != watchfaces[watch_num].display) {
                watchfaces[watch_num].display = new_tuple->value->int8;
                switch (watchfaces[watch_num].display) {
                case DISPLAY_WATCH_CONFIG_TIME:
                    if (clock_is_24h_style()) {
                        strncpy (watchfaces[watch_num].time_format, time_24h_format, sizeof(time_24h_format));
                    } else {
                        strncpy (watchfaces[watch_num].time_format, time_12h_format, sizeof(time_12h_format));
                    }
                    break;	  
                case DISPLAY_12_HOUR_TIME:
                    strncpy (watchfaces[watch_num].time_format, time_12h_format, sizeof(time_12h_format));
                    break;	  
                case DISPLAY_24_HOUR_TIME:
                default:
                    strncpy (watchfaces[watch_num].time_format, time_24h_format, sizeof(time_24h_format));
                    break;
                }	  
                update_time(&watchfaces[watch_num], watchfaces[0].gmt_sec_offset);
            }
            break;
        case PBCOMM_WEATHER_KEY:
            // Includes all weather information in a byte array
            for ( int j = 0; j < MAX_WEATHER_DAYS; j++ ) {
//              if (new_tuple->value->data[j+WEATHER_ICONS] != old_tuple->value->data[j+WEATHER_ICONS] ) {
                    if ((int8_t)new_tuple->value->data[j+WEATHER_ICONS] >= MAX_WEATHER_CONDITIONS) {
                        bitmap_layer_set_bitmap(watchfaces[watch_num].bitmap_weather_layer[j+WEATHER_ICONS],
                                                conditions[0]);
                    } else {
                        bitmap_layer_set_bitmap(watchfaces[watch_num].bitmap_weather_layer[j],
                                                conditions[(int8_t)new_tuple->value->data[j+WEATHER_ICONS]]);
                    }
                    layer_mark_dirty((Layer *)watchfaces[watch_num].bitmap_weather_layer[j]);
                    // temps_changed = true; not needed since the mark_dirty does what's needed
//              }
            }
            if ((int8_t)new_tuple->value->data[CURRENT_TEMP] != watchfaces[watch_num].temp) {
                watchfaces[watch_num].temp = (int8_t)new_tuple->value->data[CURRENT_TEMP];
                temps_changed = true;
            }
            for ( int j = 0; j < MAX_WEATHER_DAYS; j++ ) {
                if ((int8_t)new_tuple->value->data[j+MAX_TEMPS] != watchfaces[watch_num].hi_temp[j]) {
                    watchfaces[watch_num].hi_temp[j] = (int8_t)new_tuple->value->data[j+MAX_TEMPS];
                    temps_changed = true;
                }
            }
            for ( int j = 0; j < MAX_WEATHER_DAYS; j++ ) {
                if ((int8_t)new_tuple->value->data[j+MIN_TEMPS] != watchfaces[watch_num].lo_temp[j]) {
                    watchfaces[watch_num].lo_temp[j] = (int8_t)new_tuple->value->data[j+MIN_TEMPS];
                    temps_changed = true;
                }
            }
            if (temps_changed) {
                update_temps(&watchfaces[watch_num]);
            }
            if ((int8_t)new_tuple->value->data[SUNRISE_HOUR] != watchfaces[watch_num].sunrise_hour) {
                watchfaces[watch_num].sunrise_hour = (int8_t)new_tuple->value->data[SUNRISE_HOUR];
                suns_changed = true;
            }
            if ((int8_t)new_tuple->value->data[SUNRISE_MINUTE] != watchfaces[watch_num].sunrise_min) {
                watchfaces[watch_num].sunrise_min = (int8_t)new_tuple->value->data[SUNRISE_MINUTE];
                suns_changed = true;
            }
            if ((int8_t)new_tuple->value->data[SUNSET_HOUR] != watchfaces[watch_num].sunset_hour) {
                watchfaces[watch_num].sunset_hour = (int8_t)new_tuple->value->data[SUNSET_HOUR];
                suns_changed = true;
            }
            if ((int8_t)new_tuple->value->data[SUNSET_MINUTE] != watchfaces[watch_num].sunset_min) {
                watchfaces[watch_num].sunset_min = (int8_t)new_tuple->value->data[SUNSET_MINUTE];
                suns_changed = true;
            }
            if (suns_changed) {
                update_background(&watchfaces[watch_num], watchfaces[0].gmt_sec_offset);
            }
            break;
        default:
            break;
    }
}

/*
 * up_single_click_handler loads the next watchface, wrapping to the main window
 */
void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    switch (current_window) {
        case 0:
        case 1:
        case 2:
            window_stack_push(watchfaces[current_window].window, true);
            current_window++;
            break;
        case 3:
            for (int i = 0; i<MAX_WATCH_FACES; i++) {
                window_stack_pop(true);
            }
            current_window = 0;
            break;
        default:
            APP_LOG(APP_LOG_LEVEL_DEBUG, "up_single_click_handler: bad current_window: %d",
                    current_window);
            window_stack_pop_all(true);
            window_stack_push(mainwindow, true);
            current_window = 0;
            break;
    }
}

/*
 * On the main window only, this displays the update screen
 */
void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
    window_stack_push(statuswindow, true);
}
 
/*
 * On the main window only, this forces a refresh of data
 */
void select_refresh_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    request_update_from_phone();
}

/*
 * On the main window only, this forces a dump of watchface[i] data
 */
void select_dump_long_click_handler(ClickRecognizerRef recognizer, void *context) {
    for (int i=0; i < MAX_WATCH_FACES; i++) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "\n\nData for watch: %d\n", i);
        APP_LOG(APP_LOG_LEVEL_DEBUG,
            "gmt_sec_offset: %d\nbackground: %d\ndisplay: %d\ntemp: %d\n",
            (int)watchfaces[i].gmt_sec_offset, watchfaces[i].background, watchfaces[i].display,
            watchfaces[i].temp );
        for (int j=0; j < MAX_WEATHER_DAYS; j++) {
            APP_LOG(APP_LOG_LEVEL_DEBUG,
                "hi_temp[%d]: %d, lo_temp[%d]: %d\n",
                j, watchfaces[i].hi_temp[j], j, watchfaces[i].lo_temp[j] );
        }
        APP_LOG(APP_LOG_LEVEL_DEBUG,
            "sunrise_hour: %d\nsunrise_min: %d\nsunset_hour: %d\nsunset_min: %d\n",
            watchfaces[i].sunrise_hour, watchfaces[i].sunrise_min,
            watchfaces[i].sunset_hour, watchfaces[i].sunset_min );
        APP_LOG(APP_LOG_LEVEL_DEBUG,
            "time_text: %s\ntime_format: %s\ndate_text: %s\n",
            watchfaces[i].time_text, watchfaces[i].time_format, watchfaces[i].date_text);
    }
}

void statuswindow_timeout(void *callback_data) {
    
    app_timer_cancel(statuswindow_timer);
    statuswindow_timer = NULL;
    window_stack_pop(true);
    
}

void statuswindow_load () {

    BatteryChargeState batteryUpdate;
    struct tm *local_time;
    static char versionfmt[] = "%s";
    static char version[] = "worldtimej\n(JWT) v2.0";
    static char battfmt[] = "Charge: %3d%%";
    static char lastfmt[] = "%m/%d, %I:%M"; 

    statuswindow_timer = app_timer_register((uint32_t)7500, statuswindow_timeout, NULL);
    
    batteryUpdate = battery_state_service_peek();
    
    // Print the version
    status.version = text_layer_create(GRect(0,0,144,40));
    snprintf(status.version_text, sizeof(status.version_text), versionfmt, version);
    text_layer_set_text(status.version, status.version_text);
    text_layer_set_text_color(status.version, GColorWhite);
    text_layer_set_background_color(status.version, GColorClear);
    text_layer_set_font(status.version, small_bold_font);
    text_layer_set_text_alignment(status.version, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(statuswindow), (Layer *)status.version);
    layer_mark_dirty((Layer *)status.version);
    

    // Format battery percentage text
    status.batterystatus = text_layer_create(GRect(0, 40, 144, 32));
    snprintf(status.batt_text, sizeof(status.batt_text), battfmt, batteryUpdate.charge_percent);
    text_layer_set_text(status.batterystatus, status.batt_text);
    text_layer_set_text_color(status.batterystatus, GColorWhite);
    text_layer_set_background_color(status.batterystatus, GColorClear);
    text_layer_set_font(status.batterystatus, med_bold_font);
    text_layer_set_text_alignment(status.batterystatus, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(statuswindow), (Layer *)status.batterystatus);
    layer_mark_dirty((Layer *)status.batterystatus);
    
    // Format last weather update time for each watchface
    for (int i = 0; i < MAX_WATCH_FACES; i++ ) {
        status.weatherupdate[i] = text_layer_create(GRect(0,(i*20)+72,144,20));
        local_time = localtime(&watchfaces[i].last_weather_update);
        strftime(status.last_text[i], sizeof(status.last_text[i]), lastfmt, local_time);
        text_layer_set_text(status.weatherupdate[i], status.last_text[i]);
        text_layer_set_text_color(status.weatherupdate[i], GColorWhite);
        text_layer_set_background_color(status.weatherupdate[i], GColorClear);
        text_layer_set_font(status.weatherupdate[i], small_bold_font);
        text_layer_set_text_alignment(status.weatherupdate[i], GTextAlignmentCenter);
        layer_add_child(window_get_root_layer(statuswindow), (Layer *)status.weatherupdate[i]);
        layer_mark_dirty((Layer *)status.weatherupdate[i]);
    }
    
    // Format other stuff???

}

void statuswindow_unload () {

    if (statuswindow_timer != NULL) {
        app_timer_cancel(statuswindow_timer);
        statuswindow_timer = NULL;
    }

    // Deallocate version layer
    text_layer_destroy(status.version);

    // Deallocate text layer for battery percentage
    text_layer_destroy(status.batterystatus);
    
    // Deallocate text layers for last weather update time
    for (int i=0; i<MAX_WATCH_FACES; i++ ) {
        text_layer_destroy(status.weatherupdate[i]);
    }
    
}

/*
 * When on a detail window, this will cycle between displaying:
 *
 *  Today   Tomorrow  Next Day
 *  -----   --------  --------
 * current, high/low, high/low
 *   high,    high,     high
 *   low,     low,      low
 */
void select_temp_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    temp_display = temp_display < 2 ? temp_display + 1 : 0;
    update_temps(&watchfaces[current_window-1]);
}

/*
 * down_single_click_handler removes the current watchface, to the main window, and then
 * rotates to the last watchface
 */
void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
    switch (current_window) {
        case 0:
            for (int i = 0; i < MAX_WATCH_FACES; i ++) {
                window_stack_push(watchfaces[i].window, true);
            }
            current_window=3;
            break;
        case 1:
        case 2:
        case 3:
            window_stack_pop(true);
            current_window--;
            break;
        default:
            APP_LOG(APP_LOG_LEVEL_DEBUG, "up_single_click_handler: bad current_window: %d",
                    current_window);
            window_stack_pop_all(true);
            window_stack_push(mainwindow, true);
            current_window = 0;
            break;
    }
}

void mainwindow_click_config_provider(Window *window) {
    window_single_click_subscribe(BUTTON_ID_UP,        up_single_click_handler);
    window_long_click_subscribe(BUTTON_ID_UP, 500,     up_long_click_handler, NULL);
    window_single_click_subscribe(BUTTON_ID_SELECT,    select_refresh_single_click_handler);
    window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_dump_long_click_handler, NULL);
    window_single_click_subscribe(BUTTON_ID_DOWN,      down_single_click_handler);
}

void watchface_click_config_provider(Window *window) {
    window_single_click_subscribe(BUTTON_ID_UP,     up_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_temp_single_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN,   down_single_click_handler);
}

void init() {

    big_bold_font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
    med_bold_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    small_bold_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  
    conditions[WEATHER_UNKNOWN] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_UNKNOWN);
    conditions[WEATHER_CLEAR_DAY] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_CLEAR_DAY);
    conditions[WEATHER_CLEAR_NIGHT] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_CLEAR_NIGHT);
    conditions[WEATHER_RAIN] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_RAIN);
    conditions[WEATHER_SNOW] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_SNOW);
//    conditions[WEATHER_SLEET] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_SLEET);
    conditions[WEATHER_WIND] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WIND);
    conditions[WEATHER_FOG] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_FOG);
    conditions[WEATHER_CLOUDY] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_CLOUDY);
    conditions[WEATHER_PARTLY_CLOUDY_DAY] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_PARTLY_CLOUDY_DAY);
    conditions[WEATHER_PARTLY_CLOUDY_NIGHT] = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_PARTLY_CLOUDY_NIGHT);
  
    // Set up main window
    mainwindow = window_create();
    window_set_background_color(mainwindow, GColorBlack);
    window_set_fullscreen(mainwindow, true);
  
    for ( int i = 0; i < MAX_WATCH_FACES; i++) {
  
        // This layer displays the time. 12-hour, 24-hour or how the watch is configured
        watchfaces[i].main_time_layer = text_layer_create(GRect(0, i*56, 144, 32));
        text_layer_set_text(watchfaces[i].main_time_layer, watchfaces[i].time_text);
        text_layer_set_text_color(watchfaces[i].main_time_layer, GColorWhite);
        text_layer_set_background_color(watchfaces[i].main_time_layer, GColorClear);
        text_layer_set_font(watchfaces[i].main_time_layer, big_bold_font);
        text_layer_set_text_alignment(watchfaces[i].main_time_layer, GTextAlignmentCenter);
        layer_add_child(window_get_root_layer(mainwindow), (Layer *)watchfaces[i].main_time_layer);

        // This layer displays the selected time zone city, including GMT offset
        watchfaces[i].main_city_layer = text_layer_create(GRect(0, (i*56)+32, 144, 24));
        text_layer_set_text_color(watchfaces[i].main_city_layer, GColorWhite);
        text_layer_set_background_color(watchfaces[i].main_city_layer, GColorClear);
        text_layer_set_font(watchfaces[i].main_city_layer, small_bold_font);
        text_layer_set_text_alignment(watchfaces[i].main_city_layer, GTextAlignmentCenter);
        layer_add_child(window_get_root_layer(mainwindow), (Layer *)watchfaces[i].main_city_layer);
    
        watchfaces[i].window = window_create();  
        window_set_background_color(watchfaces[i].window, GColorBlack);
        window_set_fullscreen(watchfaces[i].window, true);
        window_set_click_config_provider(watchfaces[i].window,
                                         (ClickConfigProvider) watchface_click_config_provider);

        // This layer displays the time. 12-hour, 24-hour or how the watch is configured
        watchfaces[i].text_time_layer = text_layer_create(GRect(0, 6, 144, 44));
        text_layer_set_text(watchfaces[i].text_time_layer, watchfaces[i].time_text);
        text_layer_set_text_color(watchfaces[i].text_time_layer, GColorWhite);
        text_layer_set_background_color(watchfaces[i].text_time_layer, GColorClear);
        text_layer_set_font(watchfaces[i].text_time_layer, big_bold_font);
        text_layer_set_text_alignment(watchfaces[i].text_time_layer, GTextAlignmentCenter);
        layer_add_child(window_get_root_layer(watchfaces[i].window), (Layer *)watchfaces[i].text_time_layer);
    
        // This layer displays the date
        watchfaces[i].text_date_layer = text_layer_create(GRect(0, 44, 144, 28));
        text_layer_set_text(watchfaces[i].text_date_layer, watchfaces[i].date_text);
        text_layer_set_text_color(watchfaces[i].text_date_layer, GColorWhite);
        text_layer_set_background_color(watchfaces[i].text_date_layer, GColorClear);
        text_layer_set_font(watchfaces[i].text_date_layer, med_bold_font);
        text_layer_set_text_alignment(watchfaces[i].text_date_layer, GTextAlignmentCenter);
        layer_add_child(window_get_root_layer(watchfaces[i].window), (Layer *)watchfaces[i].text_date_layer);
    
        // This layer displays the selected time zone city, including GMT offset
        watchfaces[i].text_city_layer = text_layer_create(GRect(0, 72, 146, 22));
        text_layer_set_text_color(watchfaces[i].text_city_layer, GColorWhite);
        text_layer_set_background_color(watchfaces[i].text_city_layer, GColorClear);
        text_layer_set_font(watchfaces[i].text_city_layer, small_bold_font);
        text_layer_set_text_alignment(watchfaces[i].text_city_layer, GTextAlignmentCenter);
        layer_add_child(window_get_root_layer(watchfaces[i].window), (Layer *)watchfaces[i].text_city_layer);
    
        for ( int j = 0; j < MAX_WEATHER_DAYS; j++ ) {
            // This layer displays a weather image, if available.
            watchfaces[i].bitmap_weather_layer[j] = bitmap_layer_create(GRect(9+(j*(36+9)), 94, 36, 36));
            bitmap_layer_set_background_color(watchfaces[i].bitmap_weather_layer[j], GColorClear);
            bitmap_layer_set_alignment(watchfaces[i].bitmap_weather_layer[j], GAlignCenter);
            bitmap_layer_set_compositing_mode(watchfaces[i].bitmap_weather_layer[j], GCompOpAssignInverted);
            bitmap_layer_set_bitmap(watchfaces[i].bitmap_weather_layer[j], conditions[WEATHER_UNKNOWN]);
            layer_add_child(window_get_root_layer(watchfaces[i].window), (Layer *)watchfaces[i].bitmap_weather_layer[j]);

            // This layer displays the time zone temperature, high and low, below the weather icon
            watchfaces[i].text_temp_layer[j] = text_layer_create(GRect(9+(j*(36+9)), 130, 36, 38));
            text_layer_set_text(watchfaces[i].text_temp_layer[j], watchfaces[i].temps[j]);
            text_layer_set_text_color(watchfaces[i].text_temp_layer[j], GColorWhite);
            text_layer_set_background_color(watchfaces[i].text_temp_layer[j], GColorClear);
            text_layer_set_font(watchfaces[i].text_temp_layer[j], small_bold_font);
            text_layer_set_text_alignment(watchfaces[i].text_temp_layer[j], GTextAlignmentCenter);
            layer_add_child(window_get_root_layer(watchfaces[i].window), (Layer *)watchfaces[i].text_temp_layer[j]);
        }
    }
    Tuplet initial_values[] = {
        TupletInteger(LOCAL_WATCH_OFFSET+PBCOMM_GMT_SEC_OFFSET_KEY, (int32_t) -28800),
        TupletInteger(LOCAL_WATCH_OFFSET+PBCOMM_BACKGROUND_KEY,     (uint8_t) BACKGROUND_SUNS),
        TupletInteger(LOCAL_WATCH_OFFSET+PBCOMM_12_24_DISPLAY_KEY,  (uint8_t) DISPLAY_WATCH_CONFIG_TIME),
        TupletCString(LOCAL_WATCH_OFFSET+PBCOMM_CITY_KEY,                     "Watch Time"),
        TupletBytes(  LOCAL_WATCH_OFFSET+PBCOMM_WEATHER_KEY,        weather,  WEATHER_KEY_LEN),
          
        TupletInteger(TZ1_WATCH_OFFSET+PBCOMM_GMT_SEC_OFFSET_KEY,   (int32_t) 0),
        TupletInteger(TZ1_WATCH_OFFSET+PBCOMM_BACKGROUND_KEY,       (uint8_t) BACKGROUND_SUNS),
        TupletInteger(TZ1_WATCH_OFFSET+PBCOMM_12_24_DISPLAY_KEY,    (uint8_t) DISPLAY_24_HOUR_TIME),
        TupletCString(TZ1_WATCH_OFFSET+PBCOMM_CITY_KEY,                       "London, England"),
        TupletBytes(  TZ1_WATCH_OFFSET+PBCOMM_WEATHER_KEY,          weather,  WEATHER_KEY_LEN),
          
        TupletInteger(TZ2_WATCH_OFFSET+PBCOMM_GMT_SEC_OFFSET_KEY,   (int32_t) 32400),
        TupletInteger(TZ2_WATCH_OFFSET+PBCOMM_BACKGROUND_KEY,       (uint8_t) BACKGROUND_SUNS),
        TupletInteger(TZ2_WATCH_OFFSET+PBCOMM_12_24_DISPLAY_KEY,    (uint8_t) DISPLAY_24_HOUR_TIME),
        TupletCString(TZ2_WATCH_OFFSET+PBCOMM_CITY_KEY,                       "Tokyo, Japan"),
        TupletBytes(  TZ2_WATCH_OFFSET+PBCOMM_WEATHER_KEY,          weather,  WEATHER_KEY_LEN)
    };
    
    // Initialize watchfaces[].time_format as it may be used before sync_tuple_changed_callback
    // is executed to set up the proper value.
    for (int i = 0; i < MAX_WATCH_FACES; i++) {
        strncpy (watchfaces[i].time_format, time_12h_format, sizeof(time_12h_format));
    }
    
    // Initialize status window, but don't populate unless it's requested via tap
    statuswindow = window_create();
    window_set_background_color(statuswindow, GColorBlack);
    window_set_window_handlers(statuswindow, (WindowHandlers) {
        .load = statuswindow_load,
        .unload = statuswindow_unload
    });
    statuswindow_timer = NULL;
    /* decide how to get this window, long click is already used */

    
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
                  sync_tuple_changed_callback, sync_error_callback, NULL);
    window_set_click_config_provider(mainwindow,
                                     (ClickConfigProvider) mainwindow_click_config_provider);
    window_stack_push(mainwindow, true /* Animated */);  
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    request_update_from_phone();  
}

void deinit() {
    app_sync_deinit(&sync);
    gbitmap_destroy(conditions[WEATHER_UNKNOWN]);
    gbitmap_destroy(conditions[WEATHER_CLEAR_DAY]);
    gbitmap_destroy(conditions[WEATHER_CLEAR_NIGHT]);
    gbitmap_destroy(conditions[WEATHER_RAIN]);
    gbitmap_destroy(conditions[WEATHER_SNOW]);
//    gbitmap_destroy(conditions[WEATHER_SLEET]);
    gbitmap_destroy(conditions[WEATHER_WIND]);
    gbitmap_destroy(conditions[WEATHER_FOG]);
    gbitmap_destroy(conditions[WEATHER_CLOUDY]);
    gbitmap_destroy(conditions[WEATHER_PARTLY_CLOUDY_DAY]);
    gbitmap_destroy(conditions[WEATHER_PARTLY_CLOUDY_NIGHT]);
   
    // Destroy layers and window    
    for ( int i = 0; i < MAX_WATCH_FACES; i++) {
        text_layer_destroy(watchfaces[i].main_time_layer);
        text_layer_destroy(watchfaces[i].main_city_layer);
        text_layer_destroy(watchfaces[i].text_time_layer);
        text_layer_destroy(watchfaces[i].text_city_layer);
        text_layer_destroy(watchfaces[i].text_date_layer);
        for (int j = 0; j < MAX_WEATHER_DAYS; j++ ) {
            bitmap_layer_destroy(watchfaces[i].bitmap_weather_layer[j]);
            text_layer_destroy(watchfaces[i].text_temp_layer[j]);
        }
        window_destroy(watchfaces[i].window);
    }
    window_destroy(statuswindow);
    window_destroy(mainwindow);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}

