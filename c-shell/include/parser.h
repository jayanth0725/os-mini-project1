#ifndef PARSER_H
#define PARSER_H

typedef enum {
    OP_PIPE,
    OP_AMP,
    OP_SEMI,
    OP_LT,
    OP_GT,
    OP_GTGT,
    WORD
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
    struct Token *next;
} Token;

Token* tokenise_input(char *input);

int validate_grammar(Token *head);

void free_tokens(Token *head);

#endif