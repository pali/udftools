/*
 * Copyright (C) 2017 Vojtech Vladyka <vojtech.vladyka@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

// UDF fsck error codes 
#define NO_ERR          0
#define ERR_FIXED       1
#define ERR_UNFIXED     4
#define PROG_ERR        8
#define WRONG_PARS      16
#define USER_INTERRUPT  32

/**
 * \brief UDF fsck exec wrapper for simplified test writing
 *
 * This function wraps fork/exec around udfffsck calling. 
 * Expected path differs based on define BASIC_TESTS (udf-samples) or EXTRA_TESTS (udf-samples-extra)
 *
 * \return udffsck exit code
 * \param[in] medium name of tested medium. All mediums need to be at ../../udf-samples(-extra)/---MEDIUM NAME HERE---.img format
 * \param[in] args non-parametric input arguments
 * \param[in] argsB blocksize parameter. Should be "-b 2048" or something like that
 */
int fsck_wrapper(const char * medium, char *const args, char *const argB) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        strcpy(cwd + strlen(cwd), "/udffsck");
    } else {
        printf("getcwd error. Aborting.\n");
        return -1;
    }


    char medpwd[10240];
#if BASIC_TESTS
    sprintf(medpwd, "../../udf-samples/%s.img", medium);    
#elif EXTRA_TESTS
    sprintf(medpwd, "../../udf-samples-extra/%s.img", medium);    
#else
    #error NO TEST DEFINED
#endif
    printf("Medium: %s\n", medpwd);
    char * const pars[] = { 
        cwd,
        medpwd,
        args,
        argB,
        NULL
    };

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char fout[1024];
    char ferr[1024];
#if BASIC_TESTS
    sprintf(fout, "../../udf-samples/%d-%02d-%02d-%02d-%02d-%02d_%s.img.out", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, medium);
    sprintf(ferr, "../../udf-samples/%d-%02d-%02d-%02d-%02d-%02d_%s.img.err", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, medium);
#endif
#if EXTRA_TESTS
    sprintf(fout, "../../udf-samples-extra/%d-%02d-%02d-%02d-%02d-%02d_%s.img.out", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, medium);
    sprintf(ferr, "../../udf-samples-extra/%d-%02d-%02d-%02d-%02d-%02d_%s.img.err", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, medium);
#endif

    int pipefd[3];
    if(pipe(pipefd)) {
        fprintf(stderr, "Pipe opening failed\n");
        exit(-1);
    }

    int statval, exitval;
    if(fork() == 0) {
        int fdout = open(fout, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        int fderr = open(ferr, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        dup2(fdout, 1);   // make stdout go to file
        dup2(fderr, 2);   // make stderr go to file
        close(fdout);     // fd no longer needed - the dup'ed handles are sufficient
        close(fderr);     // fd no longer needed - the dup'ed handles are sufficient

        execv(cwd, pars);
    } else {
        wait(&statval);
        if(WIFEXITED(statval)) {
            printf("Child's exit code %d\n", WEXITSTATUS(statval));
            exitval = WEXITSTATUS(statval);
        } else {
            printf("Child did not terminate with exit\n");  
            exitval = -1;
        }
        return exitval;
    }
    return 0;
}

static void blank_pass(void **state) {
    (void) state;

    assert_int_equal(2, 2);
}

static void blank_fail(void **state) {
    (void) state;

    assert_int_equal(2, -3);
}

/**
 * \brief Test against unfinished write operation. Medium was not more used.
 *
 * Result of this is broken LVID's free space, UUID, timestamp and SBD.
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_dirty_file_tree_1(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-dirty-file-tree";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", "-b 2048"), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 0); //Check it
}

/**
 * \brief Test against unfinished write operation. After that, another file was deleted.
 *
 * Result of this is broken LVID's free space, UUID, timestamp and SBD.
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_dirty_file_tree_2(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-dirty-file-tree-deleted-peregrine";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", "-b 2048"), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 0); //Check it
}

/**
 * \brief Test against unfinished write operation. After that, more files were written.
 *  
 * It resulted in broken UUIDs at them (all newer files were set UUID=0, also LVID
 * timestamp was old. 
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_dirty_file_tree_3(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-broken-UUIDs";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", "-b 2048"), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 0); //Check it
}

/**
 * \brief This medium should be clean, so this is test for positive result.
 * \note Blocksize: 2048
 * \note Revisiob: 2.01
 */
static void bs2048_clean(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-clean";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 0); //Check it
}

/**
 * \brief This medium should be clean, but too new. Should fail.
 * \note Blocksize: 2048
 * \note Revision: 2.60
 */
static void bs2048_apple_r0260(void **state) {
    (void) state;
    char *medium = "bs2048-r0260-apple";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 8); //Check it
}

/**
 * \brief This medium should be clean.
 * \note Blocksize: 2048
 * \note Revision: 1.50
 * \note Apple UDF
 */
static void bs2048_apple_r0150(void **state) {
    (void) state;
    char *medium = "bs2048-r0150-apple";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 0); //Check it
}

/**
 * \brief Test against UDF from Windows.
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_windows7(void **state) {
    (void) state;
    char *medium = "udf-hdd-win7";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 512"), 0); //Check it
}

/**
 * \brief Test against udfclient 0.7.5
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_udfclient_075(void **state) {
    (void) state;
    char *medium = "udf-hdd-udfclient-0.7.5";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Test against udfclient 0.7.7
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_udfclient_077(void **state) {
    (void) state;
    char *medium = "udf-hdd-udfclient-0.7.7";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Blocksize detection test
 *
 * \note Blocksize: 512
 * \note Revision: 1.50
 */
static void bs512_blocksize_detection_test(void **state) {
    (void) state;
    char *medium = "bs512-r0150";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Blocksize detection test
 *
 * \note Blocksize: 1024
 * \note Revision: 1.50
 */
static void bs1024_blocksize_detection_test(void **state) {
    (void) state;
    char *medium = "bs1024-r0150";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Blocksize detection test
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_blocksize_detection_test(void **state) {
    (void) state;
    char *medium = "bs2048-r0201";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Blocksize detection test
 *
 * \note Blocksize: 4096
 * \note Revision: 2.01
 */
static void bs4096_blocksize_detection_test(void **state) {
    (void) state;
    char *medium = "bs4096";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Unclosed medium check
 *
 * \note Blocksize: 1024
 * \note Revision: 1.50
 */
static void bs1024_unclosed_medium(void **state) {
    (void) state;
    char *medium = "bs1024-r0150-unclosed";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-b 1024"), 0); //Check it
}

/**
 * \brief Defective primary VDS
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_defect_primary_vds(void **state) {
    (void) state;
    char *medium = "bs512-defect-primary-vds";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", ""), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Defective AVDP1
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_defect_avdp1(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-brokenAVDP1";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", ""), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Crosplatform medium test no. 1
 *
 * Clean medium right from MS Windows 7
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_crossplatform_1(void **state) {
    (void) state;
    char *medium = "bs512_windows7_udf0201";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Crosplatform medium test no. 2
 *
 * Broken file tree
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_crossplatform_2(void **state) {
    (void) state;
    char *medium = "bs512_windows7_udf0201_broken_file_tree";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", ""), 1); //fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Crosplatform medium test no. 3
 *
 * CHKDSK broken medium
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_crossplatform_3(void **state) {
    (void) state;
    char *medium = "bs512_windows7_udf0201_chkdsk";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", ""), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Crosplatform medium test no. 4
 *
 * Serial numbers broken (Linux / Win noncompatibility)
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_crossplatform_4(void **state) {
    (void) state;
    char *medium = "bs512_windows7_udf0201-serial-broken-linux-written";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", ""), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Crosplatform medium test no. 5
 *
 * Serial numbers broken afterfix write
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_crossplatform_5(void **state) {
    (void) state;
    char *medium = "bs512_windows7_udf0201-serial-broken-linux-written-afterfix-win-write";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Crosplatform medium test no. 6
 *
 * AED test case at directory .git/objects
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_crossplatform_6(void **state) {
    (void) state;
    char *medium = "bs512_windows7_udf0201-aed-test-lot-of-files-open-integrity";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", ""), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}

/**
 * \brief Crosplatform medium test no. 7
 *
 * Another medium from Win7
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_crossplatform_7(void **state) {
    (void) state;
    char *medium = "bs512_windows7_udf0201-linux-before-fix";
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 4); //Check it
    assert_int_equal(fsck_wrapper(medium, "-vvp", ""), 1); //Fix it
    assert_int_equal(fsck_wrapper(medium, "-vvc", ""), 0); //Check it
}
/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */

static void bs2048_wrong_blocksize_1(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-dirty-file-tree";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 512"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_wrong_blocksize_2(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-dirty-file-tree-deleted-peregrine";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 1024"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 2048
 * \note Revision: 2.01
 */
static void bs2048_wrong_blocksize_3(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-broken-UUIDs";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 4096"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 512
 * \note Revision: 2.01
 */
static void bs512_wrong_blocksize_1(void **state) {
    (void) state;
    char *medium = "udf-hdd-win7";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 1024"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 512
 * \note Revision: 1.50
 */
static void bs512_wrong_blocksize_2(void **state) {
    (void) state;
    char *medium = "bs512-r0150";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 512
 * \note Revision: 1.50
 */
static void bs512_wrong_blocksize_3(void **state) {
    (void) state;
    char *medium = "bs512-r0150";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 4096"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 1024
 * \note Revision: 1.50
 */
static void bs1024_wrong_blocksize_1(void **state) {
    (void) state;
    char *medium = "bs1024-r0150";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 512"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 1024
 * \note Revision: 1.50
 */
static void bs1024_wrong_blocksize_2(void **state) {
    (void) state;
    char *medium = "bs1024-r0150";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 1024
 * \note Revision: 1.50
 */
static void bs1024_wrong_blocksize_3(void **state) {
    (void) state;
    char *medium = "bs1024-r0150";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 4096"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 4096
 * \note Revision: 2.01
 */
static void bs4096_wrong_blocksize_1(void **state) {
    (void) state;
    char *medium = "bs4096";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 512"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 4096
 * \note Revision: 2.01
 */
static void bs4096_wrong_blocksize_2(void **state) {
    (void) state;
    char *medium = "bs4096";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 1024"), 8); //Check it
}

/**
 * \brief Test forced blocksize
 *
 * \note Blocksize: 4096
 * \note Revision: 2.01
 */
static void bs4096_wrong_blocksize_3(void **state) {
    (void) state;
    char *medium = "bs4096";
    assert_int_not_equal(fsck_wrapper(medium, "-vvc", "-b 2048"), 8); //Check it
}

int main(void) {
    const struct CMUnitTest tests[] = {
#ifdef DEMO
        cmocka_unit_test(blank_fail),
        cmocka_unit_test(blank_pass),
#endif
#if BASIC_TESTS
        cmocka_unit_test(bs2048_dirty_file_tree_1),
        cmocka_unit_test(bs2048_dirty_file_tree_2),
        cmocka_unit_test(bs2048_dirty_file_tree_3),
        cmocka_unit_test(bs2048_clean),
        cmocka_unit_test(bs2048_apple_r0150),
        cmocka_unit_test(bs2048_apple_r0260),
        cmocka_unit_test(bs512_windows7),
        cmocka_unit_test(bs2048_udfclient_075),
        cmocka_unit_test(bs2048_udfclient_077),
        cmocka_unit_test(bs512_blocksize_detection_test),
        cmocka_unit_test(bs1024_blocksize_detection_test),
        cmocka_unit_test(bs2048_blocksize_detection_test),
        cmocka_unit_test(bs4096_blocksize_detection_test),
        cmocka_unit_test(bs1024_unclosed_medium),
        cmocka_unit_test(bs512_defect_primary_vds),
        cmocka_unit_test(bs2048_defect_avdp1),
        cmocka_unit_test(bs512_crossplatform_6),
        cmocka_unit_test(bs512_wrong_blocksize_1),
        cmocka_unit_test(bs512_wrong_blocksize_2),
        cmocka_unit_test(bs512_wrong_blocksize_3),
        cmocka_unit_test(bs1024_wrong_blocksize_1),
        cmocka_unit_test(bs1024_wrong_blocksize_2),
        cmocka_unit_test(bs1024_wrong_blocksize_3),
        cmocka_unit_test(bs2048_wrong_blocksize_1),
        cmocka_unit_test(bs2048_wrong_blocksize_2),
        cmocka_unit_test(bs2048_wrong_blocksize_3),
        cmocka_unit_test(bs4096_wrong_blocksize_1),
        cmocka_unit_test(bs4096_wrong_blocksize_2),
        cmocka_unit_test(bs4096_wrong_blocksize_3),
#endif
#if EXTRA_TESTS==1
        cmocka_unit_test(bs512_crossplatform_1),
        cmocka_unit_test(bs512_crossplatform_2),
        cmocka_unit_test(bs512_crossplatform_3),
#endif
#if EXTRA_TESTS==2
        cmocka_unit_test(bs512_crossplatform_4),
        cmocka_unit_test(bs512_crossplatform_5),
        cmocka_unit_test(bs512_crossplatform_7),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
