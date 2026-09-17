#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ear_mar.h"
#include "perclos.h"

static void test_ear_mar_geometry(void) {
    point_t p[68] = {0};

    /* Left eye: width 10, vertical pairs 5 + 5 -> EAR = 0.5. */
    p[36] = (point_t){0.0f, 0.0f};
    p[37] = (point_t){2.0f, 2.5f};
    p[38] = (point_t){8.0f, 2.5f};
    p[39] = (point_t){10.0f, 0.0f};
    p[40] = (point_t){8.0f, -2.5f};
    p[41] = (point_t){2.0f, -2.5f};

    /* Right eye: same geometry shifted on X. */
    p[42] = (point_t){20.0f, 0.0f};
    p[43] = (point_t){22.0f, 2.5f};
    p[44] = (point_t){28.0f, 2.5f};
    p[45] = (point_t){30.0f, 0.0f};
    p[46] = (point_t){28.0f, -2.5f};
    p[47] = (point_t){22.0f, -2.5f};

    /* Mouth geometry chosen so MAR = (2 + 2 + 2) / (3 * 4) = 0.5. */
    p[48] = (point_t){0.0f, 0.0f};
    p[57] = (point_t){4.0f, 0.0f};
    p[51] = (point_t){1.0f, 1.0f};
    p[50] = (point_t){1.0f, -1.0f};
    p[62] = (point_t){2.0f, 1.0f};
    p[56] = (point_t){2.0f, -1.0f};
    p[54] = (point_t){3.0f, 1.0f};
    p[66] = (point_t){3.0f, -1.0f};

    float ear = 0.0f;
    float mar = 0.0f;
    dms_compute_ear_mar(p, &ear, &mar);

    assert(fabsf(ear - 0.5f) < 1e-5f);
    assert(fabsf(mar - 0.5f) < 1e-5f);
}

static void test_eye_closure_state_transitions(void) {
    perclos_t s;
    perclos_reset(&s);

    assert(s.level == LV_NORMAL);
    assert(strcmp(s.desc, "Normal") == 0);

    perclos_update(0.10f, 0.10f, 100, &s);
    assert(s.level == LV_NORMAL);

    perclos_update(0.10f, 0.10f, 700, &s);
    assert(s.level == LV_2_MICRO);
    assert(strcmp(s.desc, "LEVEL2:Micro-sleep") == 0);

    perclos_update(0.10f, 0.10f, 1700, &s);
    assert(s.level == LV_3_SLEEP);
    assert(strcmp(s.desc, "LEVEL3:Deep Sleep") == 0);
}

static void test_yawn_state_transition(void) {
    perclos_t s;
    perclos_reset(&s);

    perclos_update(0.30f, 0.70f, 100, &s);
    assert(s.level == LV_NORMAL);

    perclos_update(0.30f, 0.70f, 2700, &s);
    assert(s.level == LV_2_YAWN);
    assert(strcmp(s.desc, "LEVEL2:Yawning") == 0);
}

int main(void) {
    test_ear_mar_geometry();
    test_eye_closure_state_transitions();
    test_yawn_state_transition();
    puts("embedded algorithm tests: PASS");
    return 0;
}
