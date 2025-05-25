#ifndef PEW_H
#define PEW_H

#define PEW_CAP 32
#define ENEMY_PEW_CAP 32

typedef struct {
    float x;
    float y;
} Pew;

typedef struct {
    int len;
    Pew items[PEW_CAP];
} Pews;

typedef struct {
    float x;
    float y;
    float v_speed;
    float h_speed;
} EnemyPew;

typedef struct {
    int len;
    EnemyPew items[ENEMY_PEW_CAP];
} Enemy_Pews;

void pew_add(Pews* pews, Pew pew);

void pew_remove(Pews* pews, int idx);

void enemypews_add(Enemy_Pews* enemypews, EnemyPew enemey_pew);

void enemypew_remove(Enemy_Pews* enemypews, int idx);

#endif
