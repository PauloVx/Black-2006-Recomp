#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "weapon_modes.extracted.inc"

int main(int argc, char **argv)
{
    int native_flag;
    int expected_silver;
    int expected_ammo;
    if (argc != 3) {
        fprintf(stderr, "Expected silver mode and ammunition override arguments\n");
        return 2;
    }
    expected_silver = atoi(argv[1]);
    expected_ammo = atoi(argv[2]);
    for (native_flag = 0; native_flag <= 1; ++native_flag) {
        const int silver = expected_silver < 0 ? native_flag : expected_silver;
        const int ammo = expected_ammo || native_flag;
        int repeat;
        for (repeat = 0; repeat < 3; ++repeat) {
            if (black_silver_weapon_flag(native_flag) != silver ||
                black_infinite_ammo_flag(native_flag) != ammo) {
                fprintf(stderr, "Wrong runtime mode for native=%d silver=%d ammo=%d\n",
                        native_flag, expected_silver, expected_ammo);
                return 1;
            }
        }
    }
    puts("Native entitlement and independent runtime overrides passed");
    return 0;
}
