#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* TODO: Add more members if necessary */
    char *expr;
    uint32_t old;


} WP;

WP* new_wp();
void free_wp(WP *wp);
bool check_wp();
#endif
