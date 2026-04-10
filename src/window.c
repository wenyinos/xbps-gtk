#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "window.h"

static GHashTable *translations = NULL;

static void find_translations_dir(const char *exe_dir)
{
    const char *lang = getenv("LANG");
    FILE *fp = NULL;
    char line[512];
    char lang_base[32] = "en";

    /* Extract base language (e.g., "zh_CN" from "zh_CN.UTF-8") */
    if (lang && strlen(lang) >= 2) {
        strncpy(lang_base, lang, sizeof(lang_base) - 1);
        lang_base[sizeof(lang_base) - 1] = '\0';
        char *dot = strchr(lang_base, '.');
        if (dot) *dot = '\0';
    }

    translations = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    const char *dirs[4];
    char buf1[512], buf2[512], buf3[512];

    if (exe_dir) {
        snprintf(buf1, sizeof(buf1), "%s/locale", exe_dir);
        snprintf(buf2, sizeof(buf2), "%s/../locale", exe_dir);
        snprintf(buf3, sizeof(buf3), "%s/../../locale", exe_dir);
        dirs[0] = buf1;
        dirs[1] = buf2;
        dirs[2] = buf3;
    } else {
        dirs[0] = "locale";
        dirs[1] = "../locale";
        dirs[2] = "../../locale";
    }
    dirs[3] = NULL;

    for (int i = 0; dirs[i]; i++) {
        char trans_path[512];
        /* Try full locale (e.g., zh_CN.txt) */
        snprintf(trans_path, sizeof(trans_path), "%s/%s.txt", dirs[i], lang_base);
        fp = fopen(trans_path, "r");
        /* Try short locale (e.g., zh.txt) */
        if (!fp && strlen(lang_base) >= 2) {
            snprintf(trans_path, sizeof(trans_path), "%s/%.2s.txt", dirs[i], lang_base);
            fp = fopen(trans_path, "r");
        }
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                char *key, *value;
                line[strcspn(line, "\n")] = 0;
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    key = line;
                    value = eq + 1;
                    if (key[0] && value[0]) {
                        g_hash_table_insert(translations, g_strdup(key), g_strdup(value));
                    }
                }
            }
            fclose(fp);
            return;
        }
    }
}

void load_translations(const char *exe_dir)
{
    find_translations_dir(exe_dir);
}

void free_translations(void)
{
    if (translations) g_hash_table_destroy(translations);
}

const char *_(const char *msg)
{
    if (!translations) return msg;
    const char *trans = g_hash_table_lookup(translations, msg);
    return trans ? trans : msg;
}

typedef struct {
    char *name;
    char *version;
    char *desc;
    char *repo;
    gboolean installed;
} PackageItem;

static GPtrArray *packages = NULL;

static void update_list_view(XbpsGtkApp *app);

static void clear_packages(void)
{
    if (packages) {
        for (guint i = 0; i < packages->len; i++) {
            PackageItem *p = g_ptr_array_index(packages, i);
            g_free(p->name);
            g_free(p->version);
            g_free(p->desc);
            g_free(p->repo);
            g_free(p);
        }
        g_ptr_array_free(packages, TRUE);
    }
    packages = g_ptr_array_new();
}

static void add_pkg(const char *name, const char *version, const char *desc, const char *repo, gboolean installed)
{
    PackageItem *item = g_new(PackageItem, 1);
    item->name = g_strdup(name);
    item->version = g_strdup(version ? version : "-");
    item->desc = g_strdup(desc ? desc : "");
    item->repo = g_strdup(repo);
    item->installed = installed;
    g_ptr_array_add(packages, item);
}

void load_installed_packages(XbpsGtkApp *app)
{
    FILE *fp;
    char buf[1024];

    clear_packages();
    fp = popen("xbps-query -l 2>/dev/null", "r");
    if (!fp) return;

    while (fgets(buf, sizeof(buf), fp)) {
        char *name = NULL, *version = NULL;
        if (sscanf(buf, "[-] %ms %ms", &name, &version) == 2) {
            add_pkg(name, version, "", "installed", TRUE);
            free(name);
            free(version);
        }
    }
    pclose(fp);
    update_list_view(app);
    gtk_label_set_text(GTK_LABEL(app->status_label), _("Ready"));
}

void load_available_packages(XbpsGtkApp *app)
{
    FILE *fp;
    char buf[1024];

    clear_packages();
    fp = popen("xbps-query -Rs '' 2>/dev/null", "r");
    if (!fp) return;

    while (fgets(buf, sizeof(buf), fp)) {
        char *name = NULL, *version = NULL, *desc = NULL;
        if (sscanf(buf, "%ms %ms - %ms", &name, &version, &desc) >= 2) {
            add_pkg(name, version, desc, "remote", FALSE);
            free(name);
            free(version);
            if (desc) free(desc);
        }
    }
    pclose(fp);
    update_list_view(app);
}

void search_packages(XbpsGtkApp *app, const char *pattern)
{
    FILE *fp;
    char buf[1024];
    char cmd[256];

    if (!pattern || strlen(pattern) == 0) {
        load_installed_packages(app);
        return;
    }

    clear_packages();
    snprintf(cmd, sizeof(cmd), "xbps-query -Rs '%s' 2>/dev/null", pattern);
    fp = popen(cmd, "r");
    if (!fp) return;

    while (fgets(buf, sizeof(buf), fp)) {
        char *name = NULL, *version = NULL, *desc = NULL;
        if (sscanf(buf, "%ms %ms - %ms", &name, &version, &desc) >= 2) {
            add_pkg(name, version, desc, "remote", FALSE);
            free(name);
            free(version);
            if (desc) free(desc);
        }
    }
    pclose(fp);
    update_list_view(app);
}

static guint get_pkg_count(void)
{
    return packages ? packages->len : 0;
}

static PackageItem *get_pkg(guint idx)
{
    if (!packages || idx >= packages->len) return NULL;
    return g_ptr_array_index(packages, idx);
}

void show_package_details(XbpsGtkApp *app, XbpsGtkPackage *pkg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s %s", _("Name:"), pkg->name ? pkg->name : _("N/A"));
    gtk_label_set_text(GTK_LABEL(app->detail_name), buf);
    snprintf(buf, sizeof(buf), "%s %s", _("Version:"), pkg->version ? pkg->version : _("N/A"));
    gtk_label_set_text(GTK_LABEL(app->detail_version), buf);
    snprintf(buf, sizeof(buf), "%s %s", _("Description:"), pkg->description ? pkg->description : _("N/A"));
    gtk_label_set_text(GTK_LABEL(app->detail_desc), buf);
    snprintf(buf, sizeof(buf), "%s %s", _("Repository:"), pkg->repo ? pkg->repo : _("N/A"));
    gtk_label_set_text(GTK_LABEL(app->detail_repo), buf);
}

static void update_list_view(XbpsGtkApp *app)
{
    GtkStringList *list = gtk_string_list_new(NULL);
    for (guint i = 0; i < get_pkg_count(); i++) {
        PackageItem *p = get_pkg(i);
        gtk_string_list_append(list, p->name);
    }
    GtkSingleSelection *sel = GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(app->packages_list)));
    g_object_set(sel, "model", list, NULL);
}

static void on_search_changed(GtkEditable *entry, gpointer data)
{
    XbpsGtkApp *app = (XbpsGtkApp *)data;
    const gchar *text = gtk_editable_get_text(entry);
    search_packages(app, text);
    gtk_label_set_text(GTK_LABEL(app->status_label), text[0] ? text : _("Ready"));
}

static void on_sync_clicked(GtkButton *button, gpointer data)
{
    XbpsGtkApp *app = (XbpsGtkApp *)data;
    gtk_label_set_text(GTK_LABEL(app->status_label), _("Syncing repositories..."));
    system("xbps-install -S >/dev/null 2>&1");
    load_available_packages(app);
    gtk_label_set_text(GTK_LABEL(app->status_label), _("Repositories synced"));
}

static void on_pkg_selected(GtkListView *view, guint position, gpointer data)
{
    XbpsGtkApp *app = (XbpsGtkApp *)data;
    PackageItem *item = get_pkg(position);

    if (item) {
        XbpsGtkPackage pkg = { .name = item->name, .version = item->version, .description = item->desc, .repo = item->repo };
        show_package_details(app, &pkg);
        
        gtk_widget_set_sensitive(app->install_btn, item->repo && strcmp(item->repo, "installed") != 0);
        gtk_widget_set_sensitive(app->remove_btn, item->repo && strcmp(item->repo, "installed") == 0);
    }
}

static void on_install_clicked(GtkButton *button, gpointer data)
{
    XbpsGtkApp *app = (XbpsGtkApp *)data;
    GtkSingleSelection *sel = GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(app->packages_list)));
    guint pos = gtk_single_selection_get_selected(sel);

    if (pos != GTK_INVALID_LIST_POSITION) {
        PackageItem *item = get_pkg(pos);
        if (item) {
            gtk_label_set_text(GTK_LABEL(app->status_label), _("Installing..."));
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "xbps-install -y %s >/dev/null 2>&1", item->name);
            system(cmd);
            gtk_label_set_text(GTK_LABEL(app->status_label), _("Installation complete"));
            load_installed_packages(app);
        }
    }
}

static void on_remove_clicked(GtkButton *button, gpointer data)
{
    XbpsGtkApp *app = (XbpsGtkApp *)data;
    GtkSingleSelection *sel = GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(app->packages_list)));
    guint pos = gtk_single_selection_get_selected(sel);

    if (pos != GTK_INVALID_LIST_POSITION) {
        PackageItem *item = get_pkg(pos);
        if (item) {
            gtk_label_set_text(GTK_LABEL(app->status_label), _("Removing..."));
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "xbps-remove -y %s >/dev/null 2>&1", item->name);
            system(cmd);
            gtk_label_set_text(GTK_LABEL(app->status_label), _("Removal complete"));
            load_installed_packages(app);
        }
    }
}

static void create_ui(XbpsGtkApp *app)
{
    GtkBuilder *builder = gtk_builder_new_from_string(
        "<interface>"
        "  <object class='GtkWindow' id='window'>"
        "    <property name='title'>XBPS Package Manager</property>"
        "    <property name='default-width'>900</property>"
        "    <property name='default-height'>650</property>"
        "    <signal name='destroy' handler='gtk_window_destroy'/>"
        "    <child>"
        "      <object class='GtkBox'>"
        "        <property name='orientation'>vertical</property>"
        "        <property name='spacing'>6</property>"
        "        <property name='margin-start'>12</property>"
        "        <property name='margin-end'>12</property>"
        "        <property name='margin-top'>12</property>"
        "        <property name='margin-bottom'>12</property>"
        "        <child>"
        "          <object class='GtkBox'>"
        "            <property name='orientation'>horizontal</property>"
        "            <property name='spacing'>6</property>"
        "            <child>"
        "              <object class='GtkSearchEntry' id='search_entry'>"
        "                <property name='hexpand'>true</property>"
        "                <property name='placeholder-text'>Search packages...</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class='GtkButton' id='sync_btn'>"
        "                <property name='label'>Sync Repos</property>"
        "              </object>"
        "            </child>"
        "          </object>"
        "        </child>"
        "        <child>"
        "          <object class='GtkBox'>"
        "            <property name='orientation'>horizontal</property>"
        "            <property name='spacing'>6</property>"
        "            <property name='vexpand'>true</property>"
        "            <child>"
        "              <object class='GtkListView' id='packages_list'>"
        "                <property name='hexpand'>true</property>"
        "                <property name='vexpand'>true</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class='GtkBox' id='detail_box'>"
        "                <property name='orientation'>vertical</property>"
        "                <property name='spacing'>6</property>"
        "                <property name='width-request'>280</property>"
        "                <child>"
        "                  <object class='GtkLabel'>"
        "                    <property name='label'>Package Details</property>"
        "                    <property name='justify'>center</property>"
        "                  </object>"
        "                </child>"
        "                <child>"
        "                  <object class='GtkLabel' id='detail_name'>"
        "                    <property name='label'>Name:</property>"
        "                    <property name='selectable'>true</property>"
        "                    <property name='wrap'>true</property>"
        "                  </object>"
        "                </child>"
        "                <child>"
        "                  <object class='GtkLabel' id='detail_version'>"
        "                    <property name='label'>Version:</property>"
        "                    <property name='selectable'>true</property>"
        "                  </object>"
        "                </child>"
        "                <child>"
        "                  <object class='GtkLabel' id='detail_desc'>"
        "                    <property name='label'>Description:</property>"
        "                    <property name='selectable'>true</property>"
        "                    <property name='wrap'>true</property>"
        "                  </object>"
        "                </child>"
        "                <child>"
        "                  <object class='GtkLabel' id='detail_repo'>"
        "                    <property name='label'>Repository:</property>"
        "                    <property name='selectable'>true</property>"
        "                  </object>"
        "                </child>"
        "              </object>"
        "            </child>"
        "          </object>"
        "        </child>"
        "        <child>"
        "          <object class='GtkBox'>"
        "            <property name='orientation'>horizontal</property>"
        "            <property name='spacing'>6</property>"
        "            <child>"
        "              <object class='GtkButton' id='install_btn'>"
        "                <property name='label'>Install</property>"
        "                <property name='sensitive'>false</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class='GtkButton' id='remove_btn'>"
        "                <property name='label'>Remove</property>"
        "                <property name='sensitive'>false</property>"
        "              </object>"
        "            </child>"
        "            <child>"
        "              <object class='GtkButton' id='update_btn'>"
        "                <property name='label'>Update</property>"
        "                <property name='sensitive'>false</property>"
        "              </object>"
        "            </child>"
        "            <child><placeholder/></child>"
        "            <child>"
        "              <object class='GtkLabel' id='status_label'>"
        "                <property name='label'>Ready</property>"
        "              </object>"
        "            </child>"
        "          </object>"
        "        </child>"
        "      </object>"
        "    </child>"
        "  </object>"
        "</interface>",
        -1
    );

    gtk_window_set_title(GTK_WINDOW(gtk_builder_get_object(builder, "window")), _("XBPS Package Manager"));
    
    app->window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    app->search_entry = GTK_WIDGET(gtk_builder_get_object(builder, "search_entry"));
    app->packages_list = GTK_WIDGET(gtk_builder_get_object(builder, "packages_list"));
    app->detail_box = GTK_WIDGET(gtk_builder_get_object(builder, "detail_box"));
    app->sync_btn = GTK_WIDGET(gtk_builder_get_object(builder, "sync_btn"));
    app->status_label = GTK_WIDGET(gtk_builder_get_object(builder, "status_label"));
    app->install_btn = GTK_WIDGET(gtk_builder_get_object(builder, "install_btn"));
    app->remove_btn = GTK_WIDGET(gtk_builder_get_object(builder, "remove_btn"));
    app->update_btn = GTK_WIDGET(gtk_builder_get_object(builder, "update_btn"));
    app->detail_name = GTK_WIDGET(gtk_builder_get_object(builder, "detail_name"));
    app->detail_version = GTK_WIDGET(gtk_builder_get_object(builder, "detail_version"));
    app->detail_desc = GTK_WIDGET(gtk_builder_get_object(builder, "detail_desc"));
    app->detail_repo = GTK_WIDGET(gtk_builder_get_object(builder, "detail_repo"));

    gtk_button_set_label(GTK_BUTTON(app->sync_btn), _("Sync Repos"));
    gtk_button_set_label(GTK_BUTTON(app->install_btn), _("Install"));
    gtk_button_set_label(GTK_BUTTON(app->remove_btn), _("Remove"));
    gtk_button_set_label(GTK_BUTTON(app->update_btn), _("Update"));
    gtk_label_set_text(GTK_LABEL(app->status_label), _("Ready"));

    GtkStringList *list = gtk_string_list_new(NULL);
    GtkSingleSelection *sel = gtk_single_selection_new(G_LIST_MODEL(list));
    gtk_list_view_set_model(GTK_LIST_VIEW(app->packages_list), GTK_SELECTION_MODEL(sel));

    g_signal_connect(app->search_entry, "changed", G_CALLBACK(on_search_changed), app);
    g_signal_connect(app->sync_btn, "clicked", G_CALLBACK(on_sync_clicked), app);
    g_signal_connect(app->packages_list, "activate", G_CALLBACK(on_pkg_selected), app);
    g_signal_connect(app->install_btn, "clicked", G_CALLBACK(on_install_clicked), app);
    g_signal_connect(app->remove_btn, "clicked", G_CALLBACK(on_remove_clicked), app);

    g_object_unref(builder);
}

void on_activate(GtkApplication *app, gpointer data)
{
    XbpsGtkApp *xapp = g_new0(XbpsGtkApp, 1);
    xapp->xhp = (struct xbps_handle *)data;

    create_ui(xapp);
    gtk_window_set_application(GTK_WINDOW(xapp->window), app);
    gtk_widget_set_visible(xapp->window, TRUE);

    load_installed_packages(xapp);
}