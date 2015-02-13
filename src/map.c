#include "util.h"
#include "map.h"

#define MIN_CHAR 97
#define MAX_CHAR 122
#define DIFF_CHAR (MAX_CHAR - MIN_CHAR + 1)

static struct char_map
{
    int map[DIFF_CHAR];
    int num;
} id_map;

void map_init()
{
    id_map.num = 0;
}

char map_add(int id)
{
    if (id_map.num > DIFF_CHAR - 1)
    {
        LOG("too many windows, we only have %i characters\n", DIFF_CHAR);
        return -1;
    }

    int key = id_map.num;
    id_map.num++;
    id_map.map[key] = id;
    return key + MIN_CHAR;
}

int map_get(char key)
{
    if (key < MIN_CHAR || key > MIN_CHAR + id_map.num - 1)
    {
        LOG("selection not in range\n");
        return -1;
    }

    return id_map.map[key - MIN_CHAR];
}
