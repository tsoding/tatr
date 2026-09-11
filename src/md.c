// Copyright (C) 2026  Alexey Kutepov <reximkut@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.
#include "md.h"

// TASK(20260829-063210): get rid of Md struct and replace it with String_View
typedef struct {
    char *source;
    size_t cur;
} Md;

char *md_cstr(Md *md)
{
    return &md->source[md->cur];
}

char md_char(Md *md)
{
    return md->source[md->cur];
}

bool md_end(Md *md)
{
    return md_char(md) == '\0';
}

char md_next_char(Md *md)
{
    if (md_end(md)) return '\0';
    char x = md->source[md->cur++];
    return x;
}

bool md_expect_char(Md *md, char x)
{
    if (md_char(md) != x) return false;
    md_next_char(md);
    return true;
}

void md_trim_spaces(Md *md)
{
    while (isspace(md_char(md))) {
        md_next_char(md);
    }
}

bool isspace_except_newline(char x)
{
    return isspace(x) && x != '\n' && x != '\r';
}

void md_trim_spaces_except_newline(Md *md)
{
    while (isspace_except_newline(md_char(md))) {
        md_next_char(md);
    }
}

String_View md_chop_until_newline(Md *md)
{
    char *start = md_cstr(md);
    while (!md_end(md) && md_char(md) != '\n') {
        md_next_char(md);
    }
    return sv_from_parts(start, md_cstr(md) - start);
}

bool task_md_extract_title(Md *md, String_View *title)
{
    md_trim_spaces(md);
    if (!md_expect_char(md, '#')) {
        *title = SVLIT("!!! INVALID: TASK TITLE MUST START WITH # !!!");
        return false;
    }
    md_trim_spaces_except_newline(md);
    *title = md_chop_until_newline(md);
    return true;
}

void task_md_extract_properties(Md *md, Properties *ps)
{
    size_t saved_cur;

    md_trim_spaces(md);
    for (;;) {
        saved_cur = md->cur;

        md_trim_spaces_except_newline(md);
        if (!md_expect_char(md, '-')) {
            md->cur = saved_cur;
            return;
        }
        md_trim_spaces_except_newline(md);

        String_View key = sv_from_parts(md_cstr(md), 0);
        while (!md_end(md) && isalnum(md_char(md))) {
            md_next_char(md);
            key.count += 1;
        }
        md_trim_spaces_except_newline(md);

        if (key.count == 0) {
            md->cur = saved_cur;
            return;
        }

        if (!md_expect_char(md, ':')) {
            md->cur = saved_cur;
            return;
        }

        String_View value = sv_trim(md_chop_until_newline(md));
        properties_put(ps, key, value);

        md_expect_char(md, '\n');
    }

    UNREACHABLE("task_md_extract_properties");
}

void task_md_parse(char *task_md_content, String_View *title, Properties *ps, String_View *body)
{
    Md md = { .source = task_md_content };
    if (task_md_extract_title(&md, title)) {
        task_md_extract_properties(&md, ps);
        *body = sv_from_cstr(md_cstr(&md));
    }
}

void properties_put(Properties *ps, String_View key, String_View value)
{
    da_foreach(Property, p, ps) {
        if (sv_eq(p->key, key)) {
            p->value = value;
            return;
        }
    }
    da_append(ps, ((Property) {
        .key   = key,
        .value = value,
    }));
}

String_View properties_get(Properties *ps, String_View key)
{
    da_foreach(Property, p, ps) {
        if (sv_eq(p->key, key)) {
            return p->value;
        }
    }
    return (String_View){0};
}
