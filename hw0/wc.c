#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>


int main(int argc, char *argv[]) {

	int wc = 0;
    
    if (argc == 1) { // No file arg passed to wc (use stdin)

    	char ch;
    	bool iter_word = false;
		while (read(STDIN_FILENO, &ch, 1) > 0) {
			if (isspace(ch)) { // Current character is whitespace
				if (iter_word) { // Currently iterating through word
					wc++;
				}
				iter_word = false;
			} else { // Current character is not whitespace
				iter_word = true;
			}
		}

		if (iter_word) {
			wc++;
		}

    } else if (argc == 2) { // Filename arg passed to wc 

    	FILE *file;
		file = fopen(argv[1], "r");

		char ch;
    	bool iter_word = false;
		while((ch = getc(file)) != EOF) {
      		if (isspace(ch)) { // Current character is whitespace
				if (iter_word) { // Currently iterating through word
					wc++;
				}
				iter_word = false;
			} else { // Current character is not whitespace
				iter_word = true;
			}
    	}

    	if (iter_word) {
			wc++;
		}

    } else { // More than 1 arg passed to wc (error)
    	printf("%s", "wc: More than one argument passed to program.");
    	return 1;
    }

    printf("%d", wc);
    return 0;
}
