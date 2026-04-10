#include <gtk/gtk.h>
#include <stdlib.h>
#include <xbps.h>
#include <locale.h>
#include <unistd.h>
#include "window.h"

int main(int argc, char **argv)
{
    struct xbps_handle xhp = {0};
    GtkApplication *application;
    int status;

    char exe_path[512];
    char app_dir[512];
    char *dir = NULL;

    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len != -1) {
        exe_path[len] = '\0';
        dir = g_path_get_dirname(exe_path);
        g_snprintf(app_dir, sizeof(app_dir), "%s/locale", dir);
        g_setenv("XBPS_LOCALE_DIR", app_dir, FALSE);
    }

    setlocale(LC_ALL, "");
    load_translations(dir);
    g_free(dir);

    if (xbps_init(&xhp) != 0) {
        g_printerr("Failed to initialize XBPS: %s\n", strerror(errno));
        free_translations();
        return 1;
    }

    application = gtk_application_new("org.voidlinux.xbps-gtk", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), &xhp);

    status = g_application_run(G_APPLICATION(application), argc, argv);

    xbps_end(&xhp);
    free_translations();
    return status;
}