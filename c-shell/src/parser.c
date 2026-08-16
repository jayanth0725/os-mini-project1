#include "../include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct{
    char *data;
    int length;
    int capacity;
} StringBuffer;

void init_buffer(StringBuffer *sb){
    sb->capacity = 64;
    sb->data = malloc(sb->capacity);
    if(!sb->data){
        perror("Failed to allocate string buffer");
        exit(1);
    }
    sb->length = 0;
    sb->data[0] = '\0';
}

void append_char(StringBuffer *sb, char c){
    if(sb->length + 1 >= sb->capacity){
        sb->capacity *= 2;
        sb->data = realloc(sb->data, sb->capacity);
        if(!sb->data){
            perror("Failed to reallocate string buffer");
            exit(1);
        }
    }
    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
}

Token* create_token(TokenType type, const char *value){
    Token *new_token = malloc(sizeof(Token));
    if(!new_token){
        perror("Failed to allocate token");
        exit(1);
    }

    new_token->type = type;
    new_token->value = value ? strdup(value) : NULL;
    new_token->next = NULL;
    return new_token;
}

void append_token(Token **head, Token **tail, Token *new_token){
    if(*head == NULL){
        *head = new_token;
        *tail = new_token;
    }
    else{
        (*tail)->next = new_token;
        *tail = new_token;
    }
}

void free_tokens(Token *head){
    Token* current = head;
    while(current != NULL){
        Token *next = current->next;
        if(current->value != NULL){
            free(current->value);
        }
        free(current);
        current = next;
    }
}

Token* tokenise_input(char *input){
    Token *head = NULL;
    Token *tail = NULL;

    int i = 0;
    int len = strlen(input);

    while(i < len){
        char c = input[i];

        if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
            i++;
            continue;
        }

        switch(c) {
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
                StringBuffer sb;
                init_buffer(&sb);

                bool in_sq = false;
                bool in_dq = false;
                bool is_escaped = false;

                while(i < len){
                    char tc = input[i];

                    if(!in_sq && !in_dq && !is_escaped){
                        if(tc == ' ' || tc == '\t' || tc == '\n' || tc == '\r' || tc == '|' || tc == '&' || tc == ';' || tc == '<' || tc == '>'){
                            break;
                        }
                    }

                    if(is_escaped){
                        if(in_dq){
                            if(tc == '"' || tc == '\\'){
                                append_char(&sb, tc);
                            }
                            else{
                                append_char(&sb, '\\');
                                append_char(&sb, tc);
                            }
                        }
                        else{
                            append_char(&sb, tc);
                        }
                        is_escaped = false;
                    }
                    else if(tc == '\\' && !in_sq){
                        is_escaped = true;    
                    }
                    else if(tc == '\'' && !in_dq){
                        in_sq = !in_sq;
                    }
                    else if(tc == '"' && !in_sq){
                        in_dq = !in_dq;
                    }
                    else{
                        append_char(&sb, tc);
                    }

                    i++;

                }

                if(in_sq || in_dq || is_escaped){
                    printf("cshell: invalid syntax\n");
                    free(sb.data);
                    free_tokens(head);
                    return NULL;
                }

                if(sb.length > 0){
                    append_token(&head, &tail, create_token(WORD, sb.data));
                }

                free(sb.data);
                break;
        }
    }

    return head;
}

int validate_grammar(Token *head){
    if(head == NULL){
        return 1;
    }

    enum{
        EXPECT_WORD,
        EXPECT_ANY,
        EXPECT_WORD_OR_EOF
    } state = EXPECT_WORD;

    Token *current = head;
    while(current != NULL){
        switch(current->type){
            case WORD:
                state = EXPECT_ANY;
                break;

            case OP_PIPE:
            case OP_SEMI:
            case OP_LT:
            case OP_GT:
            case OP_GTGT:
                if(state != EXPECT_ANY){
                    printf("cshell: invalid syntax\n");
                    return 0;
                }
                state = EXPECT_WORD;
                break;

            case OP_AMP:
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

    if(state == EXPECT_WORD){
        printf("cshell: invalid syntax\n");
        return 0;
    }

    return 1;
}