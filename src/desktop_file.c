#include <config.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "desktop_file.h"


#include <libintl.h>
#define _(x) gettext ((x))
#define N_(x) x


typedef struct _GnomeDesktopFileSection GnomeDesktopFileSection;
typedef struct _GnomeDesktopFileLine GnomeDesktopFileLine;
typedef struct _GnomeDesktopFileParser GnomeDesktopFileParser;

struct _GnomeDesktopFileSection {
  GQuark section_name; /* 0 means just a comment block (before any section) */
  gint n_lines;
  GnomeDesktopFileLine *lines;
  gint n_allocated_lines;
};

struct _GnomeDesktopFileLine {
  GQuark key; /* 0 means comment or blank line in value */
  char *locale;
  gchar *value;
};

struct _GnomeDesktopFile {
  gint n_sections;
  GnomeDesktopFileSection *sections;
  gint n_allocated_sections;
  gint main_section;
  GnomeDesktopFileEncoding encoding;
};

struct _GnomeDesktopFileParser {
  GnomeDesktopFile *df;
  gint current_section;
  gint line_nr;
  char *line;
};

#define VALID_KEY_CHAR 1
#define VALID_LOCALE_CHAR 2
guchar valid[256] = { 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x3 , 0x2 , 0x0 , 
   0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 
   0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x0 , 0x0 , 0x0 , 0x0 , 0x2 , 
   0x0 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 
   0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x3 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
   0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 
};

static void                     report_error   (GnomeDesktopFileParser   *parser,
						char                     *message,
						GnomeDesktopParseError    error_code,
						GError                  **error);
static GnomeDesktopFileSection *lookup_section (GnomeDesktopFile         *df,
						const char               *section);
static GnomeDesktopFileLine *   lookup_line    (GnomeDesktopFile         *df,
						GnomeDesktopFileSection  *section,
						const char               *keyname,
						const char               *locale);




GQuark
gnome_desktop_parse_error_quark (void)
{
  static GQuark quark;
  if (!quark)
    quark = g_quark_from_static_string ("g_desktop_parse_error");

  return quark;
}

static void
parser_free (GnomeDesktopFileParser *parser)
{
  gnome_desktop_file_free (parser->df);
}

static void
gnome_desktop_file_line_free (GnomeDesktopFileLine *line)
{
  g_free (line->locale);
  g_free (line->value);
}

static void
gnome_desktop_file_section_free (GnomeDesktopFileSection *section)
{
  int i;

  for (i = 0; i < section->n_lines; i++)
    gnome_desktop_file_line_free (&section->lines[i]);
  
  g_free (section->lines);
}

void
gnome_desktop_file_free (GnomeDesktopFile *df)
{
  int i;

  for (i = 0; i < df->n_sections; i++)
    gnome_desktop_file_section_free (&df->sections[i]);
  g_free (df->sections);

  g_free (df);
}

static void
grow_lines_in_section (GnomeDesktopFileSection *section)
{
  int new_n_lines;

  if (section->n_allocated_lines == 0)
    new_n_lines = 1;
  else
    new_n_lines = section->n_allocated_lines*2;

  section->lines = g_realloc (section->lines,
			      sizeof (GnomeDesktopFileLine) * new_n_lines);
  section->n_allocated_lines = new_n_lines;
}

static void
grow_sections (GnomeDesktopFile *df)
{
  int new_n_sections;

  if (df->n_allocated_sections == 0)
    new_n_sections = 1;
  else
    new_n_sections = df->n_allocated_sections*2;

  df->sections = g_realloc (df->sections,
                            sizeof (GnomeDesktopFileSection) * new_n_sections);
  df->n_allocated_sections = new_n_sections;
}

static gchar *
unescape_string (gchar *str, gint len)
{
  gchar *res;
  gchar *p, *q;
  gchar *end;

  /* len + 1 is enough, because unescaping never makes the
   * string longer */
  res = g_new (gchar, len + 1);
  p = str;
  q = res;
  end = str + len;

  while (p < end)
    {
      if (*p == 0)
	{
	  /* Found an embedded null */
	  g_free (res);
	  return NULL;
	}
      if (*p == '\\')
	{
	  p++;
	  if (p >= end)
	    {
	      /* Escape at end of string */
	      g_free (res);
	      return NULL;
	    }
	  
	  switch (*p)
	    {
	    case 's':
              *q++ = ' ';
              break;
           case 't':
              *q++ = '\t';
              break;
           case 'n':
              *q++ = '\n';
              break;
           case 'r':
              *q++ = '\r';
              break;
           case '\\':
              *q++ = '\\';
              break;
           default:
	     /* Invalid escape code */
	     g_free (res);
	     return NULL;
	    }
	  p++;
	}
      else
	*q++ = *p++;
    }
  *q = 0;

  return res;
}

static gchar *
escape_string (const gchar *str, gboolean escape_first_space)
{
  gchar *res;
  char *q;
  const gchar *p;
  const gchar *end;

  /* len + 1 is enough, because unescaping never makes the
   * string longer */
  res = g_new (gchar, strlen (str)*2 + 1);
  
  p = str;
  q = res;
  end = str + strlen (str);

  while (*p)
    {
      if (*p == ' ')
	{
	  if (escape_first_space && p == str)
	    {
	      *q++ = '\\';
	      *q++ = 's';
	    }
	  else
	    *q++ = ' ';
	}
      else if (*p == '\\')
	{
	  *q++ = '\\';
	  *q++ = '\\';
	}
      else if (*p == '\t')
	{
	  *q++ = '\\';
	  *q++ = 't';
	}
      else if (*p == '\n')
	{
	  *q++ = '\\';
	  *q++ = 'n';
	}
      else if (*p == '\r')
	{
	  *q++ = '\\';
	  *q++ = 'r';
	}
      else
	*q++ = *p;
      p++;
    }
  *q = 0;

  return res;
}


static GnomeDesktopFileSection* 
new_section (GnomeDesktopFile       *df,
             const char             *name,
             GError                **error)
{
  int n;
  gboolean is_main = FALSE;

  if (name &&
      (strcmp (name, "Desktop Entry") == 0 ||
       strcmp (name, "KDE Desktop Entry") == 0))
    is_main = TRUE;

  if (is_main &&
      df->main_section >= 0)
    {
      g_set_error (error,
                   GNOME_DESKTOP_PARSE_ERROR,
                   GNOME_DESKTOP_PARSE_ERROR_INVALID_SYNTAX,                   
                   "Two [Desktop Entry] or [KDE Desktop Entry] sections seen");

      return NULL;
    }
  
  if (df->n_allocated_sections == df->n_sections)
    grow_sections (df);

  if (df->n_sections == 1 &&
      df->sections[0].section_name == 0 &&
      df->sections[0].n_lines == 0)
    {
      if (!name)
	g_warning ("non-initial NULL section\n");
      
      /* The initial section was empty. Piggyback on it. */
      df->sections[0].section_name = g_quark_from_string (name);

      if (is_main)
	df->main_section = 0;
      
      return &df->sections[0];
    }
  
  n = df->n_sections++;

  if (is_main)
    df->main_section = n;
  
  if (name)
    df->sections[n].section_name = g_quark_from_string (name);
  else
    df->sections[n].section_name = 0;

  df->sections[n].n_lines = 0;
  df->sections[n].lines = NULL;
  df->sections[n].n_allocated_lines = 0;

  grow_lines_in_section (&df->sections[n]);

  return &df->sections[n];
}

static GnomeDesktopFileSection* 
open_section (GnomeDesktopFileParser *parser,
              char                   *name,
              GError                **error)
{  
  GnomeDesktopFileSection *section;

  section = new_section (parser->df, name, error);
  if (section == NULL)
    return NULL;
  
  parser->current_section = parser->df->n_sections - 1;
  g_assert (&parser->df->sections[parser->current_section] == section);
  
  return section;
}

static GnomeDesktopFileLine*
new_line_in_section (GnomeDesktopFileSection *section)
{
  GnomeDesktopFileLine *line;
  
  if (section->n_allocated_lines == section->n_lines)
    grow_lines_in_section (section);

  line = &section->lines[section->n_lines++];

  memset (line, 0, sizeof (GnomeDesktopFileLine));
  
  return line;
}

static GnomeDesktopFileLine *
new_line (GnomeDesktopFileParser *parser)
{
  GnomeDesktopFileSection *section;

  section = &parser->df->sections[parser->current_section];

  return new_line_in_section (section);
}

static gboolean
is_blank_line (GnomeDesktopFileParser *parser)
{
  gchar *p;

  p = parser->line;

  while (*p && *p != '\n')
    {
      if (!g_ascii_isspace (*p))
	return FALSE;

      p++;
    }
  return TRUE;
}

static void
parse_comment_or_blank (GnomeDesktopFileParser *parser)
{
  GnomeDesktopFileLine *line;
  gchar *line_end;

  line_end = strchr (parser->line, '\n');
  if (line_end == NULL)
    line_end = parser->line + strlen (parser->line);

  line = new_line (parser);
  
  line->value = g_strndup (parser->line, line_end - parser->line);

    if (*line_end == '\n')
    ++line_end;
  else if (*line_end == '\0')
    line_end = NULL;
  
  parser->line = line_end;

  parser->line_nr++;
}

static gboolean
is_valid_section_name (const char *name)
{
  /* 5. Group names may contain all ASCII characters except for control characters and '[' and ']'. */
  
  while (*name)
    {
      if (!(g_ascii_isprint (*name) || *name == '\n' || *name == '\t'))
	return FALSE;
      
      name++;
    }
  
  return TRUE;
}

static gboolean
parse_section_start (GnomeDesktopFileParser *parser, GError **error)
{
  gchar *line_end;
  gchar *section_name;

  line_end = strchr (parser->line, '\n');
  if (line_end == NULL)
    line_end = parser->line + strlen (parser->line);

  if (line_end - parser->line <= 2 ||
      line_end[-1] != ']')
    {
      report_error (parser, "Invalid syntax for section header", GNOME_DESKTOP_PARSE_ERROR_INVALID_SYNTAX, error);
      parser_free (parser);
      return FALSE;
    }

  section_name = unescape_string (parser->line + 1, line_end - parser->line - 2);

  if (section_name == NULL)
    {
      report_error (parser, "Invalid escaping in section name", GNOME_DESKTOP_PARSE_ERROR_INVALID_ESCAPES, error);
      parser_free (parser);
      return FALSE;
    }

  if (!is_valid_section_name (section_name))
    {
      report_error (parser, "Invalid characters in section name", GNOME_DESKTOP_PARSE_ERROR_INVALID_CHARS, error);
      parser_free (parser);
      g_free (section_name);
      return FALSE;
    }
  
  if (open_section (parser, section_name, error) == NULL)
    {
      g_free (section_name);
      return FALSE;
    }

  if (*line_end == '\n')
    ++line_end;
  else if (*line_end == '\0')
    line_end = NULL;
  
  parser->line = line_end;
  
  parser->line_nr++;

  g_free (section_name);
  
  return TRUE;
}

static gboolean
parse_key_value (GnomeDesktopFileParser *parser, GError **error)
{
  GnomeDesktopFileLine *line;
  gchar *line_end;
  gchar *key_start;
  gchar *key_end;
  gchar *locale_start = NULL;
  gchar *locale_end = NULL;
  gchar *value_start;
  gchar *value;
  gchar *p;

  line_end = strchr (parser->line, '\n');
  if (line_end == NULL)
    line_end = parser->line + strlen (parser->line);

  p = parser->line;
  key_start = p;
  while (p < line_end &&
	 (valid[(guchar)*p] & VALID_KEY_CHAR)) 
    p++;
  key_end = p;

  if (key_start == key_end)
    {
      report_error (parser, "Empty key name", GNOME_DESKTOP_PARSE_ERROR_INVALID_SYNTAX, error);
      parser_free (parser);
      return FALSE;
    }

  if (p < line_end && *p == '[')
    {
      p++;
      locale_start = p;
      while (p < line_end &&
	     (valid[(guchar)*p] & VALID_LOCALE_CHAR)) 
	p++;
      locale_end = p;

      if (p == line_end)
	{
	  report_error (parser, "Unterminated locale specification in key", GNOME_DESKTOP_PARSE_ERROR_INVALID_SYNTAX, error);
	  parser_free (parser);
	  return FALSE;
	}
      
      if (*p != ']')
	{
	  report_error (parser, "Invalid characters in locale name", GNOME_DESKTOP_PARSE_ERROR_INVALID_CHARS, error);
	  parser_free (parser);
	  return FALSE;
	}
      
      if (locale_start == locale_end)
	{
	  report_error (parser, "Empty locale name", GNOME_DESKTOP_PARSE_ERROR_INVALID_SYNTAX, error);
	  parser_free (parser);
	  return FALSE;
	}
      p++;
    }
  
  /* Skip space before '=' */
  while (p < line_end && *p == ' ')
    p++;

  if (p < line_end && *p != '=')
    {
      report_error (parser, "Invalid characters in key name", GNOME_DESKTOP_PARSE_ERROR_INVALID_CHARS, error);
      parser_free (parser);
      return FALSE;
    }

  if (p == line_end)
    {
      report_error (parser, "No '=' in key/value pair", GNOME_DESKTOP_PARSE_ERROR_INVALID_SYNTAX, error);
      parser_free (parser);
      return FALSE;
    }

  /* Skip the '=' */
  p++;

  /* Skip space after '=' */
  while (p < line_end && *p == ' ')
    p++;

  value_start = p;

  value = unescape_string (value_start, line_end - value_start);
  if (value == NULL)
    {
      report_error (parser, "Invalid escaping in value", GNOME_DESKTOP_PARSE_ERROR_INVALID_ESCAPES, error);
      parser_free (parser);
      return FALSE;
    }

  line = new_line (parser);
  line->key = g_quark_from_static_string (g_strndup (key_start, key_end - key_start));
  if (locale_start)
    line->locale = g_strndup (locale_start, locale_end - locale_start);
  line->value = value;

  if (*line_end == '\n')
    ++line_end;
  else if (*line_end == '\0')
    line_end = NULL;
  
  parser->line = line_end;
  parser->line_nr++;
  
  return TRUE;
}


static void
report_error (GnomeDesktopFileParser *parser,
	      char                   *message,
	      GnomeDesktopParseError  error_code,
	      GError                **error)
{
  GnomeDesktopFileSection *section;
  const gchar *section_name = NULL;

  section = &parser->df->sections[parser->current_section];

  if (section->section_name)
    section_name = g_quark_to_string (section->section_name);
  
  if (error)
    {
      if (section_name)
	*error = g_error_new (GNOME_DESKTOP_PARSE_ERROR,
			      error_code,
			      "Error in section %s at line %d: %s", section_name, parser->line_nr, message);
      else
	*error = g_error_new (GNOME_DESKTOP_PARSE_ERROR,
			      error_code,
			      "Error at line %d: %s", parser->line_nr, message);
    }
}


GnomeDesktopFile *
gnome_desktop_file_new_from_string (char                       *data,
				    GError                    **error)
{
  GnomeDesktopFileParser parser;
  GnomeDesktopFileLine *line;

  parser.df = g_new0 (GnomeDesktopFile, 1);
  parser.df->main_section = -1;
  parser.current_section = -1;

  parser.line_nr = 1;

  parser.line = data;

  /* Put any initial comments in a NULL segment */
  open_section (&parser, NULL, NULL);
  
  while (parser.line && *parser.line)
    {
      if (*parser.line == '[') {
	if (!parse_section_start (&parser, error))
	  return NULL;
      } else if (is_blank_line (&parser) ||
		 *parser.line == '#')
	parse_comment_or_blank (&parser);
      else
	{
	  if (!parse_key_value (&parser, error))
	    return NULL;
	}
    }

  if (parser.df->main_section >= 0)
    {
      line = lookup_line (parser.df,
			  &parser.df->sections[parser.df->main_section],
			  "Encoding", NULL);
      if (line)
	{
	  if (strcmp (line->value, "UTF-8") == 0)
	    parser.df->encoding = GNOME_DESKTOP_FILE_ENCODING_UTF8;
	  else if (strcmp (line->value, "Legacy-Mixed") == 0)
	    parser.df->encoding = GNOME_DESKTOP_FILE_ENCODING_LEGACY;
	  else
	    parser.df->encoding = GNOME_DESKTOP_FILE_ENCODING_UNKNOWN;
	}
      else
	{
	  /* No encoding specified. We have to guess
	   * If the whole file validates as UTF-8 it's probably UTF-8.
	   * Otherwise we guess it's a Legacy-Mixed
	   */
	  if (g_utf8_validate (data, -1, NULL))
	    parser.df->encoding = GNOME_DESKTOP_FILE_ENCODING_UTF8;
	  else
	    parser.df->encoding = GNOME_DESKTOP_FILE_ENCODING_LEGACY;
	}
	
    }
  else
    parser.df->encoding = GNOME_DESKTOP_FILE_ENCODING_UNKNOWN;
  
  return parser.df;
}

GnomeDesktopFileEncoding
gnome_desktop_file_get_encoding (GnomeDesktopFile *df)
{
  return df->encoding;
}

char *
gnome_desktop_file_to_string (GnomeDesktopFile *df)
{
  GnomeDesktopFileSection *section;
  GnomeDesktopFileLine *line;
  GString *str;
  char *s;
  int i, j;
  
  str = g_string_sized_new (800);

  for (i = 0; i < df->n_sections; i ++)
    {
      section = &df->sections[i];

      if (section->section_name)
	{
	  g_string_append_c (str, '[');
	  s = escape_string (g_quark_to_string (section->section_name), FALSE);
	  g_string_append (str, s);
	  g_free (s);
	  g_string_append (str, "]\n");
	}
      
      for (j = 0; j < section->n_lines; j++)
	{
	  line = &section->lines[j];
	  
	  if (line->key == 0)
	    {
	      g_string_append (str, line->value);
	      g_string_append_c (str, '\n');
	    }
	  else
	    {
	      g_string_append (str, g_quark_to_string (line->key));
	      if (line->locale)
		{
		  g_string_append_c (str, '[');
		  g_string_append (str, line->locale);
		  g_string_append_c (str, ']');
		}
	      g_string_append_c (str, '=');
	      s = escape_string (line->value, TRUE);
	      g_string_append (str, s);
	      g_free (s);
	      g_string_append_c (str, '\n');
	    }
	}
    }
  
  return g_string_free (str, FALSE);
}

static GnomeDesktopFileSection *
lookup_section (GnomeDesktopFile  *df,
		const char        *section_name)
{
  GnomeDesktopFileSection *section;
  GQuark section_quark;
  int i;

  section_quark = g_quark_try_string (section_name);
  if (section_quark == 0)
    return NULL;
  
  for (i = 0; i < df->n_sections; i ++)
    {
      section = &df->sections[i];

      if (section->section_name == section_quark)
	return section;
    }
  return NULL;
}

static GnomeDesktopFileLine *
lookup_line (GnomeDesktopFile        *df,
	     GnomeDesktopFileSection *section,
	     const char              *keyname,
	     const char              *locale)
{
  GnomeDesktopFileLine *line;
  GQuark key_quark;
  int i;

  key_quark = g_quark_try_string (keyname);
  if (key_quark == 0)
    return NULL;
  
  for (i = 0; i < section->n_lines; i++)
    {
      line = &section->lines[i];
      
      if (line->key == key_quark &&
	  ((locale == NULL && line->locale == NULL) ||
	   (locale != NULL && line->locale != NULL && strcmp (locale, line->locale) == 0)))
	return line;
    }
  
  return NULL;
}

gboolean
gnome_desktop_file_get_raw (GnomeDesktopFile  *df,
			    const char        *section_name,
			    const char        *keyname,
			    const char        *locale,
			    const char       **val)
{
  GnomeDesktopFileSection *section;
  GnomeDesktopFileLine *line;

  *val = NULL;

  if (section_name == NULL &&
      df->main_section < 0)
    return FALSE;
  
  if (section_name == NULL)
    section = &df->sections[df->main_section];
  else
    {
      section = lookup_section (df, section_name);
      if (!section)
	return FALSE;
    }

  line = lookup_line (df,
		      section,
		      keyname,
		      locale);

  if (!line)
    return FALSE;
  
  *val = line->value;
  
  return TRUE;
}

void
gnome_desktop_file_foreach_section (GnomeDesktopFile            *df,
				    GnomeDesktopFileSectionFunc  func,
				    gpointer                     user_data)
{
  GnomeDesktopFileSection *section;
  int i;

  for (i = 0; i < df->n_sections; i ++)
    {
      section = &df->sections[i];

      (*func) (df, g_quark_to_string (section->section_name),  user_data);
    }
  return;
}

void
gnome_desktop_file_foreach_key (GnomeDesktopFile            *df,
				const char                  *section_name,
				gboolean                     include_localized,
				GnomeDesktopFileLineFunc     func,
				gpointer                     user_data)
{
  GnomeDesktopFileSection *section;
  GnomeDesktopFileLine *line;
  int i;

  if (section_name == NULL)
    section = &df->sections[df->main_section];
  else
    {
      section = lookup_section (df, section_name);
      if (!section)
	return;
    }
  
  for (i = 0; i < section->n_lines; i++)
    {
      line = &section->lines[i];

      (*func) (df, g_quark_to_string (line->key), line->locale, line->value, user_data);
    }
  
  return;
}

GnomeDesktopFile*
gnome_desktop_file_load (const char    *filename,
                         GError       **error)
{
  char *contents;
  GnomeDesktopFile *df;
  
  if (!g_file_get_contents (filename, &contents,
			    NULL, error))
    return NULL;

  df = gnome_desktop_file_new_from_string (contents, error);

  g_free (contents);

  return df;
}


gboolean
gnome_desktop_file_save (GnomeDesktopFile *df,
                         const char       *path,
                         int               mode,
                         GError          **error)
{
  char *str;
  FILE *f;
  int fd;

  f = fopen (path, "w");
  if (f == NULL)
    {
      g_set_error (error, 
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to open \"%s\": %s"),
                   path, g_strerror (errno));
      return FALSE;
    }
  
  fd = fileno (f);
  
  if (fchmod (fd, mode) < 0)
    {
      g_set_error (error, G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to set permissions %o on \"%s\": %s"),
                   mode, path, g_strerror (errno));

      fclose (f);
      unlink (path);
      
      return FALSE;
    }

  str = gnome_desktop_file_to_string (df);
  
  if (fputs (str, f) < 0)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to write to \"%s\": %s"),
                   path, g_strerror (errno));

      fclose (f);
      unlink (path);
      g_free (str);
      
      return FALSE;
    }

  g_free (str);
  
  if (fclose (f) < 0)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to close \"%s\": %s"),
                   path, g_strerror (errno));

      unlink (path);

      return FALSE;
    }

  return TRUE;
}

gboolean
gnome_desktop_file_get_strings (GnomeDesktopFile   *df,
                                const char         *section,
                                const char         *keyname,
                                const char         *locale,
                                char             ***vals,
                                int                *len)
{
  const char *raw;
  char **retval;
  int i;
  
  if (vals)
    *vals = NULL;
  if (len)
    *len = 0;
  
  if (!gnome_desktop_file_get_raw (df, section, keyname, locale, &raw))
    return FALSE;

  retval = g_strsplit (raw, ";", G_MAXINT);

  i = 0;
  while (retval[i])
    ++i;

  /* Drop the empty string g_strsplit leaves in the vector since
   * our list of strings ends in ";"
   */
  --i;
  g_free (retval[i]);
  retval[i] = NULL;
  
  if (vals)
    *vals = retval;
  else
    g_strfreev (retval);

  if (len)
    *len = i;

  return TRUE;
}

void
gnome_desktop_file_set_raw (GnomeDesktopFile  *df,
                            const char        *section_name,
                            const char        *keyname,
                            const char        *locale,
                            const char        *value)
{
  GnomeDesktopFileSection *section;
  GnomeDesktopFileLine *line;

  if (section_name == NULL &&
      df->main_section < 0)
    section_name = "Desktop Entry";
  
  if (section_name == NULL)
    section = &df->sections[df->main_section];
  else
    {
      section = lookup_section (df, section_name);
      if (section == NULL)
        {
          section = new_section (df, section_name, NULL);
          g_assert (section);
        }
    }

  line = lookup_line (df,
		      section,
		      keyname,
		      locale);

  if (line == NULL)
    line = new_line_in_section (section);

  line->key = g_quark_from_string (keyname);
  g_free (line->value);
  g_free (line->locale);
  line->value = g_strdup (value);
  line->locale = g_strdup (locale);
}

void
gnome_desktop_file_set_strings (GnomeDesktopFile  *df,
                                const char        *section_name,
                                const char        *keyname,
                                const char        *locale,
                                const char       **value)
{
  char *str;
  char *tmp;  
  
  tmp = g_strjoinv (";", (char**)value);
  str = g_strconcat (tmp, ";", NULL);
  g_free (tmp);
  gnome_desktop_file_set_raw (df, section_name, keyname, locale, str);
  g_free (str);
}


void
gnome_desktop_file_merge_string_into_list (GnomeDesktopFile *df,
                                           const char       *section,
                                           const char       *keyname,
                                           const char       *locale,
                                           const char       *value)
{
  char **values;
  int n_values;
  const char *raw;
  
  if (gnome_desktop_file_get_strings (df, section, keyname, locale,
                                      &values, &n_values))
    {
      /* Look for a duplicate */
      int i;
      gboolean found;

      found = FALSE;
      
      i = 0;
      while (i < n_values)
        {
          if (strcmp (values[i], value) == 0)
            {
              found = TRUE;
              break;
            }

          ++i;
        }

      g_strfreev (values);
      
      if (found)
        return; /* nothing to do */
    }

  if (gnome_desktop_file_get_raw (df, section, keyname, locale, &raw))
    {
      /* Append to current list */
      char *str;
      str = g_strconcat (raw, value, ";", NULL);
      gnome_desktop_file_set_raw (df, section, keyname, locale, str);
      g_free (str);
    }
  else
    {
      /* Start a new key */
      gnome_desktop_file_set_raw (df, section, keyname, locale, value);
    }
}

void
gnome_desktop_file_remove_string_from_list (GnomeDesktopFile *df,
                                            const char        *section,
                                            const char        *keyname,
                                            const char        *locale,
                                            const char        *value)
{
  char **values;
  int n_values;
  
  if (gnome_desktop_file_get_strings (df, section, keyname, locale,
                                      &values, &n_values))
    {
      int i;
      int n_found;

      n_found = 0;
      
      i = 0;
      while (i < n_values)
        {
          if (n_found > 0)
            {
              g_free (values[i]);
              values[i] = values[i+1];
              values[i+1] = NULL;
            }

          if (((i+1) < n_values) &&
              strcmp (values[i], value) == 0)
            {
              ++n_found;
            }
          else
            {
              ++i;
            }
        }

      if (n_found > 0)
        gnome_desktop_file_set_strings (df, section, keyname, locale,
                                        (const char**) values);
      
      g_strfreev (values);
    }
}

