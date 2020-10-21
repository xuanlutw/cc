#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define MAX_REG_LEN 1000

#define OP_EPS  1
#define OP_STAR 2
#define OP_PLUS 3
#define OP_QUES 4
#define OP_CON  5
#define OP_UNI  6
#define OP_DIG  7
#define OP_AZ   8
#define OP_az   9
#define OP_Az   10
#define OP_W    11

typedef struct regnode {
    char val;
    struct regnode* left;
    struct regnode* right;
} Regnode;

Regnode* new_regnode(char val) {
    Regnode* new_node = malloc(sizeof(Regnode));
    new_node->val     = val;
    new_node->left    = NULL;
    new_node->right   = NULL;
    return new_node;
}

#define TRANS_ENTRY_PER_RECORD 16
#define NUM_SYMBOLS 128
#define EPS 0
typedef struct trans_record {
    uint16_t             idx[TRANS_ENTRY_PER_RECORD];
    struct trans_record* next;
} Trans_record;

typedef struct trans {
    Trans_record* record[NUM_SYMBOLS];
} Trans;

typedef struct {
    Trans*    trans;
    uint16_t* final_status;
    uint16_t  init_state;  
    uint16_t  used_state_s;  
    uint16_t  used_state_e;  
} NFA;

typedef struct {
    NFA*  nfa;
    bool* state_now;
    bool* state_tmp;
    uint16_t state_count;
    uint16_t final_count;
    uint16_t final_status;
} NFA_config;

Trans_record* new_trans_record() {
    Trans_record* record = malloc(sizeof(Trans_record));
    memset(record->idx, 0, sizeof(uint16_t) * TRANS_ENTRY_PER_RECORD);
    record->next = NULL;
    return record;
}

void trans_add(Trans* trans, uint16_t src, uint16_t des, char symbol) {
    Trans_record* trans_src = trans[src].record[symbol];
    uint16_t count;
    if (!trans_src) {
        trans[src].record[symbol]         = new_trans_record();
        trans[src].record[symbol]->idx[0] = des;
        return;
    }
    while (trans_src) {
        for (count = 0; count < TRANS_ENTRY_PER_RECORD; ++count) {
            if (trans_src->idx[count] == des)
                return;
            else if (trans_src->idx[count] == 0) {
                trans_src->idx[count] = des;
                return;
            }
        }
        trans_src = trans_src->next;
    }
    trans_src->next        = new_trans_record();
    trans_src->next->idx[0] = des;
}

void print_nfa(NFA nfa) {
    printf("================================\n");
    printf("init_state: %d\n",   nfa.init_state);  
    printf("used_state_s: %d\n", nfa.used_state_s);  
    printf("used_state_e: %d\n", nfa.used_state_e);  
    for (uint16_t count = nfa.used_state_s; count < nfa.used_state_e; ++count) {
        bool first_time = true;
        printf("%3d ", count);
        for (uint16_t symbol = 0; symbol < NUM_SYMBOLS; ++symbol) {
            Trans_record* record = nfa.trans[count].record[symbol];
            if (!record)
                continue;
            if (first_time)
                first_time = false;
            else
                printf("    ");
            printf("%c ", symbol? symbol: ' ');
            while (record) {
                for (uint16_t idx = 0; idx < TRANS_ENTRY_PER_RECORD; ++idx)
                    if (!(record->idx[idx]))
                        break;
                    else
                        printf("%3d ", record->idx[idx]);
                record = record->next;
            }
            printf("\n");
        }
        if (first_time)
            printf("\n");
    }
    printf("================================\n");
}

// Stack
typedef struct {
    void*    val[MAX_REG_LEN + 10];
    uint16_t top_pt;
} Stack;

Stack* stack_init() {
    Stack* stack = malloc(sizeof(Stack));
    stack->top_pt = 0;
    return stack;
}

void stack_push(Stack* stack, void* val) {
    stack->val[stack->top_pt] = val;
    ++(stack->top_pt);
}

void* stack_top(Stack* stack) {
    return stack->val[stack->top_pt - 1];
}

void* stack_pop(Stack* stack) {
    --stack->top_pt;
    return stack->val[stack->top_pt];
}

uint16_t stack_len(Stack* stack) {
    return stack->top_pt;
}

void stack_destroy(Stack* stack) {
    free(stack);
}

void add_node(char val, Stack* stack_node) {
    Regnode* node1 = new_regnode(val);
    // Uniary
    if (val == OP_STAR)
        node1->left = stack_pop(stack_node);
    if (val == OP_PLUS) {
        Regnode* node2 = new_regnode(OP_STAR);
        node1->val   = OP_CON;
        node1->left  = stack_top(stack_node);
        node1->right = node2;
        node2->left  = stack_pop(stack_node);
    }
    if (val == OP_QUES) {
        Regnode* node2 = new_regnode(OP_EPS);
        node1->val   = OP_UNI;
        node1->left  = node2;
        node1->right = stack_pop(stack_node);
    }
    // Binary
    if ((val == OP_CON) || (val == OP_UNI)) {
        node1->right = stack_pop(stack_node);
        node1->left  = stack_pop(stack_node);
    }
    stack_push(stack_node, node1);
}

Regnode* regexp_to_regnode(char* regexp) {
    if (strlen(regexp) > MAX_REG_LEN) {
        printf("Regexp tooooo long\n");
        exit(-1);
    }

    Stack*   stack_op   = stack_init();
    Stack*   stack_node = stack_init();
    uint16_t regexp_idx = 0;
    char     op_con     = OP_CON;
    char     op_uni     = OP_UNI;
    bool     pre_symbol_term = false;

    for (regexp_idx = 0; regexp_idx < strlen(regexp); ++regexp_idx) { 
        if (stack_len(stack_op))
            printf(">> %c, %c, %d\n", regexp[regexp_idx],\
                   *(char*)stack_top(stack_op), stack_len(stack_op));

/*
 * {), a, *} X {a, (}
 */

        if (regexp_idx > 0 && pre_symbol_term && \
            !strchr(")*?+|", regexp[regexp_idx])) {
            stack_push(stack_op, &op_con);
        }

        if (regexp[regexp_idx] == '(') {
            pre_symbol_term = false;
            stack_push(stack_op, regexp + regexp_idx);
        }
        else if (regexp[regexp_idx] == ')') {
            pre_symbol_term = true;
            while (*(char*)stack_top(stack_op) != '(')
                add_node(*(char*)stack_pop(stack_op), stack_node);
            stack_pop(stack_op); // pop '('
        }
        else if (strchr("*", regexp[regexp_idx])) {
            pre_symbol_term = true;
            add_node(OP_STAR, stack_node);
        }
        else if (strchr("+", regexp[regexp_idx])) {
            pre_symbol_term = true;
            add_node(OP_PLUS, stack_node);
        }
        else if (strchr("?", regexp[regexp_idx])) {
            pre_symbol_term = true;
            add_node(OP_QUES, stack_node);
        }
        else if (regexp[regexp_idx] == '|') {
            pre_symbol_term = false;
            while (stack_len(stack_op) && *(char*)stack_top(stack_op) == '.')
                add_node(*(char*)stack_pop(stack_op), stack_node);
            stack_push(stack_op, &op_uni);
        }
        else if (regexp[regexp_idx] == '\\') {
            pre_symbol_term = true;
            regexp_idx++;
            if (regexp[regexp_idx] == 'e')
                add_node(OP_EPS, stack_node);
            else if (regexp[regexp_idx] == 'n')
                add_node('\n', stack_node);
            else if (regexp[regexp_idx] == 'd')
                add_node(OP_DIG, stack_node);
            else if (regexp[regexp_idx] == 'A')
                add_node(OP_AZ, stack_node);
            else if (regexp[regexp_idx] == 'a')
                add_node(OP_az, stack_node);
            else if (regexp[regexp_idx] == 'z')
                add_node(OP_Az, stack_node);
            else if (regexp[regexp_idx] == 'w')
                add_node(OP_W, stack_node);
            else
                add_node(regexp[regexp_idx], stack_node);
        }
        else {
            pre_symbol_term = true;
            add_node(regexp[regexp_idx], stack_node);
        }
    }

    while (stack_len(stack_op))
        add_node(*(char*)stack_pop(stack_op), stack_node);
    printf("FINAL %d %d \n", stack_len(stack_op), stack_len(stack_node));
    Regnode* ret =  stack_top(stack_node);

    stack_destroy(stack_op);
    stack_destroy(stack_node);

    return ret;
}

NFA regnode_to_nfa(Regnode* root, Trans* trans, uint16_t* final_status, \
                   uint16_t start_state) {
    uint16_t count;
    if (root->val == OP_STAR) {
        NFA nfa = regnode_to_nfa(root->left, trans, final_status, start_state);
        uint16_t init_state  = nfa.used_state_e;
        uint16_t final_state = nfa.used_state_e + 1;
        trans_add(trans, init_state, nfa.init_state, EPS);
        trans_add(trans, init_state, final_state, EPS);
        for (count = nfa.used_state_s; count < nfa.used_state_e; ++count)
            if (nfa.final_status[count]) {
                nfa.final_status[count] = 0;
                trans_add(trans, count, nfa.init_state, EPS);
                trans_add(trans, count, final_state, EPS);
            }
        nfa.final_status[final_state] = 1;
        nfa.init_state = init_state;  
        nfa.used_state_e += 2;  
        return nfa;
    }
    else if (root->val == OP_UNI) {
        NFA nfa1 = regnode_to_nfa(root->left, trans, final_status, start_state);
        NFA nfa2 = regnode_to_nfa(root->right, trans, final_status, nfa1.used_state_e);
        uint16_t init_state  = nfa2.used_state_e;
        trans_add(trans, init_state, nfa1.init_state, EPS);
        trans_add(trans, init_state, nfa2.init_state, EPS);
        nfa1.init_state = init_state;  
        nfa1.used_state_e = nfa2.used_state_e + 1;  
        return nfa1;
    }
    else if (root->val == OP_CON) {
        NFA nfa1 = regnode_to_nfa(root->left, trans, final_status, start_state);
        NFA nfa2 = regnode_to_nfa(root->right, trans, final_status, nfa1.used_state_e);
        for (count = nfa1.used_state_s; count < nfa1.used_state_e; ++count)
            if (nfa1.final_status[count]) {
                nfa1.final_status[count] = 0;
                trans_add(trans, count, nfa2.init_state, EPS);
            }
        nfa1.used_state_e = nfa2.used_state_e;
        return nfa1;
    }
    else if (root->val == OP_EPS) {
        NFA nfa = {
            .trans        = trans,
            .final_status = final_status,
            .init_state   = start_state,  
            .used_state_s = start_state,
            .used_state_e = start_state + 1
        };
        final_status[start_state] = 1;
        return nfa;
    }
    else {
        NFA nfa = {
            .trans        = trans,
            .final_status = final_status,
            .init_state   = start_state,  
            .used_state_s = start_state,
            .used_state_e = start_state + 2
        };
        char symbol;
        if (root->val == OP_DIG)
            for (symbol = '0'; symbol <= '9'; ++symbol)
                trans_add(trans, start_state, start_state + 1, symbol);
        else if (root->val == OP_AZ)
            for (symbol = 'A'; symbol <= 'Z'; ++symbol)
                trans_add(trans, start_state, start_state + 1, symbol);
        else if (root->val == OP_az)
            for (symbol = 'a'; symbol <= 'z'; ++symbol)
                trans_add(trans, start_state, start_state + 1, symbol);
        else if (root->val == OP_Az) {
            for (symbol = 'A'; symbol <= 'Z'; ++symbol)
                trans_add(trans, start_state, start_state + 1, symbol);
            for (symbol = 'a'; symbol <= 'z'; ++symbol)
                trans_add(trans, start_state, start_state + 1, symbol);
        }
        else if (root->val == OP_W) {
            trans_add(trans, start_state, start_state + 1, ' ');
            trans_add(trans, start_state, start_state + 1, '\n');
            trans_add(trans, start_state, start_state + 1, '\t');
        }
        else
            trans_add(trans, start_state, start_state + 1, root->val);
        final_status[start_state + 1] = 1;
        return nfa;
    }
}

void NFA_eps_closure_s(NFA_config* nfa_config, uint16_t state) {
    Trans_record* record = nfa_config->nfa->trans[state].record[EPS];
    uint16_t count;
    if (nfa_config->state_tmp[state])
        return;
    nfa_config->state_tmp[state] = true;
    ++(nfa_config->state_count);
    if (nfa_config->nfa->final_status[state])
        ++(nfa_config->final_count);
    if (nfa_config->nfa->final_status[state] > nfa_config->final_status)
        nfa_config->final_status = nfa_config->nfa->final_status[state];
    if (!record)
        return;
    while (record) {
        for (count = 0; (count < TRANS_ENTRY_PER_RECORD) && \
                        (record->idx[count] != 0); ++count) {
            NFA_eps_closure_s(nfa_config, record->idx[count]);
            nfa_config->state_tmp[record->idx[count]] = true;
        }
        record = record->next;
    }
    
}

void NFA_eps_closure(NFA_config* nfa_config) {
    uint16_t count;
    memset(nfa_config->state_tmp, 0, sizeof(bool) * MAX_REG_LEN);
    nfa_config->state_count = 0;
    nfa_config->final_count = 0;
    nfa_config->final_status = 0;
    for (count = nfa_config->nfa->used_state_s; \
         count < nfa_config->nfa->used_state_e; ++count)
        if (nfa_config->state_now[count])
            NFA_eps_closure_s(nfa_config, count);
    bool* tmp = nfa_config->state_now;
    nfa_config->state_now = nfa_config->state_tmp;
    nfa_config->state_tmp = tmp;
}

void NFA_move(NFA_config* nfa_config, char symbol) {
    uint16_t state;
    uint16_t count;
    memset(nfa_config->state_tmp, 0, sizeof(bool) * MAX_REG_LEN);
    for (state = nfa_config->nfa->used_state_s; \
         state < nfa_config->nfa->used_state_e; ++state) {
        Trans_record* record = nfa_config->nfa->trans[state].record[symbol];
        if (!record || !(nfa_config->state_now[state]))
            continue;
        /*printf("%d %d\n", state, (nfa_config->state_now[state]));*/
        /*printf("%d\n", state);*/
        while (record) {
            for (count = 0; (count < TRANS_ENTRY_PER_RECORD) && \
                            (record->idx[count] != 0); ++count)
                nfa_config->state_tmp[record->idx[count]] = true;
            record = record->next;
        }
    }
    bool* tmp = nfa_config->state_now;
    nfa_config->state_now = nfa_config->state_tmp;
    nfa_config->state_tmp = tmp;
    NFA_eps_closure(nfa_config);
}

void NFA_run_init(NFA_config* nfa_config) {
    memset(nfa_config->state_now, 0, sizeof(bool) * MAX_REG_LEN);
    nfa_config->state_now[nfa_config->nfa->init_state] = true;
    NFA_eps_closure(nfa_config);
}

void NFA_run(NFA* nfa, char* str) {
    uint16_t str_pt;
    uint16_t state;
    bool* state_now = malloc(sizeof(bool) * MAX_REG_LEN);
    bool* state_tmp = malloc(sizeof(bool) * MAX_REG_LEN);
    NFA_config nfa_config ={
        .nfa = nfa,
        .state_now = state_now,
        .state_tmp = state_tmp };
    NFA_run_init(&nfa_config);
    for (state = nfa->used_state_s; \
         state < nfa->used_state_e; ++state)
        if (nfa_config.state_now[state])
            printf("%3d ", state);
    printf("\n");
    for (str_pt = 0; str_pt < strlen(str); ++str_pt) {
        NFA_move(&nfa_config, str[str_pt]);  
        printf("%c #s = %3d, f = %3d\n", str[str_pt], nfa_config.state_count, nfa_config.final_count);
        for (state = nfa->used_state_s; \
             state < nfa->used_state_e; ++state)
            if (nfa_config.state_now[state])
                printf("%3d ", state);
        printf("\n");
    }
    if (nfa_config.final_status)
        printf("ACCEPT, %d\n", nfa_config.final_status);
    else
        printf("REJECT\n");
}

void print(Regnode* root) {
    if (!root)
        return;
    print(root->left);
    printf("%c", root->val);
    print(root->right);
}

int main() {
    /*Regnode* S = convert("(a|b)*.a.b.b.#");*/
    /*Regnode* S = convert("(0|(1.(0.1*.(0.0)*.0)*.1)*)*");*/
    /*Regnode* S = convert("(a|b.c)*");*/
    /*Regnode* S = regexp_to_regnode("(a|bc)*");*/
    /*Regnode* S = regexp_to_regnode("(0|(1(01*(00)*0)*1)*)*");*/
    /*Regnode* S = regexp_to_regnode("(a|b|#)*abb#");*/
    /*Regnode* S = regexp_to_regnode("(a|b|#)?abb#");*/
    /*Regnode* S = regexp_to_regnode("\\e|((a|b|#)*abb#)");*/
    /*Regnode* S = regexp_to_regnode("(if|a|b|#)*");*/
    /*Regnode* S = regexp_to_regnode("\\a*");*/
    Regnode* S = regexp_to_regnode("\\d*E-?\\d*");
    int _sds;
    print(S);
    printf("\n");

    Trans    trans[MAX_REG_LEN];
    uint16_t final_status[MAX_REG_LEN];
    memset(trans, 0, sizeof(Trans) * MAX_REG_LEN);
    memset(final_status, 0, sizeof(uint16_t) * MAX_REG_LEN);
    NFA nfa = regnode_to_nfa(S, trans, final_status, 1);
    print_nfa(nfa);

    /*NFA_run(&nfa, "aabb#abb#");*/
    /*NFA_run(&nfa, "abb#");*/
    /*NFA_run(&nfa, "");*/
    /*NFA_run(&nfa, "ascas");*/
    /*NFA_run(&nfa, "Aascas");*/
    NFA_run(&nfa, "123");
    NFA_run(&nfa, "123E5");
    NFA_run(&nfa, "123E-102");
    NFA_run(&nfa, "123ER10");

    return 0;
}

