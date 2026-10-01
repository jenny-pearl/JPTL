#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SLIT(x) ((String){.data = x, .count = strlen(x)})

#define da_append(da, item)\
	do {\
		if ((da)->count >= (da)->capacity) {\
			if ((da)->capacity == 0) (da)->capacity = 16;\
			else (da)->capacity *= 2;\
			(da)->items = realloc((da)->items, (da)->capacity * sizeof((item)));\
		}\
		(da)->items[(da)->count] = item;\
		(da)->count += 1;\
	} while (0)


typedef struct {
	int64_t count;
	char *data;
} String;

typedef struct {
	char input;
	char output;
	int move;
	int64_t next_state_index;
} Inst;

typedef struct {
	Inst *items;
	int64_t count;
	int64_t capacity;
} Insts;

typedef struct {
	String name;
	Insts insts;
} State;

typedef struct {
	State *items;
	int64_t count;
	int64_t capacity;
} States;

typedef char InputChar;
typedef struct {
	InputChar *items;
	int64_t count;
	int64_t capacity;
} InputChars;

typedef char TapeChar;
typedef struct {
	TapeChar *items;
	int64_t count;
	int64_t capacity;
} TapeChars;

typedef struct {
	States states;
	InputChars input_alphabet;
	TapeChars tape_alphabet;

	String tape;
} JPTL_Context;

int64_t string_equal(String one, String two)
{
	if (one.count != two.count) return 0;

	for (int64_t i = 0; i < one.count; i += 1) {
		if (one.data[i] != two.data[i]) return 0;
	}

	return 1;
}

int64_t string_n_equal(String one, String two, int64_t n)
{
	if (one.count < n || two.count < n) {
		return 0;
	}

	for (int64_t i = 0; i < n; i += 1) {
		if (one.data[i] != two.data[i]) {
			return 0;
		}
	}

	return 1;
}

void trim_left(String *string)
{
	while (string->count > 0 && isspace(*string->data)) {
		string->data += 1;
		string->count -= 1;
	}
}

String tokenize_by_delimiter(String *source, char delimiter)
{
	while (source->count > 0 && *source->data == delimiter) {
		source->data += 1;
		source->count -= 1;
	}

	String token = {0};
	token.data = source->data;

	while (source->count > 0 && *source->data != delimiter) {
		source->data += 1;
		source->count -= 1;
	}

	token.count = source->data - token.data;

	return token;
}

String tokenize_by_space(String *source)
{
	String token = {0};

	while (source->count > 0 && isspace(*source->data)) {
		source->data += 1;
		source->count -= 1;
	}

	token.data = source->data;

	while (source->count > 0 && !isspace(*source->data)) {
		source->data += 1;
		source->count -= 1;
	}

	token.count = source->data - token.data;

	return token;
}

void fetch_states(String *source, JPTL_Context *ctx)
{
	trim_left(source);

	int64_t count = strlen("STATES");
	if (!string_n_equal(*source, SLIT("STATES"), count)) {
		fprintf(stderr, "The file should start with `STATES` and then the possible states. You should look at the examples\n");
		exit(1);
	}

	source->data += count;
	source->count -= count;

	trim_left(source);

	if (source->count == 0) {
		fprintf(stderr, "Unexpected end of file\n");
	}

	if (*source->data != '[') {
		fprintf(stderr, "The states and alphabets are declared in a generic array syntax but delimited by spaces.\n"
				"\tFor example \"[ s0 s1 s2 ]\"\n");
		exit(1);
	}

	source->data += 1;
	source->count -= 1;

	trim_left(source);

	while (*source->data != ']') {
		if (source->count == 0) {
			fprintf(stderr, "Unexpected end of file\n");
			exit(1);
		}

		if (*source->data != 's') {
			// TODO(jenny): later do error reporting by line and have a caret cursor pointing to the error
			fprintf(stderr, "State names must start with an s\n");
			exit(1);
		}

		da_append(&ctx->states, (State){0});
		ctx->states.items[ctx->states.count - 1].name = tokenize_by_space(source);

		if (source->count == 0) {
			fprintf(stderr, "End of file before terminating close bracket `]`\n");
			exit(1);
		}

		trim_left(source);
	}

	source->count -= 1;
	source->data += 1;
}

void fetch_input_chars(String *source, JPTL_Context *ctx)
{
	trim_left(source);

	int64_t count = strlen("INPUTS");
	if (!string_n_equal(*source, SLIT("INPUTS"), count)) {
		fprintf(stderr, "The input tape character definitions must start with the symbol `INPUTS`. Please consult the examples.\n");
		exit(1);
	}

	source->data += count;
	source->count -= count;

	trim_left(source);

	if (source->count == 0) {
		fprintf(stderr, "Unexpected end of file\n");
		exit(1);
	}

	if (*source->data != '[') {
		fprintf(stderr, "The states and alphabets are declared in a generic array syntax but delimited by spaces.\n"
				"\tFor example \"[ 0 1 # ]\"\n");
		exit(1);
	}

	source->data += 1;
	source->count -= 1;

	trim_left(source);

	while (*source->data != ']') {
		if (source->count == 0) {
			fprintf(stderr, "Unexpected end of file\n");
			exit(1);
		}

		String character = tokenize_by_space(source);
		if (character.count != 1) {
			fprintf(stderr, "Only one character literals are allowed. Please change the input character.\n");
			exit(1);
		}

		da_append(&ctx->input_alphabet, *character.data);

		if (source->count == 0) {
			fprintf(stderr, "End of file before terminating close bracket `]`\n");
			exit(1);
		}

		trim_left(source);
	}

	source->count -= 1;
	source->data += 1;
}

void fetch_tape_chars(String *source, JPTL_Context *ctx)
{
	trim_left(source);

	int64_t count = strlen("TAPE");
	if (!string_n_equal(*source, SLIT("TAPE"), count)) {
		fprintf(stderr, "The tape alphabet character definitions must start with the symbol `TAPE`. Please consult the examples.\n");
		exit(1);
	}

	source->data += count;
	source->count -= count;

	trim_left(source);

	if (source->count == 0) {
		fprintf(stderr, "Unexpected end of file\n");
		exit(1);
	}

	if (*source->data != '[') {
		fprintf(stderr, "The states and alphabets are declared in a generic array syntax but delimited by spaces.\n"
				"\tFor example \"[ 0 1 # ]\"\n");
		exit(1);
	}

	source->data += 1;
	source->count -= 1;

	trim_left(source);

	while (*source->data != ']') {
		if (source->count == 0) {
			fprintf(stderr, "Unexpected end of file\n");
			exit(1);
		}

		String character = tokenize_by_space(source);
		if (character.count != 1) {
			fprintf(stderr, "Only one character literals are allowed. Please change the input character.\n");
			exit(1);
		}

		da_append(&ctx->tape_alphabet, *character.data);

		if (source->count == 0) {
			fprintf(stderr, "End of file before terminating close bracket `]`\n");
			exit(1);
		}

		trim_left(source);
	}

	source->count -= 1;
	source->data += 1;

}

JPTL_Context fetch_constants(String *source)
{
	JPTL_Context ctx = {0};

	fetch_states(source, &ctx);
	fetch_input_chars(source, &ctx);
	fetch_tape_chars(source, &ctx);

	return ctx;
}

void fetch_insts(String *source, JPTL_Context *ctx)
{
	trim_left(source);

	while (source->count > 0) {
		String line = tokenize_by_delimiter(source, '\n');
		trim_left(&line);

		int64_t count = strlen("inst");
		if (string_n_equal(line, SLIT("inst"), count)) {
			fprintf(stderr, "Every instruction line must start with `inst`\n");
			exit(1);
		}

		line.count -= count;
		line.data += count;

		if (line.count <= 0) {
			fprintf(stderr, "Unexpected line end\n");
			exit(1);
		}

		Token curr_state = tokenize_by_space(&line);

		int64_t current_state_index = -1;
		for (int64_t i = 0; i < ctx->states.count; i += 1) {
			if (string_equal(ctx->states.items[i].name, curr_state)) {
				current_state_index = i;
				break;
			}
		}

		if (index == -1) {
			fprintf(stderr, "Undeclared state `%.*s`. Add it to the STATES array.\n",
					(int)curr_state.count, curr_state.data);
			exit(1);
		}

/*
TODO(jenny): later please add support for
multiple characters being valid for an instruction

like instead of writing

inst s0 0 -> ...
inst s0 1 -> ...
inst s0 2 -> ...
inst s0 3 -> ...

write

inst s0 [ 0 1 2 3 ] -> ...

this would be way better

"inst s0 0 1 2 3" could be good too but I would like to
separate the state and the character more than with just spaces

I could unify the single and multiple declaration with like this syntax

inst s0 [ 0 ] -> ...

But I would like to prioritise ease of use

Also you should be able to do

inst s0 ANY -> ...

or

inst s0 any -> ...
*/

		if (line.count <= 0) {
			fprintf(stderr, "Unexpected line end\n");
			exit(1);
		}

		String read_character = tokenize_by_space(&line);

		if (read_character.count != 1) {
			fprintf(stderr, "The read character `%.*s` can only be one single symbol.\n",
					(int)read_character.count, read_character.data);
			exit(1);
		}

		int64_t read_character_index = -1;
		for (int64_t i = 0; i < ctx->tape_alphabet.count; i += 1) {
			if (ctx->tape_alphabet.items[i] == read_character.data[0]) {
				read_character_index = i;
				break;
			}
		}

		if (read_character_index == -1) {
			fprintf(stderr, "Read character `%.*s` not declared in the TAPE array.\n",
					(int)read_character.count, read_character.data);
			exit(1);
		}

		if (!string_equal(SLIT("->"), tokenize_by_space(&line))) {
			fprintf(stderr, "The transition function must have an arrow `->` operator to indicate the change.\n");
			exit(1);
		}

/*

Add support for writing no characters as

inst s0 1 -> NONE s0 .

*/

		if (line.count <= 0) {
			fprintf("Unexpected end of line\n");
			exit(1);
		}

		String input_character = tokenize_by_space(&line);

		if (input_character.count != 1) {
			fprintf("Input can only be one character long.\n");
			exit(1);
		}
	}
}

int main(void)
{
	FILE *fp = fopen("source.jptl", "rb");

	fseek(fp, 0, SEEK_END);
	int64_t count = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	String source = {0};

	source.count = count;
	source.data = malloc(sizeof(char) * count);

	fread(source.data, sizeof(char), source.count, fp);

	fclose(fp);

	while (source.count > 0) {
		JPTL_Context ctx = fetch_constants(&source);

		fetch_insts(&source, &ctx);
	}

	return 0;
}
