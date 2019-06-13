#ifndef _SG_VIEWPORT_TO_IMAGE_H_
#define _SG_VIEWPORT_TO_IMAGE_H_




#include <QLabel>
#include <QSpinBox>
#include <QPushButton>




#include "dialog.h"
#include "widget_radio_group.h"
#include "viewport_zoom.h"




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

		QString get_destination_full_path(void);
		sg_ret save_to_destination(const QString & full_path);

	private:
		sg_ret save_to_image(const QString & full_path);
		sg_ret save_to_dir(const QString & full_path);

		Viewport * viewport = NULL;
		ViewportToImage::SaveMode save_mode;
		Window * window = NULL;

		/* For storing last selections. */
		int scaled_width = 0;    /* Width of target image. */
		int scaled_height = 0;   /* Height of target image. */
		ViewportToImage::FileFormat file_format = ViewportToImage::FileFormat::JPEG;
		int n_tiles_x = 0;
		int n_tiles_y = 0;

		VikingScale original_viking_scale;  /* Viking scale of original viewport. */
		VikingScale scaled_viking_scale;    /* Viking scale of scaled viewport. */
	};




	class ViewportSaveDialog : public BasicDialog {
		Q_OBJECT
	public:
		ViewportSaveDialog(QString const & title, Viewport * viewport, QWidget * parent = NULL);
		~ViewportSaveDialog();

		void build_ui(ViewportToImage::SaveMode save_mode, ViewportToImage::FileFormat file_format);
		void get_scaled_parameters(int & width, int & height, VikingScale & scale) const;
		ViewportToImage::FileFormat get_image_format(void) const;

		int get_n_tiles_x(void) const;
		int get_n_tiles_y(void) const;


	private slots:
		void accept_cb(void);
		void get_size_from_viewport_cb(void);
		void calculate_total_area_cb();

		void handle_changed_width_cb(int w);
		void handle_changed_height_cb(int h);

	private:
		Viewport * viewport = NULL;

		QSpinBox * width_spin = NULL;
		QSpinBox * height_spin = NULL;
		QLabel * total_area_label = NULL;
		RadioGroupWidget * output_format_radios = NULL;

		/* Proportion of width/height dimensions of viewport
		   (original viewport and scaled viewport).
		   p = w/h. */
		double proportion = 0.0;

		/* Width of original viewport. */
		int original_width = 0;

		VikingScale original_viking_scale;  /* Viking scale of original viewport. */

		/* Only used for ViewportToImage::SaveMode::Directory. */
		QSpinBox * tiles_width_spin = NULL;
		QSpinBox * tiles_height_spin = NULL;
	};




} /* namespace SlavGPS */




#endif /* #ifndef _SG_VIEWPORT_TO_IMAGE_H_ */
