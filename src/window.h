#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtk.h>
#include <xbps.h>

typedef struct _XbpsGtkPackage {
    char *name;
    char *version;
    char *description;
    char *repo;
    gboolean installed;
    gboolean updatable;
    char *new_version;
} XbpsGtkPackage;

typedef struct {
    GtkWidget *window;
    GtkWidget *search_entry;
    GtkWidget *packages_list;
    GtkWidget *detail_box;
    GtkWidget *sync_btn;
    GtkWidget *status_label;
    GtkWidget *install_btn;
    GtkWidget *remove_btn;
    GtkWidget *update_btn;
    GtkWidget *detail_name;
    GtkWidget *detail_version;
    GtkWidget *detail_desc;
    GtkWidget *detail_repo;
    GtkListStore *package_store;

    struct xbps_handle *xhp;
} XbpsGtkApp;

void load_translations(void);
void free_translations(void);
const char *_(const char *msg);

void on_activate(GtkApplication *app, gpointer data);
void load_installed_packages(XbpsGtkApp *app);
void load_available_packages(XbpsGtkApp *app);
void search_packages(XbpsGtkApp *app, const char *pattern);
void show_package_details(XbpsGtkApp *app, XbpsGtkPackage *pkg);

#endif