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
 */




#include <list>
#include <cmath>




#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QThreadPool>




#include "background.h"
#include "application_state.h"
#include "util.h"
#include "ui_builder.h"
#include "window.h"
#include "statusbar.h"
#ifdef K_INCLUDES
#include "preferences.h"
#endif




using namespace SlavGPS;




#define SG_MODULE "Background"
#define PREFIX ": Background:" << __FUNCTION__ << __LINE__ << ">"




enum {
	RoleBackgroundData = Qt::UserRole + 1
};



static Background g_background;




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




BackgroundJob::~BackgroundJob()
{
	qDebug() << "II" PREFIX "destructing job" << this->description << ", job index" << (this->index->isValid() ? "is valid" : "is invalid");

	this->detach_from_window(g_background.bgwindow);

	if (this->n_items) {
		g_background.n_items -= this->n_items;
		g_background.update_status_indication();
	}
}




/**
   @progress: The value should be between 0 and 100 indicating percentage of the task complete

   @return true if job should be ended/terminated
   @return false otherwise
*/
bool BackgroundJob::set_progress_state(int new_progress)
{
	const bool end_job = this->test_termination_condition();

	if (this->index->isValid()) {
		this->progress = new_progress;

		//gtk_list_store_set(GTK_LIST_STORE(bgstore), this->index, PROGRESS_COLUMN, myfraction * 100, -1);
	}

	this->n_items--;
	g_background.n_items--;
	g_background.update_status_indication();

	return end_job;
}




bool BackgroundJob::test_termination_condition(void)
{
	if (Background::test_global_termination_condition()) {
		qDebug() << SG_PREFIX_I << "Background job termination: global stop";
		return true;
	}

#ifdef K_FIXME_RESTORE /* Should we call ::cleanup_on_cancel() in test function? */
	if (this->remove_from_list) {
		this->cleanup_on_cancel();
		qDebug() << "WW" PREFIX << "background job termination: remove from list";
		return true;
	}
#endif
	return false;
}




/**
   @brief Run a job in background

   The job is executed as background thread.

   @pool_type: Which pool this thread should run in
*/
void BackgroundJob::run_in_background(ThreadPoolType pool_type)
{
	qDebug() << "II" PREFIX << "creating background thread for job" << this->description;

	this->remove_from_list = true;

	this->progress = 0;
	g_background.bgwindow->append_job(this);

	g_background.n_items += this->n_items;

	/* Run the thread in the background. */
	qDebug() << "II" PREFIX << "adding job" << this->description << "to thread pool";
	QThreadPool::globalInstance()->start(this);
}




void BackgroundWindow::remove_job(QStandardItem * item)
{
	QStandardItem * parent_item = this->model->invisibleRootItem();
	QStandardItem * child = parent_item->child(item->row(), PROGRESS_COLUMN);

	BackgroundJob * bg_job = (BackgroundJob *) child->data(RoleLayerData).toULongLong();

	if (bg_job) {
		bg_job->detach_from_window(this);
	}
}




void BackgroundJob::detach_from_window(BackgroundWindow * window)
{
	this->mutex.lock();
	if (this->remove_from_list && this->index) {

		qDebug() << "II" PREFIX << "detaching job" << this->description << "from list of jobs";

		if (this->index->isValid()) {
			window->remove_job(this->index);

		}

		this->remove_from_list = false;
		delete this->index;
		this->index = NULL;
	}
	this->mutex.unlock();
}




#define VIK_SETTINGS_BACKGROUND_MAX_THREADS "background_max_threads"
#define VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL "background_max_threads_local"





/**
   Just setup any preferences
*/
void Background::init()
{
}




/**
   Initialize background feature
*/
void Background::post_init(void)
{
	/* Initialize thread pools. TODO_LATER: we don't have local/remote pools anymore. Address this fact in this file. */
	int max_threads = 10;  /* Limit maximum number of threads running at one time. */
	int maxt;
	if (ApplicationState::get_integer(VIK_SETTINGS_BACKGROUND_MAX_THREADS, &maxt)) {
		max_threads = maxt;
	}

	if (ApplicationState::get_integer(VIK_SETTINGS_BACKGROUND_MAX_THREADS_LOCAL, &maxt)) {
		max_threads = maxt;
	} else {
		const int threads = Util::get_number_of_threads();
		max_threads = threads > 1 ? threads - 1 : 1; /* Don't use all available CPUs! */
	}

	qDebug() << "II" PREFIX << "setting threads limit to" << max_threads;
	QThreadPool::globalInstance()->setMaxThreadCount(max_threads);
	QThreadPool::globalInstance()->setExpiryTimeout(-1); /* No expiry. */
}




void Background::post_init_window(QWidget * parent_widget)
{
	g_background.bgwindow = new BackgroundWindow(parent_widget);
}




/**
   Uninitialize background feature
*/
void Background::uninit(void)
{
	g_background.stop_all_threads = true;

	delete g_background.bgwindow;
}



/* Update information about current background jobs, displayed in
   status bar of main window (or multiple main windows, if there is
   more than one). */
void Background::update_status_indication(void)
{
	for (auto iter = g_background.main_windows.begin(); iter != g_background.main_windows.end(); iter++) {
		(*iter)->statusbar_update(StatusBarField::ITEMS, QObject::tr("%n background items", "", g_background.n_items));
	}

	return;
}



bool Background::test_global_termination_condition(void)
{
	if (g_background.stop_all_threads) {
		qDebug() << SG_PREFIX_I << "Job termination: global stop condition is present";
		return true;
	}
	return false;
}




/**
   Display the background jobs window
*/
void Background::show_window()
{
	g_background.bgwindow->show();
}




void Background::add_window(Window * window)
{
	g_background.main_windows.push_front(window);
}




void Background::remove_window(Window * window)
{
	g_background.main_windows.remove(window);
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

		qDebug() << "II" PREFIX << "adding to initial list:" << (*iter);

		BackgroundJob * bg_job = new BackgroundJob();
		bg_job->set_description(*iter);
		bg_job->progress = value;
		this->append_job(bg_job);
		qDebug() << "II" PREFIX << "added to list an item with index" << bg_job->index->isValid();

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
	g_background.bgwindow->hide();
}




void BackgroundWindow::remove_selected_cb()
{
	QModelIndexList indexes = this->view->selectionModel()->selectedIndexes();

	while (!indexes.isEmpty()) {

		QModelIndex index = indexes.last();
		if (!index.isValid()) {
			continue;
		}
		QStandardItem * item = this->model->itemFromIndex(index);
		this->remove_job(item);

		this->model->removeRows(indexes.last().row(), 1);
		indexes.removeLast();
		indexes = this->view->selectionModel()->selectedIndexes();
	}

	g_background.update_status_indication();
}




void BackgroundWindow::remove_all_cb()
{
	QModelIndex parent_index = QModelIndex();
	for (int r = this->model->rowCount(parent_index) - 1; r >= 0; --r) {
		QModelIndex index = this->model->index(r, 0, parent_index);
		QVariant name = model->data(index);
		qDebug() << "II" PREFIX << "removing job" << name;

		QStandardItem * item = this->model->itemFromIndex(index);
		this->remove_job(item);
	}

	g_background.update_status_indication();
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
		qDebug() << "II" PREFIX << "clearing current selection";
		selection_model->select(index, QItemSelectionModel::Clear | QItemSelectionModel::Deselect);
	}

	this->remove_selected_state_cb();
	this->show();
}




void BackgroundWindow::append_job(BackgroundJob * bg_job)
{
	QList<QStandardItem *> items;
	QStandardItem * item = NULL;
	QVariant variant;

	/* TITLE_COLUMN */
	item = new QStandardItem(bg_job->description);
	item->setToolTip(bg_job->description);
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
	bg_job->index = new QPersistentModelIndex(this->model->indexFromItem(item));

#if 1
	this->view->setVisible(false);
	this->view->resizeRowsToContents();
	//this->view->resizeColumnsToContents();
	this->view->setVisible(true);
#endif

	return;
}




void BackgroundWindow::remove_job(QPersistentModelIndex * index)
{
	this->model->removeRow(index->row());
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
