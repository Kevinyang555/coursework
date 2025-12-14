// test/string.c - Unit tests for the string.c functions
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#include <stddef.h>
#include "common.h"
#include "string.h"

static void test_strlen(void);
static void test_strcmp(void);
static void test_strtoul(void);

int main(void) {
    test_strlen();
    test_strcmp();
    test_strtoul();
    FINISH();
}

// strlen

static void test_strlen_null(void) {
    // not required by standard afaik
    TEST_ASSERT (strlen(NULL) == 0);
}

static void test_strlen_zlen(void) {
    TEST_ASSERT (strlen("") == 0);
}

static void test_strlen_nzlen(void) {
    TEST_ASSERT (strlen("hello") == 5);
}

void test_strlen(void) {
    test_strlen_null();
    test_strlen_zlen();
    test_strlen_nzlen();
}

// strcmp

void test_strcmp_null_eq_null(void) {
    // not required by standard afaik
    TEST_ASSERT (strcmp(NULL, NULL) == 0);
}

void test_strcmp_null_lt_zlen(void) {
    // not required by standard afaik
    TEST_ASSERT (strcmp(NULL, "") == -1);
}

void test_strcmp_zlen_eq_zlen(void) {
    TEST_ASSERT (strcmp("", "") == 0);
}

void test_strcmp_zlen_lt_nzlen(void) {
    TEST_ASSERT (strcmp("", "hello") == -1);
}

void test_strcmp_nzlen_gt_zlen(void) {
    TEST_ASSERT (strcmp("hello", "") == 1);
}

void test_strcmp_nzlen_eq_nzlen(void) {
    TEST_ASSERT (strcmp("hello", "hello") == 0);
}

void test_strcmp_nzlen_lt_nzlen(void) {
    TEST_ASSERT (strcmp("aloha", "hello") == -1);
}

void test_strcmp_nzlen_gt_nzlen(void) {
    TEST_ASSERT (strcmp("hello", "aloha") == 1);
}

void test_strcmp(void) {
    test_strcmp_null_eq_null();
    test_strcmp_null_lt_zlen();
    test_strcmp_zlen_eq_zlen();
    test_strcmp_zlen_lt_nzlen();
    test_strcmp_nzlen_gt_zlen();
    test_strcmp_nzlen_eq_nzlen();
    test_strcmp_nzlen_lt_nzlen();
    test_strcmp_nzlen_gt_nzlen();
}

void test_strtoul_base10_empty(void) {
    const char * str = "";
    char * end = NULL;

    strtoul(str, &end, 10);
    TEST_ASSERT (end == str);
}

void test_strtoul_base10_nondigit(void) {
    const char * str = "hello";
    char * end = NULL;

    strtoul(str, &end, 10);
    TEST_ASSERT (end == str);
}

void test_strtoul_base10_small_positive(void) {
    const char * str = "123";
    unsigned long n;
    char * end = NULL;

    n = strtoul(str, &end, 10);
    TEST_ASSERT (end != NULL && *end == '\0');
    TEST_ASSERT (n == 123UL);
}

void test_strtoul_base10_small_negative(void) {
    const char * str = "-123";
    unsigned long n;
    char * end = NULL;

    n = strtoul(str, &end, 10);
    TEST_ASSERT (end != NULL && *end == '\0');
    TEST_ASSERT (n == -123UL);
}

void test_strtoul(void) {
    test_strtoul_base10_empty();
    test_strtoul_base10_nondigit();
    test_strtoul_base10_small_positive();
    test_strtoul_base10_small_negative();
}