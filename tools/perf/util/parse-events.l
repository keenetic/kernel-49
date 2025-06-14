
%option reentrant
%option bison-bridge
%option prefix="parse_events_"
%option stack
%option bison-locations
%option yylineno

%{
#include <errno.h>
#include "../perf.h"
#include "parse-events.h"
#include "parse-events-bison.h"

char *parse_events_get_text(yyscan_t yyscanner);
YYSTYPE *parse_events_get_lval(yyscan_t yyscanner);

static int __value(YYSTYPE *yylval, char *str, int base, int token)
{
	u64 num;

	errno = 0;
	num = strtoull(str, NULL, base);
	if (errno)
		return PE_ERROR;

	yylval->num = num;
	return token;
}

static int value(yyscan_t scanner, int base)
{
	YYSTYPE *yylval = parse_events_get_lval(scanner);
	char *text = parse_events_get_text(scanner);

	return __value(yylval, text, base, PE_VALUE);
}

static int raw(yyscan_t scanner)
{
	YYSTYPE *yylval = parse_events_get_lval(scanner);
	char *text = parse_events_get_text(scanner);

	return __value(yylval, text + 1, 16, PE_RAW);
}

static int str(yyscan_t scanner, int token)
{
	YYSTYPE *yylval = parse_events_get_lval(scanner);
	char *text = parse_events_get_text(scanner);

	yylval->str = strdup(text);
	return token;
}

/*
 * This function is called when the parser gets two kind of input:
 *
 * 	@cfg1 or @cfg2=config
 *
 * The leading '@' is stripped off before 'cfg1' and 'cfg2=config' are given to
 * bison.  In the latter case it is necessary to keep the string intact so that
 * the PMU kernel driver can determine what configurable is associated to
 * 'config'.
 */
static int drv_str(yyscan_t scanner, int token)
{
	YYSTYPE *yylval = parse_events_get_lval(scanner);
	char *text = parse_events_get_text(scanner);

	/* Strip off the '@' */
	yylval->str = strdup(text + 1);
	return token;
}

#define REWIND(__alloc)				\
do {								\
	YYSTYPE *__yylval = parse_events_get_lval(yyscanner);	\
	char *text = parse_events_get_text(yyscanner);		\
								\
	if (__alloc)						\
		__yylval->str = strdup(text);			\
								\
	yycolumn -= strlen(text);				\
	yyless(0);						\
} while (0)

static int pmu_str_check(yyscan_t scanner)
{
	YYSTYPE *yylval = parse_events_get_lval(scanner);
	char *text = parse_events_get_text(scanner);

	yylval->str = strdup(text);
	switch (perf_pmu__parse_check(text)) {
		case PMU_EVENT_SYMBOL_PREFIX:
			return PE_PMU_EVENT_PRE;
		case PMU_EVENT_SYMBOL_SUFFIX:
			return PE_PMU_EVENT_SUF;
		case PMU_EVENT_SYMBOL:
			return PE_KERNEL_PMU_EVENT;
		default:
			return PE_NAME;
	}
}

static int sym(yyscan_t scanner, int type, int config)
{
	YYSTYPE *yylval = parse_events_get_lval(scanner);

	yylval->num = (type << 16) + config;
	return type == PERF_TYPE_HARDWARE ? PE_VALUE_SYM_HW : PE_VALUE_SYM_SW;
}

static int term(yyscan_t scanner, int type)
{
	YYSTYPE *yylval = parse_events_get_lval(scanner);

	yylval->num = type;
	return PE_TERM;
}

#define YY_USER_ACTION					\
do {							\
	yylloc->last_column  = yylloc->first_column;	\
	yylloc->first_column = yycolumn;		\
	yycolumn += yyleng;				\
} while (0);

%}

%x mem
%s config
%x event
%x array

group		[^,{}/]*[{][^}]*[}][^,{}/]*
event_pmu	[^,{}/]+[/][^/]*[/][^,{}/]*
event		[^,{}/]+
bpf_object	[^,{}]+\.(o|bpf)
bpf_source	[^,{}]+\.c

num_dec		[0-9]+
num_hex		0x[a-fA-F0-9]+
num_raw_hex	[a-fA-F0-9]+
name		[a-zA-Z_*?][a-zA-Z0-9_*?.]*
name_minus	[a-zA-Z_*?][a-zA-Z0-9\-_*?.:]*
drv_cfg_term	[a-zA-Z0-9_\.]+(=[a-zA-Z0-9_*?\.:]+)?
/* If you add a modifier you need to update check_modifier() */
modifier_event	[ukhpPGHSDI]+
modifier_bp	[rwx]{1,3}

%%

%{
	{
		int start_token;

		start_token = (int) parse_events_get_extra(yyscanner);

		if (start_token == PE_START_TERMS)
			BEGIN(config);
		else if (start_token == PE_START_EVENTS)
			BEGIN(event);

		if (start_token) {
			parse_events_set_extra(NULL, yyscanner);
			/*
			 * The flex parser does not init locations variable
			 * via the scan_string interface, so we need do the
			 * init in here.
			 */
			yycolumn = 0;
			return start_token;
		}
         }
%}

<event>{

{group}		{
			BEGIN(INITIAL);
			REWIND(0);
		}

{event_pmu}	|
{bpf_object}	|
{bpf_source}	|
{event}		{
			BEGIN(INITIAL);
			REWIND(1);
			return PE_EVENT_NAME;
		}

<<EOF>>		{
			BEGIN(INITIAL);
			REWIND(0);
		}

}

<array>{
"]"			{ BEGIN(config); return ']'; }
{num_dec}		{ return value(yyscanner, 10); }
{num_hex}		{ return value(yyscanner, 16); }
,			{ return ','; }
"\.\.\."		{ return PE_ARRAY_RANGE; }
}

<config>{
	/*
	 * Please update config_term_names when new static term is added.
	 */
config			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_CONFIG); }
config1			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_CONFIG1); }
config2			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_CONFIG2); }
name			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_NAME); }
period			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_SAMPLE_PERIOD); }
freq			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_SAMPLE_FREQ); }
branch_type		{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_BRANCH_SAMPLE_TYPE); }
time			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_TIME); }
call-graph		{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_CALLGRAPH); }
stack-size		{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_STACKSIZE); }
max-stack		{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_MAX_STACK); }
inherit			{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_INHERIT); }
no-inherit		{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_NOINHERIT); }
overwrite		{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_OVERWRITE); }
no-overwrite		{ return term(yyscanner, PARSE_EVENTS__TERM_TYPE_NOOVERWRITE); }
,			{ return ','; }
"/"			{ BEGIN(INITIAL); return '/'; }
{name_minus}		{ return str(yyscanner, PE_NAME); }
\[all\]			{ return PE_ARRAY_ALL; }
"["			{ BEGIN(array); return '['; }
@{drv_cfg_term}		{ return drv_str(yyscanner, PE_DRV_CFG_TERM); }
}

<mem>{
{modifier_bp}		{ return str(yyscanner, PE_MODIFIER_BP); }
:			{ return ':'; }
"/"			{ return '/'; }
{num_dec}		{ return value(yyscanner, 10); }
{num_hex}		{ return value(yyscanner, 16); }
	/*
	 * We need to separate 'mem:' scanner part, in order to get specific
	 * modifier bits parsed out. Otherwise we would need to handle PE_NAME
	 * and we'd need to parse it manually. During the escape from <mem>
	 * state we need to put the escaping char back, so we dont miss it.
	 */
.			{ unput(*yytext); BEGIN(INITIAL); }
	/*
	 * We destroy the scanner after reaching EOF,
	 * but anyway just to be sure get back to INIT state.
	 */
<<EOF>>			{ BEGIN(INITIAL); }
}

cpu-cycles|cycles				{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES); }
stalled-cycles-frontend|idle-cycles-frontend	{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND); }
stalled-cycles-backend|idle-cycles-backend	{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND); }
instructions					{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS); }
cache-references				{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES); }
cache-misses					{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES); }
branch-instructions|branches			{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS); }
branch-misses					{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES); }
bus-cycles					{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES); }
ref-cycles					{ return sym(yyscanner, PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES); }
cpu-clock					{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK); }
task-clock					{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK); }
page-faults|faults				{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS); }
minor-faults					{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN); }
major-faults					{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ); }
context-switches|cs				{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES); }
cpu-migrations|migrations			{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS); }
alignment-faults				{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS); }
emulation-faults				{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS); }
dummy						{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_DUMMY); }
bpf-output					{ return sym(yyscanner, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_BPF_OUTPUT); }

	/*
	 * We have to handle the kernel PMU event cycles-ct/cycles-t/mem-loads/mem-stores separately.
	 * Because the prefix cycles is mixed up with cpu-cycles.
	 * loads and stores are mixed up with cache event
	 */
cycles-ct					{ return str(yyscanner, PE_KERNEL_PMU_EVENT); }
cycles-t					{ return str(yyscanner, PE_KERNEL_PMU_EVENT); }
mem-loads					{ return str(yyscanner, PE_KERNEL_PMU_EVENT); }
mem-stores					{ return str(yyscanner, PE_KERNEL_PMU_EVENT); }
topdown-[a-z-]+					{ return str(yyscanner, PE_KERNEL_PMU_EVENT); }

L1-dcache|l1-d|l1d|L1-data		|
L1-icache|l1-i|l1i|L1-instruction	|
LLC|L2					|
dTLB|d-tlb|Data-TLB			|
iTLB|i-tlb|Instruction-TLB		|
branch|branches|bpu|btb|bpc		|
node					{ return str(yyscanner, PE_NAME_CACHE_TYPE); }

load|loads|read				|
store|stores|write			|
prefetch|prefetches			|
speculative-read|speculative-load	|
refs|Reference|ops|access		|
misses|miss				{ return str(yyscanner, PE_NAME_CACHE_OP_RESULT); }

mem:			{ BEGIN(mem); return PE_PREFIX_MEM; }
r{num_raw_hex}		{ return raw(yyscanner); }
{num_dec}		{ return value(yyscanner, 10); }
{num_hex}		{ return value(yyscanner, 16); }

{modifier_event}	{ return str(yyscanner, PE_MODIFIER_EVENT); }
{bpf_object}		{ return str(yyscanner, PE_BPF_OBJECT); }
{bpf_source}		{ return str(yyscanner, PE_BPF_SOURCE); }
{name}			{ return pmu_str_check(yyscanner); }
"/"			{ BEGIN(config); return '/'; }
-			{ return '-'; }
,			{ BEGIN(event); return ','; }
:			{ return ':'; }
"{"			{ BEGIN(event); return '{'; }
"}"			{ return '}'; }
=			{ return '='; }
\n			{ }
.			{ }

%%

int parse_events_wrap(void *scanner __maybe_unused)
{
	return 1;
}
