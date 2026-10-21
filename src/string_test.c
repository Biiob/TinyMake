#include <stdio.h>

#include "tinymake_string.h"

int main()
{
    printf("\n********** string_create **********\n");
    string_t str = string_create();
    printf("Capacity of str: %ld\n", str.capacity);
    printf("Length of str: %ld\n", str.len);
    printf("Content of str: %s\n", string_get(&str));
    string_free(&str);

    printf("\n********** string_create_with_capacity **********\n");
    str = string_create_with_capacity(1024*1024);
    printf("Capacity of str: %ld\n", str.capacity);
    printf("Length of str: %ld\n", str.len);
    printf("Content of str: %s\n", string_get(&str));
    string_free(&str);

    printf("\n********** string_free **********\n");
    str = string_create_with_capacity(1024*1024);
    printf("Capacity of str: %ld\n", str.capacity);
    string_free(&str);
    printf("Capacity of str after free: %ld\n", str.capacity);

    printf("\n********** string_copy_from_char_arr **********\n");
    str = string_copy_from_char_arr("Hello world!");
    printf("Capacity of str: %ld\n", str.capacity);
    printf("Length of str: %ld\n", str.len);
    printf("Content of str: %s\n", string_get(&str));

    printf("\n********** string_copy_from_string **********\n");
    string_t str2 = string_copy_from_string(&str);
    printf("Capacity of str: %ld\n", str2.capacity);
    printf("Length of str: %ld\n", str2.len);
    printf("Content of str: %s\n", string_get(&str2));
    string_free(&str);
    string_free(&str2);

    printf("\n********** string_append_from_string **********\n");
    str = string_copy_from_char_arr("Hello");
    str2 = string_copy_from_char_arr(" world!");
    string_append_from_string(&str2, &str);
    printf("Concatenated content: %s\n", string_get(&str));
    string_free(&str);
    string_free(&str2);

    printf("\n********** string_append_from_string **********\n");
    str = string_copy_from_char_arr("Hello");
    string_append_from_char_arr(" world!", &str);
    printf("Capacity of str: %ld\n", str.capacity);
    printf("Length of str: %ld\n", str.len);
    printf("Concatenated content: %s\n", string_get(&str));
    string_free(&str);

    printf("\n********** string_append_from_string **********\n");
    str = string_copy_from_char_arr("Hello");
    string_append_from_char_arr(" world!", &str);
    printf("Capacity of str: %ld\n", str.capacity);
    printf("Length of str: %ld\n", str.len);
    printf("Concatenated content: %s\n", string_get(&str));
    string_free(&str);

    printf("\n********** string_format **********\n");
    str = string_create();
    char user[] = "Fabien";
    string_format(&str, "Hello %s", user);
    printf("Capacity of str: %ld\n", str.capacity);
    printf("Length of str: %ld\n", str.len);
    printf("Content: %s\n", string_get(&str));
    string_free(&str);

    return 1;
}
