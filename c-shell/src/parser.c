#include "../include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// StringBuffer struct for tokenising a word in the input.
typedef struct{
    char *data;
    int length;
    int capacity;
} StringBuffer;

// Helper function that initialises the StringBuffer struct.
static void init_buffer(StringBuffer *sb){
    // Default size of the buffer is 64 bytes, if the malloc fails calls perror() and stops the shell.
    sb->capacity = 64;
    sb->data = malloc(sb->capacity);
    if(!sb->data){
        perror("Failed to allocate string buffer");
        exit(1);
    }
    sb->length = 0;
    sb->data[0] = '\0';
}

// Helper function that appends a character to the StringBuffer struct.
static void append_char(StringBuffer *sb, char c){
    // Dynamically allocates more memory to the sb->data string if the default capacity is exceeded.
    if(sb->length + 1 >= sb->capacity){
        sb->capacity *= 2;
        sb->data = realloc(sb->data, sb->capacity);
        if(!sb->data){
            perror("Failed to reallocate string buffer");
            exit(1);
        }
    }

    // The character is appended, followed by the null character.
    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
}

// Helper function that creates a Token struct and returns a pointer to it.
static Token* create_token(TokenType type, const char *value){
    // Allocates memory for the Token, with exit fallback for malloc failure.
    Token *new_token = malloc(sizeof(Token));
    if(!new_token){
        perror("Failed to allocate token");
        exit(1);
    }

    // Allocates values to the components of the struct and returns the pointer to it.
    new_token->type = type;
    new_token->value = value ? strdup(value) : NULL;
    new_token->next = NULL;
    return new_token;
}

// Helper function that appends a Token to the linked list of Tokens.
static void append_token(Token **head, Token **tail, Token *new_token){
    // If the list is empty, assigns the head and tail of the list to point to the Token. 
    // Otherwise, updates the pointer of the previous Token to point to the newly appended one and sets the tail to point to it.
    if(*head == NULL){
        *head = new_token;
        *tail = new_token;
    }
    else{
        (*tail)->next = new_token;
        *tail = new_token;
    }
}

// Function that frees the linked list of tokens to avoid memory leaks.
void free_tokens(Token *head){
    //Iterates through the list from the head till it reaches the end.
    Token* current = head;
    while(current != NULL){
        // Frees the value of the current token and the token struct itself.
        // Also updates the current pointer to point to the next token.
        Token *next = current->next;
        if(current->value != NULL){
            free(current->value);
        }
        free(current);
        current = next;
    }
}

// Splits the input buffer into a linked list of tokens.
Token* tokenise_input(char *input){
    Token *head = NULL;
    Token *tail = NULL;

    int i = 0;
    int len = strlen(input);

    // Iterates through the entire input buffer.
    while(i < len){
        char c = input[i];

        // These four characters are considered to be whitespace, skip them.
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
            i++;
            continue;
        }

        // Otherwise, check if it is an operator or a character.
        switch(c) {
            // If it is an operator, create a token consisting of it and append to the linked list.
            case '|':
                append_token(&head, &tail, create_token(OP_PIPE, NULL));
                i++;
                break;
            case '&':
                append_token(&head, &tail, create_token(OP_AMP, NULL));
                i++;
                break;
            case ';':
                append_token(&head, &tail, create_token(OP_SEMI, NULL));
                i++;
                break;
            case '<':
                append_token(&head, &tail, create_token(OP_LT, NULL));
                i++;
                break;
            case '>':
                // Maximal munch is applied: the >> operator is given priority over the > operator.
                if(i + 1 < len && input[i + 1] == '>'){
                    append_token(&head, &tail, create_token(OP_GTGT, NULL));
                    i += 2;
                }
                else{
                    append_token(&head, &tail, create_token(OP_GT, NULL));
                    i++;
                }
                break;
            default:
                // The character is a part of a word, initiate a string buffer for it.
                StringBuffer sb;
                init_buffer(&sb);

                // Boolean flags to track whether single quotes, double quotes or an escaped literal has been encountered.
                bool in_sq = false;
                bool in_dq = false;
                bool is_escaped = false;

                // Iterates through the consecutive characters to build a single word token.
                while(i < len){
                    char tc = input[i];

                    // If not inside any quotes or escape sequence, a whitespace or operator signifies the end of the current word.
                    if(!in_sq && !in_dq && !is_escaped){
                        if(tc == ' ' || tc == '\t' || tc == '\n' || tc == '\r' || tc == '|' || tc == '&' || tc == ';' || tc == '<' || tc == '>'){
                            break;
                        }
                    }

                    // If the previous character was a backslash, process the current character based on quoting rules.
                    if(is_escaped){
                        // If inside double quotes, apply double-quote specific escaping rules.
                        if(in_dq){
                            // Only double quotes and backslashes lose their escape character inside double quotes.
                            if(tc == '"' || tc == '\\'){
                                append_char(&sb, tc);
                            }
                            // Any other escaped character retains the backslash literally.
                            else{
                                append_char(&sb, '\\');
                                append_char(&sb, tc);
                            }
                        }
                        // If outside double quotes, the backslash is consumed and the character is appended literally.
                        else{
                            append_char(&sb, tc);
                        }
                        is_escaped = false;
                    }
                    // If a backslash is encountered outside of single quotes, flag the next character to be escaped.
                    else if(tc == '\\' && !in_sq){
                        is_escaped = true;    
                    }
                    // If a single quote is encountered outside of double quotes, toggle the single quote flag.
                    else if(tc == '\'' && !in_dq){
                        in_sq = !in_sq;
                    }
                    // If a double quote is encountered outside of single quotes, toggle the double quote flag.
                    else if(tc == '"' && !in_sq){
                        in_dq = !in_dq;
                    }
                    // If it is a regular character, append it to the string buffer.
                    else{
                        append_char(&sb, tc);
                    }

                    i++;

                }

                // If the boolean flags are still true, either a quote is still open or there is a trailing backslash.
                // The grammar is violated, print invalid syntax to stdout and free the input buffer and linked list.
                if(in_sq || in_dq || is_escaped){
                    printf("cshell: invalid syntax\n");
                    free(sb.data);
                    free_tokens(head);
                    return NULL;
                }

                // The input buffer is proper, create a token for the word and append it to the linked list.
                if(sb.length > 0){
                    append_token(&head, &tail, create_token(WORD, sb.data));
                }

                // Free the input buffer and continue tokenising the input.
                free(sb.data);
                break;
        }
    }

    // Return the head of the newly created linked list of tokens.
    return head;
}

// Parser function that checks if the linked list of tokens is syntactically correct by passing it through a DFA that recognises only the language generated by the grammar.
int validate_grammar(Token *head){
    // If the list is empty, return with failure value.
    if(head == NULL){
        return 1;
    }

    // State enum for the DFA with three states: EXPECT_WORD, EXPECT_ANY, EXPECT_WORD_OR_EOF.
    enum{
        EXPECT_WORD,
        EXPECT_ANY,
        EXPECT_WORD_OR_EOF
    } state = EXPECT_WORD;

    // Iterates through the linked list, transitioning between states based on the current token's type.
    Token *current = head;
    while(current != NULL){
        switch(current->type){
            // A WORD token fulfills the expectation, allowing operators or the end of input to follow.
            case WORD:
                state = EXPECT_ANY;
                break;

            // These operators require a WORD to precede them and a WORD to immediately follow them.
            case OP_PIPE:
            case OP_SEMI:
            case OP_LT:
            case OP_GT:
            case OP_GTGT:
                // This means that the operator was placed either at the end or an operator has already appeared right before it.
                if(state != EXPECT_ANY){
                    printf("cshell: invalid syntax\n");
                    return 0;
                }
                // The next token should be a word otherwise the grammar is violated.
                state = EXPECT_WORD;
                break;

            // The ampersand operator signals a background process.
            case OP_AMP:
                // If a WORD hasn't appeared right before the ampersand, it's a syntax error.
                if(state != EXPECT_ANY) {
                    printf("cshell: invalid syntax\n");
                    return 0;
                }
                state = EXPECT_WORD_OR_EOF;
                break;

            default:
                break;
        }

        current = current->next;
    }

    // If the state is still expecting a word, then there is a grammar violation, return 0 to indicate invalid grammar.
    if(state == EXPECT_WORD){
        printf("cshell: invalid syntax\n");
        return 0;
    }

    // Input is grammatically correct.
    return 1;
}