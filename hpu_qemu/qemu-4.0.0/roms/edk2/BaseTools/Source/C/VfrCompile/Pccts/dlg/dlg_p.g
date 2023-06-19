/*  This is the parser for the dlg
 *  This is a part of the Purdue Compiler Construction Tool Set
 *
 * SOFTWARE RIGHTS
 *
 * We reserve no LEGAL rights to the Purdue Compiler Construction Tool
 * Set (PCCTS) -- PCCTS is in the public domain.  An individual or
 * company may do whatever they wish with source code distributed with
 * PCCTS or the code generated by PCCTS, including the incorporation of
 * PCCTS, or its output, into commerical software.
 *
 * We encourage users to develop software with PCCTS.  However, we do ask
 * that credit is given to us for developing PCCTS.  By "credit",
 * we mean that if you incorporate our source code into one of your
 * programs (commercial product, research project, or otherwise) that you
 * acknowledge this fact somewhere in the documentation, research report,
 * etc...  If you like PCCTS and have developed a nice tool with the
 * output, please mention that you developed it using PCCTS.  In
 * addition, we ask that this header remain intact in our source code.
 * As long as these guidelines are kept, we expect to continue enhancing
 * this system and expect to make other tools available as they are
 * completed.
 *
 * DLG 1.33
 * Will Cohen
 * With mods by Terence Parr; AHPCRC, University of Minnesota
 * 1989-1995
 */

#header	<<
#include <ctype.h>
#include "dlg.h"
>>

<<

/* MR20 G. Hobbelt 
   Fix for Borland C++ 4.x & 5.x compiling with ALL warnings enabled
*/

#ifdef __TURBOC__
#pragma warn -aus  /* unused assignment of 'xxx' */
#endif

#pragma clang diagnostic ignored "-Wparentheses-equality"

int	action_no = 0;	   /* keep track of actions outputted */
int	nfa_allocated = 0; /* keeps track of number of nfa nodes */
nfa_node **nfa_array = NULL;/* root of binary tree that stores nfa array */
nfa_node nfa_model_node;   /* model to initialize new nodes */
set	used_chars;	   /* used to label trans. arcs */
set	used_classes;	   /* classes or chars used to label trans. arcs */
set	normal_chars;	   /* mask to get rid elements that aren't used
			      in set */
int	flag_paren = FALSE;
int	flag_brace = FALSE;
int	mode_counter = 0;  /* keep track of number of %%names */

>>

#lexaction <<
int	func_action;		/* should actions be turned into functions?*/
int	lex_mode_counter = 0;	/* keeps track of the number of %%names */
/* MR1									    */
/* MR1  11-Apr-97	Provide mechanism for inserting code into DLG class */
/* MR1				via <<%%lexmember...>>			    */
/* MR1									    */
int	lexMember = 0;		/* <<%%lexmemeber ...>>	   		MR1 */
int	lexAction = 0;		/* <<%%lexaction ...>>			MR1 */
int	parserClass = 0;	/* <<%%parserclass ...>>        MR1 */
int	lexPrefix = 0;		/* <<%%lexprefix ...>>			MR1 */
char	theClassName[100];						     /* MR11 */
char	*pClassName=theClassName;					 /* MR11 */
int	firstLexMember=1;					             /* MR1 */

#ifdef __USE_PROTOS
void  xxputc(int c) {						/* MR1 */
#else
void xxputc(c)							/* MR1 */
  int	c;							/* MR1 */
{								/* MR1 */
#endif
  if (parserClass) {						/* MR1 */
    *pClassName++=c;						/* MR1 */
    *pClassName=0;						/* MR1 */
  } else if (lexMember || lexPrefix) {				/* MR1 */
    if (class_stream != NULL) fputc(c,class_stream);		/* MR1 */
  } else {							/* MR1 */
    fputc(c,OUT);						/* MR1 */
  };								/* MR1 */
}  								/* MR1 */

#ifdef __USE_PROTOS
void xxprintf(char *format,char *string) {			/* MR1 */
#else
void xxprintf(format,string) 					/* MR1 */
  char *format;							/* MR1 */
  char *string;							/* MR1 */
{								/* MR1 */
#endif
  if (lexMember || lexPrefix || parserClass) {			/* MR1 */
    if (class_stream != NULL)					/* MR1 */
	 fprintf(class_stream,format,string);			/* MR1 */
  } else {							/* MR1 */
    fprintf(OUT,format,string);					/* MR1 */
  };								/* MR1 */
}  								/* MR1 */
>>

#token "[\r\t\ ]+"	<< zzskip(); >>						/* Ignore white */
#token "\n"			<< zzline++; zzskip(); DAWDLE; >>	/* Track Line # */
#token L_EOF		"\@"
#token PER_PER		"\%\%"
#token NAME_PER_PER	"\%\%[a-zA-Z_][a-zA-Z0-9_]*"
		<< p_mode_def(&zzlextext[2],lex_mode_counter++); >>

#token LEXMEMBER	"\<\<\%\%lexmember"			/* MR1 */
		<<lexMember=1;					/* MR1 */
	          if (firstLexMember != 0) {			/* MR1 */
	            firstLexMember=0;				/* MR1 */
	            p_class_def1();				/* MR1 */
		  };						/* MR1 */
	          zzmode(ACT);					/* MR1 */
                >>						/* MR1 */
#token LEXACTION	"\<\<\%\%lexaction"			/* MR1 */
		<<lexAction=1;zzmode(ACT);>>			/* MR1 */
#token PARSERCLASS	"\<\<\%\%parserclass"			/* MR1 */
		<<parserClass=1;				/* MR1 */
		  zzmode(ACT);					/* MR1 */
		>>						/* MR1 */
#token LEXPREFIX	"\<\<\%\%lexprefix"			/* MR1 */
		<<lexPrefix=1;zzmode(ACT);>>			/* MR1 */

#token ACTION		"\<\<"
		<< if (func_action)
			fprintf(OUT,"\n%s %sact%d()\n{ ",
					gen_cpp?"ANTLRTokenType":"static void",
					gen_cpp?ClassName("::"):"", ++action_no);
		   zzmode(ACT); zzskip();
		>>
#token GREAT_GREAT	"\>\>"
#token L_BRACE		"\{"
#token R_BRACE		"\}"
#token L_PAR		"\("
#token R_PAR		"\)"
#token L_BRACK		"\["
#token R_BRACK		"\]"
#token ZERO_MORE	"\*"
#token ONE_MORE		"\+"
#token OR		"\|"
#token RANGE		"\-"
#token NOT		"\~"
#token OCTAL_VALUE "\\0[0-7]*"		
	<< {int t; sscanf(&zzlextext[1],"%o",&t); zzlextext[0] = t;}>>
#token HEX_VALUE   "\\0[Xx][0-9a-fA-F]+"
	<< {int t; sscanf(&zzlextext[3],"%x",&t); zzlextext[0] = t;}>>
#token DEC_VALUE   "\\[1-9][0-9]*"
	<< {int t; sscanf(&zzlextext[1],"%d",&t); zzlextext[0] = t;}>>
#token TAB		"\\t"		<< zzlextext[0] = '\t';>>
#token NL		"\\n"		<< zzlextext[0] = '\n';>>
#token CR		"\\r"		<< zzlextext[0] = '\r';>>
#token BS		"\\b"		<< zzlextext[0] = '\b';>>

/* MR1									*/
/* MR1 10-Apr-97 MR1	Allow #token regular expressions to cross lines	*/
/* MR1									*/
#token CONTINUATION	"\\ \n"		<< zzline++; zzskip();>> /* MR1 */

/* NOTE: this takes ANYTHING after the \ */
#token LIT		"\\~[tnrb]"	<< zzlextext[0] = zzlextext[1];>>

/* NOTE: this takes ANYTHING that doesn't match the other tokens */
#token REGCHAR		"~[\\]"


grammar		:   << p_head(); p_class_hdr(); func_action = FALSE;>>
		 ( {LEXACTION | LEXMEMBER | LEXPREFIX | PARSERCLASS } ACTION)* /* MR1 */
		    <<if ( gen_cpp ) p_includes();>>
		    start_states
		    << func_action = FALSE; p_tables(); p_tail(); >>
		    (ACTION)* "@"
			<< if (firstLexMember != 0) p_class_def1(); >> 		/* MR1 */
		;

start_states	: ( PER_PER do_conversion
		  | NAME_PER_PER do_conversion (NAME_PER_PER do_conversion)*)
		    PER_PER
		;

do_conversion	: <<new_automaton_mode(); func_action = TRUE;>>
			rule_list
			<<
				dfa_class_nop[mode_counter] =
					relabel($1.l,comp_level);
				if (comp_level)
					p_shift_table(mode_counter);
				dfa_basep[mode_counter] = dfa_allocated+1;
				make_dfa_model_node(dfa_class_nop[mode_counter]);
				nfa_to_dfa($1.l);
				++mode_counter;
		    		func_action = FALSE;
#ifdef HASH_STAT
				fprint_hash_stats(stderr);
#endif
			>>
		;

rule_list	: rule <<$$.l=$1.l; $$.r=$1.r;>>
			(rule
				<<{nfa_node *t1;
				   t1 = new_nfa_node();
				   (t1)->trans[0]=$$.l;
				   (t1)->trans[1]=$1.l;
				   /* all accept nodes "dead ends" */
				   $$.l=t1; $$.r=NULL;
				   }
				>>
			)*
		| /* empty */
			<<$$.l = new_nfa_node(); $$.r = NULL;
			   warning("no regular expressions", zzline);
			>>
		;

rule	: reg_expr ACTION
/* MR23 */		<< if ($1.r != NULL) {
					$$.l=$1.l; $$.r=$1.r; ($1.r)->accept=action_no;
				   }
				>>
		| ACTION
			<<$$.l = NULL; $$.r = NULL;
			  error("no expression for action  ", zzline);
			>>
		;

reg_expr	: and_expr <<$$.l=$1.l; $$.r=$1.r;>>
			(OR and_expr
				<<{nfa_node *t1, *t2;
				   t1 = new_nfa_node(); t2 = new_nfa_node();
				   (t1)->trans[0]=$$.l;
				   (t1)->trans[1]=$2.l;
/* MR23 */		   if ($$.r != NULL) ($$.r)->trans[1]=t2;
                   if ($2.r) {
    				   ($2.r)->trans[1]=t2;     /* MR20 */
                   }
				   $$.l=t1; $$.r=t2;
				  }
				>>
			)*
		;

and_expr	: repeat_expr
					<<
						$$.l=$1.l; $$.r=$1.r;
				    >>
			(repeat_expr 
/* MR23 */				<< if ($$.r != NULL) {
							($$.r)->trans[1]=$1.l;
							$$.r=$1.r;
						   }
						>>
			)*
		;

repeat_expr	: expr <<$$.l=$1.l; $$.r=$1.r;>>
			{ ZERO_MORE
			<<{	nfa_node *t1,*t2;
/* MR23 */		if ($$.r != NULL) ($$.r)->trans[0] = $$.l;
				t1 = new_nfa_node(); t2 = new_nfa_node();
				t1->trans[0]=$$.l;
				t1->trans[1]=t2;
/* MR23 */		if ($$.r != NULL) ($$.r)->trans[1]=t2;
				$$.l=t1;$$.r=t2;
			  }
			>>
			| ONE_MORE
/* MR23 */		<<if ($$.r != NULL) ($$.r)->trans[0] = $$.l;>>
			}
		| ZERO_MORE
			<< error("no expression for *", zzline);>>
		| ONE_MORE
			<< error("no expression for +", zzline);>>
		;

expr	: << $$.l = new_nfa_node();
			 $$.r = new_nfa_node();
		  >>
		  L_BRACK atom_list R_BRACK
			<<
/* MR23 */		if ($$.l != NULL) {
					($$.l)->trans[0] = $$.r;
					($$.l)->label = set_dup($2.label);
					set_orin(&used_chars,($$.l)->label);
				}
			>>
		| NOT L_BRACK atom_list R_BRACK
			<<
/* MR23 */		if ($$.l != NULL) {
					($$.l)->trans[0] = $$.r;
					($$.l)->label = set_dif(normal_chars,$3.label);
					set_orin(&used_chars,($$.l)->label);
				}
			>>
		| L_PAR reg_expr R_PAR
			<<
/* MR23 */		if ($$.l != NULL) {				
					($$.l)->trans[0] = $2.l;
					if ($2.r) {
    					($2.r)->trans[1] = $$.r;    /* MR20 */
					}
				}
			>>
		| L_BRACE reg_expr R_BRACE
			<<
/* MR23 */		if ($$.l != NULL) {
					($$.l)->trans[0] = $2.l;
					($$.l)->trans[1] = $$.r;
			        if ($2.r) {
    					($2.r)->trans[1] = $$.r;    /* MR20 */
					}
				}
			>>
		| atom
			<<
/* MR23 */		if ($$.l != NULL) {
					($$.l)->trans[0] = $$.r;
					($$.l)->label = set_dup($1.label);
					set_orin(&used_chars,($$.l)->label);
				}
			>>
		;

atom_list	: << set_free($$.label); >>
				(near_atom <<set_orin(&($$.label),$1.label);>>)*
		;

near_atom	: << register int i;
		     register int i_prime;
		  >>
		  anychar
			<<$$.letter=$1.letter; $$.label=set_of($1.letter);
			i_prime = $1.letter + MIN_CHAR;
			if (case_insensitive && islower(i_prime))
				set_orel(toupper(i_prime)-MIN_CHAR,
					&($$.label));
			if (case_insensitive && isupper(i_prime))
	 			set_orel(tolower(i_prime)-MIN_CHAR,
					&($$.label));
			>>
			{ RANGE anychar
				<< if (case_insensitive){
					i_prime = $$.letter+MIN_CHAR;
					$$.letter = (islower(i_prime) ?
						toupper(i_prime) : i_prime)-MIN_CHAR;
					i_prime = $2.letter+MIN_CHAR;
					$2.letter = (islower(i_prime) ?
						toupper(i_prime) : i_prime)-MIN_CHAR;
				   }
				   /* check to see if range okay */
					{
					    int debugLetter1 = $$.letter;
						int debugLetter2 = $2.letter;
					}
				   if ($$.letter > $2.letter 
                                       && $2.letter != 0xff){       /* MR16 */
					  error("invalid range  ", zzline);
				   }
				   for (i=$$.letter; i<= (int)$2.letter; ++i){
					set_orel(i,&($$.label));
					i_prime = i+MIN_CHAR;
					if (case_insensitive && islower(i_prime))
						set_orel(toupper(i_prime)-MIN_CHAR,
							&($$.label));
					if (case_insensitive && isupper(i_prime))
		 				set_orel(tolower(i_prime)-MIN_CHAR,
							&($$.label));
					}
				>>
			}
		;

atom		: << register int i_prime;>>
		  anychar
		  <<$$.label = set_of($1.letter);
		    i_prime = $1.letter + MIN_CHAR;
		    if (case_insensitive && islower(i_prime))
			set_orel(toupper(i_prime)-MIN_CHAR,
				&($$.label));
		    if (case_insensitive && isupper(i_prime))
	 		set_orel(tolower(i_prime)-MIN_CHAR,
				&($$.label));
		  >>
		;

anychar		: REGCHAR	<<$$.letter = $1.letter - MIN_CHAR;>>
		| OCTAL_VALUE	<<$$.letter = $1.letter - MIN_CHAR;>>
		| HEX_VALUE	<<$$.letter = $1.letter - MIN_CHAR;>>
		| DEC_VALUE	<<$$.letter = $1.letter - MIN_CHAR;>>
		| TAB		<<$$.letter = $1.letter - MIN_CHAR;>>
		| NL		<<$$.letter = $1.letter - MIN_CHAR;>>
		| CR		<<$$.letter = $1.letter - MIN_CHAR;>>
		| BS		<<$$.letter = $1.letter - MIN_CHAR;>>
		| LIT		<<$$.letter = $1.letter - MIN_CHAR;>>
		/* NOTE: LEX_EOF is ALWAYS shifted to 0 = MIN_CHAR - MIN_CHAR*/
		| L_EOF		<<$$.letter = 0;>>
		;

<</* empty action */>>

#lexclass ACT
#token "@"	<< error("unterminated action", zzline); zzmode(START); >>
#token ACTION "\>\>"
		<< if (func_action) fprintf(OUT,"}\n\n");
		   zzmode(START);
/* MR1									    */
/* MR1  11-Apr-97	Provide mechanism for inserting code into DLG class */
/* MR1				via <<%%lexmember ...>>			    */
/* MR1			This is a consequence of not saving actions         */
/* MR1									    */
/* MR1 */	   parserClass=0;		
/* MR1 */	   lexPrefix=0;
/* MR1 */	   lexAction=0;
/* MR1 */	   lexMember=0;
		>>
#token "\>"		<< xxputc(zzlextext[0]); zzskip(); >>		/* MR1 */
#token "\\\>"		<< xxputc('>'); zzskip(); >>			/* MR1 */
#token "\\"		<< xxputc('\\'); zzskip(); >>			/* MR1 */
#token "\n"		<< xxputc(zzlextext[0]); ++zzline; zzskip(); >>	/* MR1 */
#token "/\*"		<< zzmode(ACTION_COMMENTS);			/* MR1 */
			   xxprintf("%s", &(zzlextext[0])); zzskip();	/* MR1 */
			>>						/* MR1 */
#token "//"		<< zzmode(ACTION_CPP_COMMENTS);			/* MR1 */
			   xxprintf("%s", &(zzlextext[0])); zzskip();	/* MR1 */
			>>						/* MR1 */
#token "~[]"		<< xxputc(zzlextext[0]); zzskip(); >>		/* MR1 */
									/* MR1 */
#lexclass ACTION_COMMENTS						/* MR1 */
#token "\*/"		<< zzmode(ACT);					/* MR1 */
			   xxprintf("%s", &(zzlextext[0])); zzskip();	/* MR1 */
			>>						/* MR1 */
#token "[\n\r]"		<< zzline++; xxputc(zzlextext[0]); zzskip();>>	/* MR1 */
#token "~[]"		<< xxputc(zzlextext[0]); zzskip();>>		/* MR1 */
									/* MR1 */
#lexclass ACTION_CPP_COMMENTS						/* MR1 */
#token "[\n\r]"		<< zzmode(ACT); zzline++;			/* MR1 */
			   xxprintf("%s", &(zzlextext[0])); zzskip();	/* MR1 */
			>>						/* MR1 */
#token "~[]"		<< xxputc(zzlextext[0]); zzskip();>>		/* MR1 */

<<
/* adds a new nfa to the binary tree and returns a pointer to it */
nfa_node *
#ifdef __USE_PROTOS
new_nfa_node(void)
#else
new_nfa_node()
#endif
{
	register nfa_node *t;
	static int nfa_size=0;	/* elements nfa_array[] can hold */

	++nfa_allocated;
	if (nfa_size<=nfa_allocated){
		/* need to redo array */
		if (!nfa_array){
			/* need some to do inital allocation */
			nfa_size=nfa_allocated+NFA_MIN;
			nfa_array=(nfa_node **) malloc(sizeof(nfa_node*)*
				nfa_size);
		}else{
			/* need more space */
			nfa_size=2*(nfa_allocated+1);
			nfa_array=(nfa_node **) realloc(nfa_array,
				sizeof(nfa_node*)*nfa_size);
		}
	}
	/* fill out entry in array */
	t = (nfa_node*) malloc(sizeof(nfa_node));
	nfa_array[nfa_allocated] = t;
	*t = nfa_model_node;
	t->node_no = nfa_allocated;
	return t;
}


/* initialize the model node used to fill in newly made nfa_nodes */
void
#ifdef __USE_PROTOS
make_nfa_model_node(void)
#else
make_nfa_model_node()
#endif
{
	nfa_model_node.node_no = -1; /* impossible value for real nfa node */
	nfa_model_node.nfa_set = 0;
	nfa_model_node.accept = 0;   /* error state default*/
	nfa_model_node.trans[0] = NULL;
	nfa_model_node.trans[1] = NULL;
	nfa_model_node.label = empty;
}
>>

<<
#if defined(DEBUG) || defined(_DEBUG)

/* print out the pointer value and the node_number */
void
#ifdef __USE_PROTOS
fprint_dfa_pair(FILE *f, nfa_node *p)
#else
fprint_dfa_pair(f, p)
FILE *f;
nfa_node *p;
#endif
{
	if (p){
		fprintf(f, "%x (%d)", p, p->node_no);
	}else{
		fprintf(f, "(nil)");
	}
}

/* print out interest information on a set */
void
#ifdef __USE_PROTOS
fprint_set(FILE *f, set s)
#else
fprint_set(f,s)
FILE *f;
set s;
#endif
{
	unsigned int *x;

	fprintf(f, "n = %d,", s.n);
	if (s.setword){
		fprintf(f, "setword = %x,   ", s.setword);
		/* print out all the elements in the set */
		x = set_pdq(s);
		while (*x!=nil){
			fprintf(f, "%d ", *x);
			++x;
		}
	}else{
		fprintf(f, "setword = (nil)");
	}
}

/* code to be able to dump out the nfas
	return 0 if okay dump
	return 1 if screwed up
 */
int
#ifdef __USE_PROTOS
dump_nfas(int first_node, int last_node)
#else
dump_nfas(first_node, last_node)
int first_node;
int last_node;
#endif
{
	register int i;
	nfa_node *t;

	for (i=first_node; i<=last_node; ++i){
		t = NFA(i);
		if (!t) break;
		fprintf(stderr, "nfa_node %d {\n", t->node_no);
		fprintf(stderr, "\n\tnfa_set = %d\n", t->nfa_set);
		fprintf(stderr, "\taccept\t=\t%d\n", t->accept);
		fprintf(stderr, "\ttrans\t=\t(");
		fprint_dfa_pair(stderr, t->trans[0]);
		fprintf(stderr, ",");
		fprint_dfa_pair(stderr, t->trans[1]);
		fprintf(stderr, ")\n");
		fprintf(stderr, "\tlabel\t=\t{ ");
		fprint_set(stderr, t->label);
		fprintf(stderr, "\t}\n");
		fprintf(stderr, "}\n\n");
	}
	return 0;
}
#endif
>>

<<
/* DLG-specific syntax error message generator
 * (define USER_ZZSYN when compiling so don't get 2 definitions)
 */
void
#ifdef __USE_PROTOS
zzsyn(char *text, int tok, char *egroup, SetWordType *eset, int etok, int k, char *bad_text)
#else
zzsyn(text, tok, egroup, eset, etok, k, bad_text)
char *text, *egroup, *bad_text;
int tok;
int etok;
int k;
SetWordType *eset;
#endif
{
	fprintf(stderr, ErrHdr, file_str[0]!=NULL?file_str[0]:"stdin", zzline);
	fprintf(stderr, " syntax error at \"%s\"", (tok==zzEOF_TOKEN)?"EOF":text);
	if ( !etok && !eset ) {fprintf(stderr, "\n"); return;}
	if ( k==1 ) fprintf(stderr, " missing");
	else
	{
		fprintf(stderr, "; \"%s\" not", bad_text);
		if ( zzset_deg(eset)>1 ) fprintf(stderr, " in");
	}
	if ( zzset_deg(eset)>0 ) zzedecode(eset);
	else fprintf(stderr, " %s", zztokens[etok]);
	if ( strlen(egroup) > (size_t)0 ) fprintf(stderr, " in %s", egroup);
	fprintf(stderr, "\n");
}
>>
