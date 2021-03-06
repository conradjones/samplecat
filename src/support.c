/**
* +----------------------------------------------------------------------+
* | This file is part of Samplecat. http://ayyi.github.io/samplecat/     |
* | copyright (C) 2007-2014 Tim Orford <tim@orford.org>                  |
* +----------------------------------------------------------------------+
* | This program is free software; you can redistribute it and/or modify |
* | it under the terms of the GNU General Public License version 3       |
* | as published by the Free Software Foundation.                        |
* +----------------------------------------------------------------------+
*
*/
#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/param.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "debug/debug.h"
#include "file_manager/file_manager.h"
#include "dir_tree/typedefs.h"
#include "dir_tree/ui_fileops.h"
#include <gimp/gimpaction.h>
#include <gimp/gimpactiongroup.h>
#include "typedefs.h"
#include "src/support.h"
#include "application.h"

#define HEX_ESCAPE '%'
#define N_(A) A 
#define gettext(A) A


#if 0
/* Used as the sort function for sorting GPtrArrays */
gint 
strcmp2(gconstpointer a, gconstpointer b)
{
	const char *aa = *(char **) a;
	const char *bb = *(char **) b;

	return g_strcasecmp(aa, bb);
}

/* Returns an array listing all the names in the directory 'path'.
 * The array is sorted.
 * '.' and '..' are skipped.
 * On error, the error is reported with g_warning and NULL is returned.
 */
GPtrArray*
list_dir(const guchar *path)
{
	GDir *dir;
	GError *error = NULL;
	const char *leaf;
	GPtrArray *names;
	
	dir = g_dir_open((char*)path, 0, &error);
	if (error)
	{
		g_warning("Can't list directory:\n%s", error->message);
		g_error_free(error);
		return NULL;
	}

	names = g_ptr_array_new();

	while ((leaf = g_dir_read_name(dir))) {
		if (leaf[0] != '.')
			g_ptr_array_add(names, g_strdup(leaf));
	}

	g_dir_close(dir);

	g_ptr_array_sort(names, strcmp2);

	return names;
}
#endif //0

void
colour_get_style_fg(GdkColor* color, GtkStateType state)
{
	//gives the default style foreground colour for the given widget state.

	GtkStyle* style = gtk_widget_get_style(app->window);
	color->red   = style->fg[state].red;
	color->green = style->fg[state].green;
	color->blue  = style->fg[state].blue;
}

void
colour_get_style_bg(GdkColor* color, int state)
{
	//gives the default style foreground colour for the given widget state.

	GtkStyle* style = gtk_widget_get_style(app->window);
	color->red   = style->bg[state].red;
	color->green = style->bg[state].green;
	color->blue  = style->bg[state].blue;
}


void
colour_get_style_base(GdkColor* color, int state)
{
	//gives the default style base colour for the given widget state.

	GtkStyle* style = gtk_widget_get_style(app->window);
	color->red   = style->base[state].red;
	color->green = style->base[state].green;
	color->blue  = style->base[state].blue;
}


void
colour_get_style_text(GdkColor* color, int state)
{
	//gives the default style text colour for the given widget state.

	GtkStyle* style = gtk_widget_get_style(app->window);
	color->red   = style->text[state].red;
	color->green = style->text[state].green;
	color->blue  = style->text[state].blue;
}


gchar*
gdkcolor_get_hexstring(GdkColor* c)
{
	return g_strdup_printf("%02x%02x%02x", c->red >> 8, c->green >> 8, c->blue >> 8);
}


void
hexstring_from_gdkcolor(char* hexstring, GdkColor* c)
{
	snprintf(hexstring, 7, "%02x%02x%02x", c->red >> 8, c->green >> 8, c->blue >> 8);
}

#if NEVER
void
color_rgba_to_gdk(GdkColor* colour, uint32_t rgba)
{
	g_return_if_fail(colour, "colour");

	colour->red   = (rgba & 0xff000000) >> 16;
	colour->green = (rgba & 0x00ff0000) >> 8;
	colour->blue  = (rgba & 0x0000ff00);
}
#endif


gboolean
colour_lighter(GdkColor* lighter, GdkColor* colour)
{
	lighter->red   = MIN(colour->red   * 1.2 + 0x600, 0xffff);
	lighter->green = MIN(colour->green * 1.2 + 0x600, 0xffff);
	lighter->blue  = MIN(colour->blue  * 1.2 + 0x600, 0xffff);

	if(is_white(lighter)) return FALSE; else return TRUE;
}


gboolean
colour_darker(GdkColor* darker, GdkColor* colour)
{
	darker->red   = MAX(colour->red   * 0.8, 0x0000);
	darker->green = MAX(colour->green * 0.8, 0x0000);
	darker->blue  = MAX(colour->blue  * 0.8, 0x0000);

	if(is_black(darker)) return FALSE; else return TRUE;
}


gboolean
is_white(GdkColor* colour)
{
	if(colour->red >= 0xffff && colour->green >= 0xffff && colour->blue >= 0xffff) return TRUE;
	else return FALSE;
}


gboolean
is_black(GdkColor* colour)
{
	if(colour->red < 1 && colour->green < 1 && colour->blue < 1) return TRUE;
	else return FALSE;
}


gboolean
is_dark(GdkColor* colour)
{
	int average = (colour->red + colour->green + colour->blue) / 3;
	return (average < 0x7fff);
}

#if NEVER
gboolean
is_similar(GdkColor* colour1, GdkColor* colour2, int min_diff)
{
	GdkColor difference;
	difference.red   = ABS(colour1->red   - colour2->red);
	difference.green = ABS(colour1->green - colour2->green);
	difference.blue  = ABS(colour1->blue  - colour2->blue);

	if(difference.red + difference.green + difference.blue < (min_diff << 8)){
		dbg(2, "is similar! #%02x%02x%02x = %02x%02x%02x", colour1->red >> 8, colour1->green >> 8, colour1->blue >> 8, colour2->red >> 8, colour2->green >> 8, colour2->blue >> 8);
		return TRUE;
	}
	dbg(2, "not similar #%02x%02x%02x = #%02x%02x%02x", colour1->red >> 8, colour1->green >> 8, colour1->blue >> 8, colour2->red >> 8, colour2->green >> 8, colour2->blue >> 8);

	return FALSE;
}


gboolean
is_similar_rgb(unsigned colour1, unsigned colour2)
{
	char r1 = (colour1 & 0xff000000 ) >> 24; 
	char g1 = (colour1 & 0x00ff0000 ) >> 16; 
	char b1 = (colour1 & 0x0000ff00 ) >>  8; 
	char r2 = (colour1 & 0xff000000 ) >> 24; 
	char g2 = (colour1 & 0x00ff0000 ) >> 16; 
	char b2 = (colour1 & 0x0000ff00 ) >>  8; 

	int d_red = ABS(r1 - r2);
	int d_grn = ABS(g1 - g2);
	int d_blu = ABS(b1 - b2);

	if(d_red + d_grn + d_blu < 0x20){
		dbg(0, "is similar! %x = %x", r1, r2);
		return TRUE;
	}

	return FALSE;
}
#endif


char*
str_array_join(const char** array, const char* separator)
{
	//result must be freed using g_free()
	g_return_val_if_fail(separator, NULL);

	int sep_len = strlen(separator);
	int i = 0;
	int len = 0;
	while(array[i]){
		len += strlen(array[i]) + sep_len;
		i++;
	}
	if(!len) return NULL;
	char* s = g_new0(char, len);
	char* t = s;
	i = 0;
	while(array[i]){
		strcat(s, array[i]);
		strcat(s, separator);
		i++;
	}
	return t;
}


gint
treecell_get_row(GtkWidget* treeview, GdkRectangle* cell_area)
{
	//return the row number for the cell with the given area.

	GtkTreePath* path;
	gint x = cell_area->x + 1;
	gint y = cell_area->y + 1;
	gint *cell_x = NULL; //not used.
	gint *cell_y = NULL;
	if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), x, y, &path, NULL, cell_x, cell_y)){
		gint *i;
		i = gtk_tree_path_get_indices(path);
		dbg(1, "treecell_get_row() i[0]=%i", i[0]);
		gint row = i[0];
		gtk_tree_path_free(path);
		return row;
	}
	else pwarn("no row found.");
	return 0;
}


void
statusbar_print(int n, char* fmt, ...)
{
	if(n<1||n>3) n = 1;

	char s[128];
	va_list argp;

	va_start(argp, fmt);
	vsnprintf(s, 127, fmt, argp);
	va_end(argp);

	if(_debug_) printf("%s\n", s);

	if(n ==1 && app->no_gui) printf("%s\n", s);

	GtkWidget* statusbar = NULL;
	if     (n==1) statusbar = app->statusbar;
	else if(n==2) statusbar = app->statusbar2;
	else { perr("bad statusbar index (%i)\n", n); n=1; }

	if(!statusbar) return; //window may not be open.

	gint cid = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "dummy");
	gtk_statusbar_push(GTK_STATUSBAR(statusbar), cid, s);
}


static void
shortcuts_add_action(GtkAction* action, GimpActionGroup* action_group)
{
	g_return_if_fail(action_group);

	gtk_action_group_add_action(GTK_ACTION_GROUP(action_group), action);
	dbg(2, "group=%s group_size=%i action=%s", gtk_action_group_get_name(GTK_ACTION_GROUP(action_group)), g_list_length(gtk_action_group_list_actions(GTK_ACTION_GROUP(action_group))), gtk_action_get_name(action));
}


/**
 *  Add keyboard shortcuts from the key array to the accelerator group.
 *
 *  @param action_group - if NULL, global group is used.
 */
void
make_accels(GtkAccelGroup* accel_group, GimpActionGroup* action_group, Accel* keys, int count, gpointer user_data)
{
	g_return_if_fail(accel_group);
	g_return_if_fail(keys);

	void add_action(Accel* key, const gchar* path, GtkAccelGroup* accel_group, GimpActionGroup* action_group, bool alt)
	{
		gchar* label = key->name;
		guchar* stock_id = (guchar*)(key->stock_item
			? (alt
				? g_strconcat(key->stock_item->stock_id, "-ALT", NULL) // free !
				: key->stock_item->stock_id)
			: "gtk-file");
		GtkAction* action = gtk_action_new((alt ? (char*)stock_id : (char*)key->name), label, "Tooltip", (char*)stock_id);
		gtk_action_set_accel_path(action, path);
		gtk_action_set_accel_group(action, accel_group);
		shortcuts_add_action(action, action_group);
	}

	int k;
	for(k=0;k<count;k++){
		Accel* key = &keys[k];
		dbg (2, "user_data=%p %s.", user_data, key->name);
		GClosure* closure = NULL;
		if(key->callback){
			closure = g_cclosure_new(G_CALLBACK(key->callback), keys[k].user_data ? keys[k].user_data : user_data, NULL);
#ifdef REAL
			gtk_accel_group_connect(accel_group, keys[k].keycode, keys[k].mask, GTK_ACCEL_MASK, closure);
#endif
		}

		gchar path[64]; sprintf(path, "<%s>/Categ/%s", action_group ? gtk_action_group_get_name(GTK_ACTION_GROUP(action_group)) : "Global", keys[k].name);
		dbg(2, "path=%s", path);
		if(closure) gtk_accel_group_connect_by_path(accel_group, path, closure);
		gtk_accel_map_add_entry(path, key->key[0].code, key->key[0].mask);

		struct s_key* alt_key = &key->key[1];
		int n_keys = alt_key->code ? 2 : 1;   // accels either have 1 or 2 keys
		int i; for(i=0;i<n_keys;i++){
			add_action(key, path, accel_group, action_group, (i > 0));
		}
	}
}


void
add_menu_items_from_defn(GtkWidget* menu, MenuDef* menu_def, int n)
{
	int i; for(i=0;i<n;i++){
		MenuDef* item = &menu_def[i];
		if(!item->label[0] == '\0'){
			GtkWidget* menu_item = gtk_image_menu_item_new_with_label (item->label);
			gtk_menu_shell_append (GTK_MENU_SHELL(menu), menu_item);
			if(item->stock_id){
				GtkWidget* ico = gtk_image_new_from_stock(item->stock_id, GTK_ICON_SIZE_MENU);
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), ico);
			}
			if(item->callback) g_signal_connect (G_OBJECT(menu_item), "activate", G_CALLBACK(item->callback), GINT_TO_POINTER(i));
			if(!item->sensitive) gtk_widget_set_sensitive(GTK_WIDGET(menu_item), false);
		}else{
			GtkWidget* menu_item = gtk_separator_menu_item_new();
			gtk_menu_shell_append (GTK_MENU_SHELL(menu), menu_item);
		}
	}
}


/**
 * gimp_strip_uline:
 * @str: underline infested string (or %NULL)
 *
 * This function returns a copy of @str stripped of underline
 * characters. This comes in handy when needing to strip mnemonics
 * from menu paths etc.
 *
 * In some languages, mnemonics are handled by adding the mnemonic
 * character in brackets (like "File (_F)"). This function recognizes
 * this construct and removes the whole bracket construction to get
 * rid of the mnemonic (see bug #157561).
 *
 * Return value: A (possibly stripped) copy of @str which should be
 *               freed using g_free() when it is not needed any longer.
 **/
gchar*
gimp_strip_uline (const gchar* str)
{
  gchar    *escaped;
  gchar    *p;
  gboolean  past_bracket = FALSE;

  if (! str) return NULL;

  p = escaped = g_strdup (str);

  while (*str)
    {
      if (*str == '_')
        {
          /*  "__" means a literal "_" in the menu path  */
          if (str[1] == '_')
            {
             *p++ = *str++;
             *p++ = *str++;
            }

          /*  find the "(_X)" construct and remove it entirely  */
          if (past_bracket && str[1] && *(g_utf8_next_char (str + 1)) == ')')
            {
              str = g_utf8_next_char (str + 1) + 1;
              p--;
            }
          else
            {
              str++;
            }
        }
      else
        {
          past_bracket = (*str == '(');

          *p++ = *str++;
        }
    }

  *p = '\0';

  return escaped;
}

/*  The format string which is used to display modifier names
 *  <Shift>, <Ctrl> and <Alt>
 */
#define GIMP_MOD_NAME_FORMAT_STRING N_("<%s>")

const gchar*
gimp_get_mod_name_shift ()
{
  static gchar *mod_name_shift = NULL;

  if (! mod_name_shift)
    {
      GtkAccelLabelClass *accel_label_class;

      accel_label_class = g_type_class_ref (GTK_TYPE_ACCEL_LABEL);
      mod_name_shift = g_strdup_printf (gettext (GIMP_MOD_NAME_FORMAT_STRING),
                                        accel_label_class->mod_name_shift);
      g_type_class_unref (accel_label_class);
    }

  return (const gchar *) mod_name_shift;
}


const gchar*
gimp_get_mod_name_control()
{
  static gchar *mod_name_control = NULL;

  if (! mod_name_control)
    {
      GtkAccelLabelClass *accel_label_class;

      accel_label_class = g_type_class_ref (GTK_TYPE_ACCEL_LABEL);
      mod_name_control = g_strdup_printf (gettext (GIMP_MOD_NAME_FORMAT_STRING),
                                          accel_label_class->mod_name_control);
      g_type_class_unref (accel_label_class);
    }

  return (const gchar *) mod_name_control;
}


const gchar*
gimp_get_mod_name_alt ()
{
  static gchar *mod_name_alt = NULL;

  if (! mod_name_alt)
    {
      GtkAccelLabelClass *accel_label_class;

      accel_label_class = g_type_class_ref (GTK_TYPE_ACCEL_LABEL);
      mod_name_alt = g_strdup_printf (gettext (GIMP_MOD_NAME_FORMAT_STRING),
                                      accel_label_class->mod_name_alt);
      g_type_class_unref (accel_label_class);
    }

  return (const gchar *) mod_name_alt;
}


const gchar*
gimp_get_mod_separator ()
{
  static gchar *mod_separator = NULL;

  if (! mod_separator)
    {
      GtkAccelLabelClass *accel_label_class;

      accel_label_class = g_type_class_ref (GTK_TYPE_ACCEL_LABEL);
      mod_separator = g_strdup (accel_label_class->mod_separator);
      g_type_class_unref (accel_label_class);
    }

  return (const gchar *) mod_separator;
}


const gchar*
gimp_get_mod_string (GdkModifierType modifiers)
{
  static struct
  {
    GdkModifierType  modifiers;
    gchar           *name;
  }
  modifier_strings[] =
  {
    { GDK_SHIFT_MASK,                                    NULL },
    { GDK_CONTROL_MASK,                                  NULL },
    { GDK_MOD1_MASK,                                     NULL },
    { GDK_SHIFT_MASK | GDK_CONTROL_MASK,                 NULL },
    { GDK_SHIFT_MASK | GDK_MOD1_MASK,                    NULL },
    { GDK_CONTROL_MASK | GDK_MOD1_MASK,                  NULL },
    { GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK, NULL }
  };

  gint i;

  for (i = 0; i < G_N_ELEMENTS (modifier_strings); i++)
    {
      if (modifiers == modifier_strings[i].modifiers)
        {
          if (! modifier_strings[i].name)
            {
              GString *str = g_string_new ("");

              if (modifiers & GDK_SHIFT_MASK)
                {
                  g_string_append (str, gimp_get_mod_name_shift ());
                }

              if (modifiers & GDK_CONTROL_MASK)
                {
                  if (str->len)
                    g_string_append (str, gimp_get_mod_separator ());

                  g_string_append (str, gimp_get_mod_name_control ());
                }
              if (modifiers & GDK_MOD1_MASK)
                {
                  if (str->len)
                    g_string_append (str, gimp_get_mod_separator ());

                  g_string_append (str, gimp_get_mod_name_alt ());
                }

              modifier_strings[i].name = g_string_free (str, FALSE);
            }

          return modifier_strings[i].name;
        }
    }

  return NULL;
}


static void
gimp_substitute_underscores (gchar *str)
{
  gchar *p;

  for (p = str; *p; p++)
    if (*p == '_')
      *p = ' ';
}


gchar*
gimp_get_accel_string (guint key, GdkModifierType modifiers)
{
  GtkAccelLabelClass *accel_label_class;
  GString            *gstring;
  gunichar            ch;

  accel_label_class = g_type_class_peek (GTK_TYPE_ACCEL_LABEL);

  gstring = g_string_new (gimp_get_mod_string (modifiers));

  if (gstring->len > 0)
    g_string_append (gstring, gimp_get_mod_separator ());

  ch = gdk_keyval_to_unicode (key);

  if (ch && (g_unichar_isgraph (ch) || ch == ' ') &&
      (ch < 0x80 || accel_label_class->latin1_to_char))
    {
      switch (ch)
        {
        case ' ':
          g_string_append (gstring, "Space");
          break;
        case '\\':
          g_string_append (gstring, "Backslash");
          break;
        default:
          g_string_append_unichar (gstring, g_unichar_toupper (ch));
          break;
        }
    }
  else
    {
      gchar *tmp;

      tmp = gtk_accelerator_name (key, 0);

      if (tmp[0] != 0 && tmp[1] == 0)
        tmp[0] = g_ascii_toupper (tmp[0]);

      gimp_substitute_underscores (tmp);
      g_string_append (gstring, tmp);
      g_free (tmp);
    }

  return g_string_free (gstring, FALSE);
}


gchar *
str_replace(const gchar* string, const gchar* search, const gchar* replacement)
{
	gchar *str, **arr;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (search != NULL, NULL);

	if (replacement == NULL) replacement = "";

	arr = g_strsplit (string, search, -1);
	if (arr != NULL && arr[0] != NULL)
		str = g_strjoinv (replacement, arr);
	else
		str = g_strdup (string);

	g_strfreev (arr);

	return str;
}


char*
remove_trailing_slash(char* path)
{
	size_t len = strlen(path);
	if((len > 0) && (path[len-1] == '/')) path[len-1] = '\0';
	return path;
}


//from Rox:

/* Convert a list of URIs as a string into a GList of EscapedPath URIs.
 * No unescaping is done.
 * Lines beginning with # are skipped.
 * The text block passed in is zero terminated (after the final CRLF)
 */
GList*
uri_list_to_glist(const char* uri_list)
{
	GList* list = NULL;

	while (*uri_list){
		char* linebreak;
		int	length;

		linebreak = strchr(uri_list, 13);

		if (!linebreak){ errprintf ("uri_list_to_glist(): %s: %s", "missing line break in text/uri-list data", uri_list); return list; }
		if (linebreak[1] != 10){
			errprintf ("uri_list_to_glist(): %s", "Incorrect line break in text/uri-list data");
			return list;
		}

		length = linebreak - uri_list;

		if (length && uri_list[0] != '#')
			list = g_list_append(list, g_strndup(uri_list, length));

		uri_list = linebreak + 2;
	}

	return list;
}


void
uri_list_free(GList* list)
{
  GList* l = list;
  for(;l;l=l->next){
    if(l->data) g_free(l->data);
  }
  g_list_free(list);
}


const gchar*
vfs_get_method_string(const gchar *substring, gchar **method_string)
{
	const gchar *p;
	char *method;
	
	for (p = substring;
	     g_ascii_isalnum (*p) || *p == '+' || *p == '-' || *p == '.';
	     p++)
		;

	if (*p == ':') {
		/* Found toplevel method specification.  */
		method = g_strndup (substring, p - substring);
		*method_string = g_ascii_strdown (method, -1);
		g_free (method);
		p++;
	} else {
		*method_string = g_strdup ("file");
		p = substring;
	}
	return p;
}


static int
hex_to_int (gchar c)
{
    return  c >= '0' && c <= '9' ? c - '0'
        : c >= 'A' && c <= 'F' ? c - 'A' + 10
        : c >= 'a' && c <= 'f' ? c - 'a' + 10
        : -1;
}


static int
unescape_character (const char *scanner)
{
    int first_digit;
    int second_digit;

    first_digit = hex_to_int (*scanner++);
    if (first_digit < 0) {
        return -1;
    }

    second_digit = hex_to_int (*scanner++);
    if (second_digit < 0) {
        return -1;
    }

    return (first_digit << 4) | second_digit;
}


/**
 * gnome_vfs_unescape_string:
 * @escaped_string: an escaped URI, path, or other string
 * @illegal_characters: a string containing a sequence of characters
 * considered "illegal" to be escaped, '\0' is automatically in this list.
 *
 * Decodes escaped characters (i.e. PERCENTxx sequences) in @escaped_string.
 * Characters are encoded in PERCENTxy form, where xy is the ASCII hex code
 * for character 16x+y.
 *
 * Return value: a newly allocated string with the unescaped
 * equivalents, or %NULL if @escaped_string contained an escaped
 * encoding of one of the characters in @illegal_characters.
 **/
char*
vfs_unescape_string (const gchar *escaped_string, const gchar *illegal_characters)
{
    const gchar *in;
    gint character;

    if (escaped_string == NULL) return NULL;

    gchar* result = g_malloc (strlen (escaped_string) + 1);

    gchar* out = result;
    for (in = escaped_string; *in != '\0'; in++) {
        character = *in;
        if (*in == HEX_ESCAPE) {
            character = unescape_character (in + 1);

            /* Check for an illegal character. We consider '\0' illegal here. */
            if (character <= 0
                || (illegal_characters != NULL
                && strchr (illegal_characters, (char)character) != NULL)) {
                g_free (result);
                return NULL;
            }
            in += 2;
        }
        *out++ = (char)character;
    }

    *out = '\0';
    g_assert (out - result <= strlen (escaped_string));
    return result;

}


float
gain2db(float gain)
{
	union {float f; int i;} t;
	t.f = gain;
	int * const    exp_ptr =  &t.i;
	int            x = *exp_ptr;
	const int      log_2 = ((x >> 23) & 255) - 128;
	x &= ~(255 << 23);
	x += 127 << 23;
	*exp_ptr = x;

	gain = ((-1.0f/3) * t.f + 2) * t.f - 2.0f/3;

	return 20.0f * (gain + log_2) * 0.69314718f;
}


char*
gain2dbstring(float gain)
{
	//result must be freed by caller

	float dB = gain2db(gain);

	if(dB < -200)
		//return g_strdup_printf("-200 dB");
		return g_strdup("");

	return g_strdup_printf("%.2f dBA", gain2db(gain));
}


void
show_widget_if(GtkWidget* widget, gboolean show)
{
	if(show) gtk_widget_show(widget);
	else gtk_widget_hide(widget);
}


GtkWidget*
scrolled_window_new()
{
	GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL); //adjustments created automatically.
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	return scroll;
}


gboolean
keyword_is_dupe(const char* new, const char* existing)
{
	//return true if the word 'new' is contained in the 'existing' string.

	if(!existing) return false;

	//FIXME make case insensitive.
	//gint g_ascii_strncasecmp(const gchar *s1, const gchar *s2, gsize n);
	//gchar*      g_utf8_casefold(const gchar *str, gssize len);

	gboolean found = false;

	//split the existing keyword string into separate words.
	gchar** split = g_strsplit(existing, " ", 0);
	int word_index = 0;
	while(split[word_index]){
		if(!strcmp(split[word_index], new)){
			dbg(1, "'%s' already in string '%s'", new, existing);
			found = true;
			break;
		}
		word_index++;
	}

	g_strfreev(split);
	return found;
}


/*
 *  This Idle object wraps a signal handler such that it is called in an idle callback.
 *  It will only be called once even if there are multiple emissions in quick succession.
 *  Currently will only work for signals with no additional args.
 *
 *  It is probably not useful other for use with signals.
 */
Idle*
idle_new(ObjectCallback fn, gpointer user_data)
{
	bool run(gpointer _idle)
	{
		Idle* idle = _idle;
		idle->fn(idle->object, idle->user_data);
		idle->id = 0;
		return G_SOURCE_REMOVE;
	}

	void queue(GObject* object, gpointer _idle)
	{
		Idle* idle = _idle;
		idle->object = object;
		if(!idle->id) idle->id = g_idle_add(run, idle);
	}

	Idle* idle = g_new0(Idle, 1);
	idle->fn = fn;
	idle->user_data = user_data;
	idle->run = queue;
	return idle;
}


void
idle_free(Idle* idle)
{
	if(idle->id) g_source_remove(idle->id);
	g_free(idle);
}


#if 0
void
print_widget_tree(GtkWidget* widget)
{
	UNDERLINE;
	g_return_if_fail(widget);
	void print_children(GtkWidget* widget, int* depth)
	{
		if(GTK_IS_CONTAINER(widget)){
			GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
			GList* l = children;
			for(;l;l=l->next){
				GtkWidget* child = l->data;
				char indent[128];
				snprintf(indent, 127, "%%%is%%s %%s %%p\n", *depth * 3);
				printf(indent, " ", G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(child)), gtk_widget_get_name(child), child);
				if(GTK_IS_CONTAINER(widget)){
					(*depth)++;
					print_children(child, depth);
					(*depth)--;
				}
			}
			g_list_free(children);
		}
	}

	int depth = 0;
	if(GTK_IS_CONTAINER(widget)){
		int n_children = g_list_length(gtk_container_get_children(GTK_CONTAINER(widget)));
		if(n_children){
			print_children(widget, &depth);
		}
		else dbg(0, "is empty container: %s %s", G_OBJECT_CLASS_NAME(G_OBJECT_GET_CLASS(widget)), gtk_widget_get_name(widget));
	}
	else dbg(0, "widget has no children. %i x %i %i", widget->allocation.width, widget->allocation.height, gtk_widget_get_visible(widget));
	UNDERLINE;
}
#endif

