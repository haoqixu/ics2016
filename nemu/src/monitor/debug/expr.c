#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum {
	NOTYPE = 256,
    DEREF_, NEG_,
    EQ, NEQ, LE, GE, LT, GT, AND, OR, PLUS, SUB, LPARE, RPARE, MUL, DIV, NOT,
    EOS_,
    REG, HEX, OCT, DEC,
	/* TODO: Add more token types */
};

#define G '>',
#define L '<',
#define E '=',
#define _ 0,
static int8_t preced[][EOS_-NOTYPE] = {
	/* DEREF */ { L L G G G G G G G G G G L G G G L G },
	/* NEG_  */ { L L G G G G G G G G G G L G G G L G },
	/* EQ    */ { L L G G G G G G G G L L L G L L L G },
	/* NEQ   */ { L L G G G G G G G G L L L G L L L G },
	/* LE    */ { L L G G G G G G G G L L L G L L L G },
	/* GE    */ { L L G G G G G G G G L L L G L L L G },
	/* LT    */ { L L G G G G G G G G L L L G L L L G },
	/* GT    */ { L L G G G G G G G G L L L G L L L G },
	/* AND   */ { L L L L L L L L G G L L L G L L L G },
	/* OR    */ { L L L L L L L L L G L L L G L L L G },
	/* PLUS  */ { L L G G G G G G G G G G L G L L L G },
	/* SUB   */ { L L G G G G G G G G G G L G L L L G },
	/* LPARE */ { L L L L L L L L L L L L L E L L L _ },
	/* RPARE */ { G G G G G G G G G G G G E G G G G G },
	/* MUL   */ { L L G G G G G G G G G G L G G G L G },
	/* DIV   */ { L L G G G G G G G G G G L G G G L G },
	/* NOT   */ { L L G G G G G G G G G G L G G G L G },
	/* EOS_  */ { L L L L L L L L L L L L L _ L L L E },
};
#undef G
#undef L
#undef E
#undef _

/*
 * op1: the operator on the top of stack
 * op2: the operator get from tokens
 */
static inline int8_t op_preced(int op1, int op2)
{
	return preced[op1-NOTYPE-1][op2-NOTYPE-1];
}


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
    {"\\(", LPARE},
    {"\\)", RPARE},
	{"\\+", PLUS},
	{"-", SUB},
	{"\\*", MUL},
	{"/", DIV},
	{"==", EQ},						// equal
	{"!=", NEQ},					// not equal
	{"<=", LE},						// less or equal
	{">=", GE},						// greater or equal
    {">", GT},
    {"<", LT},
	{"&&", AND},
	{"\\|\\|", OR},
    {"!", NOT},                     // logcal not
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
                    case NOTYPE: break;
					default:
                        tokens[nr_token].str[0] = '\0';
                        strncat(tokens[nr_token].str, substr_start,
                                (substr_len < sizeof(tokens[nr_token].str))
                                ? substr_len : sizeof(tokens[nr_token].str));
                        /* through down */
                    case EQ: case NEQ: case LE: case GE: case AND: case OR:
                    case PLUS: case SUB: case LPARE: case RPARE: case MUL:
                    case DIV: case LT: case GT: case NOT: case EOS_:
                        tokens[nr_token].type = rules[i].token_type;
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

static inline bool is_op(int type)
{
    if (type > NOTYPE && type <= EOS_)
        return true;
    return false;
}


static uint32_t operate(int op, uint32_t obj1, uint32_t obj2)
{
    switch (op) {
        case DEREF_:    return swaddr_read(obj1, 4);
        case NEG_:      return -obj1;
        case NOT:       return !obj1;
        case MUL:       return obj1 * obj2;
        case DIV:       return obj1 / obj2;
        case PLUS:      return obj1 + obj2;
        case SUB:       return obj1 - obj2;
        case EQ:        return obj1 == obj2;
        case NEQ:       return obj1 != obj2;
        case LE:        return obj1 <= obj2;
        case GE:        return obj1 >= obj2;
        case LT:        return obj1 < obj2;
        case GT:        return obj1 > obj2;
        case AND:       return obj1 && obj2;
        case OR:        return obj1 || obj2;
        default:        return 0;
    }
}


#define PUSH_OP(x) do { op_stack[op_i++] = (x); } while (0)
#define PUSH_OBJ(x) do { obj_stack[obj_i++] = (x); } while (0)
#define TOP_OP (op_stack[op_i-1])
#define POP_OP() do { --op_i; } while (0)
#define POP_OBJ(x) do { x = obj_stack[--obj_i]; } while (0)
static uint32_t eval(bool *success)
{
    static int op_stack[32];
    static uint32_t obj_stack[32];
    int obj_i = 0, op_i = 0;
    int token_type, i;
    int8_t op;
    uint32_t o1, o2;

    *success = true;
    PUSH_OP(EOS_);
    tokens[nr_token].type = EOS_;  /* guard */
    nr_token++;
    for (i = 0; TOP_OP != EOS_ || tokens[i].type != EOS_; ) {
        if (is_op((token_type = tokens[i].type))) {
            switch (op_preced(TOP_OP, token_type)) {
                case '<':
                    PUSH_OP(token_type);
                    i++;
                    break;
                case '>':
                    op = TOP_OP;
                    POP_OP();
                    if (op == NOT || op == NEG_ || op == DEREF_) {
                        POP_OBJ(o1);
                        PUSH_OBJ(operate(op, o1, 0));
                    } else {
                        POP_OBJ(o2);
                        POP_OBJ(o1);
                        PUSH_OBJ(operate(op, o1, o2));
                    }
                    break;
                case '=':
                    POP_OP();
                    i++;
                    break;
                default:
                    *success = false;
                    return -1;
            }
        } else if (token_type == REG) {
            int j;
            for (j = R_EAX; j <= R_EDI; j++) {
                if (strcmp(regsl[j], tokens[i].str+1) == 0) {  /* skip '$' */
                    PUSH_OBJ(reg_l(j));
                    break;
                }
                if (strcmp(regsw[j], tokens[i].str+1) == 0) {  /* skip '$' */
                    PUSH_OBJ(reg_l(j));
                    break;
                }
            }
            if (j > R_EDI) {
                *success = false;
                return 0;
            }
            i++;
        } else {
            int j;
            sscanf(tokens[i].str, "%i", &j);
            PUSH_OBJ(j);
            i++;
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

