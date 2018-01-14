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

#include "udffsck.h"
#include "log.h"

    
verbosity_e verbosity = DBG;

// Support functions
void clear_dstring(dstring *string, size_t field_length) {
    // clear string
    for(int i = 0; i < (int)field_length; ++i) {
        string[i] = 0;
    }
}

void generate_valid_dstring_u8(dstring *string, size_t field_length, uint8_t compID, uint8_t length, uint8_t content) {
    // clear string
    clear_dstring(string, field_length);

    string[0] = compID;
    string[field_length-1] = length > 0 ? length +1 : 0;

    for(int i = 0; i < length; ++i) {
        string[i+1] = content;
    }
}

void generate_valid_dstring_u16(dstring *string, size_t field_length, uint8_t compID, uint8_t length, uint16_t content) {
    // clear string
    clear_dstring(string, field_length);

    string[0] = compID;
    string[field_length-1] = length > 0 ? length +1 : 0;

    for(int i = 0; i < length; i+=2) {
        string[i+1] = content >> 8;
        string[i+2] = content & 0xFF;
    }
}

void print_dstring(dstring *string, size_t field_length) {
    printf("[ ");
    for(int i=0; i<(int)field_length; ++i) {
        printf("%02x ", string[i]);
    }
    printf("]\n");
}

//int check_dstring(dstring *in, size_t field_size);

 void dstring_check_u8_ok_1(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 8, 16, 0xDA);
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u8_ok_2(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 8, 30, 0xDA);
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u8_ok_3(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u8(array, 128, 8, 30, 0x02);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), 0); //Check it 
}

 void dstring_check_u8_ok_4(void **state) {
    (void) state;
    dstring array[256];
    generate_valid_dstring_u8(array, 256, 8, 254, 0x02);
    print_dstring(array, 256);
    assert_int_equal(check_dstring(array, 256), 0); //Check it 
}

 void dstring_check_u8_empty(void **state) {
    (void) state;
    dstring array[256];
    generate_valid_dstring_u8(array, 256, 0, 0, 0);
    print_dstring(array, 256);
    assert_int_equal(check_dstring(array, 256), 0); //Check it 
}

 void dstring_check_u16_ok_1(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u16(array, 32, 16, 4*2, 0xDEAD);
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u16_ok_2(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u16(array, 32, 16, 14*2, 0xBEEF);
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u16_ok_3(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u16(array, 128, 16, 30*2, 0xDEAD);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), 0); //Check it 
}

 void dstring_check_u16_ok_4(void **state) {
    (void) state;
    dstring array[256];
    generate_valid_dstring_u16(array, 256, 16, 127*2, 0xBEEF);
    print_dstring(array, 256);
    assert_int_equal(check_dstring(array, 256), 0); //Check it 
}

 void dstring_check_u16_empty(void **state) {
    (void) state;
    dstring array[256];
    generate_valid_dstring_u16(array, 256, 0, 0, 0);
    print_dstring(array, 256);
    assert_int_equal(check_dstring(array, 256), 0); //Check it 
}

 void dstring_check_u8_non_empty_1(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u8(array, 128, 8, 0, 0x0);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_NOT_EMPTY); //Check it 
}

 void dstring_check_u8_non_empty_2(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u8(array, 128, 0, 30, 0x10);
    array[127] = 0; //rewrite length
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_NOT_EMPTY); //Check it 
}

 void dstring_check_u8_non_empty_3(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u16(array, 128, 0, 30, 0x10);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_NOT_EMPTY); //Check it 
}

 void dstring_check_u16_non_empty_1(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u16(array, 128, 16, 0, 0x0);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_NOT_EMPTY); //Check it 
}

 void dstring_check_u16_non_empty_2(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u16(array, 128, 0, 30*2, 0x10);
    array[127] = 0; //rewrite length
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_NOT_EMPTY); //Check it 
}

 void dstring_check_u16_non_empty_3(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u16(array, 128, 0, 30*2, 0x10);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_NOT_EMPTY); //Check it 
}

 void dstring_check_u8_padding_1(void **state) {
    (void) state;
    dstring array[36];
    generate_valid_dstring_u8(array, 36, 8, 10, 0x10);
    array[12] = 0xde;
    array[13] = 0xad;
    array[14] = 0xbe;
    array[15] = 0xef;
    print_dstring(array, 36);
    assert_int_equal(check_dstring(array, 36), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u8_padding_2(void **state) {
    (void) state;
    dstring array[36];
    generate_valid_dstring_u8(array, 36, 8, 10, 0x10);
    array[31] = 0xde;
    array[32] = 0xad;
    array[33] = 0xbe;
    array[34] = 0xef;
    print_dstring(array, 36);
    assert_int_equal(check_dstring(array, 36), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u8_padding_3(void **state) {
    (void) state;
    dstring array[36];
    generate_valid_dstring_u8(array, 36, 8, 10, 0x10);
    array[12] = 0xde;
    print_dstring(array, 36);
    assert_int_equal(check_dstring(array, 36), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u8_padding_4(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u8(array, 16, 8, 10, 0x10);
    array[12] = 0xde;
    array[13] = 0xad;
    array[14] = 0xbe;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u16_padding_1(void **state) {
    (void) state;
    dstring array[36];
    generate_valid_dstring_u16(array, 36, 16, 5*2, 0x10);
    array[13] = 0xde;
    array[14] = 0xad;
    array[15] = 0xbe;
    array[16] = 0xef;
    print_dstring(array, 36);
    assert_int_equal(check_dstring(array, 36), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u16_padding_2(void **state) {
    (void) state;
    dstring array[36];
    generate_valid_dstring_u16(array, 36, 16, 5*2, 0x10);
    array[31] = 0xde;
    array[32] = 0xad;
    array[33] = 0xbe;
    array[34] = 0xef;
    print_dstring(array, 36);
    assert_int_equal(check_dstring(array, 36), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u16_padding_3(void **state) {
    (void) state;
    dstring array[36];
    generate_valid_dstring_u16(array, 36, 16, 5*2, 0x10);
    array[13] = 0xde;
    print_dstring(array, 36);
    assert_int_equal(check_dstring(array, 36), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u16_padding_4(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0x10);
    array[13] = 0xde;
    array[14] = 0xad;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_NONZERO_PADDING); //Check it 
}

 void dstring_check_u16_invalid_chars_1(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xFFFE);
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

 void dstring_check_u16_invalid_chars_2(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xFEFF);
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

 void dstring_check_u16_invalid_chars_3(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xFEFE);
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), 0); //Check it 
}

 void dstring_check_u16_invalid_chars_4(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xFFFF);
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), 0); //Check it 
}

 void dstring_check_u16_invalid_chars_5(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xabcd);
    array[1] = 0xFF;
    array[2] = 0xFE;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

 void dstring_check_u16_invalid_chars_6(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xabcd);
    array[1] = 0xFE;
    array[2] = 0xFF;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

 void dstring_check_u16_invalid_chars_7(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xabcd);
    array[3] = 0xFF;
    array[4] = 0xFE;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

 void dstring_check_u16_invalid_chars_8(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xabcd);
    array[3] = 0xFE;
    array[4] = 0xFF;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

 void dstring_check_u16_invalid_chars_9(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xabcd);
    array[2] = 0xFF;
    array[3] = 0xFE;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), 0); //Check it 
}

 void dstring_check_u16_invalid_chars_10(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xabcd);
    array[2] = 0xFE;
    array[3] = 0xFF;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), 0); //Check it 
}

 void dstring_check_u8_wrong_length_1(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u8(array, 16, 8, 5, 0xab);
    array[6] = 0x7f;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_WRONG_LENGTH); //Check it 
}

 void dstring_check_u8_wrong_length_2(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u8(array, 16, 8, 5, 0xab);
    array[15] = 0xFF;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_WRONG_LENGTH); //Check it 
}

 void dstring_check_u8_wrong_length_3(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u8(array, 128, 8, 30, 0x0);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_WRONG_LENGTH); //Check it 
}

 void dstring_check_u16_wrong_length_1(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xab);
    array[12] = 0x7f;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_WRONG_LENGTH); //Check it 
}

 void dstring_check_u16_wrong_length_2(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 16, 5*2, 0xab);
    array[15] = 0xFF;
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_WRONG_LENGTH); //Check it 
}

 void dstring_check_u16_wrong_length_3(void **state) {
    (void) state;
    dstring array[128];
    generate_valid_dstring_u16(array, 128, 16, 30*2, 0x0);
    print_dstring(array, 128);
    assert_int_equal(check_dstring(array, 128), DSTRING_E_WRONG_LENGTH); //Check it 
}

 void dstring_check_u8_compID_1(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u8(array, 16, 42, 5, 0xab);
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_UNKNOWN_COMP_ID); //Check it 
}

 void dstring_check_u16_compID_1(void **state) {
    (void) state;
    dstring array[16];
    generate_valid_dstring_u16(array, 16, 42, 5*2, 0xab);
    print_dstring(array, 16);
    assert_int_equal(check_dstring(array, 16), DSTRING_E_UNKNOWN_COMP_ID); //Check it 
}

 void dstring_check_u8_old_mkudffs_1(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 8, 10, 0x24);
    array[31] = 0;
    array[5] = 0x80;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), DSTRING_E_NOT_EMPTY); //Check it 
}

 void dstring_check_u8_dchars_1(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 254, 10, 0x24);
    array[31] = 0;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u8_dchars_2(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 254, 31, 0x24);
    array[31] = 0x24;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u16_dchars_1(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 255, 20, 0x24);
    array[31] = 0;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u16_dchars_2(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 255, 31, 0x24);
    array[31] = 0x24;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u16_dchars_3(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 255, 31, 0x24);
    array[31] = 0xFF;
    array[30] = 0xFE;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), 0); //Check it 
}

 void dstring_check_u16_dchars_4(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 255, 31, 0x24);
    array[3] = 0xFE;
    array[4] = 0xFF;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

 void dstring_check_u16_dchars_5(void **state) {
    (void) state;
    dstring array[32];
    generate_valid_dstring_u8(array, 32, 255, 31, 0x24);
    array[29] = 0xFF;
    array[30] = 0xFE;
    print_dstring(array, 32);
    assert_int_equal(check_dstring(array, 32), DSTRING_E_INVALID_CHARACTERS); //Check it 
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(dstring_check_u8_ok_1),
        cmocka_unit_test(dstring_check_u8_ok_2),
        cmocka_unit_test(dstring_check_u8_ok_3),
        cmocka_unit_test(dstring_check_u8_ok_4),
        cmocka_unit_test(dstring_check_u8_empty),
        cmocka_unit_test(dstring_check_u16_ok_1),
        cmocka_unit_test(dstring_check_u16_ok_2),
        cmocka_unit_test(dstring_check_u16_ok_3),
        cmocka_unit_test(dstring_check_u16_ok_4),
        cmocka_unit_test(dstring_check_u16_empty),
        cmocka_unit_test(dstring_check_u8_non_empty_1),
        cmocka_unit_test(dstring_check_u8_non_empty_2),
        cmocka_unit_test(dstring_check_u8_non_empty_3),
        cmocka_unit_test(dstring_check_u16_non_empty_1),
        cmocka_unit_test(dstring_check_u16_non_empty_2),
        cmocka_unit_test(dstring_check_u16_non_empty_3),
        cmocka_unit_test(dstring_check_u8_padding_1),
        cmocka_unit_test(dstring_check_u8_padding_2),
        cmocka_unit_test(dstring_check_u8_padding_3),
        cmocka_unit_test(dstring_check_u8_padding_4),
        cmocka_unit_test(dstring_check_u16_padding_1),
        cmocka_unit_test(dstring_check_u16_padding_2),
        cmocka_unit_test(dstring_check_u16_padding_3),
        cmocka_unit_test(dstring_check_u16_padding_4),
        cmocka_unit_test(dstring_check_u16_invalid_chars_1),
        cmocka_unit_test(dstring_check_u16_invalid_chars_2),
        cmocka_unit_test(dstring_check_u16_invalid_chars_3),
        cmocka_unit_test(dstring_check_u16_invalid_chars_4),
        cmocka_unit_test(dstring_check_u16_invalid_chars_5),
        cmocka_unit_test(dstring_check_u16_invalid_chars_6),
        cmocka_unit_test(dstring_check_u16_invalid_chars_7),
        cmocka_unit_test(dstring_check_u16_invalid_chars_8),
        cmocka_unit_test(dstring_check_u16_invalid_chars_9),
        cmocka_unit_test(dstring_check_u16_invalid_chars_10),
        cmocka_unit_test(dstring_check_u8_wrong_length_1),
        cmocka_unit_test(dstring_check_u8_wrong_length_2),
        cmocka_unit_test(dstring_check_u8_wrong_length_3),
        cmocka_unit_test(dstring_check_u16_wrong_length_1),
        cmocka_unit_test(dstring_check_u16_wrong_length_2),
        cmocka_unit_test(dstring_check_u16_wrong_length_3),
        cmocka_unit_test(dstring_check_u8_compID_1),
        cmocka_unit_test(dstring_check_u16_compID_1),
        cmocka_unit_test(dstring_check_u8_old_mkudffs_1),
        cmocka_unit_test(dstring_check_u8_dchars_1),
        cmocka_unit_test(dstring_check_u8_dchars_2),
        cmocka_unit_test(dstring_check_u16_dchars_1),
        cmocka_unit_test(dstring_check_u16_dchars_2),
        cmocka_unit_test(dstring_check_u16_dchars_3),
        cmocka_unit_test(dstring_check_u16_dchars_4),
        cmocka_unit_test(dstring_check_u16_dchars_5),
    };


    return cmocka_run_group_tests(tests, NULL, NULL);
}
