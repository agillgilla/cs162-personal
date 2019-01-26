#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>


int main(int argc, char *argv[]) {

	int lines = 0;

	int words = 0;

	int chars = 0;
    
    if (argc == 1) { // No file arg passed to wc (use stdin)

    	char ch;
    	bool iter_word = false;
		while (read(STDIN_FILENO, &ch, 1) > 0) {
			if (isspace(ch) || ch == '\0') { // Current character is whitespace
				if (iter_word) { // Currently iterating through word
					words++;
				}
				if (ch == '\n') { // Current character is newline
					lines++;
				}
				iter_word = false;
			} else { // Current character is not whitespace
				iter_word = true;
			}
			chars++;
		}

		if (iter_word) {
			words++;
		}

    } else if (argc == 2) { // Filename arg passed to wc 

    	FILE *file;
		file = fopen(argv[1], "r");

		char ch;
    	bool iter_word = false;
		while((ch = getc(file)) != EOF) {
      		if (isspace(ch) || ch == '\0') { // Current character is whitespace
      			printf("%s\n", "found space");
				if (iter_word) { // Currently iterating through word
					words++;
				}
				if (ch == '\n') { // Current character is newline
					lines++;
				}
				iter_word = false;
			} else { // Current character is not whitespace
				printf("%s\n", "found non-space");
				iter_word = true;
			}
			chars++;
    	}

    	if (iter_word) {
			words++;
		}

    } else { // More than 1 arg passed to wc (error)
    	printf("%s", "wc: More than one argument passed to program.");
    	return 1;
    }

    printf("%d\t%d\t%d\n", lines, words, chars);

    return 0;
}
