#ifndef SLAV_QT_H
#define SLAV_QT_H


/* Mock definitions used during migration of SlavGPS to QT. */



#ifdef SLAVGPS_QT
typedef int GdkGC;
typedef int GdkFont;
typedef int GdkColor;
typedef int GtkWindow;
typedef int GtkWidget;
typedef void GdkPixbuf;
typedef void GdkFunction;
typedef int PangoLayout;
typedef int GdkPixmap;
typedef int GtkDrawingArea;
typedef void GObject;
typedef int ui_change_values;
typedef int GtkTreeIter;
typedef int GtkMenu;
typedef int GdkEventButton;
typedef int GdkEventMotion;
typedef int GtkTreePath;
typedef int GdkEventKey;
typedef struct { int*a; int*b; int*c; int*d; int*e; int f; } GtkRadioActionEntry;
typedef int GdkPixdata;
typedef int GdkCursor;
typedef int GdkCursorType;
typedef int GtkTreeDragSource;
typedef int GtkTreeDragDest;
typedef int GtkSelectionData;
typedef int GtkCellRenderer;
typedef int GtkCellRendererToggle;
typedef int GtkCellRendererText;
typedef int GtkCellEditable;
typedef int GtkTooltip;
typedef int GtkTreeSelection;
typedef int GtkTreeModel;


#define GDK_CURSOR_IS_PIXMAP 1

namespace SlavGPS {

	typedef int Track;
	struct track_layer_t {};
	struct waypoint_layer_t {};

}

#endif

#endif
