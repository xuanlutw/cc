#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_REG_LEN 1000
#define MAX_TOKEN_LEN 100

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

#define TRANS_ENTRY_PER_RECORD 4
#define NUM_SYMBOLS 128
#define EPS 0

// =============================================================================
// Stack
typedef struct {
    void**   val;
    uint16_t top_pt;
} Stack;

Stack* stack_init(uint16_t max_size) {
    Stack* stack = malloc(sizeof(Stack));
    stack->val   = calloc(sizeof(void**), max_size);
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
    free(stack->val);
    free(stack);
}
// =============================================================================

// =============================================================================
// Parse Regular Expression
typedef struct regnode {
    char val;
    struct regnode* left;
    struct regnode* right;
} Regnode;

Regnode* regnode_init(char val) {
    Regnode* new_node = malloc(sizeof(Regnode));
    new_node->val     = val;
    new_node->left    = NULL;
    new_node->right   = NULL;
    return new_node;
}

void regnode_print(Regnode* root) {
    if (!root)
        return;
    if (isprint(root->val))
        printf("%c", root->val);
    else
        printf("[%02d]", root->val);
    regnode_print(root->left);
    regnode_print(root->right);
}

void regnode_treelize(Regnode* root) {
    if (!root->val)
        return;
    if (root->left) {
        if (root->left->val) {
            regnode_treelize(root->left);
            root->left->val = 0;
        }
        else
            root->left = NULL;
    }
    if (root->right) {
        if (root->right->val) {
            regnode_treelize(root->right);
            root->right->val = 0;
        }
        else
            root->right = NULL;
    }
}

void regnode_destroy(Regnode* root) {
    regnode_treelize(root);
    if (root->left)
        regnode_destroy(root->left);
    if (root->right)
        regnode_destroy(root->right);
    free(root);
}

void push_regnode(char val, Stack* stack_node) {
    Regnode* node1 = regnode_init(val);

    // Uniary
    if (val == OP_STAR)
        node1->left = stack_pop(stack_node);
    if (val == OP_PLUS) {
        Regnode* node2 = regnode_init(OP_STAR);
        node1->val     = OP_CON;
        node1->left    = stack_top(stack_node);
        node1->right   = node2;
        node2->left    = stack_pop(stack_node);
    }
    if (val == OP_QUES) {
        Regnode* node2 = regnode_init(OP_EPS);
        node1->val     = OP_UNI;
        node1->left    = node2;
        node1->right   = stack_pop(stack_node);
    }

    // Binary
    if ((val == OP_CON) || (val == OP_UNI)) {
        node1->right = stack_pop(stack_node);
        node1->left  = stack_pop(stack_node);
    }
    stack_push(stack_node, node1);
}

Regnode* regexp_to_regnode(char* regexp) {
    Stack*   stack_op        = stack_init(strlen(regexp) * 2);
    Stack*   stack_node      = stack_init(strlen(regexp) * 2);
    uint16_t regexp_idx      = 0;
    char     op_con          = OP_CON;
    char     op_uni          = OP_UNI;
    bool     pre_symbol_term = false;


    if (strlen(regexp) > MAX_REG_LEN) {
        printf("Regexp tooooo long\n");
        exit(-1);
    }

    for (regexp_idx = 0; regexp_idx < strlen(regexp); ++regexp_idx) { 
        // Add omit concat
        if (pre_symbol_term && !strchr(")*?+|", regexp[regexp_idx]))
            stack_push(stack_op, &op_con);

        // Parse
        switch (regexp[regexp_idx]) {
            case '(':
                pre_symbol_term = false;
                stack_push(stack_op, regexp + regexp_idx);
                break;
            case ')':
                pre_symbol_term = true;
                while (*(char*)stack_top(stack_op) != '(')
                    push_regnode(*(char*)stack_pop(stack_op), stack_node);
                stack_pop(stack_op); // pop '('
                break;
            case '*':
                pre_symbol_term = true;
                push_regnode(OP_STAR, stack_node);
                break;
            case '+':
                pre_symbol_term = true;
                push_regnode(OP_PLUS, stack_node);
                break;
            case '?':
                pre_symbol_term = true;
                push_regnode(OP_QUES, stack_node);
                break;
            case '|':
                pre_symbol_term = false;
                while (stack_len(stack_op) && *(char*)stack_top(stack_op) == OP_CON)
                    push_regnode(*(char*)stack_pop(stack_op), stack_node);
                stack_push(stack_op, &op_uni);
                break;
            case '\\':
                pre_symbol_term = true;
                regexp_idx++;
                if (regexp[regexp_idx] == 'e')
                    push_regnode(OP_EPS, stack_node);
                else if (regexp[regexp_idx] == 'n')
                    push_regnode('\n', stack_node);
                else if (regexp[regexp_idx] == 'd')
                    push_regnode(OP_DIG, stack_node);
                else if (regexp[regexp_idx] == 'A')
                    push_regnode(OP_AZ, stack_node);
                else if (regexp[regexp_idx] == 'a')
                    push_regnode(OP_az, stack_node);
                else if (regexp[regexp_idx] == 'z')
                    push_regnode(OP_Az, stack_node);
                else if (regexp[regexp_idx] == 'w')
                    push_regnode(OP_W, stack_node);
                else
                    push_regnode(regexp[regexp_idx], stack_node);
                break;
            default:
                pre_symbol_term = true;
                push_regnode(regexp[regexp_idx], stack_node);
        }
    }

    while (stack_len(stack_op))
        push_regnode(*(char*)stack_pop(stack_op), stack_node);
    printf("Parse regexp done, remain op = %d. remain node = %d\n", \
           stack_len(stack_op), stack_len(stack_node));
    Regnode* root = stack_top(stack_node);
    stack_destroy(stack_op);
    stack_destroy(stack_node);
    return root;
}
// =============================================================================

// =============================================================================
// NFA
typedef struct trans {
    void*         des[TRANS_ENTRY_PER_RECORD];
    struct trans* next;
} Trans;

typedef struct state {
    Trans*        trans[NUM_SYMBOLS];
    uint16_t      final_status;
    bool          select_now;
    bool          select_tmp;
    struct state* next;
} State;

Trans* trans_init() {
    Trans* trans = malloc(sizeof(Trans));
    memset(trans->des, 0, sizeof(void*) * TRANS_ENTRY_PER_RECORD);
    trans->next = NULL;
    return trans;
}

Trans* trans_add_des(Trans* trans, State* des) {
    uint16_t count;
    if (!trans) {
        trans         = trans_init();
        trans->des[0] = des;
        return trans;
    }
    for (count = 0; count < TRANS_ENTRY_PER_RECORD; ++count) {
        if (trans->des[count] == des)
            return trans;
        else if (!trans->des[count]) {
            trans->des[count] = des;
            return trans;
        }
    }
    trans->next = trans_add_des(trans->next, des);
    return trans;
}

void trans_add(State* src, char symbol, State* des) {
    src->trans[symbol] = trans_add_des(src->trans[symbol], des);
}

void trans_destroy(Trans* trans) {
    if (!trans)
        return;
    trans_destroy(trans->next);
    free(trans);
}

State* state_init(uint16_t final_status, State* next) {
    State* state        = malloc(sizeof(State));
    state->final_status = final_status;
    state->next         = next;
    memset(state->trans, 0, sizeof(Trans*) * NUM_SYMBOLS);
    return state;
}

void state_append(State* src, State* des) {
    if (src->next)
        state_append(src->next, des);
    else
        src->next = des;
}

void state_clean_select(State* src) {
    if (src) {
        src->select_tmp = false;
        state_clean_select(src->next);
    }
}

void state_copy_select(State* src) {
    if (src) {
        src->select_now = src->select_tmp;
        state_copy_select(src->next);
    }
}

void state_destroy(State* src) {
    uint16_t symbol;
    if (src) {
        for (symbol = 0; symbol < NUM_SYMBOLS; ++symbol)
            trans_destroy(src->trans[symbol]);
        state_destroy(src->next);
        free(src);
    }
}

void print_nfa(State* state) {
    uint16_t symbol;
    uint16_t idx;
    Trans* trans;
    printf("================================\n");
    for (; state; state = state->next) {
        printf("%2d %p:\n", state->final_status, state);
        for (symbol = 0; symbol < NUM_SYMBOLS; ++symbol) {
            if (!state->trans[symbol])
                continue;
            printf("\t%c: ", symbol? symbol: ' ');
            for (trans = state->trans[symbol]; trans; trans = trans->next)
                for (idx = 0; (idx < TRANS_ENTRY_PER_RECORD) && (trans->des[idx]); ++idx)
                    printf("%p ", trans->des[idx]);
            printf("\n");
        }
    }
}

State* regnode_to_nfa(Regnode* root, uint16_t final_status) {
    uint16_t count;
    if (root->val == OP_STAR) {
        State* nfa         = regnode_to_nfa(root->left, final_status);
        State* final_state = state_init(final_status, nfa);
        State* init_state  = state_init(0, final_state);
        State* state;
        trans_add(init_state, EPS, nfa);
        trans_add(init_state, EPS, final_state);
        for (state = nfa; state; state = state->next)
            if (state->final_status) {
                state->final_status = 0;
                trans_add(state, EPS, nfa);
                trans_add(state, EPS, final_state);
            }
        return init_state;
    }
    else if (root->val == OP_UNI) {
        State* nfa1        = regnode_to_nfa(root->left, final_status);
        State* nfa2        = regnode_to_nfa(root->right, final_status);
        State* init_state  = state_init(0, nfa1);
        state_append(init_state, nfa2);
        trans_add(init_state, EPS, nfa1);
        trans_add(init_state, EPS, nfa2);
        return init_state;
    }
    else if (root->val == OP_CON) {
        State* nfa1  = regnode_to_nfa(root->left, final_status);
        State* nfa2  = regnode_to_nfa(root->right, final_status);
        State* state;
        for (state = nfa1; state; state = state->next)
            if (state->final_status) {
                state->final_status = 0;
                trans_add(state, EPS, nfa2);
            }
        state_append(nfa1, nfa2);
        return nfa1;
    }
    else if (root->val == OP_EPS) {
        State* init_state = state_init(final_status, NULL);
        return init_state;
    }
    else {
        State* final_state = state_init(final_status, NULL);
        State* init_state  = state_init(0, final_state);
        char symbol;
        if (root->val == OP_DIG)
            for (symbol = '0'; symbol <= '9'; ++symbol)
                trans_add(init_state, symbol, final_state);
        else if (root->val == OP_AZ)
            for (symbol = 'A'; symbol <= 'Z'; ++symbol)
                trans_add(init_state, symbol, final_state);
        else if (root->val == OP_az)
            for (symbol = 'a'; symbol <= 'z'; ++symbol)
                trans_add(init_state, symbol, final_state);
        else if (root->val == OP_Az) {
            for (symbol = 'A'; symbol <= 'Z'; ++symbol)
                trans_add(init_state, symbol, final_state);
            for (symbol = 'a'; symbol <= 'z'; ++symbol)
                trans_add(init_state, symbol, final_state);
        }
        else if (root->val == OP_W) {
            trans_add(init_state, ' ',  final_state);
            trans_add(init_state, '\n', final_state);
            trans_add(init_state, '\t', final_state);
        }
        else
            trans_add(init_state, root->val, final_state);
        return init_state;
    }
}

void state_eps_closure(State* state, uint16_t* select_count, uint16_t* final_status) {
    Trans* trans;
    uint16_t count;

    if (state->select_tmp)
        return;
    state->select_tmp = true;
    ++(*select_count);
    if (state->final_status > (*final_status))
        (*final_status) = state->final_status;

    for (trans = state->trans[EPS]; trans; trans = trans->next)
        for (count = 0; \
                (count < TRANS_ENTRY_PER_RECORD) && (trans->des[count]); ++count)
            if (!((State*)trans->des[count])->select_tmp)
                state_eps_closure(trans->des[count], select_count, final_status);
}

void NFA_eps_closure(State* NFA, uint16_t* select_count, uint16_t* final_status) {
    State* state;
    state_clean_select(NFA);
    *select_count = 0;
    *final_status = 0;
    for (state = NFA; state; state = state->next)
        if (state->select_now)
            state_eps_closure(state, select_count, final_status);
    state_copy_select(NFA);
}

void NFA_move(State* NFA, uint16_t* select_count, uint16_t* final_status, char symbol) {
    State* state;
    Trans* trans;
    uint16_t count;
    state_clean_select(NFA);
    for (state = NFA; state; state = state->next)
        for (trans = state->trans[symbol]; state->select_now && trans; trans = trans->next)
            for (count = 0; \
                    (count < TRANS_ENTRY_PER_RECORD) && (trans->des[count]); ++count)
                ((State*)trans->des[count])->select_tmp = true;
    state_copy_select(NFA);
    NFA_eps_closure(NFA, select_count, final_status);
}

void NFA_init(State* NFA, uint16_t* select_count, uint16_t* final_status) {
    state_clean_select(NFA);
    *select_count = 0;
    *final_status = 0;
    state_eps_closure(NFA, select_count, final_status);
    state_copy_select(NFA);
}

void NFA_run(State* NFA, char* str) {
    uint16_t str_pt;
    uint16_t select_count;
    uint16_t final_status;
    State* state;
    NFA_init(NFA, &select_count, &final_status);
    printf("================================\n\t");
    for (state = NFA; state; state = state->next)
        if (state->select_now)
            printf("%p ", state);
    printf("\n");
    for (str_pt = 0; str_pt < strlen(str); ++str_pt) {
        NFA_move(NFA, &select_count, &final_status, str[str_pt]);  
        printf("%c, #s = %3d, f = %3d\n\t", str[str_pt], select_count, final_status);
        for (state = NFA; state; state = state->next)
            if (state->select_now)
                printf("%p ", state);
        printf("\n");
    }
    if (final_status)
        printf("ACCEPT by status %d\n", final_status);
    else
        printf("REJECT\n");
}
// =============================================================================

// =============================================================================
// Symbol Table
#define SYMBOL_NUM 1009

typedef struct {
    uint16_t class;
    char*    content;
} Symbol;

typedef struct {
    Symbol symbol[SYMBOL_NUM];
    uint16_t count;
} Symbol_Table;

Symbol_Table* symbol_table_init() {
    Symbol_Table* table = malloc(sizeof(Symbol_Table));
    memset(table->symbol, 0, SYMBOL_NUM * sizeof(Symbol));
    return table;
}

uint16_t BKDR_hash(char* str) {
    uint16_t seed = 131;
    uint16_t hash = 0;
    for (; *str; ++str)
        hash = (hash * seed + *str) % SYMBOL_NUM;
    return hash;
}

Symbol* push_symbol(Symbol_Table* table, uint16_t class, char* content) {
    uint16_t hash = BKDR_hash(content);

    if (table->count >= SYMBOL_NUM) {
        printf("TOO MANY SYMBOL\n");
        exit(-1);
    }
    for (;; hash = (hash + 1) % SYMBOL_NUM)
        if (!table->symbol[hash].content) {
            table->symbol[hash].content = malloc(sizeof(char) * (strlen(content) + 1));
            table->symbol[hash].class   = class;
            strcpy(table->symbol[hash].content, content);
            return table->symbol + hash;
        }
        else if (table->symbol[hash].class == class && \
                !strcmp(table->symbol[hash].content, content))
            return table->symbol + hash;
}

// =============================================================================
// Lexical
#define MAX_KEYWORD_LEN 10

#define CLASS_WHITE      1
#define CLASS_IDENTIFIER 2
#define CLASS_NUMBER     3
#define CLASS_KEYWORD    4
#define CLASS_OPERATOR   5
#define CLASS_PUNCTUATOR 6

union Token_content {
    char     c[MAX_KEYWORD_LEN];
    int64_t  d;
    Symbol*  s; 
};

typedef struct {
    uint16_t            class;
    union Token_content content;
} Token;

typedef struct {
    State*   nfa; 
    FILE*    stream;
    uint16_t final_status;
    Token*   (*post_process)(uint16_t, char*, Symbol_Table*);
} Lex;

Lex* lex_init(char* input_filename, Token* (*post_process)(uint16_t, char*, Symbol_Table*)) {
    Lex* lex          = malloc(sizeof(Lex));
    lex->nfa          = state_init(0, NULL);
    lex->stream       = fopen(input_filename, "r");
    lex->final_status = 0;
    lex->post_process = post_process;

    return lex;
}

void lex_destroy(Lex* lex) {
    fclose(lex->stream);
    state_destroy(lex->nfa);
    free(lex);
}

void lex_append_rule(Lex* lex, char* rule, uint16_t final_status) {
    Regnode* root;
    State*   nfa;
    root = regexp_to_regnode(rule);
    nfa  = regnode_to_nfa(root, final_status);
    state_append(lex->nfa, nfa);
    trans_add(lex->nfa, EPS, nfa);
    /*print_nfa(nfa);*/
    /*regnode_print(root);*/
    /*printf("\n");*/
    regnode_destroy(root);
}

Token* lex_get_token(Lex* lex, Symbol_Table* table) {
    char symbol;
    char content[MAX_TOKEN_LEN];
    uint16_t select_count;
    uint16_t final_status;
    uint16_t pre_final_status;
    uint64_t content_pos_s = ftell(lex->stream);
    uint64_t content_pos_e;
    Token* token;

    NFA_init(lex->nfa, &select_count, &final_status);
    lex->final_status = 0;

    while (fread(&symbol, sizeof(char), 1, lex->stream)) {
        NFA_move(lex->nfa, &select_count, &final_status, symbol);  
        if (!select_count) {
            if (lex->final_status) {
                content_pos_e = ftell(lex->stream) - 1;
                fseek(lex->stream, content_pos_s, SEEK_SET);
                fread(content, sizeof(char), content_pos_e - content_pos_s, lex->stream); 
                content[content_pos_e - content_pos_s] = 0;
                /*printf("%d %s\n", lex->final_status, content);*/
            }
            else
                printf("Lexical Error\n");
            token = lex->post_process(lex->final_status, content, table);
            if (token)
                return token;
            else
                return lex_get_token(lex, table);

        }
        else
            lex->final_status = final_status;
    }
    return NULL;
}
// =============================================================================

Token* lex_post_process(uint16_t final_status, char* content, Symbol_Table* table) {
    if (final_status == CLASS_WHITE)
        return NULL;

    Token* token = malloc(sizeof(Token));
    token->class = final_status;
    switch (final_status) {
        case CLASS_IDENTIFIER:
            token->content.s = push_symbol(table, final_status, content);
            return token;
        case CLASS_NUMBER:
            token->content.d = atoi(content);
            return token;
        case CLASS_KEYWORD:
        case CLASS_OPERATOR:
        case CLASS_PUNCTUATOR:
            strcpy(token->content.c, content);
            return token;
        default:
            return NULL;
    }
}

int main() {
    /*Regnode* S = regexp_to_regnode("(a|b)*abb#");*/
    /*regnode_print(S);*/
    /*printf("\n");*/

    /*State* nfa = regnode_to_nfa(S, 1);*/
    /*print_nfa(nfa);*/

    /*NFA_run(&nfa, "aabb#abb#");*/
    /*NFA_run(&nfa, "abb#");*/
    /*NFA_run(&nfa, "");*/
    /*NFA_run(&nfa, "ascas");*/
    /*state_destroy(nfa);*/
    /*regnode_destroy(S);*/

    Symbol_Table* table = symbol_table_init();
    Lex* lex = lex_init("test.c", lex_post_process);
    lex_append_rule(lex, "\\w+"               , 1); // White Space
    lex_append_rule(lex, "(_|\\z)(_|\\z|\\d)*", 2); // Identifier
    lex_append_rule(lex, "\\d+"               , 3); // Number
    lex_append_rule(lex, "auto|else|long|switch|         \
                          break|enum|register|typedef|   \
                          case|extern|restrict|union|    \
                          char|float|return|unsigned|    \
                          const|for|short|void|          \
                          continue|goto|signed|volatile| \
                          default|if|sizeof|while|       \
                          do|inline|static|              \
                          double|int|struct"  , 4); // Key Word
    lex_append_rule(lex, "[|]|->|.| \
                          \\+|-|\\*|/|&|~|\\+\\+|--|    \
                          >>|<<|>|<|>=|<=|==|!=|        \
                          &|\\||^|&&|\\|\\||!|\\?|:|    \
                          =|\\+=|-=|\\*=|/=|%=|>>=|<<=| \
                          &=|\\|=|^=|,"       , 5); // Operator
    lex_append_rule(lex, ",|;|\\(|\\)|{|}"    , 6); // Punctuators

    for (uint16_t count = 0; count < 10; ++count) {
        Token* token = lex_get_token(lex, table);
        if (!token)
            break;
        switch (token->class) {
            case CLASS_WHITE:
                printf("HAIYAA\n");
                break;
            case CLASS_IDENTIFIER:
                printf("ID  %s\n", token->content.s->content);
                break;
            case CLASS_NUMBER:
                printf("NUM %ld\n", token->content.d);
                break;
            case CLASS_KEYWORD:
                printf("KEY %s\n", token->content.c);
                break;
            case CLASS_OPERATOR:
                printf("OPE %s\n", token->content.c);
                break;
            case CLASS_PUNCTUATOR:
                printf("PUN %s\n", token->content.c);
                break;
        }
    }

    lex_destroy(lex);

    return 0;
}

