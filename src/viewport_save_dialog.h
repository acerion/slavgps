#ifndef _SG_VIEWPORT_SAVE_DIALOG_H_
#define _SG_VIEWPORT_SAVE_DIALOG_H_




#include <QLabel>
#include <QSpinBox>
#include <QPushButton>




#include "dialog.h"
#include "widget_radio_group.h"




namespace SlavGPS {




	class Viewport;
	class Window;




	class ViewportToImage {
	public:

		enum class FileFormat {
			PNG  = 0,
			JPEG = 1
		};

		enum class SaveMode {
			File, /* png or jpeg. */
			Directory,
			FileKMZ,
		};

		ViewportToImage(Viewport * viewport, ViewportToImage::SaveMode save_mode, Window * window);
		~ViewportToImage();

		bool run_dialog(const QString & title);

		void save_to_image(const QString & full_path, const VikingZoomLevel & viking_zoom_level);
		bool save_to_dir(const QString & full_path, const VikingZoomLevel & viking_zoom_level);
		QString get_full_path(void);


		Viewport * viewport = NULL;
		ViewportToImage::SaveMode save_mode;
		Window * window = NULL;

		/* For storing last selections. */
		int viewport_save_width = 0;
		int viewport_save_height = 0;
		ViewportToImage::FileFormat file_format = ViewportToImage::FileFormat::JPEG;
		int viewport_save_n_tiles_x = 0;
		int viewport_save_n_tiles_y = 0;
	};




	class ViewportSaveDialog : public BasicDialog {
		Q_OBJECT
	public:
		ViewportSaveDialog(QString const & title, Viewport * viewport, QWidget * parent = NULL);
		~ViewportSaveDialog();

		void build_ui(ViewportToImage::SaveMode save_mode, ViewportToImage::FileFormat file_format);
		int get_width(void) const;
		int get_height(void) const;
		ViewportToImage::FileFormat get_image_format(void) const;

	private slots:
		void accept_cb(void);
		void get_size_from_viewport_cb(void);
		void calculate_total_area_cb();

		void handle_changed_width_cb(int w);
		void handle_changed_height_cb(int h);

	private:
		Viewport * viewport = NULL;
		QWidget * parent = NULL;

		QSpinBox * width_spin = NULL;
		QSpinBox * height_spin = NULL;
		QLabel * total_area_label = NULL;
		RadioGroupWidget * output_format_radios = NULL;

		double proportion = 0.0; /* Proportion of width/height dimensions of viewport and of target pixmap. */

	public:
		/* Only used for ViewportToImage::SaveMode::Directory. */
		QSpinBox * tiles_width_spin = NULL;
		QSpinBox * tiles_height_spin = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_SAVE_DIALOG_H_ */
