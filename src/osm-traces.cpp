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

#include <QDebug>

#include <curl/curl.h>
#include <curl/easy.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "window.h"
#include "layer_trw.h"
#include "layer_trw_track_internal.h"
#include "osm-traces.h"
#include "gpx.h"
#include "background.h"
#include "preferences.h"
#include "application_state.h"
#include "globals.h"
#include "util.h"




using namespace SlavGPS;




/* Params will be osm_traces.username, osm_traces.password */
/* We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_OSM_TRACES "osm_traces"

#define VIK_SETTINGS_OSM_TRACE_VIS "osm_trace_visibility"
static int last_active = -1;

/**
   Login to use for OSM uploading.
*/
static QString osm_user;

/**
   Password to use for OSM uploading.
*/
static QString osm_password;

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

	QString name;
	QString description;
	QString tags;
	bool anonymize_times = false; /* ATM only available on a single track. */
	const OsmTraceVis_t * vistype = NULL;
	LayerTRW * trw = NULL;
	Track * trk = NULL;
};




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_OSM_TRACES, "username", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, QObject::tr("OSM username:"), WidgetType::ENTRY,    NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_OSM_TRACES, "password", SGVariantType::STRING, PARAMETER_GROUP_GENERIC, QObject::tr("OSM password:"), WidgetType::PASSWORD, NULL, NULL, NULL, NULL },
	{ 2, NULL,                             NULL,       SGVariantType::EMPTY,  PARAMETER_GROUP_GENERIC, QString(""),                  WidgetType::NONE,     NULL, NULL, NULL, NULL } /* Guard. */
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




void SlavGPS::osm_save_current_credentials(const QString & user, const QString & password)
{
	login_mutex.lock();

	osm_user = user;
	osm_password = password;

	login_mutex.unlock();
}



QString SlavGPS::osm_get_current_credentials()
{
	QString user_pass;
	login_mutex.lock();
	user_pass = QString("%1:%2").arg(osm_user).arg(osm_password);
	login_mutex.unlock();
	return user_pass;
}




/* Initialization. */
void SlavGPS::osm_traces_init()
{
	Preferences::register_group(PREFERENCES_NAMESPACE_OSM_TRACES, QObject::tr("OpenStreetMap Traces"));

	Preferences::register_parameter(prefs + 0, SGVariant(""));
	Preferences::register_parameter(prefs + 1, SGVariant(""));
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
static int osm_traces_upload_file(const QString & user,
				  const QString & password,
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

	/* TODO: why do we pass user and password to this function if we create user_pass here? */
	char * user_pass = g_strdup(osm_get_current_credentials().toUtf8().constData());

	int result = 0; /* Default to it worked! */

	qDebug() << "DD: OSM Traces:" << __FUNCTION__ << user << password << file << filename << description << tags;

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
			qDebug() << "DD: OSM Traces: received valid curl response:" << code;
			if (code != 200) {
				qDebug() << "WW: OSM Traces: failed to upload data: HTTP response is" << code;
				result = code;
			}
		} else {
			qDebug() << "EE: OSM Traces: curl_easy_getinfo failed:" << res;
			result = -1;
		}
	} else {
		qDebug() << "WW: OSM Traces: curl request failed:" << curl_error_buffer;
		result = -2;
	}

	/* Memory. */
	free(user_pass);
	user_pass = NULL;

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
			filename = a_gpx_write_track_tmp_file(oti->trk, &options);
		}
	} else {
		/* Upload the whole LayerTRW. */
		filename = a_gpx_write_tmp_file(oti->trw, &options);
	}

	if (!filename) {
		return -1;
	}

	/* Finally, upload it. */
	int ans = osm_traces_upload_file(osm_user, osm_password, filename, oti->name.toUtf8().constData(), oti->description.toUtf8().constData(), oti->tags.toUtf8().constData(), oti->vistype);

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
		qDebug() << "EE: OSM Traces: failed to unlink temporary file:" << strerror(errno);
	}
	return ret;
}




void SlavGPS::osm_fill_credentials_widgets(QLineEdit & user_entry, QLineEdit & password_entry)
{
	const char *default_user = get_default_user();
	const QString pref_user = Preferences::get_param_value(PREFERENCES_NAMESPACE_OSM_TRACES ".username")->val_string;
	const QString pref_password = Preferences::get_param_value(PREFERENCES_NAMESPACE_OSM_TRACES ".password")->val_string;


	if (!osm_user.isEmpty()) {
		user_entry.setText(osm_user);
	} else if (!pref_user.isEmpty()) {
		user_entry.setText(pref_user);
	} else if (default_user != NULL) {
		user_entry.setText(QString(default_user));
	}

	if (!osm_password.isEmpty()) {
		password_entry.setText(osm_password);
	} else if (!pref_password.isEmpty()) {
		password_entry.setText(pref_password);
	}
	/* This is a password -> invisible. kamilTODO: shouldn't this be already set elsewhere as invisible? */
	password_entry.setEchoMode(QLineEdit::Password);
}




/**
 * Uploading a LayerTRW.
 *
 * @param trw LayerTRW
 * @param trk if not null, the track to upload
 */
void SlavGPS::osm_traces_upload_viktrwlayer(LayerTRW * trw, Track * trk)
{
	BasicDialog dialog(trw->get_window());
	dialog.setWindowTitle(QObject::tr("OSM upload"));


	int row = 0;
	QCheckBox * anonymize_checkbutton = NULL;
	QComboBox * visibility_combo = NULL;
	const OsmTraceVis_t * vis_t = NULL;


	QLabel  * user_label = new QLabel(QObject::tr("Email:"), &dialog);
	QLineEdit * user_entry = new QLineEdit(&dialog);
	user_entry->setToolTip(QObject::tr("The email used as login\n"
					   "<small>Enter the email you use to login into www.openstreetmap.org.</small>"));
	dialog.grid->addWidget(user_label, row, 0);
	dialog.grid->addWidget(user_entry, row, 1);
	row++;



	QLabel * password_label = new QLabel(QObject::tr("Password:"), &dialog);
	QLineEdit * password_entry = new QLineEdit(&dialog);
	password_entry->setToolTip(QObject::tr("The password used to login\n"
					       "<small>Enter the password you use to login into www.openstreetmap.org.</small>"));
	dialog.grid->addWidget(password_label, row, 0);
	dialog.grid->addWidget(password_entry, row, 1);
	row++;



	osm_fill_credentials_widgets(*user_entry, *password_entry);



	QLabel * name_label = new QLabel(QObject::tr("File's name:"), &dialog);
	QLineEdit * name_entry = new QLineEdit(&dialog);
	const QString name = trk ? trk->name : trw->get_name();

	name_entry->setText(name);
	name_entry->setToolTip(QObject::tr("The name of the file on OSM\n"
					   "<small>This is the name of the file created on the server."
					   "This is not the name of the local file.</small>"));
	dialog.grid->addWidget(name_label, row, 0);
	dialog.grid->addWidget(name_entry, row, 1);
	row++;



	QLabel * description_label = new QLabel(QObject::tr("Description:"));
	QLineEdit * description_entry = new QLineEdit();
	QString description;
	if (trk != NULL) {
		description = trk->description;
	} else {
		TRWMetadata * md = trw->get_metadata();
		description = md ? md->description : NULL;
	}
	if (!description.isEmpty()) {
		description_entry->setText(description);
	}

	description_entry->setToolTip(QObject::tr("The description of the trace"));
	dialog.grid->addWidget(description_label, row, 0);
	dialog.grid->addWidget(description_entry, row, 1);
	row++;



	if (trk != NULL) {
		QLabel * label = new QLabel(QObject::tr("Anonymize Times:"));
		anonymize_checkbutton = new QCheckBox();
		anonymize_checkbutton->setToolTip(QObject::tr("Anonymize times of the trace.\n"
							      "<small>You may choose to make the trace identifiable, yet mask the actual real time values</small>"));
		dialog.grid->addWidget(label, row, 0);
		dialog.grid->addWidget(anonymize_checkbutton, row, 1);
		row++;
	}



	QLabel * tags_label = new QLabel(QObject::tr("Tags:"));
	QLineEdit * tags_entry = new QLineEdit();
	TRWMetadata * md = trw->get_metadata();
	if (md && !md->keywords.isEmpty()) {
		tags_entry->setText(md->keywords);
	}
	tags_entry->setToolTip(QObject::tr("The tags associated to the trace"));
	dialog.grid->addWidget(tags_label, row, 0);
	dialog.grid->addWidget(tags_entry, row, 1);
	row++;



	QLabel * visibility_label = new QLabel(QObject::tr("Visibility:"));
	visibility_combo = new QComboBox();
	for (vis_t = OsmTraceVis; vis_t->combostr != NULL; vis_t++) {
		visibility_combo->addItem(vis_t->combostr);
	}

	/* Set identifiable by default or use the settings for the value. */
	if (last_active < 0) {
		int find_entry = -1;
		int wanted_entry = -1;
		QString vis;
		if (ApplicationState::get_string(VIK_SETTINGS_OSM_TRACE_VIS, vis)) {
			/* Use setting. */
			if (!vis.isEmpty()) {
				for (vis_t = OsmTraceVis; vis_t->apistr != NULL; vis_t++) {
					find_entry++;
					if (vis == QString(vis_t->apistr)) {
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
	visibility_combo->setCurrentIndex(last_active);
	dialog.grid->addWidget(visibility_label, row, 0);
	dialog.grid->addWidget(visibility_combo, row, 1);
	row++;



	/* User should think about it first... */
	dialog.button_box->button(QDialogButtonBox::Cancel)->setDefault(true);

	description_entry->setFocus();

	if (dialog.exec() == QDialog::Accepted) {

		/* Overwrite authentication info. */
		osm_save_current_credentials(user_entry->text(), password_entry->text());

		/* Storing data for the future thread. */
		OsmTracesInfo * info = new OsmTracesInfo(trw, trk);
		info->name        = name_entry->text();
		info->description = description_entry->text();
		/* TODO Normalize tags: they will be used as URL part. */
		info->tags        = tags_entry->text();
		info->vistype     = &OsmTraceVis[visibility_combo->currentIndex()];

		if (trk != NULL && anonymize_checkbutton != NULL) {
			info->anonymize_times = anonymize_checkbutton->isChecked();
		} else {
			info->anonymize_times = false;
		}

		/* Save visibility value for default reuse. */
		last_active = visibility_combo->currentIndex();
		ApplicationState::set_string(VIK_SETTINGS_OSM_TRACE_VIS, OsmTraceVis[last_active].apistr);

		const QString job_description = QObject::tr("Uploading %1 to OSM").arg(info->name);

		a_background_thread(info, ThreadPoolType::REMOTE, job_description);
	}
}
