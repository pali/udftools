
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "udffsck.h"
#include <libudffs.h>
#include <ecma_167.h>



uint8_t avdp_mock[] = {
0x02,0,0x02,0,0xCE,0,0,0,0x01,0xD7,0xF0,0x01,0,0x01,0,0,0,0x80,0,0,0x20,0,0,0,0,0x80,0,0,0x30
};



static void avdp_first_ok(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 500;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+256*sectorsize, avdp_mock, sizeof(avdp_mock));
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, FIRST_AVDP), 0);
    free(dev);
}

static void avdp_first_checksum1(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 500;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+256*sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+256*sectorsize+2))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, FIRST_AVDP), -2);
    free(dev);
}

static void avdp_first_checksum2(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 500;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+256*sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+256*sectorsize+0))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, FIRST_AVDP), -2);
    free(dev);
}

static void avdp_first_checksum3(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 500;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+256*sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+256*sectorsize+sizeof(tag)-1))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, FIRST_AVDP), -2);
    free(dev);
}

static void avdp_first_crc1(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 500;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+256*sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+256*sectorsize+299))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, FIRST_AVDP), -3);
    free(dev);
}

static void avdp_second_ok(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 5000;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+size*sectorsize-sectorsize, avdp_mock, sizeof(avdp_mock));
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, SECOND_AVDP), 0);
    free(dev);
}

static void avdp_second_checksum1(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 5000;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+size*sectorsize-sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+size*sectorsize-sectorsize+2))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, SECOND_AVDP), -2);
    free(dev);
}

static void avdp_second_checksum2(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 5000;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+size*sectorsize-sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+size*sectorsize-sectorsize+0))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, SECOND_AVDP), -2);
    free(dev);
}

static void avdp_second_checksum3(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 5000;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+size*sectorsize-sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+size*sectorsize-sectorsize+sizeof(tag)-1))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, SECOND_AVDP), -2);
    free(dev);
}

static void avdp_second_crc1(void **state) {
    (void) state;
    uint8_t *dev;
    uint32_t size = 5000;
    uint16_t sectorsize = 2048;
    struct udf_disc disc = {0};

    dev = malloc(size*sectorsize);
    memcpy(dev+256*sectorsize, avdp_mock, sizeof(avdp_mock));
    (*(uint8_t *)(dev+size*sectorsize-sectorsize+229))++;
    
    assert_int_equal(get_avdp(dev, &disc, sectorsize, size, SECOND_AVDP), -3);
    free(dev);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(avdp_first_ok),
        cmocka_unit_test(avdp_first_checksum1),
        cmocka_unit_test(avdp_first_checksum2),
        cmocka_unit_test(avdp_first_checksum3),
        cmocka_unit_test(avdp_first_crc1),
        cmocka_unit_test(avdp_second_ok),
        cmocka_unit_test(avdp_second_checksum1),
        cmocka_unit_test(avdp_second_checksum2),
        cmocka_unit_test(avdp_second_checksum3),
        cmocka_unit_test(avdp_second_crc1),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
