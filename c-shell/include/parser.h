#ifndef PARSER_H
#define PARSER_H

// Enum for all the seven defined token types: |, &, ;, <, >, >>, word.
typedef enum {
    OP_PIPE,
    OP_AMP,
    OP_SEMI,
    OP_LT,
    OP_GT,
    OP_GTGT,
    WORD
} TokenType;

// Token struct that stores the token type, the token value and a link to the next following token as a Linked List.
typedef struct Token {
    TokenType type;
    char *value;
    struct Token *next;
} Token;

Token* tokenise_input(char *input);

int validate_grammar(Token *head);

void free_tokens(Token *head);

#endif