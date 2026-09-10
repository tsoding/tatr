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
#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#include "./thirdparty/nob.h"
#define FLAG_IMPLEMENTATION
#include "./thirdparty/flag.h"

#define BUILD_FOLDER      "build/"
#define SRC_FOLDER        "src/"
#define THIRDPARTY_FOLDER "thirdparty/"

typedef enum {
    CC,
    GCC,
    CLANG,
    TCC,
    __compiler_count,
} Compiler;

static_assert(__compiler_count == 4, "Amount of compilers have changed");
const char *compiler_names[__compiler_count] = {
    [CC]    = "cc",
    [GCC]   = "gcc",
    [CLANG] = "clang",
    [TCC]   = "tcc",
};

bool compiler_by_name(const char *name, Compiler *compiler)
{
    for (int i = 0; i < __compiler_count; ++i) {
        if (strcmp(name, compiler_names[i]) == 0) {
            *compiler = i;
            return true;
        }
    }
    return false;
}

typedef struct {
    Cmd cmd;
    String_Builder sb_stdout;
    String_Builder sb_stderr;
    bool ok;
} Test_Runner;

bool run_query_payload(Test_Runner *r, const char *query_payload)
{
    const char *test_stdout_path = BUILD_FOLDER"test_stdout.txt";
    const char *test_stderr_path = BUILD_FOLDER"test_stderr.txt";
    cmd_append(&r->cmd, BUILD_FOLDER"tatr");
    cmd_append(&r->cmd, "ls");
    cmd_append(&r->cmd, "-debug");
    cmd_append(&r->cmd, query_payload);
    Log_Handler *saved_log_handler = get_log_handler();
    set_log_handler(null_log_handler);
    {
        r->ok = cmd_run(
            &r->cmd,
            .stdout_path = test_stdout_path,
            .stderr_path = test_stderr_path
        );
    }
    set_log_handler(saved_log_handler);
    r->sb_stdout.count = 0;
    if (!read_entire_file(test_stdout_path, &r->sb_stdout)) return false;
    r->sb_stderr.count = 0;
    if (!read_entire_file(test_stderr_path, &r->sb_stderr)) return false;
    return true;
}

bool expect_failure(Test_Runner *r)
{
    if (r->ok) {
        nob_log(ERROR, "Command succeeded, but should've failed");
        String_View svout = sb_to_sv(r->sb_stdout);
        nob_log(ERROR, "STDOUT:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(svout));
        String_View sverr = sb_to_sv(r->sb_stderr);
        nob_log(ERROR, "STDERR:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(sverr));
        return false;
    }
    return true;
}

bool expect_success(Test_Runner *r)
{
    if (!r->ok) {
        nob_log(ERROR, "Command failed, but should've suceeded");
        String_View svout = sb_to_sv(r->sb_stdout);
        nob_log(ERROR, "STDOUT:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(svout));
        String_View sverr = sb_to_sv(r->sb_stderr);
        nob_log(ERROR, "STDERR:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(sverr));
        return false;
    }
    return true;
}

bool assert_test_output(const char *output_label, String_View expected_stdout, String_View actual_stdout)
{
    if (!sv_eq(expected_stdout, actual_stdout)) {
        nob_log(ERROR, "UNEXPECTED %s !!!", output_label);
        nob_log(ERROR, "EXPECTED:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(expected_stdout));
        nob_log(ERROR, "ACTUAL:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(actual_stdout));
        return false;
    }
    return true;
}

bool test_ls_query_negation_of_complex_expression_in_parens(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, "not [tagged or :bug and :test and :foo and :bar]")) return false;
    if (!expect_success(r)) return false;
    if (!assert_test_output(
        "STDOUT",
        sv_from_cstr(
            "TOKENS:\n"
            "    not\n"
            "    [\n"
            "    tagged\n"
            "    or\n"
            "    :bug\n"
            "    and\n"
            "    :test\n"
            "    and\n"
            "    :foo\n"
            "    and\n"
            "    :bar\n"
            "    ]\n"
            "\n"
            "OPS:\n"
            "    OP_TAGGED\n"
            "    OP_TAG bug\n"
            "    OP_TAG test\n"
            "    OP_AND\n"
            "    OP_TAG foo\n"
            "    OP_AND\n"
            "    OP_TAG bar\n"
            "    OP_AND\n"
            "    OP_OR\n"
            "    OP_NOT\n"),
        sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output("STDERR", (String_View){0}, sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

bool test_ls_query_not_stuck_to_open_paren(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, "not[:bug and :test] and :query")) return false;
    if (!expect_success(r)) return false;
    if (!assert_test_output(
        "STDOUT",
        sv_from_cstr(
            "TOKENS:\n"
            "    not\n"
            "    [\n"
            "    :bug\n"
            "    and\n"
            "    :test\n"
            "    ]\n"
            "    and\n"
            "    :query\n"
            "\n"
            "OPS:\n"
            "    OP_TAG bug\n"
            "    OP_TAG test\n"
            "    OP_AND\n"
            "    OP_NOT\n"
            "    OP_TAG query\n"
            "    OP_AND\n"),
        sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output("STDERR", (String_View){0}, sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

bool test_ls_query_report_error_utf8(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, ":привет hello")) return false;
    if (!expect_failure(r)) return false;
    if (!assert_test_output("STDOUT", (String_View){0}, sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output(
        "STDERR",
        sv_from_cstr(
            "Supported infix operators:\n"
            "\n"
            "    and  or                  - logical operators\n"
            "    lt  le  gt  ge  eq  ne   - comparison operators\n"
            "\n"
            ":привет hello\n"
            "        ^\n"
            "ERROR: Unexpected infix operator `hello`\n"
            ),
        sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

bool test_ls_query_priority_above_20(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, "priority gt 20")) return false;
    if (!expect_success(r)) return false;
    if (!assert_test_output(
        "STDOUT",
        SVLIT(
            "TOKENS:\n"
            "    priority\n"
            "    gt\n"
            "    20\n"
            "\n"
            "OPS:\n"
            "    OP_PRIORITY\n"
            "    OP_INTEGER 20\n"
            "    OP_GT\n"),
        sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output(
        "STDERR",
        (String_View){0},
        sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

void cc(Cmd *cmd, Compiler compiler)
{
    static_assert(__compiler_count == 4, "Amount of compilers have changed");

    switch (compiler) {
    case CC:    cmd_append(cmd, "cc");    break;
    case GCC:   cmd_append(cmd, "gcc");   break;
    case CLANG: cmd_append(cmd, "clang"); break;
    case TCC:   cmd_append(cmd, "tcc");   break;
    case __compiler_count:
    default:
        UNREACHABLE("Compiler");
    }

    cmd_append(cmd, "-Wall");
    cmd_append(cmd, "-Wextra");
    cmd_append(cmd, "-Wswitch-enum");
    cmd_append(cmd, "-Wno-unused-function");
    cmd_append(cmd, "-I"THIRDPARTY_FOLDER);
    cmd_append(cmd, "-I"BUILD_FOLDER);
    if (compiler == CLANG) cmd_append(cmd, "-fsanitize=memory");
    cmd_append(cmd, "-ggdb");
}

const char *get_current_date(void)
{
    static const char *WEEKDAYS[] = {
        "Mon", "Tue", "Wed",
        "Thu", "Fri", "Sat",
        "Sun",
    };

    static const char *MONTHS[] = {
        "Jan", "Feb", "Mar",
        "Apr", "May", "Jun",
        "Jul", "Aug", "Sep",
        "Oct", "Nov", "Dec",
    };

    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);

    String_Builder sb = {0};
    sb_appendf(&sb, "%s, ",  WEEKDAYS[timeinfo->tm_wday - 1]);
    sb_appendf(&sb, "%02d ", timeinfo->tm_mday);
    sb_appendf(&sb, "%s ",   MONTHS[timeinfo->tm_mon]);
    sb_appendf(&sb, "%04d ", timeinfo->tm_year+1900);
    sb_appendf(&sb, "%02d:", timeinfo->tm_hour);
    sb_appendf(&sb, "%02d:", timeinfo->tm_min);
    sb_appendf(&sb, "%02d ", timeinfo->tm_sec);
    sb_appendf(&sb, "%s",    timeinfo->tm_zone);
    sb_append_null(&sb);
    return sb.items;
}

bool make_build_h(Compiler compiler);

int main(int argc, char **argv)
{
    GO_REBUILD_URSELF(argc, argv);
    Cmd cmd = {0};
    Procs procs = {0};

    bool test = false;
    bool run = false;
    bool help = false;
    bool debug = false;
    char *compiler_name = NULL;
    flag_bool_var(&test, "test", false, "Run the tests after building");
    flag_bool_var(&run, "run", false, "Run the app after the build");
    flag_bool_var(&debug, "debug", false, "Run the app in the debugger (gf2 specifically)");
    flag_bool_var(&help, "help", false, "Print this help message");
    flag_str_var(&compiler_name, "cc", "cc", "Compiler to use");

    if (!flag_parse(argc, argv)) {
        fprintf(stderr, "Usage: %s [OPTIONS]\n", flag_program_name());
        flag_print_options(stderr);
        flag_print_error(stderr);
        return 1;
    }

    Compiler compiler;
    if (!compiler_by_name(compiler_name, &compiler)) {
        nob_log(ERROR, "Unknown compiler \"%s\"", compiler_name);
        nob_log(ERROR, "Supported compilers:", compiler_name);
        for (int i = 0; i < __compiler_count; ++i) {
            nob_log(ERROR, "  %s", compiler_names[i]);
        }
        return 1;
    }

    argc = flag_rest_argc();
    argv = flag_rest_argv();

    if (help) {
        fprintf(stderr, "Usage: %s [OPTIONS]\n", flag_program_name());
        flag_print_options(stderr);
        return 0;
    }

    if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    if (!make_build_h(compiler)) return 1;

    cc(&cmd, compiler);
    cmd_append(&cmd, "-o", BUILD_FOLDER"tatr");
    cmd_append(&cmd, SRC_FOLDER"tatr.c");
    if (!cmd_run(&cmd)) return 1;

    if (test) {
        cc(&cmd, compiler);
        cmd_append(&cmd, "-DTASKS_TEST");
        cmd_append(&cmd, "-o", BUILD_FOLDER"tatr-test");
        cmd_append(&cmd, SRC_FOLDER"tatr.c");
        if (!cmd_run(&cmd)) return 1;

        cmd_append(&cmd, BUILD_FOLDER"tatr-test");
        if (!cmd_run(&cmd)) return 1;

        Test_Runner r = {0};
        if (!test_ls_query_negation_of_complex_expression_in_parens(&r)) return 1;
        if (!test_ls_query_not_stuck_to_open_paren(&r)) return 1;
        if (!test_ls_query_report_error_utf8(&r)) return 1;
        if (!test_ls_query_priority_above_20(&r)) return 1;
    }

    if (run) {
        if (debug) cmd_append(&cmd, "gf2");
        cmd_append(&cmd, BUILD_FOLDER"tatr");
        da_append_many(&cmd, argv, argc);
        if (!cmd_run(&cmd)) return false;
    }

    return 0;
}

bool make_build_h(Compiler compiler)
{
    Cmd cmd = {0};
    String_Builder sb_build_h = {0};
    sb_appendf(&sb_build_h, "#ifndef BUILD_H_\n");
    sb_appendf(&sb_build_h, "#define BUILD_H_\n");

    // TASK(20260901-051445): If git state is dirty the baked git hash should indicate that
    cmd_append(&cmd, "git");
    cmd_append(&cmd, "rev-parse");
    cmd_append(&cmd, "--short");
    cmd_append(&cmd, "HEAD");
    if (cmd_run(&cmd, .stdout_path = BUILD_FOLDER"git_hash.txt")) {
        sb_appendf(&sb_build_h, "#define GIT_HASH \"");
        if (!read_entire_file(BUILD_FOLDER"git_hash.txt", &sb_build_h)) return false;
        while (sb_build_h.count > 0 && isspace(da_last(&sb_build_h))) {
            da_pop(&sb_build_h);
        }
        sb_appendf(&sb_build_h, "\"\n");
    } else {
        nob_log(WARNING, "git hash will not be included in `tatr-version`");
    }

    String_Builder sb_compiler_version_txt = {0};
    static_assert(__compiler_count == 4, "Amount of compilers have changed");
    switch (compiler) {
    case CC:    cmd_append(&cmd, "cc",    "--version"); break;
    case GCC:   cmd_append(&cmd, "gcc",   "--version"); break;
    case CLANG: cmd_append(&cmd, "clang", "--version"); break;
    case TCC:   cmd_append(&cmd, "tcc",   "--version"); break;
    case __compiler_count:
    default:
        UNREACHABLE("Compiler");
    }
    if (cmd_run(&cmd, .stdout_path = BUILD_FOLDER"compiler-version.txt")) {
        if (!read_entire_file(BUILD_FOLDER"compiler-version.txt", &sb_compiler_version_txt)) return false;
        String_View compiler_version_txt = sb_to_sv(sb_compiler_version_txt);
        String_View compiler_version = sv_chop_by_delim(&compiler_version_txt, '\n');
        sb_appendf(&sb_build_h, "#define COMPILER_VERSION \""SV_Fmt"\"\n", SV_Arg(compiler_version));
    } else {
        nob_log(WARNING, "compiler version will not be included in `tatr-version`");
    }

    sb_appendf(&sb_build_h, "#define BUILD_TIME \"%s\"\n", get_current_date());

    const char *tasks_readme_md_file = "tasks/README.md";
    if (file_exists(tasks_readme_md_file)) {
        String_Builder sb_tasks_readme_md = {0};
        if (!read_entire_file(tasks_readme_md_file, &sb_tasks_readme_md)) return false;
        sb_appendf(&sb_build_h, "unsigned char tasks_readme_md[] = {\n");
        for (size_t i = 0; i < sb_tasks_readme_md.count;) {
            sb_appendf(&sb_build_h, "    ");
            for (size_t j = 0; i < sb_tasks_readme_md.count && j < 20; ++i, ++j) {
                sb_appendf(&sb_build_h, "0x%02X,", sb_tasks_readme_md.items[i]);
            }
            sb_appendf(&sb_build_h, "\n");
        }
        sb_appendf(&sb_build_h, "};\n");
        sb_appendf(&sb_build_h, "#define TASKS_README_MD tasks_readme_md\n");
    } else {
        nob_log(WARNING, "%s file doesn't exist. `tatr-init` will not create it along with the tasks/ folder", tasks_readme_md_file);
    }

    sb_appendf(&sb_build_h, "#endif // BUILD_H_\n");

    if (!write_entire_file(BUILD_FOLDER"build.h", sb_build_h.items, sb_build_h.count)) return false;
    nob_log(INFO, "Generated "BUILD_FOLDER"build.h");
    return true;
}
