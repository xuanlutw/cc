#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define MAX_REG_LEN 1000

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
    while (true) {
        for (count = 0; count < TRANS_ENTRY_PER_RECORD; ++count) {
            if (trans_src->idx[count] == des)
                return;
            else if (trans_src->idx[count] == 0) {
                trans_src->idx[count] = des;
                return;
            }
        }
        if (!(trans_src->next))
            break;
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
    /*Trans*    trans;*/
    /*uint16_t* final_status;*/
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
    Regnode* node = new_regnode(val);
    if (val == '*')
        // Uniary
        node->left = stack_pop(stack_node);
    if (strchr("|.", val)) {
        // Binary
        node->right = stack_pop(stack_node);
        node->left  = stack_pop(stack_node);
    }
    stack_push(stack_node, node);
}

Regnode* regexp_to_regnode(char* regexp) {
    if (strlen(regexp) > MAX_REG_LEN) {
        printf("Regexp tooooo long\n");
        exit(-1);
    }

    Stack*   stack_op   = stack_init();
    Stack*   stack_node = stack_init();
    uint16_t regexp_idx = 0;
    char     concat     = '.';

    for (regexp_idx = 0; regexp_idx < strlen(regexp); ++regexp_idx) { 
        if (stack_len(stack_op))
            printf(">> %c, %c, %d\n", regexp[regexp_idx],\
                   *(char*)stack_top(stack_op), stack_len(stack_op));


/*
 * {), a, *} X {a, (}
 */
        if (regexp_idx > 0 && \
            !strchr("(|",  regexp[regexp_idx - 1]) &&\
            !strchr(")*|", regexp[regexp_idx])) {
            stack_push(stack_op, &concat);
        }

        if (regexp[regexp_idx] == '(')
            stack_push(stack_op, regexp + regexp_idx);
        else if (regexp[regexp_idx] == ')') {
            while (*(char*)stack_top(stack_op) != '(')
                add_node(*(char*)stack_pop(stack_op), stack_node);
            stack_pop(stack_op); // pop '('
        }
        else if (regexp[regexp_idx] == '*')
            add_node('*', stack_node);
        else if (regexp[regexp_idx] == '|') {
            while (*(char*)stack_top(stack_op) == '.')
                add_node(*(char*)stack_pop(stack_op), stack_node);
            stack_push(stack_op, regexp + regexp_idx);
        }
        else
            add_node(regexp[regexp_idx], stack_node);
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
    if (root->val == '*') {
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
    else if (root->val == '|') {
        NFA nfa1 = regnode_to_nfa(root->left, trans, final_status, start_state);
        NFA nfa2 = regnode_to_nfa(root->right, trans, final_status, nfa1.used_state_e);
        uint16_t init_state  = nfa2.used_state_e;
        trans_add(trans, init_state, nfa1.init_state, EPS);
        trans_add(trans, init_state, nfa2.init_state, EPS);
        nfa1.init_state = init_state;  
        nfa1.used_state_e = nfa2.used_state_e + 1;  
        return nfa1;
    }
    else if (root->val == '.') {
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
    else {
        NFA nfa = {
            .trans        = trans,
            .final_status = final_status,
            .init_state   = start_state,  
            .used_state_s = start_state,
            .used_state_e = start_state + 2
        };
        trans_add(trans, start_state, start_state + 1, root->val);
        final_status[start_state + 1] = 1;
        return nfa;
    }
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
    Regnode* S = regexp_to_regnode("(a|b)*abb#");
    print(S);
    printf("\n");

    Trans    trans[MAX_REG_LEN];
    uint16_t final_status[MAX_REG_LEN];
    memset(trans, 0, sizeof(Trans) * MAX_REG_LEN);
    memset(final_status, 0, sizeof(uint16_t) * MAX_REG_LEN);
    NFA nfa = regnode_to_nfa(S, trans, final_status, 1);
    print_nfa(nfa);

    return 0;
}

