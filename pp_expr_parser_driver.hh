#ifndef PP_EXPR_PARSER_DRIVER_HH
#define PP_EXPR_PARSER_DRIVER_HH

#include <functional>
#include "pp_expr_parser.hh"
#include "pp_tokens.hh"
#include "code_remarks.hh"

namespace klp
{
  namespace ccp
  {
    namespace yy
    {
      class pp_expr_parser_driver
      {
      public:
	pp_expr_parser_driver(const std::function<pp_token()> &token_reader,
			      const pp_tracking &pp_tracking);

	~pp_expr_parser_driver() noexcept;

	void parse();

	code_remarks& get_remarks() noexcept
	{ return _remarks; }

	ast::ast_pp_expr grab_result();

      private:
	klp::ccp::yy::pp_expr_parser::token_type
	lex(pp_expr_parser::semantic_type *value,
	    pp_expr_parser::location_type *loc);

	void error(const pp_expr_parser::location_type& loc,
		   const std::string& msg);

	std::function<pp_token()> _token_reader;
	pp_tokens _tokens;
	ast::expr *_result;

	pp_expr_parser _parser;

	code_remarks _remarks;

	friend class pp_expr_parser;
      };
    }
  }
}

#endif
