//20170403
#include "pgenheaders.h"
#include <ctype.h>
#include "tokenizer.h"
#include "errcode.h"

extern char *PyOS_Readline(char *);

#define TABSIZE 8
#define Py_CHARMASK(c)		((c) & 0xff)

static struct tok_state *tok_new();
static int tok_nextc(struct tok_state *tok);
static void tok_backup(struct tok_state *tok, int c);

char *_PyParser_TokenNames[] = {
	"ENDMARKER",
	"NAME",
	"NUMBER",
	"STRING",
	"NEWLINE",
	"INDENT",
	"DEDENT",
	"LPAR",
	"RPAR",
	"LSQB",
	"RSQB",
	"COLON",
	"COMMA",
	"SEMI",
	"PLUS",
	"MINUS",
	"STAR",
	"SLASH",
	"VBAR",
	"AMPER",
	"LESS",
	"GREATER",
	"EQUAL",
	"DOT",
	"PERCENT",
	"BACKQUOTE",
	"LBRACE",
	"RBRACE",
	"EQEQUAL",
	"NOTEQUAL",
	"LESSEQUAL",
	"GREATEREQUAL",
	"TILDE",
	"CIRCUMFLEX",
	"LEFTSHIFT",
	"RIGHTSHIFT",
	"DOUBLESTAR",
	"PLUSEQUAL",
	"MINEQUAL",
	"STAREQUAL",
	"SLASHEQUAL",
	"PERCENTEQUAL",
	"AMPEREQUAL",
	"VBAREQUAL",
	"CIRCUMFLEXEQUAL",
	"LEFTSHIFTEQUAL",
	"RIGHTSHIFTEQUAL",
	"DOUBLESTAREQUAL",
	"DOUBLESLASH",
	"DOUBLESLASHEQUAL",
	"OP",
	"<ERRORTOKEN>",
	"<N_TOKENS>"
};

static struct tok_state *tok_new()
{
	struct tok_state *tok = PyMem_NEW(struct tok_state, 1);
	if (tok == NULL)
	{
		return NULL;
	}
	tok->buf = tok->cur = tok->end = tok->inp = tok->start = NULL;
	tok->done = E_OK;
	tok->fp = NULL;
	tok->tabsize = TABSIZE;
	tok->indent = 0;
	tok->indstack[0] = 0;
	tok->atbol = 1;
	tok->pendin = 0;
	tok->prompt = tok->nextprompt = NULL;
	tok->lineno = 0;
	tok->level = 0;
	tok->filename = NULL;
	tok->altwarning = 0;
	tok->alterror = 0;
	tok->alttabsize = 1;
	tok->altindstack[0] = 0;
	return tok;
}

struct tok_state *PyTokenizer_FromString(char *str)
{
	struct tok_state *tok = tok_new();
	if (tok == NULL)
	{
		return NULL;
	}
	tok->buf = tok->cur = tok->end = tok->inp = str;
	return tok;
}

struct tok_state *PyTokenizer_FromFile(FILE *fp, char *ps1, char *ps2)
{
	struct tok_state *tok = tok_new();
	if (tok == NULL)
	{
		return NULL;
	}
	if ((tok->buf = PyMem_NEW(char, BUFSIZ)) == NULL) 
	{
		PyMem_DEL(tok);
		return NULL;
	}
	tok->cur = tok->inp = tok->buf;
	tok->end = tok->buf + BUFSIZ;
	tok->fp = fp;
	tok->prompt = ps1;
	tok->nextprompt = ps2;
	return tok;
}

void PyTokenizer_Free(struct tok_state *tok)
{
	if (tok->fp != NULL && tok->buf != NULL)
	{
		PyMem_DEL(tok->buf);
	}
	PyMem_DEL(tok);
}

static int tok_nextc(struct tok_state *tok)
{
	for (;;) 
	{
		if (tok->cur != tok->inp) 
		{
			return Py_CHARMASK(*tok->cur++);
		}
		if (tok->done != E_OK)
		{
			return EOF;
		}
		if (tok->fp == NULL) 
		{
			char *end = strchr(tok->inp, '\n');
			if (end != NULL)
			{
				end++;
			}
			else 
			{
				end = strchr(tok->inp, '\0');
				if (end == tok->inp) 
				{
					tok->done = E_EOF;
					return EOF;
				}
			}
			if (tok->start == NULL)
			{
				tok->buf = tok->cur;
			}
			tok->lineno++;
			tok->inp = end;
			return Py_CHARMASK(*tok->cur++);
		}
		if (tok->prompt != NULL) 
		{
			char *new_ = PyOS_Readline(tok->prompt);
			if (tok->nextprompt != NULL)
			{
				tok->prompt = tok->nextprompt;
			}
			if (new_ == NULL)
			{
				tok->done = E_INTR;
			}
			else if (*new_ == '\0') 
			{
				PyMem_FREE(new_);
				tok->done = E_EOF;
			}
			else if (tok->start != NULL) 
			{
				size_t start = tok->start - tok->buf;
				size_t oldlen = tok->cur - tok->buf;
				size_t newlen = oldlen + strlen(new_);
				char *buf = tok->buf;
				PyMem_RESIZE(buf, char, newlen+1);
				tok->lineno++;
				if (buf == NULL) 
				{
					PyMem_DEL(tok->buf);
					tok->buf = NULL;
					PyMem_FREE(new_);
					tok->done = E_NOMEM;
					return EOF;
				}
				tok->buf = buf;
				tok->cur = tok->buf + oldlen;
				strcpy(tok->buf + oldlen, new_);
				PyMem_FREE(new_);
				tok->inp = tok->buf + newlen;
				tok->end = tok->inp + 1;
				tok->start = tok->buf + start;
			}
			else 
			{
				tok->lineno++;
				if (tok->buf != NULL)
				{
					PyMem_DEL(tok->buf);
				}
				tok->buf = new_;
				tok->cur = tok->buf;
				tok->inp = strchr(tok->buf, '\0');
				tok->end = tok->inp + 1;
			}
		}
		else 
		{
			int done = 0;
			int cur = 0;
			char *pt;
			if (tok->start == NULL) 
			{
				if (tok->buf == NULL) 
				{
					tok->buf = PyMem_NEW(char, BUFSIZ);
					if (tok->buf == NULL) 
					{
						tok->done = E_NOMEM;
						return EOF;
					}
					tok->end = tok->buf + BUFSIZ;
				}
				if (fgets(tok->buf, (int)(tok->end - tok->buf),
					  tok->fp) == NULL) 
				{
					tok->done = E_EOF;
					done = 1;
				}
				else 
				{
					tok->done = E_OK;
					tok->inp = strchr(tok->buf, '\0');
					done = tok->inp[-1] == '\n';
				}
			}
			else 
			{
				cur = tok->cur - tok->buf;
				if (feof(tok->fp)) 
				{
					tok->done = E_EOF;
					done = 1;
				}
				else
				{
					tok->done = E_OK;
				}
			}
			tok->lineno++;
			while (!done) 
			{
				int curstart = tok->start == NULL ? -1 :
					       tok->start - tok->buf;
				int curvalid = tok->inp - tok->buf;
				int newsize = curvalid + BUFSIZ;
				char *newbuf = tok->buf;
				PyMem_RESIZE(newbuf, char, newsize);
				if (newbuf == NULL) 
				{
					tok->done = E_NOMEM;
					tok->cur = tok->inp;
					return EOF;
				}
				tok->buf = newbuf;
				tok->inp = tok->buf + curvalid;
				tok->end = tok->buf + newsize;
				tok->start = curstart < 0 ? NULL :
					     tok->buf + curstart;
				if (fgets(tok->inp,
					       (int)(tok->end - tok->inp),
					       tok->fp) == NULL) 
				{
					strcpy(tok->inp, "\n");
				}
				tok->inp = strchr(tok->inp, '\0');
				done = tok->inp[-1] == '\n';
			}
			tok->cur = tok->buf + cur;

			pt = tok->inp - 2;
			if (pt >= tok->buf && *pt == '\r') 
			{
				*pt++ = '\n';
				*pt = '\0';
				tok->inp = pt;
			}
		}
		if (tok->done != E_OK) 
		{
			if (tok->prompt != NULL)
			{
				PySys_WriteStderr("\n");
			}
			tok->cur = tok->inp;
			return EOF;
		}
	}
}

static void tok_backup(struct tok_state *tok, int c)
{
	if (c != EOF) 
	{
		if (--tok->cur < tok->buf)
		{
			Py_FatalError("tok_backup: begin of buffer");
		}
		if (*tok->cur != c)
		{
			*tok->cur = c;
		}
	}
}

int PyToken_OneChar(int c)
{
	switch (c) {
	case '(':	
		return LPAR;

	case ')':	
		return RPAR;

	case '[':	
		return LSQB;

	case ']':	
		return RSQB;

	case ':':	
		return COLON;
	
	case ',':	
		return COMMA;
	
	case ';':	
		return SEMI;
	
	case '+':	
		return PLUS;
	
	case '-':	
		return MINUS;
	
	case '*':	
		return STAR;
	
	case '/':	
		return SLASH;
	
	case '|':	
		return VBAR;
	
	case '&':	
		return AMPER;
	
	case '<':	
		return LESS;
	
	case '>':	
		return GREATER;
	
	case '=':	
		return EQUAL;
	
	case '.':	
		return DOT;
	
	case '%':	
		return PERCENT;
	
	case '`':	
		return BACKQUOTE;
	
	case '{':	
		return LBRACE;
	
	case '}':	
		return RBRACE;
	
	case '^':	
		return CIRCUMFLEX;
	
	case '~':	
		return TILDE;
	
	default:	
		return OP;
	}
}


int PyToken_TwoChars(int c1, int c2)
{
	switch (c1) 
	{
	case '=':
		switch (c2) 
		{
		case '=':	
			return EQEQUAL;
		}
		break;

	case '!':
		switch (c2) 
		{
		case '=':	
			return NOTEQUAL;
		}
		break;

	case '<':
		switch (c2) 
		{
		case '>':	
			return NOTEQUAL;
		
		case '=':	
			return LESSEQUAL;
		
		case '<':	
			return LEFTSHIFT;
		}
		break;

	case '>':
		switch (c2) 
		{
		case '=':	
			return GREATEREQUAL;
		
		case '>':	
			return RIGHTSHIFT;
		}
		break;
	
	case '+':
		switch (c2) 
		{
		case '=':	
			return PLUSEQUAL;
		}
		break;

	case '-':
		switch (c2) 
		{
		case '=':	
			return MINEQUAL;
		}
		break;

	case '*':
		switch (c2) 
		{
		case '*':	
			return DOUBLESTAR;
		
		case '=':	
			return STAREQUAL;
		}
		break;

	case '/':
		switch (c2) 
		{
		case '/':	
			return DOUBLESLASH;
		
		case '=':	
			return SLASHEQUAL;
		}
		break;

	case '|':
		switch (c2) 
		{
		case '=':	
			return VBAREQUAL;
		}
		break;

	case '%':
		switch (c2) 
		{
		case '=':	
			return PERCENTEQUAL;
		}
		break;

	case '&':
		switch (c2) 
		{
		case '=':	
			return AMPEREQUAL;
		}
		break;

	case '^':
		switch (c2) 
		{
		case '=':	
			return CIRCUMFLEXEQUAL;
		}
		break;
	}
	return OP;
}

int PyToken_ThreeChars(int c1, int c2, int c3)
{
	switch (c1) 
	{
	case '<':
		switch (c2) 
		{
		case '<':
			switch (c3) 
			{
			case '=':
				return LEFTSHIFTEQUAL;
			}
			break;
		}
		break;

	case '>':
		switch (c2) 
		{
		case '>':
			switch (c3) 
			{
			case '=':
				return RIGHTSHIFTEQUAL;
			}
			break;
		}
		break;

	case '*':
		switch (c2) 
		{
		case '*':
			switch (c3) 
			{
			case '=':
				return DOUBLESTAREQUAL;
			}
			break;
		}
		break;

	case '/':
		switch (c2) 
		{
		case '/':
			switch (c3) 
			{
			case '=':
				return DOUBLESLASHEQUAL;
			}
			break;
		}
		break;
	}
	return OP;
}

static int indenterror(struct tok_state *tok)
{
	if (tok->alterror) 
	{
		tok->done = E_TABSPACE;
		tok->cur = tok->inp;
		return 1;
	}
	if (tok->altwarning) 
	{
		PySys_WriteStderr("%s: inconsistent use of tabs and spaces "
                                  "in indentation\n", tok->filename);
		tok->altwarning = 0;
	}
	return 0;
}

int PyTokenizer_Get(struct tok_state *tok, char **p_start,
		char **p_end)
{
	int c;
	int blankline;

	*p_start = *p_end = NULL;
nextline:
	tok->start = NULL;
	blankline = 0;

	if (tok->atbol) 
	{
		int col = 0;
		int altcol = 0;
		tok->atbol = 0;
		for (;;) 
		{
			c = tok_nextc(tok);
			if (c == ' ')
			{
				col++, altcol++;
			}
			else if (c == '\t') 
			{
				col = (col/tok->tabsize + 1) * tok->tabsize;
				altcol = (altcol/tok->alttabsize + 1)
					* tok->alttabsize;
			}
			else if (c == '\014')
			{
				col = altcol = 0;
			}
			else
			{
				break;
			}
		}
		tok_backup(tok, c);
		if (c == '#' || c == '\n') 
		{
			if (col == 0 && c == '\n' && tok->prompt != NULL)
			{
				blankline = 0;
			}
			else
			{
				blankline = 1;
			}
		}
		if (!blankline && tok->level == 0) 
		{
			if (col == tok->indstack[tok->indent]) 
			{
				if (altcol != tok->altindstack[tok->indent]) 
				{
					if (indenterror(tok))
					{
						return ERRORTOKEN;
					}
				}
			}
			else if (col > tok->indstack[tok->indent]) 
			{
				if (tok->indent+1 >= MAXINDENT) 
				{
					tok->done = E_TOODEEP;
					tok->cur = tok->inp;
					return ERRORTOKEN;
				}
				if (altcol <= tok->altindstack[tok->indent]) 
				{
					if (indenterror(tok))
					{
						return ERRORTOKEN;
					}
				}
				tok->pendin++;
				tok->indstack[++tok->indent] = col;
				tok->altindstack[tok->indent] = altcol;
			}
			else 
			{
				while (tok->indent > 0 &&
					col < tok->indstack[tok->indent]) 
				{
					tok->pendin--;
					tok->indent--;
				}
				if (col != tok->indstack[tok->indent]) 
				{
					tok->done = E_DEDENT;
					tok->cur = tok->inp;
					return ERRORTOKEN;
				}
				if (altcol != tok->altindstack[tok->indent]) 
				{
					if (indenterror(tok))
					{
						return ERRORTOKEN;
					}
				}
			}
		}
	}
	
	tok->start = tok->cur;
	
	if (tok->pendin != 0) 
	{
		if (tok->pendin < 0) 
		{
			tok->pendin++;
			return DEDENT;
		}
		else 
		{
			tok->pendin--;
			return INDENT;
		}
	}
	
again:
	tok->start = NULL;
	do 
	{
		c = tok_nextc(tok);
	} while (c == ' ' || c == '\t' || c == '\014');
	
	tok->start = tok->cur - 1;
	
	if (c == '#') 
	{
		static char *tabforms[] = {
			"tab-width:",	
			":tabstop=",		
			":ts=",			
			"set tabsize=",
		};
		char cbuf[80];
		char *tp, **cp;
		tp = cbuf;
		do 
		{
			*tp++ = c = tok_nextc(tok);
		} while (c != EOF && c != '\n' &&
			 tp - cbuf + 1 < sizeof(cbuf));
		*tp = '\0';
		for (cp = tabforms; 
		     cp < tabforms + sizeof(tabforms)/sizeof(tabforms[0]);
		     cp++) 
		{
			if ((tp = strstr(cbuf, *cp))) 
			{
				int newsize = atoi(tp + strlen(*cp));

				if (newsize >= 1 && newsize <= 40) 
				{
					tok->tabsize = newsize;
					if (Py_VerboseFlag)
					{
					    PySys_WriteStderr(
						"Tab size set to %d\n",
						newsize);
					}
				}
			}
		}
		while (c != EOF && c != '\n')
		{
			c = tok_nextc(tok);
		}
	}
	
	if (c == EOF) 
	{
		return tok->done == E_EOF ? ENDMARKER : ERRORTOKEN;
	}
	

	if (isalpha(c) || c == '_') 
	{
		switch (c) 
		{
		case 'r':
		case 'R':
			c = tok_nextc(tok);
			if (c == '"' || c == '\'')
			{
				goto letter_quote;
			}
			break;
		
		case 'u':
		case 'U':
			c = tok_nextc(tok);
			if (c == 'r' || c == 'R')
			{
				c = tok_nextc(tok);
			}
			if (c == '"' || c == '\'')
			{
				goto letter_quote;
			}
			break;
		}
		while (isalnum(c) || c == '_') 
		{
			c = tok_nextc(tok);
		}
		tok_backup(tok, c);
		*p_start = tok->start;
		*p_end = tok->cur;
		return NAME;
	}
	
	if (c == '\n') 
	{
		tok->atbol = 1;
		if (blankline || tok->level > 0)
		{
			goto nextline;
		}
		*p_start = tok->start;
		*p_end = tok->cur - 1;
		return NEWLINE;
	}
		
	if (c == '.') 
	{
		c = tok_nextc(tok);
		if (isdigit(c)) 
		{
			goto fraction;
		}
		else 
		{
			tok_backup(tok, c);
			*p_start = tok->start;
			*p_end = tok->cur;
			return DOT;
		}
	}

	if (isdigit(c)) 
	{
		if (c == '0') 
		{
			c = tok_nextc(tok);
			if (c == '.')
			{
				goto fraction;
			}
			if (c == 'j' || c == 'J')
			{
				goto imaginary;
			}
			if (c == 'x' || c == 'X') 
			{
				do 
				{
					c = tok_nextc(tok);
				} while (isxdigit(c));
			}
			else 
			{
				int found_decimal = 0;
				while ('0' <= c && c < '8') 
				{
					c = tok_nextc(tok);
				}
				if (isdigit(c)) 
				{
					found_decimal = 1;
					do 
					{
						c = tok_nextc(tok);
					} while (isdigit(c));
				}
				if (c == '.')
				{
					goto fraction;
				}
				else if (c == 'e' || c == 'E')
				{
					goto exponent;
				}

				else if (c == 'j' || c == 'J')
				{
					goto imaginary;
				}

				else if (found_decimal) 
				{
					tok->done = E_TOKEN;
					tok_backup(tok, c);
					return ERRORTOKEN;
				}
			}
			if (c == 'l' || c == 'L')
			{
				c = tok_nextc(tok);
			}
		}
		else 
		{
			do 
			{
				c = tok_nextc(tok);
			} while (isdigit(c));
			if (c == 'l' || c == 'L')
			{
				c = tok_nextc(tok);
			}
			else 
			{
				if (c == '.') 
				{
fraction:
					do 
					{
						c = tok_nextc(tok);
					} while (isdigit(c));
				}
				if (c == 'e' || c == 'E') 
				{
exponent:
					c = tok_nextc(tok);
					if (c == '+' || c == '-')
					{
						c = tok_nextc(tok);
					}
					if (!isdigit(c)) 
					{
						tok->done = E_TOKEN;
						tok_backup(tok, c);
						return ERRORTOKEN;
					}
					do 
					{
						c = tok_nextc(tok);
					} while (isdigit(c));
				}
				if (c == 'j' || c == 'J')
				{
imaginary:
					c = tok_nextc(tok);
				}
			}
		}
		tok_backup(tok, c);
		*p_start = tok->start;
		*p_end = tok->cur;
		return NUMBER;
	}

letter_quote:
	if (c == '\'' || c == '"') 
	{
		int quote2 = tok->cur - tok->start + 1;
		int quote = c;
		int triple = 0;
		int tripcount = 0;
		for (;;) 
		{
			c = tok_nextc(tok);
			if (c == '\n') 
			{
				if (!triple) 
				{
					tok->done = E_TOKEN;
					tok_backup(tok, c);
					return ERRORTOKEN;
				}
				tripcount = 0;
			}
			else if (c == EOF) 
			{
				tok->done = E_TOKEN;
				tok->cur = tok->inp;
				return ERRORTOKEN;
			}
			else if (c == quote) 
			{
				tripcount++;
				if (tok->cur - tok->start == quote2) 
				{
					c = tok_nextc(tok);
					if (c == quote) 
					{
						triple = 1;
						tripcount = 0;
						continue;
					}
					tok_backup(tok, c);
				}
				if (!triple || tripcount == 3)
				{
					break;
				}
			}
			else if (c == '\\') 
			{
				tripcount = 0;
				c = tok_nextc(tok);
				if (c == EOF) 
				{
					tok->done = E_TOKEN;
					tok->cur = tok->inp;
					return ERRORTOKEN;
				}
			}
			else
			{
				tripcount = 0;
			}
		}
		*p_start = tok->start;
		*p_end = tok->cur;
		return STRING;
	}
	
	if (c == '\\') 
	{
		c = tok_nextc(tok);
		if (c != '\n') 
		{
			tok->done = E_TOKEN;
			tok->cur = tok->inp;
			return ERRORTOKEN;
		}
		goto again;
	}
	
	{
		int c2 = tok_nextc(tok);
		int token = PyToken_TwoChars(c, c2);
		if (token != OP) 
		{
			int c3 = tok_nextc(tok);
			int token3 = PyToken_ThreeChars(c, c2, c3);
			if (token3 != OP) 
			{
				token = token3;
			} 
			else 
			{
				tok_backup(tok, c3);
			}
			*p_start = tok->start;
			*p_end = tok->cur;
			return token;
		}
		tok_backup(tok, c2);
	}
	
	switch (c) 
	{
	case '(':
	case '[':
	case '{':
		tok->level++;
		break;

	case ')':
	case ']':
	case '}':
		tok->level--;
		break;
	}
	
	*p_start = tok->start;
	*p_end = tok->cur;
	return PyToken_OneChar(c);
}

#ifdef _DEBUG
void tok_dump(int type, char *start, char *end)
{
	printf("%s", _PyParser_TokenNames[type]);
	if (type == NAME || type == NUMBER || type == STRING || type == OP)
	{
		printf("(%.*s)", (int)(end - start), start);
	}
}
#endif
