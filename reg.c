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

Regnode* convert(char* regexp) {
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
    /*Regnode* S = convert("(a|bc)*");*/
    Regnode* S = convert("(0|(1(01*(00)*0)*1)*)*");
    /*Regnode* S = convert("(a|b)*abb#");*/
    print(S);
    printf("\n");
    return 0;
}

