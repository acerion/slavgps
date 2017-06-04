/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (c) 2015, Rob Norris <rw_norris@hotmail.com>
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


#include <cstdlib>
#include <list>

#include <glib.h>
#include <glib/gi18n.h>

#include "background.h"
#include "settings.h"
#include "util.h"
#include "math.h"
#include "uibuilder_qt.h"
#include "globals.h"
#if 0
#include "preferences.h"
#endif
#include "window.h"




using namespace SlavGPS;




enum {
	RoleBackgroundData = Qt::UserRole + 1
};




static BackgroundWindow * bgwindow = NULL;

static GThreadPool *thread_pool_remote = NULL;
static GThreadPool *thread_pool_local = NULL;
#ifdef HAVE_LIBMAPNIK
static GThreadPool *thread_pool_local_mapnik = NULL;
#endif
static bool stop_all_threads = false;

/* Still only actually updating the statusbar though. */
static std::list<Window *> windows_to_update;

static int bgitemcount = 0;




enum {
	TITLE_COLUMN = 0,
	PROGRESS_COLUMN,
	DATA_COLUMN,
	N_COLUMNS,
};




class BackgroundProgress : public QStyledItemDelegate {
public:
	BackgroundProgress(QObject * parent_object) : QStyledItemDelegate(parent_object) {};
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};




static void background_thread_update()
{
	for (auto i = windows_to_update.begin(); i != windows_to_update.end(); i++) {
		(*i)->statusbar_update(StatusBarField::ITEMS, QString("%1 items").arg(bgitemcount));
	}

	return;
}




/**
 * @callbackdata: Thread data
 * @progress:     The value should be between 0 and 100 indicating percentage of the task complete
 */
int a_background_thread_progress(BackgroundJob * bg_job, int progress)
{
	int res = a_background_testcancel(bg_job);

	if (bg_job->index->isValid()) {
		bg_job->progress = progress;

		//gtk_list_store_set(GTK_LIST_STORE(bgstore), bg_job->index, PROGRESS_COLUMN, myfraction * 100, -1);
	}

	bg_job->n_items--;
	bgitemcount--;
	background_thread_update();

	return res;
}




static void thread_die(BackgroundJob * bg_job)
{
	if (bg_job->n_items) {
		bgitemcount -= bg_job->n_items;
		background_thread_update();
	}

	delete bg_job->index;
	bg_job->index = NULL;

	delete bg_job;
}




int a_background_testcancel(BackgroundJob * bg_job)
{
	if (stop_all_threads) {
		return -1;
	}

	if (bg_job && bg_job->remove_from_list) {
		bg_job->cleanup_on_cancel();
		return -1;
	}
	return 0;
}




static void thread_helper(void * job_, void * unused_user_data)
{
	BackgroundJob * bg_job = (BackgroundJob *) job_;

	qDebug() << "II: Background: helper function: starting worker function";

	bg_job->thread_fn(bg_job);

	qDebug() << "II: Background: helper function: worker function returned, remove_from_list =" << bg_job->remove_from_list << ", job index = " << bg_job->index << bg_job->index->isValid();

	if (bg_job && bg_job->remove_from_list) {
		if (bg_job->index && bg_job->index->isValid()) {
			qDebug() << "II: Background: removing job from list";
			bgwindow->model->removeRow(bg_job->index->row());
		}
	}

	thread_die(bg_job);
}




/**
   @brief Run a thread function in background

   @bg_job: data for thread function (contains pointer to the function)
   @pool_type: Which pool this thread should run in
   @job_description:
*/
void a_background_thread(BackgroundJob * bg_job, ThreadPoolType pool_type, const QString & job_description)
{
	qDebug() << "II: Background: creating background thread" << job_description;

	bg_job->remove_from_list = true;

	bg_job->progress = 0;
	bg_job->index = bgwindow->insert_job(job_description, bg_job);

	bg_job = bg_job;

	bgitemcount += bg_job->n_items;

	/* Run the thread in the background. */
	if (pool_type == ThreadPoolType::REMOTE) {
		g_thread_pool_push(thread_pool_remote, bg_job, NULL);
#ifdef HAVE_LIBMAPNIK
	} else if (pool_type == ThreadPoolType::LOCAL_MAPNIK) {
		g_thread_pool_push(thread_pool_local_mapnik, bg_job, NULL);

#endif
	} else {
		g_thread_pool_push(thread_pool_local, bg_job, NULL);
	}
}




/**
 * Display the background jobs window.
 */
void a_background_show_window()
{
	bgwindow->show();

}




void BackgroundWindow::remove_job(QStandardItem * item)
{
	QStandardItem * parent_item = this->model->invisibleRootItem();
	QStandardItem * child = parent_item->child(item->row(), PROGRESS_COLUMN);

	BackgroundJob * bg_job = (BackgroundJob *) child->data(RoleLayerData).toULongLong();

	if (bg_job->index && bg_job->index->isValid()) {
		qDebug() << "II: Background: removing job" << parent_item->child(item->row(), TITLE_COLUMN)->text();

		bg_job->remove_from_list = false;
		this->model->removeRow(bg_job->index->row());
	}
}




#define VIK_SETTINGS_BACKGROUND_MAX_THREADS "background_max_threads"
#define VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL "background_max_threads_local"

#ifdef HAVE_LIBMAPNIK
ParameterScale params_threads[] = { {1, 64, 1, 0} }; /* 64 threads should be enough for anyone... */
/* Implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues. */
static Parameter prefs_mapnik[] = {
	{ 0, "mapnik.background_max_threads_local_mapnik", ParameterType::UINT, VIK_LAYER_GROUP_NONE, N_("Threads:"), WidgetType::SPINBUTTON, params_threads, NULL, N_("Number of threads to use for Mapnik tasks. You need to restart Viking for a change to this value to be used"), NULL, NULL, NULL },
};
#endif




/**
 * Just setup any preferences.
 */
void a_background_init()
{
#if 0
#ifdef HAVE_LIBMAPNIK
	ParameterValue tmp;
	/* Implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues. */
	tmp.u = 1; /* Default to 1 thread due to potential crashing issues. */
	a_preferences_register(&prefs_mapnik[0], tmp, "mapnik");
#endif
#endif
}




/**
 * Initialize background feature.
 */
void a_background_post_init()
{
	/* Initialize thread pools. */
	int max_threads = 10;  /* Limit maximum number of threads running at one time. */
	int maxt;
	if (a_settings_get_integer(VIK_SETTINGS_BACKGROUND_MAX_THREADS, &maxt)) {
		max_threads = maxt;
	}

	thread_pool_remote = g_thread_pool_new((GFunc) thread_helper, NULL, max_threads, false, NULL);

	if (a_settings_get_integer(VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL, &maxt)) {
		max_threads = maxt;
	} else {
		unsigned int cpus = util_get_number_of_cpus();
		max_threads = cpus > 1 ? cpus-1 : 1; /* Don't use all available CPUs! */
	}

	thread_pool_local = g_thread_pool_new((GFunc) thread_helper, NULL, max_threads, false, NULL);
#if 0
#ifdef HAVE_LIBMAPNIK
	/* Implicit use of 'MAPNIK_PREFS_NAMESPACE' to avoid dependency issues. */
	unsigned int mapnik_threads = a_preferences_get("mapnik.background_max_threads_local_mapnik")->u;
	thread_pool_local_mapnik = g_thread_pool_new((GFunc) thread_helper, NULL, mapnik_threads, false, NULL);
#endif

	/* Don't destroy win. */
	g_signal_connect(G_OBJECT(bgwindow), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
#endif
}




void a_background_post_init_window(QWidget * parent_widget)
{
	bgwindow = new BackgroundWindow(parent_widget);
}




/**
 * Uninitialize background feature.
 */
void a_background_uninit()
{
	stop_all_threads = true;
	/* Wait until these threads stop. */
	g_thread_pool_free(thread_pool_remote, true, true);
	/* Don't wait for these. */
	g_thread_pool_free(thread_pool_local, true, false);
#ifdef HAVE_LIBMAPNIK
	g_thread_pool_free(thread_pool_local_mapnik, true, false);
#endif
#if 0
	gtk_list_store_clear(bgstore);
	g_object_unref(bgstore);
#endif

	delete bgwindow;
}




void a_background_add_window(Window * window)
{
	windows_to_update.push_front(window);
}




void a_background_remove_window(Window * window)
{
	windows_to_update.remove(window);
}




BackgroundWindow::BackgroundWindow(QWidget * parent_widget) : QDialog(parent_widget)
{
	this->button_box = new QDialogButtonBox();
	this->close = this->button_box->addButton("&Close", QDialogButtonBox::ActionRole);
	this->close->setDefault(true);
	this->remove_selected = this->button_box->addButton("Remove &Selected", QDialogButtonBox::ActionRole);
	this->remove_all = this->button_box->addButton("Remove &All",  QDialogButtonBox::ActionRole);


	this->model = new QStandardItemModel();
	this->model->setHorizontalHeaderItem(TITLE_COLUMN, new QStandardItem("Job"));
	this->model->setHorizontalHeaderItem(PROGRESS_COLUMN, new QStandardItem("Progress"));


	this->view = new QTableView();
	this->view->setSelectionBehavior(QAbstractItemView::SelectRows); /* Single click in a cell selects whole row. */
	this->view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	this->view->verticalHeader()->setVisible(false);
	this->view->setWordWrap(false);
	this->view->setTextElideMode(Qt::ElideLeft); /* The most interesting part should be on the right-hand side of text. */
	this->view->setShowGrid(false);
	this->view->setModel(this->model);
	this->view->show();


	this->view->horizontalHeader()->setSectionHidden(DATA_COLUMN, true);
	this->view->horizontalHeader()->setDefaultSectionSize(200);
	this->view->horizontalHeader()->setSectionResizeMode(TITLE_COLUMN, QHeaderView::Interactive);
	this->view->horizontalHeader()->setStretchLastSection(true);


	BackgroundProgress * delegate = new BackgroundProgress(this);
	this->view->setItemDelegateForColumn(PROGRESS_COLUMN, delegate);


	this->vbox = new QVBoxLayout;
	this->vbox->addWidget(this->view);
	this->vbox->addWidget(this->button_box);


	QLayout * old = this->layout();
	delete old;
	this->setLayout(this->vbox);

	this->setWindowTitle("Viking Background Jobs");

#if 1
	/* This artificial string list is just for testing purposes. */
	QStringList job_list;
	job_list << "aa" << "bb" << "cc" << "dd" << "ee" << "ff";
	int value = 0;

	for (auto iter = job_list.begin(); iter != job_list.end(); ++iter) {

		qDebug() << "II: Background: adding to initial list:" << (*iter);

		BackgroundJob * bg_job = new BackgroundJob();
		bg_job->progress = value;
		bg_job->index = this->insert_job(*iter, bg_job);
		qDebug() << "II: Background: added to list an item with index" << bg_job->index->isValid();

		value += 10;
	}
#endif


	connect(this->close, SIGNAL(clicked()), this, SLOT(close_cb()));
	connect(this->remove_selected, SIGNAL(clicked()), this, SLOT(remove_selected_cb()));
	connect(this->remove_all, SIGNAL(clicked()), this, SLOT(remove_all_cb()));

	QItemSelectionModel * selection_model = this->view->selectionModel();
	connect(selection_model, SIGNAL(selectionChanged(const QItemSelection, const QItemSelection)), this, SLOT(remove_selected_state_cb(void)));
	this->remove_selected_state_cb();

	this->resize(QSize(400, 400));
}




void BackgroundWindow::close_cb()
{
	bgwindow->hide();
}




void BackgroundWindow::remove_selected_cb()
{
	QStandardItem * item = NULL;
	QModelIndexList indexes = this->view->selectionModel()->selectedIndexes();

	while (!indexes.isEmpty()) {

		QModelIndex index = indexes.last();
		if (!index.isValid()) {
			continue;
		}
		QStandardItem * item_ = this->model->itemFromIndex(index);
		this->remove_job(item_);

		this->model->removeRows(indexes.last().row(), 1);
		indexes.removeLast();
		indexes = this->view->selectionModel()->selectedIndexes();
	}

	background_thread_update();
}




void BackgroundWindow::remove_all_cb()
{
	QModelIndex parent_index = QModelIndex();
	for (int r = this->model->rowCount(parent_index) - 1; r >= 0; --r) {
		QModelIndex index = this->model->index(r, 0, parent_index);
		QVariant name = model->data(index);
		qDebug() << "II: Background: removing job" << name;

		QStandardItem * item = this->model->itemFromIndex(index);
		this->remove_job(item);
	}

	background_thread_update();
}




void BackgroundWindow::remove_selected_state_cb(void)
{
	QModelIndexList indexes = this->view->selectionModel()->selectedIndexes();
	if (indexes.isEmpty()) {
		this->remove_selected->setEnabled(false);
	} else {
		this->remove_selected->setEnabled(true);
	}
}




void BackgroundWindow::show_window(void)
{
	QItemSelectionModel * selection_model = this->view->selectionModel();
	QModelIndex index = selection_model->currentIndex();
	if (index.isValid()) {
		qDebug() << "II: Background: clearing current selection";
		selection_model->select(index, QItemSelectionModel::Clear | QItemSelectionModel::Deselect);
	}

	this->remove_selected_state_cb();
	this->show();
}




QPersistentModelIndex * BackgroundWindow::insert_job(const QString & message, BackgroundJob* bg_job)
{
	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;

	/* TITLE_COLUMN */
	item = new QStandardItem(QString(message));
	item->setToolTip(QString(message));
	items << item;

	/* PROGRESS_COLUMN */
	item = new QStandardItem();
	variant = QVariant::fromValue((qulonglong) bg_job);
	item->setData(variant, RoleBackgroundData);
	items << item;

	this->model->invisibleRootItem()->appendRow(items);

	/* We generate index of item in second column. Notice that the
	   index is valid only after inserting items (i.e. appending row)
	   into model, that's why we do it right at the end. */
	QPersistentModelIndex * index = new QPersistentModelIndex(this->model->indexFromItem(item));

#if 1
	this->view->setVisible(false);
	this->view->resizeRowsToContents();
	//this->view->resizeColumnsToContents();
	this->view->setVisible(true);
#endif

	return index;
}




/**
   @brief Reimplemented ::paint() method, drawing a progress bar in PROGRESS_COLUMN column
*/
void BackgroundProgress::paint(QPainter * painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	BackgroundJob * bg_job = (BackgroundJob *) index.data(RoleLayerData).toULongLong();

	QRect rect(option.rect.x() + 1, option.rect.y() + 1, option.rect.width() - 2, option.rect.height() - 2);

	/* http://doc.qt.io/qt-5/qabstractitemdelegate.html#details */

        QStyleOptionProgressBar progressBarOption;
        progressBarOption.rect = option.rect;
        progressBarOption.minimum = 0; /* We could get min/max/unit from BackgroundJob for non-percentage progress indicators. */
        progressBarOption.maximum = 100;
        progressBarOption.progress = bg_job->progress;
        progressBarOption.text = QString::number(bg_job->progress) + "%";
        progressBarOption.textVisible = true;

        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
}




BackgroundJob::BackgroundJob()
{

}
