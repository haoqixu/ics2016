#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum {
	NOTYPE = 256, EQ, NEQ, LE, GE, AND, OR, REG, HEX, OCT, DEC,
    DEREF_, NEG_,

	/* TODO: Add more token types */

};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

    /* Priority:
     * ! -(neg) *(deref)
     * / *
     * + -
     * == != <= >= > <
     * &&
     * ||
     */

	{" +",	NOTYPE},				// spaces
    {"\\(", '('},
    {"\\)", ')'},
	{"\\+", '+'},
	{"-", '-'},
	{"\\*", '*'},
	{"/", '/'},
	{"==", EQ},						// equal
	{"!=", NEQ},					// not equal
	{"<=", LE},						// less or equal
	{">=", GE},						// greater or equal
    {">", '>'},
    {"<", '<'},
	{"&&", AND},
	{"\\|\\|", OR},
    {"!", '!'},                     // logcal not
    {"\\$.+", REG},                 // register name
    {"0x[0-9a-fA-F]+", HEX},
    {"0[0-7]+", OCT},
    {"[0-9]+", DEC},
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s",
                   error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0
                && pmatch.rm_so == 0)
            {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
                    i, rules[i].regex, position, substr_len,
                    substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */

				switch(rules[i].token_type) {
                    case NOTYPE: continue;
					default:
                        tokens[nr_token].type = rules[i].token_type;
                        tokens[nr_token].str[0] = '\0';
                        strncat(tokens[nr_token].str, substr_start,
                                (substr_len < sizeof(tokens[nr_token].str))
                                ? substr_len : sizeof(tokens[nr_token].str));
                        nr_token++;
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n",
                   position, e, position, "");
			return false;
		}
	}

	return true; 
}

static int8_t op_cmp(int op1, int op2)
{
    int i = 2;
    int op = op1;
    int8_t op_priority[2];

    for (i = 0; i < 2; i++, op = op2) {
        switch (op) {
            case DEREF_: case NEG_:
                op_priority[i] = 5;
                break;
            case '*': case '/':
                op_priority[i] = 4;
                break;
            case '+': case '-':
                op_priority[i] = 3;
                break;
            case EQ: case NEQ: case LE: case GE: case '<': case '>':
                op_priority[i] = 2;
                break;
            case AND:
                op_priority[i] = 1;
                break;
            case OR:
                op_priority[i] = 0;
                break;
            default:
                break;
        }
    }
    return op_priority[0] - op_priority[1];
}

static inline bool is_op(int type)
{
    switch (type) {
        case '(': case ')': case '+': case '-': case '*': case '/':
        case EQ: case NEQ: case LE: case GE: case '>': case '<':
        case AND: case OR: case '!':
            return true;
        default:
            return false;
    }
}


static int op_stack[32];
static uint32_t obj_stack[32];
static int i, op_i, obj_i;

static uint32_t operate(int op, uint32_t obj1, uint32_t obj2)
{
    switch (op) {
        case DEREF_:    return swaddr_read(obj1, 4);
        case NEG_:      return -obj1;
        case '*':       return obj1 * obj2;
        case '/':       return obj1 / obj2;
        case '+':       return obj1 + obj2;
        case '-':       return obj1 - obj2;
        case EQ:        return obj1 == obj2;
        case NEQ:       return obj1 != obj2;
        case LE:        return obj1 <= obj2;
        case GE:        return obj1 >= obj2;
        case '<':       return obj1 < obj2;
        case '>':       return obj1 > obj2;
        case AND:       return obj1 && obj2;
        case OR:        return obj1 || obj2;
        default:        return 0;
    }
}


static uint32_t eval(bool *success)
{
    int op;

    *success = true;
    for (i = op_i = obj_i = 0; i < nr_token; i++) {
        if (is_op(tokens[i].type)) {
            /* is an operator */

            if (i == 0 || is_op(tokens[i-1].type)) {
                if (tokens[i].type == '-') {
                    tokens[i].type = NEG_;
                } else if (tokens[i].type == '*') {
                    tokens[i].type = DEREF_;
                } else {
                    *success = false;
                    return 0;
                }
            }

            if (op_i == 0 || tokens[i].type == '('){
                op_stack[op_i++] = tokens[i].type;
            } else if (tokens[i].type == ')') {
                while ((op = op_stack[--op_i]) != '(') {
                    if (op == DEREF_ || op == NEG_) {
                        obj_stack[obj_i-1] = operate(op, obj_stack[obj_i-1], 0);
                    } else {
                        obj_stack[obj_i-2] = operate(op,
                            obj_stack[obj_i-2], obj_stack[obj_i-1]);
                        obj_i--;
                    }
                }
            } else {
                op = op_stack[op_i-1];
                while (op != '(' && op_cmp(tokens[i].type, op) < 0) {
                    if (op == DEREF_ || op == NEG_) {
                        obj_stack[obj_i-1] = operate(op, obj_stack[obj_i-1], 0);
                    } else {
                        obj_stack[obj_i-2] = operate(op,
                            obj_stack[obj_i-2], obj_stack[obj_i-1]);
                        obj_i--;
                    }
                    op_i--;
                    op = op_stack[op_i-1];
                }
                op_stack[op_i++] = tokens[i].type;
            }

        } else {
            /* is a number or register */
            /* TODO: Modify this code if cpu struct is changed. */
            if (tokens[i].type == REG) {
                int j;
                for (j = R_EAX; j <= R_EDI; j++) {
                    if (strcmp(regsl[i], tokens[i].str+1) == 0)
                        obj_stack[obj_i++] = reg_l(j);
                    else if (strcmp(regsw[i], tokens[i].str+1) == 0)
                        obj_stack[obj_i++] = reg_w(j);
                    else if (strcmp(regsb[i], tokens[i].str+1) == 0)
                        obj_stack[obj_i++] = reg_b(j);
                }
                if (j > R_EDI) {
                    *success = false;
                    return 0;
                }
            } else {
                sscanf(tokens[i].str, "%i", &obj_stack[obj_i++]);
            }
        }
    }
    while (op_i-- > 0) {
        op = op_stack[op_i];
        if (op == DEREF_ || op == NEG_) {
            obj_stack[obj_i-1] = operate(op, obj_stack[obj_i-1], 0);
        } else {
            obj_stack[obj_i-2] = operate(op,
                obj_stack[obj_i-2], obj_stack[obj_i-1]);
            obj_i--;
        }
    }
    return obj_stack[0];

}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}

	/* TODO: Insert codes to evaluate the expression. */
    return eval(success);
}

