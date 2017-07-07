#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_x(char *args) {
    int n;
    bool success;
    uint32_t base;
	char *N = strtok(NULL, " ");
    char *expr_str = args + strlen(N) + 1;

    if (N == NULL || args == NULL) {
        printf("Invalid command.\n");
        return 0;
    }
    base = expr(expr_str, &success);
    if (!success) {
        printf("Invalid command.\n");
        return 0;
    }
    if ((sscanf(N, "%i", &n) != 1))
        n = 1;

    while (n--) {
        printf("0x%08x: 0x%08x\n", base, swaddr_read(base, 4));
        base += 4;
    }

    return 0;
}

static int cmd_info(char *args) {
	char *subcmd = strtok(NULL, " ");
	int i;

    if (subcmd == NULL) {
        printf("nothing to do.\n");
    } else if (strcmp(subcmd, "r") == 0) {
	    for(i = R_EAX; i <= R_EDI; i ++)
            printf("%s\t%#x\n", regsl[i], reg_l(i));
    } else if (strcmp(subcmd, "w") == 0) {
        /* TODO: implement info watchpoint */
        print_wp();
    }
	return 0;
}

static int cmd_si(char *args) {
    int n;
    if (args == NULL || (sscanf(args, "%i", &n) != 1))
        n = 1;
    cpu_exec(n);
    return 0;
}

static int cmd_p(char *args) {
    bool success;
    uint32_t result;
    if (args)
        result = expr(args, &success);
    else
        goto err;

    if (success) {
        printf("%d\n", result);
        goto out;
    }

err:
    printf("Invalid expression.\n");
out:
    return 0;
}

static int cmd_w(char *args) {
    bool success;
    uint32_t result;
    if (!args)
        goto err;

    result = expr(args, &success);
    if (!success)
        goto err;

    WP *wp = new_wp();
    wp->expr = strdup(args);
    wp->old = result;
    printf("Watchpoint %d: %s\n", wp->NO, wp->expr);
    return 0;

err:
    printf("Invalid expression.\n");
    return 0;
}

static int cmd_d(char *args) {
    int n;
    WP *wp;
    if (args == NULL || (sscanf(args, "%i", &n) != 1)) {
        printf("Invalid watchpoint number: \'%s\'.", args);
        return 0;
    }
    wp = find_wp(n);
    if (!wp) {
        printf("Watchpoint %d doesn't exist.", n);
        return 0;
    }
    free_wp(wp);
    printf("Watchpoint %d is deleted.", n);
    return 0;
}

static int cmd_help(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
    { "si", "Step [N] instruction exactly.", cmd_si },
    { "info", "[r] List registers; [w] List watchpoints.", cmd_info },
    { "x", "Examine the contents of memory.", cmd_x },
    { "p", "Print the value of the expression", cmd_p},
    { "w", "Watchpoint", cmd_w},
    { "d", "Delete watchpoint", cmd_d},

	/* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
