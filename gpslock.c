#include "gpslock.h"
#include <privacy_privilege_manager.h>
#include <system_info.h>
#include <feedback.h>
#include <time.h>

typedef struct appdata {
	location_manager_h location;
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *nf;
	Evas_Object *ly;
	Evas_Object *box;
	Evas_Object *status;
	Evas_Object *time;
	Elm_Transit *transit;
} appdata_s;
static Ecore_Animator *anim;
static struct stopwatch_info {
	int running;
	long long time_ref;
	long long time_lap_diff;
	long long time_elapse_sum;
} s_info = {
	.running = 0,
	.time_ref = 0,
	.time_lap_diff = 0,
	.time_elapse_sum = 0,
};

/**
 * @brief Gets system time by milliseconds.
 */
static long long _get_current_ms_time(void)
{
	struct timespec tp;
	long long res = 0;

	if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
		/*
		 * Zero mean invalid time
		 */
		return 0;
	} else {
		/*
		 * Calculate milliseconds time
		 */
		res = tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
		return res;
	}
}

static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	ui_app_exit();
}

static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	appdata_s *ad = data;
	/* Let window go to hide state. */
	elm_win_lower(ad->win);
}

static void
create_base_gui(appdata_s *ad)
{
	/* Window */
	/* Create and initialize elm_win.
	   elm_win is mandatory to manipulate window. */
	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	elm_win_autodel_set(ad->win, EINA_TRUE);
	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}
	evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

    //create conformant
	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	//create naviframe
	ad->nf = elm_naviframe_add(ad->conform);
    elm_naviframe_prev_btn_auto_pushed_set(ad->nf, EINA_TRUE);
    elm_object_content_set(ad->conform, ad->nf);
    evas_object_show(ad->nf);

    //create box layout
	ad->box = elm_box_add(ad->nf);

	//add box to naviframe
	elm_naviframe_item_push(ad->nf, "Nav", NULL, NULL, ad->box, "default");

	/* Label */
	ad->status = elm_label_add(ad->box);
	elm_object_text_set(ad->status, "<align=center>Hello Tizen</align>");
	evas_object_size_hint_weight_set(ad->status, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_show(ad->status);

	ad->time = elm_label_add(ad->box);
	elm_object_text_set(ad->time, "<align=center>00:00</align>");
	evas_object_size_hint_weight_set(ad->time, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_show(ad->time);

	//add label to box
	elm_box_pack_end(ad->box, ad->status);
	elm_box_pack_end(ad->box, ad->time);
	evas_object_show(ad->box);

	/* Show window after base gui is set up */
	evas_object_show(ad->win);

	efl_util_set_window_screen_mode(ad->win,EFL_UTIL_SCREEN_MODE_ALWAYS_ON);


	ad->transit = elm_transit_add();
	elm_transit_object_add(ad->transit, ad->status);
	elm_transit_effect_color_add(ad->transit, /* Target object */
	                             255, 0, 0, 255, /* From color, with alpha channel 255 */
	                             0, 0, 255, 255); /* To color, with alpha channel 255 */
	elm_transit_duration_set(ad->transit, 3);
	elm_transit_auto_reverse_set(ad->transit, EINA_TRUE);
	elm_transit_go(ad->transit);
	elm_transit_repeat_times_set(ad->transit, -1);
}

static int numofactive = 10;
static int numofinview = 10;
void _gps_satellite_updated_cb(int num_of_active, int num_of_inview, time_t timestamp, void *user_data)
{
	appdata_s *ad = (appdata_s *)user_data;
	numofactive = num_of_active;
	numofinview = num_of_inview;
	char styled_text[1024];
	snprintf(styled_text, sizeof(styled_text), "<align=center>%i / %i</align>", numofactive, numofinview);
  	elm_object_text_set(ad->status, styled_text);

}

void _position_updated_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
    appdata_s *ad = (appdata_s *) user_data;

    struct tm * timeinfo = localtime (&timestamp);
    char buffer [512];
	strftime (buffer,512,"<align=center>%I:%M:%S<align=center>",timeinfo);
    elm_object_text_set(ad->status, buffer);

    feedback_play_type(FEEDBACK_TYPE_VIBRATION, FEEDBACK_PATTERN_GENERAL);

    elm_transit_del(ad->transit);

	s_info.running=0;
}

/**
 * @brief Updates animation changing object's position.
 * @param[in] user_data The user data to be passed to the callback functions
 */
Eina_Bool stopwatch_update_animation(void *data)
{
	appdata_s *ad = (appdata_s *)data;

	int msec = 0;
	int sec = 0;
	int min = 0;
	char min_str[3] = {0, };
	char sec_str[3] = {0, };
	char msec_str[3] = {0, };
	long long current_time = _get_current_ms_time();
	long long current_sum;

	/*
	 * Calculate sum of elapse time at this moment
	 */
	current_sum = current_time - s_info.time_ref + s_info.time_elapse_sum;

	/*
	 * Parse millisecond time to various format (minutes, seconds, milliseconds)
	 */
	sec = current_sum / 1000;
	msec = current_sum % 1000;
	min = sec / 60;
	sec = sec % 60;

	/*
	 * Update Stopwatch time number
	 */
	if (min < 10)
		sprintf(min_str, "0%d", min);
	else
		sprintf(min_str, "%d", min);

	if (sec < 10)
		sprintf(sec_str, "0%d", sec);
	else
		sprintf(sec_str, "%d", sec);

	if (msec < 100)
		sprintf(msec_str, "0%d", msec/10);
	else
		sprintf(msec_str, "%d", msec/10);

	char styled_text[1024];
	snprintf(styled_text, sizeof(styled_text), "<align=center>%s:%s:%s</align>", min_str, sec_str, msec_str);
  	elm_object_text_set(ad->time, styled_text);

  	if(s_info.running==1)
  		return ECORE_CALLBACK_RENEW;
  	else
		return ECORE_CALLBACK_DONE;
}

/* Create the location service */
static void create_location_service(void *data)
{
    appdata_s *ad = (appdata_s *)data;
    location_manager_h manager;

    int ret = location_manager_create(LOCATIONS_METHOD_GPS, &manager);

    ad->location = manager;
    ret = location_manager_set_position_updated_cb(manager, _position_updated_cb, 5, ad);

}

static void
start_location_service(void *data)
{
    appdata_s *ad = (appdata_s *) data;

    int ret = location_manager_start(ad->location);
    if (ret != LOCATIONS_ERROR_NONE)
        dlog_print(DLOG_ERROR, LOG_TAG, "location_manager_start() failed. (%d)", ret);

    s_info.time_ref = _get_current_ms_time();
    s_info.running = 1;
    anim = ecore_animator_add(stopwatch_update_animation, ad);
    elm_object_text_set(ad->status, "<align=center>searching</align>");
}

/* Stop the location service */
static void
stop_location_service(void *data)
{
    appdata_s *ad = (appdata_s *) data;

    int ret = location_manager_stop(ad->location);
    if (ret != LOCATIONS_ERROR_NONE)
        dlog_print(DLOG_ERROR, LOG_TAG, "location_manager_stop() failed. (%d)", ret);
}


//ret = gps_status_set_satellite_updated_cb(manager, _gps_satellite_updated_cb, 5, ad);
//bool value;
//ret = system_info_get_platform_bool("http://tizen.org/feature/location.gps.satellite", &value);
//char styled_text[1024];
//snprintf(styled_text, sizeof(styled_text), "<align=center>%i</align>", value);
//elm_object_text_set(ad->label, styled_text);



void
app_request_response_cb(ppm_call_cause_e cause, ppm_request_result_e result,
const char *privilege, void *user_data)
{
	appdata_s *ad = (appdata_s *)user_data;

    if (cause == PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR) {
        /* Log and handle errors */
        return;
    }

    switch (result) {
        case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_ALLOW_FOREVER:
            /* Update UI and start accessing protected functionality */
        	create_location_service(ad);
            if (ad->location)
                start_location_service(ad);
            break;
        case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
            /* Show a message and terminate the application */
            break;
        case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
            /* Show a message with explanation */
            break;
    }
}

static bool
app_create(void *data)
{
	/* Hook to take necessary actions before main event loop starts
		Initialize UI resources and application's data
		If this function returns true, the main loop of application starts
		If this function returns false, the application is terminated */

	appdata_s *ad = data;
	create_base_gui(ad);

	feedback_initialize();

    ppm_check_result_e result;
    const char *privilege = "http://tizen.org/privilege/location";
    int ret = ppm_check_permission(privilege, &result);
    if (ret == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
            switch (result) {
                case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ALLOW:
                    /* Update UI and start accessing protected functionality */
                	create_location_service(ad);
                    if (ad->location)
                        start_location_service(ad);
                    break;
                case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_DENY:
					 /* Show a message and terminate the application */
					 break;
                case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ASK:
					ret = ppm_request_permission(privilege, app_request_response_cb, ad);
					/* Log and handle errors */
					break;
            }
    }

	return true;
}

static void
app_control(app_control_h app_control, void *data)
{

}

static void
app_pause(void *data)
{
	dlog_print(DLOG_ERROR, LOG_TAG, "pausing");
	appdata_s *ad = data;

	stop_location_service(ad);

	ecore_animator_del(anim);
	anim = NULL;
}

static void
app_resume(void *data)
{
	appdata_s *ad = data;

	start_location_service(ad);
}

static void
app_terminate(void *data)
{
	dlog_print(DLOG_ERROR, LOG_TAG, "terminated");
	appdata_s *ad = (appdata_s *)data;

	if (ad->location) {
		int ret = location_manager_destroy(ad->location);
		if (ret != LOCATIONS_ERROR_NONE)
			dlog_print(DLOG_ERROR, LOG_TAG, "location_manager_destroy() failed.(%d)", ret);
		else
			ad->location = NULL;
	}
	else
		dlog_print(DLOG_ERROR, LOG_TAG, "no manager");


	ecore_animator_del(anim);
	anim = NULL;
}

static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	char *locale = NULL;
	system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}

static void
ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_DEVICE_ORIENTATION_CHANGED*/
	return;
}

static void
ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_BATTERY*/
}

static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_MEMORY*/
}

int
main(int argc, char *argv[])
{
	appdata_s ad = {0,};
	int ret = 0;

	ui_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;

	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED], APP_EVENT_DEVICE_ORIENTATION_CHANGED, ui_app_orient_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);

	ret = ui_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
	}

	return ret;
}
