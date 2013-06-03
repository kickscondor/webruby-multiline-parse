#include <stdio.h>
#include <string.h>

#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/proc.h>

static mrbc_context* s_cxt = NULL;

/* Guess if the user might want to enter more
 * or if he wants an evaluation of his code now
 * Taken from mruby-bin-mirb gem
 */
static int
is_code_block_open(struct mrb_parser_state *parser)
{
  int code_block_open = FALSE;

  /* check for heredoc */
  if (parser->parsing_heredoc != NULL) return TRUE;
  if (parser->heredoc_end_now) {
    parser->heredoc_end_now = FALSE;
    return FALSE;
  }

  /* check if parser error are available */
  if (0 < parser->nerr) {
    const char *unexpected_end = "syntax error, unexpected $end";
    const char *message = parser->error_buffer[0].message;

    /* a parser error occur, we have to check if */
    /* we need to read one more line or if there is */
    /* a different issue which we have to show to */
    /* the user */

    if (strncmp(message, unexpected_end, strlen(unexpected_end)) == 0) {
      code_block_open = TRUE;
    }
    else if (strcmp(message, "syntax error, unexpected keyword_end") == 0) {
      code_block_open = FALSE;
    }
    else if (strcmp(message, "syntax error, unexpected tREGEXP_BEG") == 0) {
      code_block_open = FALSE;
    }
    return code_block_open;
  }

  /* check for unterminated string */
  if (parser->lex_strterm) return TRUE;

  switch (parser->lstate) {

    /* all states which need more code */

    case EXPR_BEG:
      /* an expression was just started, */
      /* we can't end it like this */
      code_block_open = TRUE;
      break;
    case EXPR_DOT:
      /* a message dot was the last token, */
      /* there has to come more */
      code_block_open = TRUE;
      break;
    case EXPR_CLASS:
      /* a class keyword is not enough! */
      /* we need also a name of the class */
      code_block_open = TRUE;
      break;
    case EXPR_FNAME:
      /* a method name is necessary */
      code_block_open = TRUE;
      break;
    case EXPR_VALUE:
      /* if, elsif, etc. without condition */
      code_block_open = TRUE;
      break;

      /* now all the states which are closed */

    case EXPR_ARG:
      /* an argument is the last token */
      code_block_open = FALSE;
      break;

      /* all states which are unsure */

    case EXPR_CMDARG:
      break;
    case EXPR_END:
      /* an expression was ended */
      break;
    case EXPR_ENDARG:
      /* closing parenthese */
      break;
    case EXPR_ENDFN:
      /* definition end */
      break;
    case EXPR_MID:
      /* jump keyword like break, return, ... */
      break;
    case EXPR_MAX_STATE:
      /* don't know what to do with this token */
      break;
    default:
      /* this state is unexpected! */
      break;
  }

  return code_block_open;
}

static int
check_and_print_result(mrb_state* mrb, mrb_value result,
                       int code_block_open, int print_level)
{
  if (code_block_open) { return 2; }
  if (mrb->exc && (print_level > 0)) {
    mrb_p(mrb, mrb_obj_value(mrb->exc));
    mrb->exc = 0;
    return 1;
  }
  if (print_level > 1) {
    mrb_p(mrb, result);
  }
  return 0;
}

/* This is linked and used at JS side */
/*
 * Returned results:
 * 0 - Parsed and executed successfully
 * 1 - Error occurs, but code block is closed
 * 2 - Code block is open
 */
int multiline_parse_run_source(mrb_state* mrb, const char* src, int print_level)
{
  struct mrb_parser_state *parser;
  int code_block_open = FALSE;
  int n;
  mrb_value result = mrb_nil_value();

  parser = mrb_parser_new(mrb);
  parser->s = src;
  parser->send = src + strlen(src);
  parser->lineno = 1;
  mrb_parser_parse(parser, s_cxt);
  code_block_open = is_code_block_open(parser);

  if (!code_block_open) {
    if (parser->nerr > 0) {
      /* Same behavior as load_exec() in src/parse.y */
      char buf[256];

      n = snprintf(buf, sizeof(buf), "line %d: %s\n",
                   parser->error_buffer[0].lineno,
                   parser->error_buffer[0].message);
      mrb->exc = mrb_obj_ptr(mrb_exc_new(mrb, E_SYNTAX_ERROR, buf, n));
    } else {
      n = mrb_generate_code(mrb, parser);
      result = mrb_run(mrb,
                       mrb_proc_new(mrb, mrb->irep[n]),
                       mrb_top_self(mrb));
    }
  }
  mrb_parser_free(parser);

  return check_and_print_result(mrb, result, code_block_open, print_level);
}

void
mrb_webruby_multiline_parse_gem_init(mrb_state* mrb)
{
  s_cxt = mrbc_context_new(mrb);
  s_cxt->capture_errors = 1;
}

void mrb_webruby_multiline_parse_gem_final(mrb_state* mrb)
{
  mrbc_context_free(mrb, s_cxt);
}
