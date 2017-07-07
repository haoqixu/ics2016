#include <stdlib.h>
#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* new_wp()
{
    Assert(free_, "There is no more WP.");
    WP *t = free_;
    free_ = free_->next;
    t->next = head;
    head = t;
    return t;
}

void free_wp(WP *wp)
{
    WP **cur;
    for (cur = &head; *cur; cur = &(*cur)->next) {
        if (*cur == wp) {
            *cur = wp->next;
            wp->next = free_;
            free(wp->expr);
            free_ = wp;
            break;
        }
    }
}

bool check_wp()
{
    WP *p;
    int val;
    bool success;
    bool if_change = false;

    for (p = head; p; p = p->next) {
        val = expr(p->expr, &success);
        Assert(success, "invalid expression.");
        if (val != p->old) {
            printf("Old value = %d\nNew value = %d\n", p->old, val);
            p->old = val;
            if_change = true;
        }
    }
    return !if_change;
}

WP *find_wp(int n)
{
    WP *p;
    for (p = head; p; p = p->next) {
        if (p->NO == n)
            return p;
    }
    return NULL;
}

void print_wp()
{
    WP *p;
    printf("Num\tExpression\n");
    for (p = head; p; p = p->next)
        printf("%d\t%s\n", p->NO, p->expr);
}
