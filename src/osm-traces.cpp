/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2007, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <mutex>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <time.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "layer_trw.h"
#include "osm-traces.h"
#include "gpx.h"
#include "background.h"
#include "preferences.h"
#include "settings.h"
#include "globals.h"




using namespace SlavGPS;




/* Params will be osm_traces.username, osm_traces.password */
/* We have to make sure these don't collide. */
#define VIKING_OSM_TRACES_PARAMS_GROUP_KEY "osm_traces"
#define VIKING_OSM_TRACES_PARAMS_NAMESPACE "osm_traces."

#define VIK_SETTINGS_OSM_TRACE_VIS "osm_trace_visibility"
static int last_active = -1;

/**
 * Login to use for OSM uploading.
 */
static char *osm_user = NULL;

/**
 * Password to use for OSM uploading.
 */
static char *osm_password = NULL;

/**
 * Mutex to protect auth. token
 */
static std::mutex login_mutex;

/**
 * Different type of trace visibility.
 */
typedef struct _OsmTraceVis_t {
	const char *combostr;
	const char *apistr;
} OsmTraceVis_t;

static const OsmTraceVis_t OsmTraceVis[] = {
	{ N_("Identifiable (public w/ timestamps)"),	"identifiable" },
	{ N_("Trackable (private w/ timestamps)"),	"trackable" },
	{ N_("Public"),					"public" },
	{ N_("Private"),				"private" },
	{ NULL, NULL },
};




static int osm_traces_upload_thread(BackgroundJob * bg_job);




/**
 * Struct hosting needed info.
 */
class OsmTracesInfo : public BackgroundJob {
public:
	OsmTracesInfo(LayerTRW * trw_, Track * trk_);
	~OsmTracesInfo();

	char * name = NULL;
	char * description = NULL;
	char * tags = NULL;
	bool anonymize_times = false; /* ATM only available on a single track. */
	const OsmTraceVis_t * vistype = NULL;
	LayerTRW * trw = NULL;
	Track * trk = NULL;
};




static Parameter prefs[] = {
	{ 0, VIKING_OSM_TRACES_PARAMS_NAMESPACE "username", ParameterType::STRING, VIK_LAYER_GROUP_NONE, N_("OSM username:"), WidgetType::ENTRY,    NULL, NULL, NULL, NULL, NULL, NULL },
	{ 1, VIKING_OSM_TRACES_PARAMS_NAMESPACE "password", ParameterType::STRING, VIK_LAYER_GROUP_NONE, N_("OSM password:"), WidgetType::PASSWORD, NULL, NULL, NULL, NULL, NULL, NULL },

	{ 2, NULL,                                          ParameterType::STRING, VIK_LAYER_GROUP_NONE, "",                  WidgetType::NONE,     NULL, NULL, NULL, NULL, NULL, NULL } /* Guard. */
};




OsmTracesInfo::OsmTracesInfo(LayerTRW * trw_, Track * trk_)
{
	this->thread_fn = osm_traces_upload_thread;
	this->n_items = 1;

	this->trw = trw_;
	this->trk = trk_;
}




OsmTracesInfo::~OsmTracesInfo()
{
	/* Fields have been g_strdup'ed. */
	free(this->name);
	this->name = NULL;

	free(this->description);
	this->description = NULL;

	free(this->tags);
	this->tags = NULL;

	this->trw->unref();
	this->trw = NULL;
}




static const char *get_default_user()
{
	const char *default_user = NULL;

	/* Retrieve "standard" EMAIL varenv. */
	default_user = g_getenv("EMAIL");

	return default_user;
}




void SlavGPS::osm_set_login(const char *user, const char *password)
{
	login_mutex.lock();

	free(osm_user);
	osm_user = NULL;

	free(osm_password);
	osm_password = NULL;

	osm_user = g_strdup(user);
	osm_password = g_strdup(password);

	login_mutex.unlock();
}



char * SlavGPS::osm_get_login()
{
	char * user_pass = NULL;
	login_mutex.lock();
	user_pass = g_strdup_printf("%s:%s", osm_user, osm_password);
	login_mutex.unlock();
	return user_pass;
}




/* Initialization. */
void SlavGPS::osm_traces_init()
{
	/* Preferences. */
	a_preferences_register_group(VIKING_OSM_TRACES_PARAMS_GROUP_KEY, _("OpenStreetMap Traces"));

	ParameterValue tmp;
	tmp.s = "";
	a_preferences_register(prefs, tmp, VIKING_OSM_TRACES_PARAMS_GROUP_KEY);
	tmp.s = "";
	a_preferences_register(prefs+1, tmp, VIKING_OSM_TRACES_PARAMS_GROUP_KEY);
}




void SlavGPS::osm_traces_uninit()
{
}




/*
 * Upload a file.
 * Returns a basic status:
 *   < 0  : curl error
 *   == 0 : OK
 *   > 0  : HTTP error
  */
static int osm_traces_upload_file(const char *user,
				  const char *password,
				  const char *file,
				  const char *filename,
				  const char *description,
				  const char *tags,
				  const OsmTraceVis_t *vistype)
{
	CURL *curl;
	CURLcode res;
	char curl_error_buffer[CURL_ERROR_SIZE];
	struct curl_slist *headers = NULL;
	struct curl_httppost *post=NULL;
	struct curl_httppost *last=NULL;

	char *base_url = (char *) "http://www.openstreetmap.org/api/0.6/gpx/create";

	char *user_pass = osm_get_login();

	int result = 0; /* Default to it worked! */

	fprintf(stderr, "DEBUG: %s: %s %s %s %s %s %s\n", __FUNCTION__,
		user, password, file, filename, description, tags);

	/* Init CURL. */
	curl = curl_easy_init();

	/* Filling the form. */
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "description",
		     CURLFORM_COPYCONTENTS, description, CURLFORM_END);
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "tags",
		     CURLFORM_COPYCONTENTS, tags, CURLFORM_END);
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "visibility",
		     CURLFORM_COPYCONTENTS, vistype->apistr, CURLFORM_END);
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "file",
		     CURLFORM_FILE, file,
		     CURLFORM_FILENAME, filename,
		     CURLFORM_CONTENTTYPE, "text/xml", CURLFORM_END);

	/* Prepare request. */
	/* As explained in http://wiki.openstreetmap.org/index.php/User:LA2 */
	/* Expect: header seems to produce incompatibilites between curl and httpd. */
	headers = curl_slist_append(headers, "Expect: ");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_URL, base_url);
	curl_easy_setopt(curl, CURLOPT_USERPWD, user_pass);
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_buffer);
	if (vik_verbose) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}

	/* Execute request. */
	res = curl_easy_perform(curl);
	if (res == CURLE_OK) {
		long code;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (res == CURLE_OK){
			fprintf(stderr, "DEBUG: received valid curl response: %ld\n", code);
			if (code != 200) {
				fprintf(stderr, _("WARNING: failed to upload data: HTTP response is %ld\n"), code);
				result = code;
			}
		} else {
			fprintf(stderr, _("CRITICAL: curl_easy_getinfo failed: %d\n"), res);
			result = -1;
		}
	} else {
		fprintf(stderr, _("WARNING: curl request failed: %s\n"), curl_error_buffer);
		result = -2;
	}

	/* Memory. */
	free(user_pass); user_pass = NULL;

	curl_formfree(post);
	curl_easy_cleanup(curl);
	return result;
}




/**
 * Uploading function executed by the background" thread.
 */
static int osm_traces_upload_thread(BackgroundJob * bg_job)
{
	OsmTracesInfo * oti = (OsmTracesInfo *) bg_job;
	/* Due to OSM limits, we have to enforce ele and time fields
	   also don't upload invisible tracks. */
	static GpxWritingOptions options = { true, true, false, false };

	if (!oti) {
		return -1;
	}

	char *filename = NULL;

	/* Writing gpx file. */
	if (oti->trk != NULL) {
		/* Upload only the selected track. */
		if (oti->anonymize_times) {
			Track * trk = new Track(*oti->trk);
			trk->anonymize_times();
			filename = a_gpx_write_track_tmp_file(trk, &options);
			trk->free();
		} else {
			filename = a_gpx_write_track_tmp_file (oti->trk, &options);
		}
	} else {
		/* Upload the whole LayerTRW. */
		filename = a_gpx_write_tmp_file(oti->trw, &options);
	}

	if (!filename) {
		return -1;
	}

	/* Finally, upload it. */
	int ans = osm_traces_upload_file(osm_user, osm_password, filename,
					 oti->name, oti->description, oti->tags, oti->vistype);

	/* Show result in statusbar or failure in dialog for user feedback. */

	/* Get current time to put into message to show when result was generated
	   since need to show difference between operations (when displayed on statusbar).
	   NB If on dialog then don't need time. */
	time_t timenow;
	struct tm* timeinfo;
	time(&timenow);
	timeinfo = localtime(&timenow);
	char timestr[80];
	/* Compact time only - as days/date isn't very useful here. */
	strftime(timestr, sizeof(timestr), "%X)", timeinfo);

	/* Test to see if window it was invoked on is still valid.
	   Not sure if this test really works! (i.e. if the window was closed in the mean time). */
	Window * w = oti->trw->get_window();
	if (w) {
		QString msg;
		if (ans == 0) {
			/* Success. */
			msg = QString("%1 (@%2)").arg("Uploaded to OSM").arg(timestr);
		} else if (ans < 0) {
			/* Use UPPER CASE for bad news. */
			msg = QString("%1 (@%2)").arg("FAILED TO UPLOAD DATA TO OSM - CURL PROBLEM").arg(timestr);
		} else {
			/* Use UPPER CASE for bad news. */
			msg = QString("%1 : %2 %3 (@%4)").arg("FAILED TO UPLOAD DATA TO OSM").arg("HTTP response code").arg(ans).arg(timestr);
		}
		w->statusbar_update(StatusBarField::INFO, msg);
	}
	/* Removing temporary file. */
	int ret = g_unlink(filename);
	if (ret != 0) {
		fprintf(stderr, _("CRITICAL: failed to unlink temporary file: %s\n"), strerror(errno));
	}
}




void SlavGPS::osm_login_widgets(GtkWidget *user_entry, GtkWidget *password_entry)
{
	if (!user_entry || !password_entry) {
		return;
	}

	const char *default_user = get_default_user();
	const char *pref_user = a_preferences_get(VIKING_OSM_TRACES_PARAMS_NAMESPACE "username")->s;
	const char *pref_password = a_preferences_get(VIKING_OSM_TRACES_PARAMS_NAMESPACE "password")->s;

#ifdef K

	if (osm_user != NULL && osm_user[0] != '\0') {
		gtk_entry_set_text(GTK_ENTRY(user_entry), osm_user);
	} else if (pref_user != NULL && pref_user[0] != '\0') {
		gtk_entry_set_text(GTK_ENTRY(user_entry), pref_user);
	} else if (default_user != NULL) {
		gtk_entry_set_text(GTK_ENTRY(user_entry), default_user);
	}

	if (osm_password != NULL && osm_password[0] != '\0') {
		gtk_entry_set_text(GTK_ENTRY(password_entry), osm_password);
	} else if (pref_password != NULL) {
		gtk_entry_set_text(GTK_ENTRY(password_entry), pref_password);
	}
	/* This is a password -> invisible. */
	gtk_entry_set_visibility(GTK_ENTRY(password_entry), false);
#endif
}




/**
 * Uploading a LayerTRW.
 *
 * @param trw LayerTRW
 * @param trk if not null, the track to upload
 */
void SlavGPS::osm_traces_upload_viktrwlayer(LayerTRW * trw, Track * trk)
{
#ifdef K
	GtkWidget *dia = gtk_dialog_new_with_buttons(_("OSM upload"),
						     trw->get_window(),
						     (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
						     GTK_STOCK_CANCEL,
						     GTK_RESPONSE_REJECT,
						     GTK_STOCK_OK,
						     GTK_RESPONSE_ACCEPT,
						     NULL);

	const char *name = NULL;
	GtkWidget *user_entry;
	GtkWidget *password_entry;
	GtkWidget *name_entry;
	GtkWidget *description_entry;
	GtkWidget *tags_entry;
	QComboBox * visibility_combo = NULL;
	GtkWidget *anonymize_checkbutton = NULL;
	const OsmTraceVis_t *vis_t;

	QLabel  * user_label = new QLabel(QObject::tr("Email:"));
	user_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), user_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), user_entry, false, false, 0);
	user_entry->setToolTip(QObject::tr("The email used as login\n"
					   "<small>Enter the email you use to login into www.openstreetmap.org.</small>"));

	QLabel * password_label = new QLabel(QObject::tr("Password:"));
	password_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), password_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), password_entry, false, false, 0);
	password_entry->setToolTip(QObject::tr("The password used to login\n"
					       "<small>Enter the password you use to login into www.openstreetmap.org.</small>"));

	osm_login_widgets(user_entry, password_entry);

	QLabel * name_label = new QLabel(QObject::tr("File's name:"));
	name_entry = gtk_entry_new();
	if (trk != NULL) {
		name = trk->name;
	} else {
		name = trw->get_name();
	}
	gtk_entry_set_text(GTK_ENTRY(name_entry), name);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), name_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), name_entry, false, false, 0);
	name_entry->setToolTip(QObject::tr("The name of the file on OSM\n"
					   "<small>This is the name of the file created on the server."
					   "This is not the name of the local file.</small>"));

	QLabel * description_label = new QLabel(QObject::tr("Description:"));
	description_entry = gtk_entry_new();
	const char *description = NULL;
	if (trk != NULL) {
		description = trk->description;
	} else {
		TRWMetadata * md = trw->get_metadata();
		description = md ? md->description : NULL;
	}
	if (description) {
		gtk_entry_set_text(GTK_ENTRY(description_entry), description);
	}
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), description_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), description_entry, false, false, 0);
	description_entry->setToolTip(QObject::tr("The description of the trace"));

	if (trk != NULL) {
		QLabel * label = new QLabel(QObject::tr("Anonymize Times:"));
		anonymize_checkbutton = gtk_check_button_new();
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), label, false, false, 0);
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), anonymize_checkbutton, false, false, 0);
		anonymize_checkbutton->setToolTip(QObject::tr("Anonymize times of the trace.\n"
							      "<small>You may choose to make the trace identifiable, yet mask the actual real time values</small>"));
	}

	QLabel * tags_label = new QLabel(QObject::tr("Tags:"));
	tags_entry = gtk_entry_new();
	TRWMetadata * md = trw->get_metadata();
	if (md && md->keywords) {
		gtk_entry_set_text(GTK_ENTRY(tags_entry), md->keywords);
	}
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), tags_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), tags_entry, false, false, 0);
	tags_entry->setToolTip(QObject::tr("The tags associated to the trace"));

	visibility_combo = new QComboBox();
	for (vis_t = OsmTraceVis; vis_t->combostr != NULL; vis_t++) {
		vik_combo_box_text_append(visibility_combo, vis_t->combostr);
	}

	/* Set identifiable by default or use the settings for the value. */
	if (last_active < 0) {
		int find_entry = -1;
		int wanted_entry = -1;
		char *vis = NULL;
		if (a_settings_get_string(VIK_SETTINGS_OSM_TRACE_VIS, &vis)) {
			/* Use setting. */
			if (vis) {
				for (vis_t = OsmTraceVis; vis_t->apistr != NULL; vis_t++) {
					find_entry++;
					if (!strcmp(vis, vis_t->apistr)) {
						wanted_entry = find_entry;
					}
				}
			}
			/* If not found set it to the first entry, otherwise use the entry. */
			last_active = (wanted_entry < 0) ? 0 : wanted_entry;
		} else {
			last_active = 0;
		}
	}
	gtk_combo_box_set_active(visibility_combo, last_active);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dia))), visibility_combo, false, false, 0);

	/* User should think about it first... */
	gtk_dialog_set_default_response(GTK_DIALOG(dia), GTK_RESPONSE_REJECT);

	gtk_widget_show_all(dia);
	gtk_widget_grab_focus(description_entry);

	if (gtk_dialog_run(GTK_DIALOG(dia)) == GTK_RESPONSE_ACCEPT) {

		/* Overwrite authentication info. */
		osm_set_login(gtk_entry_get_text(GTK_ENTRY(user_entry)),
			      gtk_entry_get_text(GTK_ENTRY(password_entry)));

		/* Storing data for the future thread. */
		OsmTracesInfo * info = new OsmTracesInfo(trw, trk);
		info->name        = g_strdup(gtk_entry_get_text(GTK_ENTRY(name_entry)));
		info->description = g_strdup(gtk_entry_get_text(GTK_ENTRY(description_entry)));
		/* TODO Normalize tags: they will be used as URL part. */
		info->tags        = g_strdup(gtk_entry_get_text(GTK_ENTRY(tags_entry)));
		info->vistype     = &OsmTraceVis[gtk_combo_box_get_active(visibility_combo)];

		if (trk != NULL && anonymize_checkbutton != NULL) {
			info->anonymize_times = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(anonymize_checkbutton));
		} else {
			info->anonymize_times = false;
		}

		/* Save visibility value for default reuse. */
		last_active = gtk_combo_box_get_active(visibility_combo);
		a_settings_set_string(VIK_SETTINGS_OSM_TRACE_VIS, OsmTraceVis[last_active].apistr);

		const QString job_description = QString(tr("Uploading %1 to OSM")).arg(info->name);

		a_background_thread(info, ThreadPoolType::REMOTE, job_description);
	}
	gtk_widget_destroy(dia);
#endif
}
