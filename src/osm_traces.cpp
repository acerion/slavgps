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
#include "osm_traces.h"
#include "gpx.h"
#include "background.h"
#include "preferences.h"
#include "application_state.h"
#include "util.h"
#include "statusbar.h"




using namespace SlavGPS;




extern bool vik_verbose;




/* Params will be osm_traces.username, osm_traces.password */
/* We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_OSM_TRACES "osm_traces."

#define VIK_SETTINGS_OSM_TRACE_VIS "osm_trace_visibility"

#define INVALID_ENTRY_INDEX -1

/* Index of the last visibility selected. */
static int g_last_visibility_index = -1;

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
   Different type of trace visibility.
*/
class OsmTraceVisibility {
public:
	const QString combostr;
	const QString apistr;
};

static const OsmTraceVisibility trace_visibilities[] = {
	{ QObject::tr("Identifiable (public w/ timestamps)"), "identifiable" },
	{ QObject::tr("Trackable (private w/ timestamps)"),   "trackable"    },
	{ QObject::tr("Public"),                              "public"       },
	{ QObject::tr("Private"),                             "private"      },
	{ "", "" },
};




static int find_initial_visibility_index(void);




class OSMTracesInfo : public BackgroundJob {
public:
	OSMTracesInfo(LayerTRW * trw, Track * trk);
	~OSMTracesInfo();

	void run(void);

	QString name;
	QString description;
	QString tags;
	bool anonymize_times = false; /* ATM only available on a single track. */
	const OsmTraceVisibility * visibility = NULL;
	LayerTRW * trw = NULL;
	Track * trk = NULL;
};




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_OSM_TRACES "username", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("OSM username:"), WidgetType::Entry,    NULL, NULL, NULL, NULL },
	{ 1, PREFERENCES_NAMESPACE_OSM_TRACES "password", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("OSM password:"), WidgetType::Password, NULL, NULL, NULL, NULL },
	{ 2,                                  "",         SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, "",                           WidgetType::None,     NULL, NULL, NULL, NULL } /* Guard. */
};




OSMTracesInfo::OSMTracesInfo(LayerTRW * new_trw, Track * new_trk)
{
	this->n_items = 1;

	this->trw = new_trw;
	this->trk = new_trk;
}




OSMTracesInfo::~OSMTracesInfo()
{
	this->trw->unref();
	this->trw = NULL;
}




static QString get_default_user(void)
{
	return qgetenv("EMAIL");
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
	int i = 0;

	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_OSM_TRACES, QObject::tr("OpenStreetMap Traces"));

	Preferences::register_parameter_instance(prefs[i], SGVariant("", prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs[i], SGVariant("", prefs[i].type_id));
	i++;
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
				  const QString & file_full_path,
				  const char *filename,
				  const char *description,
				  const char *tags,
				  const OsmTraceVisibility * visibility)
{
	CURL *curl;
	CURLcode res;
	char curl_error_buffer[CURL_ERROR_SIZE];
	struct curl_slist *headers = NULL;
	struct curl_httppost *post=NULL;
	struct curl_httppost *last=NULL;

	const QString base_url = "http://www.openstreetmap.org/api/0.6/gpx/create";

	/* TODO_LATER: why do we pass user and password to this function if we create user_pass here? */
	const QString user_pass = osm_get_current_credentials();

	int result = 0; /* Default to it worked! */

	qDebug() << "DD: OSM Traces:" << __FUNCTION__ << user << password << file_full_path << filename << description << tags;

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
		     CURLFORM_COPYCONTENTS, visibility->apistr, CURLFORM_END);
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "file",
		     CURLFORM_FILE, file_full_path.toUtf8().constData(), /* TODO_LATER: verify whether we can pass string like that. */
		     CURLFORM_FILENAME, filename,
		     CURLFORM_CONTENTTYPE, "text/xml", CURLFORM_END);

	/* Prepare request. */
	/* As explained in http://wiki.openstreetmap.org/index.php/User:LA2 */
	/* Expect: header seems to produce incompatibilites between curl and httpd. */
	headers = curl_slist_append(headers, "Expect: ");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_URL, base_url.toUtf8().constData());
	curl_easy_setopt(curl, CURLOPT_USERPWD, user_pass.toUtf8().constData());
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
	curl_formfree(post);
	curl_easy_cleanup(curl);

	return result;
}




/**
   Uploading function executed by the background" thread.
*/
void OSMTracesInfo::run(void)
{
	/* Due to OSM limits, we have to enforce ele and time fields
	   also don't upload invisible tracks. */
	static GPXWriteOptions options(true, true, false, false);

	QString filename;

	/* Writing gpx file. */
	if (this->trk != NULL) {
		/* Upload only the selected track. */
		if (this->anonymize_times) {
			Track * trk2 = new Track(*this->trk);
			trk2->anonymize_times();
			filename = GPX::write_track_tmp_file(trk2, &options);
			trk2->free();
		} else {
			filename = GPX::write_track_tmp_file(this->trk, &options);
		}
	} else {
		/* Upload the whole LayerTRW. */
		filename = GPX::write_tmp_file(this->trw, &options);
	}

	if (filename.isEmpty()) {
		return;
	}

	/* Finally, upload it. */
	int ans = osm_traces_upload_file(osm_user, osm_password, filename, this->name.toUtf8().constData(), this->description.toUtf8().constData(), this->tags.toUtf8().constData(), this->visibility);

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
	Window * w = this->trw->get_window();
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
	int ret = 0;
	if (!QFile::remove(filename)) {
		qDebug() << "EE: OSM Traces: failed to remove temporary file" << filename;
		ret = -1;
	}
	return;
}




void SlavGPS::osm_fill_credentials_widgets(QLineEdit & user_entry, QLineEdit & password_entry)
{
	const QString default_user = get_default_user();
	const QString pref_user = Preferences::get_param_value(PREFERENCES_NAMESPACE_OSM_TRACES "username").val_string;
	const QString pref_password = Preferences::get_param_value(PREFERENCES_NAMESPACE_OSM_TRACES "password").val_string;


	if (!osm_user.isEmpty()) {
		user_entry.setText(osm_user);
	} else if (!pref_user.isEmpty()) {
		user_entry.setText(pref_user);
	} else if (!default_user.isEmpty()) {
		user_entry.setText(default_user);
	}

	if (!osm_password.isEmpty()) {
		password_entry.setText(osm_password);
	} else if (!pref_password.isEmpty()) {
		password_entry.setText(pref_password);
	}
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
					       "Enter the password you use to login into www.openstreetmap.org."));
	password_entry->setEchoMode(QLineEdit::Password);
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
	for (const OsmTraceVisibility * vis = trace_visibilities; !vis->combostr.isEmpty(); vis++) {
		visibility_combo->addItem(vis->combostr);
	}

	if (g_last_visibility_index == INVALID_ENTRY_INDEX) {
		g_last_visibility_index = find_initial_visibility_index();
	}
	/* After this the index is valid. */

	visibility_combo->setCurrentIndex(g_last_visibility_index);
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
		OSMTracesInfo * info = new OSMTracesInfo(trw, trk);
		info->name        = name_entry->text();
		info->description = description_entry->text();
		/* TODO_LATER Normalize tags: they will be used as URL part. */
		info->tags        = tags_entry->text();
		info->visibility  = &trace_visibilities[visibility_combo->currentIndex()];

		if (trk != NULL && anonymize_checkbutton != NULL) {
			info->anonymize_times = anonymize_checkbutton->isChecked();
		} else {
			info->anonymize_times = false;
		}

		/* Save visibility value for default reuse. */
		g_last_visibility_index = visibility_combo->currentIndex();
		ApplicationState::set_string(VIK_SETTINGS_OSM_TRACE_VIS, trace_visibilities[g_last_visibility_index].apistr);

		const QString job_description = QObject::tr("Uploading %1 to OSM").arg(info->name);
		info->set_description(job_description);

		info->run_in_background(ThreadPoolType::Remote);
	}
}




int find_initial_visibility_index(void)
{
	QString visibility;
	if (ApplicationState::get_string(VIK_SETTINGS_OSM_TRACE_VIS, visibility)) {
		/* Use setting. */
	} else {
		/* Default to this value if necessary. */
		visibility = "identifiable";
	}

	int entry_index = INVALID_ENTRY_INDEX;
	bool found = false;
	if (!visibility.isEmpty()) {
		for (const OsmTraceVisibility * vis = trace_visibilities; !vis->apistr.isEmpty(); vis++) {
			entry_index++;
			if (QString(vis->apistr) == visibility) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		/* First entry in trace_visibilities. */
		entry_index = 0;
	}

	return entry_index;
}
