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
 */




#include <mutex>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <time.h>




#include <QDebug>




#include <curl/curl.h>
#include <curl/easy.h>




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




#define SG_MODULE "OSM Traces"




extern bool vik_verbose;




/* Params will be osm_traces.username, osm_traces.password */
/* We have to make sure these don't collide. */
#define PREFERENCES_NAMESPACE_OSM_TRACES "osm_traces."

#define VIK_SETTINGS_OSM_TRACE_VIS "osm_trace_visibility"

#define INVALID_VISIBILITY_INDEX -1

/* Notice the https protocol. */
#define OSM_TRACES_SERVICE_URL   "https://www.openstreetmap.org/api/0.6/gpx/create"




/* Index of the last visibility selected. */
static int g_last_visibility_index = INVALID_VISIBILITY_INDEX;

/* Login to use for OSM uploading. */
static QString g_osm_user;

/* Password to use for OSM uploading. */
static QString g_osm_password;

/* Mutex to protect authentication token. */
static std::mutex g_login_mutex;




/**
   Different type of trace visibility.
*/
class OsmTraceVisibility {
public:
	OsmTraceVisibility(const QString & c, const QString & a) : combostr(c), apistr(a) {};
	const QString combostr;
	const QString apistr;
};

static const std::vector<OsmTraceVisibility> trace_visibilities = {
	OsmTraceVisibility(QObject::tr("Identifiable (public w/ timestamps)"), "identifiable"),
	OsmTraceVisibility(QObject::tr("Trackable (private w/ timestamps)"),   "trackable"),
	OsmTraceVisibility(QObject::tr("Public"),                              "public"),
	OsmTraceVisibility(QObject::tr("Private"),                             "private")
};




static int find_initial_visibility_index(void);




class OSMTracesUpload : public BackgroundJob {
public:
	OSMTracesUpload(LayerTRW * trw, Track * trk);
	~OSMTracesUpload();

	void run(void);

	QString file_name;
	QString description;
	QString tags;
	QString visibility_apistr;
	bool anonymize_times = false; /* ATM only available on a single track. */

	LayerTRW * trw = NULL;
	Track * trk = NULL;
private:
	int upload_file(const QString & file_full_path);
};




static ParameterSpecification prefs[] = {
	{ 0, PREFERENCES_NAMESPACE_OSM_TRACES "username", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("OSM username:"), WidgetType::Entry,    NULL, NULL, "" },
	{ 1, PREFERENCES_NAMESPACE_OSM_TRACES "password", SGVariantType::String, PARAMETER_GROUP_GENERIC, QObject::tr("OSM password:"), WidgetType::Password, NULL, NULL, "" },
	{ 2,                                  "",         SGVariantType::Empty,  PARAMETER_GROUP_GENERIC, "",                           WidgetType::None,     NULL, NULL, "" } /* Guard. */
};




OSMTracesUpload::OSMTracesUpload(LayerTRW * new_trw, Track * new_trk)
{
	this->n_items = 1;

	this->trw = new_trw;
	this->trk = new_trk;
}




OSMTracesUpload::~OSMTracesUpload()
{
	this->trw->unref_layer();
	this->trw = NULL;
}




static QString get_default_user(void)
{
	return qgetenv("EMAIL");
}




void OSMTraces::save_current_credentials(const QString & user, const QString & password)
{
	g_login_mutex.lock();

	g_osm_user = user;
	g_osm_password = password;

	g_login_mutex.unlock();
}




QString OSMTraces::get_current_credentials(void)
{
	QString user_pass;
	g_login_mutex.lock();
	user_pass = QString("%1:%2").arg(g_osm_user).arg(g_osm_password);
	g_login_mutex.unlock();
	return user_pass;
}




/* Module initialization. */
void OSMTraces::init(void)
{
	int i = 0;

	Preferences::register_parameter_group(PREFERENCES_NAMESPACE_OSM_TRACES, QObject::tr("OpenStreetMap Traces"));

	Preferences::register_parameter_instance(prefs[i], SGVariant("", prefs[i].type_id));
	i++;
	Preferences::register_parameter_instance(prefs[i], SGVariant("", prefs[i].type_id));
	i++;
}




void OSMTraces::uninit(void)
{
}




/*
  Upload a file.
  Returns a basic status:
  < 0  : curl error
  == 0 : OK
  > 0  : HTTP error
*/
int OSMTracesUpload::upload_file(const QString & file_full_path)
{
	const QString user_pass = OSMTraces::get_current_credentials();

	qDebug() << SG_PREFIX_D << file_full_path << this->file_name << this->description << this->tags;

	CURL * curl = curl_easy_init();
	struct curl_httppost * post = NULL;
	struct curl_httppost * last = NULL;

	/* Filling the form. */
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "description",
		     CURLFORM_COPYCONTENTS, this->description.toUtf8().constData(), CURLFORM_END);
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "tags",
		     CURLFORM_COPYCONTENTS, this->tags.toUtf8().constData(), CURLFORM_END);
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "visibility",
		     CURLFORM_COPYCONTENTS, this->visibility_apistr, CURLFORM_END);
	curl_formadd(&post, &last,
		     CURLFORM_COPYNAME, "file",
		     CURLFORM_FILE, file_full_path.toUtf8().constData(), /* TODO_LATER: verify whether we can pass string like that. */
		     CURLFORM_FILENAME, this->file_name.toUtf8().constData(),
		     CURLFORM_CONTENTTYPE, "text/xml", CURLFORM_END);

	/* Prepare request. */
	/* As explained in http://wiki.openstreetmap.org/index.php/User:LA2 */
	/* Expect: header seems to produce incompatibilites between curl and httpd. */
	char curl_error_buffer[CURL_ERROR_SIZE];
	struct curl_slist * headers = NULL;
	headers = curl_slist_append(headers, "Expect: ");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_URL, OSM_TRACES_SERVICE_URL);
	curl_easy_setopt(curl, CURLOPT_USERPWD, user_pass.toUtf8().constData());
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_buffer);
	if (vik_verbose) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	}

	/* Execute request. */
	int result = 0; /* Default to it worked! */
	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK) {
		long code;
		res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (res == CURLE_OK){
			qDebug() << SG_PREFIX_D << "Received valid curl response:" << code;
			if (code != 200) {
				qDebug() << SG_PREFIX_W << "Failed to upload data: HTTP response is" << code;
				result = code;
			}
		} else {
			qDebug() << SG_PREFIX_E << "curl_easy_getinfo failed:" << res;
			result = -1;
		}
	} else {
		qDebug() << SG_PREFIX_W << "curl request failed:" << curl_error_buffer;
		result = -2;
	}

	/* Memory. */
	curl_formfree(post);
	curl_easy_cleanup(curl);

	return result;
}




/**
   Uploading function executed in the background thread
*/
void OSMTracesUpload::run(void)
{
	/* Due to OSM limits, we have to enforce ele and time fields
	   also don't upload invisible tracks. */
	static GPXWriteOptions options(true, true, false, false);

	QString file_full_path;
	SaveStatus save_status;

	/* Writing gpx file. */
	if (this->trk != NULL) {
		/* Upload only the selected track. */
		if (this->anonymize_times) {
			/* Constructor only copies properties... */
			Track anonymous(*this->trk);
			/* therefore we have to do deep copy of trackpoints explicitly. */
			anonymous.copy_trackpoints_from(this->trk->begin(), this->trk->end());
			anonymous.anonymize_times();
			save_status = GPX::write_track_to_tmp_file(file_full_path, &anonymous, &options);
		} else {
			save_status = GPX::write_track_to_tmp_file(file_full_path, this->trk, &options);
		}
	} else {
		/* Upload the whole LayerTRW. */
		save_status = GPX::write_layer_to_tmp_file(file_full_path, this->trw, &options);
	}

	if (SaveStatus::Code::Success != save_status) {
		return;
	}

	/* Finally, upload it. */
	int ans = this->upload_file(file_full_path);

	/* Show result in statusbar or failure in dialog for user feedback. */

	/* Get current time to put into message to show when result was generated
	   since need to show difference between operations (when displayed on statusbar).
	   NB If on dialog then don't need time. */
	time_t timenow;
	time(&timenow);
	struct tm * timeinfo = localtime(&timenow);
	char timestr[80];
	/* Compact time only - as days/date isn't very useful here. */
	strftime(timestr, sizeof(timestr), "%X)", timeinfo);

	/* Test to see if window it was invoked on is still valid.
	   Not sure if this test really works! (i.e. if the window was closed in the mean time). */
	Window * window = this->trw->get_window();
	if (window) {
		QString msg;
		if (ans == 0) {
			/* Success. */
			msg = tr("Uploaded to OSM (@%1)").arg(timestr);
		} else if (ans < 0) {
			msg = tr("Failed to upload data to OSM: curl problem (@%1)").arg(timestr);
		} else {
			msg = tr("Failed to upload data to OSM: HTTP response code %3 (@%4)").arg(ans).arg(timestr);
		}
		window->statusbar_update(StatusBarField::Info, msg);
	}

	/* Removing temporary file. */
	int ret = 0;
	if (!QFile::remove(file_full_path)) {
		qDebug() << SG_PREFIX_E << "Failed to remove temporary file" << file_full_path;
		ret = -1;
	}
	return;
}




void OSMTraces::fill_credentials_widgets(QLineEdit & user_entry, QLineEdit & password_entry)
{
	const QString default_user = get_default_user();
	const QString pref_user = Preferences::get_param_value(PREFERENCES_NAMESPACE_OSM_TRACES "username").val_string;
	const QString pref_password = Preferences::get_param_value(PREFERENCES_NAMESPACE_OSM_TRACES "password").val_string;


	if (!g_osm_user.isEmpty()) {
		user_entry.setText(g_osm_user);
	} else if (!pref_user.isEmpty()) {
		user_entry.setText(pref_user);
	} else if (!default_user.isEmpty()) {
		user_entry.setText(default_user);
	}

	if (!g_osm_password.isEmpty()) {
		password_entry.setText(g_osm_password);
	} else if (!pref_password.isEmpty()) {
		password_entry.setText(pref_password);
	}
}




/**
   @brief Upload a TRW layer or a track from TRW layer

   @param trw - TRW layer to upload
   @param trk - if not NULL, the track to upload
*/
void OSMTraces::upload_trw_layer(LayerTRW * trw, Track * trk)
{
	BasicDialog dialog(trw->get_window());
	dialog.setWindowTitle(QObject::tr("OSM upload"));


	int row = 0;


	QLabel  * user_label = new QLabel(QObject::tr("Email:"), &dialog);
	QLineEdit * user_entry = new QLineEdit(&dialog);
	user_entry->setToolTip(QObject::tr("The email used as login\n"
					   "Enter the email you use to login into www.openstreetmap.org"));
	dialog.grid->addWidget(user_label, row, 0);
	dialog.grid->addWidget(user_entry, row, 1);
	row++;



	QLabel * password_label = new QLabel(QObject::tr("Password:"), &dialog);
	QLineEdit * password_entry = new QLineEdit(&dialog);
	password_entry->setToolTip(QObject::tr("The password used to login\n"
					       "Enter the password you use to login into www.openstreetmap.org"));
	password_entry->setEchoMode(QLineEdit::Password);
	dialog.grid->addWidget(password_label, row, 0);
	dialog.grid->addWidget(password_entry, row, 1);
	row++;



	OSMTraces::fill_credentials_widgets(*user_entry, *password_entry);


	QLabel * name_label = new QLabel(QObject::tr("File's name:"), &dialog);
	QLineEdit * name_entry = new QLineEdit(&dialog);
	const QString name = trk ? trk->name : trw->get_name();

	name_entry->setText(name);
	name_entry->setToolTip(QObject::tr("The name of the file on OSM\n"
					   "This is the name of the file created on the server\n"
					   "This is not the name of the local file"));
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
		description = md ? md->description : "";
	}
	if (!description.isEmpty()) {
		description_entry->setText(description);
	}

	description_entry->setToolTip(QObject::tr("The description of the trace"));
	dialog.grid->addWidget(description_label, row, 0);
	dialog.grid->addWidget(description_entry, row, 1);
	row++;



	QCheckBox * anonymize_checkbutton = NULL;
	if (trk != NULL) {
		QLabel * label = new QLabel(QObject::tr("Anonymize Times:"));
		anonymize_checkbutton = new QCheckBox();
		anonymize_checkbutton->setToolTip(QObject::tr("Anonymize times of the trace\n"
							      "You may choose to make the trace identifiable, yet mask the actual real time values"));
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
	QComboBox * visibility_combo = new QComboBox();
	for (auto iter = trace_visibilities.begin(); iter != trace_visibilities.end(); iter++) {
		const OsmTraceVisibility & vis = *iter;
		visibility_combo->addItem(vis.combostr);
	}

	if (g_last_visibility_index == INVALID_VISIBILITY_INDEX) {
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

		OSMTraces::save_current_credentials(user_entry->text(), password_entry->text());

		OSMTracesUpload * job = new OSMTracesUpload(trw, trk);
		job->file_name         = name_entry->text();
		job->description       = description_entry->text();
		job->tags              = tags_entry->text(); /* TODO_LATER Normalize tags: they will be used as URL part. */
		job->visibility_apistr = trace_visibilities[visibility_combo->currentIndex()].apistr;
		if (trk != NULL && anonymize_checkbutton != NULL) {
			job->anonymize_times = anonymize_checkbutton->isChecked();
		} else {
			job->anonymize_times = false;
		}

		/* Save visibility value for default reuse. */
		g_last_visibility_index = visibility_combo->currentIndex();
		ApplicationState::set_string(VIK_SETTINGS_OSM_TRACE_VIS, trace_visibilities[g_last_visibility_index].apistr);

		const QString job_description = QObject::tr("Uploading %1 to OSM").arg(job->file_name);
		job->set_description(job_description);

		job->run_in_background(ThreadPoolType::Remote);
	}
}




int find_initial_visibility_index(void)
{
	QString initial_visibility;
	if (ApplicationState::get_string(VIK_SETTINGS_OSM_TRACE_VIS, initial_visibility)) {
		/* Use setting read from ApplicationState. */
	} else {
		/* Default to this value if necessary. */
		initial_visibility = "identifiable";
	}

	int entry_index = INVALID_VISIBILITY_INDEX;
	if (!initial_visibility.isEmpty()) {
		for (auto iter = trace_visibilities.begin(); iter != trace_visibilities.end(); iter++) {
			const OsmTraceVisibility & vis = *iter;
			entry_index++;
			if (vis.apistr == initial_visibility) {
				break;
			}
		}
	}

	if (entry_index == INVALID_VISIBILITY_INDEX) {
		/* First entry in trace_visibilities. */
		entry_index = 0;
	}

	return entry_index;
}
