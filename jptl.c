#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SLIT(x) ((String){.data = x, .count = strlen(x)})

#define da_append_item(da, item)\
	do {\
		if ((da)->count >= (da)->capacity) {\
			if ((da)->capacity == 0) (da)->capacity = 16;\
			else (da)->capacity *= 2;\
			(da)->items = realloc((da)->items, (da)->capacity * sizeof((item)));\
		}\
		(da)->items[(da)->count] = item;\
		(da)->count += 1;\
	} while (0)

#define da_append_da(da, da_)\
	do {\
		if ((da)->count + (da_)->count >= (da)->capacity) {\
			if ((da)->capacity == 0) (da)->capacity = 16 > (da_)->count ? 16 : (da_)->count;\
			else (da)->capacity += (da)->capacity + (da_)->count;\
			(da)->items = realloc((da)->items, (da)->capacity * sizeof((da_)->items[0]));\
		}\
		memcpy((da)->items + (da)->count, (da_)->items, (da_)->items * (da_)->items[0]);\
		(da)->count += (da_)->count;\
	} while (0)


typedef struct {
	int64_t count;
	char *data;
} String;

typedef char InputChar;
typedef struct {
	InputChar *items;
	int64_t count;
	int64_t capacity;
} InputChars;

typedef struct {
	InputChars triggers;
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
} Routine;

typedef struct {
	Routine *items;
	int64_t count;
	int64_t capacity;
} Routines;

typedef char TapeChar;
typedef struct {
	TapeChar *items;
	int64_t count;
	int64_t capacity;
} TapeChars;

typedef struct {
	Routines routines;
	InputChars input_alphabet;
	TapeChars tape_alphabet;

	String tape;
} JPTL_Context;

void report_error(String token, char *error_message)
{
	fprintf(stderr, "[ERROR] :: %s :: erroneous token `%.*s`\n", error_message, (int)token.count, token.data);
	exit(1);
}

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

void trim_right(String *string)
{
	while (string->count > 0 && isspace(string->data[string->count - 1])) {
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
		String error = {.data = source->data, .count = 0};
		report_error(error, "The file should start with `STATES` and then the possible states. You should look at the examples");
	}

	source->data += count;
	source->count -= count;

	trim_left(source);

	if (source->count == 0) {
		String error = {.data = source->data, .count = 0};
		report_error(error, "Unexpected end of file");
	}

	if (*source->data != '[') {
		String error = {.data = source->data, .count = 1};
		report_error(error, "The states and alphabets are declared in a generic array syntax but delimited by spaces."
				"\tFor example \"[ s0 s1 s2 ]\"");
	}

	source->data += 1;
	source->count -= 1;

	trim_left(source);

	while (*source->data != ']') {
		if (source->count == 0) {
			String error = {.data = source->data, .count = 0};
			report_error(error, "Unexpected end of file");
		}

		String state_name = tokenize_by_space(source);

		if (*state_name.data != 's') {
			// TODO(jenny): later do error reporting by line and have a caret cursor pointing to the error
			report_error(state_name, "State names must start with an s");
		}

		da_append_item(&ctx->routines, (Routine){0});
		ctx->routines.items[ctx->routines.count - 1].name = state_name;

		if (source->count == 0) {
			String error = {.data = source->data, .count = 0};
			report_error(error, "End of file before terminating close bracket `]`");
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
		String error = {.data = source->data, .count = source->count > count ? count : source->count};
		report_error(error, "The input tape character definitions must start with the symbol `INPUTS`. Please consult the examples.");
	}

	source->data += count;
	source->count -= count;

	trim_left(source);

	if (source->count == 0) {
		String error = {.data = source->data, .count = 0};
		report_error(error, "Unexpected end of file");
	}

	if (*source->data != '[') {
		String error = {.data = source->data, .count = 1};
		report_error(error, "The states and alphabets are declared in a generic array syntax but delimited by spaces."
				"\tFor example \"[ 0 1 # ]\"");
	}

	source->data += 1;
	source->count -= 1;

	trim_left(source);

	while (*source->data != ']') {
		if (source->count == 0) {
			String error = {.data = source->data, .count = 1};
			report_error(error, "Unexpected end of file");
		}

		String character = tokenize_by_space(source);
		if (character.count != 1) {
			report_error(character, "Only one character literals are allowed. Please change the input character.");
		}

		da_append_item(&ctx->input_alphabet, *character.data);

		if (source->count == 0) {
			String error = {.data = source->data, .count = 0};
			report_error(error, "End of file before terminating close bracket `]`");
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
		String error = {.data = source->data, .count = source->count < count ? source->count : count};
		report_error(error, "The tape alphabet character definitions must start with the symbol `TAPE`. Please consult the examples.");
	}

	source->data += count;
	source->count -= count;

	trim_left(source);

	if (source->count == 0) {
		String error = {.data = source->data, .count = 0};
		report_error(error, "Unexpected end of file");
	}

	if (*source->data != '[') {
		String error = {.data = source->data, .count = 1};
		report_error(error, "The states and alphabets are declared in a generic array syntax but delimited by spaces."
				"\tFor example \"[ 0 1 # ]\"");
	}

	source->data += 1;
	source->count -= 1;

	trim_left(source);

	while (*source->data != ']') {
		if (source->count == 0) {
			String error = {.data = source->data, .count = 0};
			report_error(error, "Unexpected end of file");
		}

		String character = tokenize_by_space(source);
		if (character.count != 1) {
			report_error(character, "Only one character literals are allowed. Please change the input character.");
		}

		da_append_item(&ctx->tape_alphabet, *character.data);

		if (source->count == 0) {
			String error = {.data = source->data, .count = 0};
			report_error(error, "End of file before terminating close bracket `]`");
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
		trim_right(&line);

		if (line.count == 0) {
			continue;
		}

		Inst instruction = {0};

		int64_t count = strlen("inst");
		if (!string_n_equal(line, SLIT("inst"), count)) {
			String line = {.data = source->data, .count = line.count > count ? count : line.count};
			report_error(line, "Every instruction line must start with `inst`");
		}

		line.count -= count;
		line.data += count;

		if (line.count <= 0) {
			report_error(line, "Unexpected line end");
		}

		String current_routine = tokenize_by_space(&line);

		int64_t current_routine_index = -1;
		// index from this value into routines dynamic array
		for (int64_t i = 0; i < ctx->routines.count; i += 1) {
			if (string_equal(ctx->routines.items[i].name, current_routine)) {
				current_routine_index = i;
				break;
			}
		}

		if (current_routine_index == -1) {
			report_error(current_routine, "Undeclared state. Add it to the STATES array.");
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
			report_error(line, "Unexpected line end");
		}

		String read_character = tokenize_by_space(&line);

		if (read_character.count != 1) {
			report_error(read_character, "The read character can only be one single symbol.");
		}

		int64_t read_character_index = -1;
		for (int64_t i = 0; i < ctx->tape_alphabet.count; i += 1) {
			if (ctx->tape_alphabet.items[i] == read_character.data[0]) {
				read_character_index = i;
				break;
			}
		}

		if (read_character_index == -1) {
			report_error(read_character, "Read character not declared in the TAPE array.");
		}

		da_append_item(&instruction.triggers, read_character.data[0]);

		if (!string_equal(SLIT("->"), tokenize_by_space(&line))) {
			report_error(line, "The transition function must have an arrow `->` operator to indicate the change.");
		}

/*

Add support for writing no characters as

inst s0 1 -> NONE s0 .

*/

		if (line.count <= 0) {
			report_error(line, "Unexpected end of line");
		}

		String input_character = tokenize_by_space(&line);

		if (input_character.count != 1) {
			report_error(input_character, "Input can only be one character long.");
		}

		int64_t input_character_index = -1;
		for (int64_t i = 0; i < ctx->input_alphabet.count; i += 1) {
			if (input_character.data[0] == ctx->input_alphabet.items[i]) {
				input_character_index = i;
				break;
			}
		}

		if (input_character_index == -1) {
			report_error(input_character, "The input character was not declared in the global input characters array.");
		}

		instruction.output = input_character.data[0];

		if (line.count <= 0) {
			report_error(line, "Unexpected line end");
		}

		String next_state = tokenize_by_space(&line);

		int64_t next_state_index = -1;
		for (int64_t i = 0; i < ctx->routines.count; i += 1) {
			if (string_equal(ctx->routines.items[i].name, next_state)) {
				next_state_index = i;
				break;
			}
		}

		if (next_state_index == -1) {
			report_error(next_state, "Undeclared state. Add it to the STATES array.");
		}

		instruction.next_state_index = next_state_index;

		if (line.count == 0) {
			report_error(line, "Unexpected end of line\n");
		}

		String move = tokenize_by_space(&line);

		if (move.count != 1) {
			report_error(move, "Only possible values of move is '.', '<' or '>'");
		}

		switch (move.data[0]) {
		case '.': {
			instruction.move = 0;
		} break;
		case '>': {
			instruction.move = 1;
		} break;
		case '<': {
			instruction.move = -1;
		} break;
		default: {
			report_error(move, "Only possible values of move is '.', '<' or '>'\n");
		} break;
		}

		da_append_item(&ctx->routines.items[current_routine_index].insts, instruction);
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

	JPTL_Context ctx = {0};

	while (source.count > 0) {
		ctx = fetch_constants(&source);
		fetch_insts(&source, &ctx);
	}

	for (int64_t i = 0; i < ctx.routines.count; i += 1) {
		printf("Routine State :: %.*s\n", (int)ctx.routines.items[i].name.count, ctx.routines.items[i].name.data);
		for (int64_t j = 0; j < ctx.routines.items[i].insts.count; j += 1) {
			printf("\tInst %ld ->\n", j);
			printf("\t\tInputChars :: ");
			for (int64_t k = 0; k < ctx.routines.items[i].insts.items[j].triggers.count; k += 1) {
				printf("%c ", (int)ctx.routines.items[i].insts.items[j].triggers.items[k]);
			}
			printf("\n");
			printf("\t\tOutput :: %c\n", ctx.routines.items[i].insts.items[j].output);
			printf("\t\tMove :: ");
			switch (ctx.routines.items[i].insts.items[j].move) {
				case 1: printf("RIGHT\n"); break;
				case 0: printf("STAY\n"); break;
				case -1: printf("LEFT\n"); break;
			}
			printf("\t\tNext State :: %.*s\n", (int)ctx.routines.items[i].name.count, ctx.routines.items[i].name.data);
		}
	}

	return 0;
}
