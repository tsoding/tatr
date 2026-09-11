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
#ifndef TASK_H_
#define TASK_H_

typedef struct {
    String_View *items;
    size_t count;
    size_t capacity;
} Tags;

void parse_tags(Tags *tags, String_View sv);
bool tags_contains(Tags tags, String_View tag);

typedef struct {
    char *id;
    String_View title;
    String_View status;
    Tags tags;
    int priority;
    Properties properties;
    String_View body;
    String_View task_md_content;
} Task;

void print_task(const char *rel_path, Task *task);
void append_task_md_content(String_Builder *sb, Task task);

typedef struct {
    Task *items;
    size_t count;
    size_t capacity;
} Tasks;

bool load_tasks(Tasks *tasks, const char *dir_path);

typedef int (*Task_Compare)(const void *a, const void *b);

Task_Compare task_sorter(bool by_id);

#endif // TASK_H_
