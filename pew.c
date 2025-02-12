#include "pew.h"
#include <core/check.h>
#include <stdlib.h>

void pew_add(Pews* pews, const Pew pew) {
    furi_assert(pews->len == PEW_CAP, "cannot add to full pews");
    pews->items[pews->len++] = pew;
}

void pew_remove(Pews* pews, const int index) {
    furi_assert(pews->len == 0, "cannot remove from empty pew");
    furi_assert(index >= pews->len, "index out of bounds");
    for (int i = index; i < pews->len - 1; i++) {
        pews->items[i] = pews->items[i + 1];
    }
    pews->len--;
}

void enemypews_add(EnemyPews* enemy_pews, EnemyPew enemey_pew) {
    (void)enemy_pews;
    (void)enemey_pew;
    if (enemy_pews->len + 1 != ENEMY_PEW_CAP) {
        enemy_pews->items[enemy_pews->len++] = enemey_pew;
    }
}

void enemypew_remove(EnemyPews* enemypews, int index) {
    (void)enemypews;
    (void)index;
    for (int i = index; i < enemypews->len - 1; i++) {
        enemypews->items[i] = enemypews->items[i + 1];
    }
    enemypews->len--;
}
