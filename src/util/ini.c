/* inih -- simple .INI file parser

SPDX-License-Identifier: BSD-3-Clause

Copyright (C) 2009-2020, Ben Hoyt

inih is released under the New BSD license (see LICENSE.txt). Go to the project
home page for more info:

https://github.com/benhoyt/inih

Simplified vita version:
Copyright (C) 2023, Cat

*/

#include "ini.h"

#include <ctype.h>
#include <string.h>

#define MAX_SECTION 50
#define MAX_NAME 50
#define INI_START_COMMENT_PREFIXES ";#"
#define INI_MAX_LINE 200

#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/sysclib.h>

char *scefgets(char *s, int n, SceUID iop)
{
  char c;
  register char *cs;
  cs = s;
  int ret;

  while (--n > 0 && ((ret = ksceIoRead(iop, &c, 1)) > 0))
  {
    if ((*cs++ = c) == '\n')
      break;
  }

  *cs = '\0';
  return (ret <= 0) ? NULL : s;
}

int _isspace(char c)
{
  return (c == ' ' || c == 0x0c || c == 0x0a || c == 0x0d || c == 0x09 || c == 0x0b);
}

/* Strip whitespace chars off end of given string, in place. Return s. */
static char *rstrip(char *s)
{
  char *p = s + strnlen(s, 256);
  while (p > s && _isspace((unsigned char)(*--p)))
    *p = '\0';
  return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char *lskip(const char *s)
{
  while (*s && _isspace((unsigned char)(*s)))
    s++;
  return (char *)s;
}

/* Return pointer to first char (of chars) or inline comment in given string,
   or pointer to NUL at end of string if neither found. Inline comment must
   be prefixed by a whitespace character to register as a comment. */
static char *find_chars_or_comment(const char *s, const char *chars)
{
  while (*s && (!chars || !strchr(chars, *s)))
  {
    s++;
  }
  return (char *)s;
}

/* Similar to strncpy, but ensures dest (size bytes) is
   NUL-terminated, and doesn't pad with NULs. */
static char *strncpy0(char *dest, const char *src, size_t size)
{
  /* Could use strncpy internally, but it causes gcc warnings (see issue #91) */
  size_t i;
  for (i = 0; i < size - 1 && src[i]; i++)
    dest[i] = src[i];
  dest[i] = '\0';
  return dest;
}

int ini_parse(const char *filename, ini_handler handler, void *user, char* target_section)
{
  char line[INI_MAX_LINE];
  char section[MAX_SECTION];

  memset(line, 0, INI_MAX_LINE);
  memset(section, 0, MAX_SECTION);

  char *start;
  char *end;
  char *name;
  char *value;
  int lineno = 0;
  int error  = 0;
  int found  = 0;

  SceUID file;

  file = ksceIoOpen(filename, SCE_O_RDONLY, 0600);
  if (file < 0)
    return file;

  /* Scan through stream line by line */
  while (scefgets(line, INI_MAX_LINE, file) != NULL)
  {
    lineno++;

    start = line;
    if (lineno == 1 && (unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB
        && (unsigned char)start[2] == 0xBF)
    {
      start += 3;
    }
    start = lskip(rstrip(start));

    if (strchr(INI_START_COMMENT_PREFIXES, *start))
    {
      /* Start-of-line comment */
    }
    else if (*start == '[')
    {
      /* A "[section]" line */
      end = find_chars_or_comment(start + 1, "]");
      if (*end == ']')
      {
        *end = '\0';
        strncpy0(section, start + 1, sizeof(section));
        // break early if we parsed target section
        if (strcmp(section, target_section) == 0)
        {
          found = 1;
        }
        if(found && strcmp(section, target_section) != 0)
        {
          break;
        }
      }
      else if (!error)
      {
        /* No ']' found on section line */
        error = lineno;
      }
    }
    else if (*start)
    {
      // skip other sections
      if (strcmp(section, target_section) != 0)
      {
        continue;
      }
      /* Not a comment, must be a name[=:]value pair */
      end = find_chars_or_comment(start, "=:");
      if (*end == '=' || *end == ':')
      {
        *end  = '\0';
        name  = rstrip(start);
        value = end + 1;
        value = lskip(value);
        rstrip(value);

        /* Valid name[=:]value pair found, call handler */
        if (!handler(user, section, name, value) && !error)
          error = lineno;
      }
      else if (!error)
      {
        /* No '=' or ':' found on name[=:]value line */
        error = lineno;
      }
    }
  }

  ksceIoClose(file);
  return error;
}
