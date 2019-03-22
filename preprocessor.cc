#include <cassert>
#include <functional>
#include "preprocessor.hh"
#include "architecture.hh"
#include "pp_except.hh"
#include "parse_except.hh"
#include "semantic_except.hh"
#include "macro_undef.hh"
#include "inclusion_tree.hh"
#include "path.hh"
#include "pp_expr_parser_driver.hh"

using namespace klp::ccp;

preprocessor::preprocessor(header_inclusion_roots_type &header_inclusion_roots,
			   const header_resolver &header_resolver,
			   const architecture &arch)
  : _header_resolver(header_resolver), _arch(arch),
    _tracking(new pp_tracking()),
    _header_inclusion_roots(header_inclusion_roots),
    _cur_header_inclusion_root(_header_inclusion_roots.begin()),
    _cond_incl_nesting(0), _root_expansion_state(), __counter__(0),
    _eof_tok(pp_token::type::empty, "", file_range()),
    _maybe_pp_directive(true), _line_empty(true)
{
  assert(!header_inclusion_roots.empty());
  _tokenizers.emplace(**_cur_header_inclusion_root);
  _inclusions.emplace(std::ref(**_cur_header_inclusion_root));
}

pp_token preprocessor::read_next_token()
{
  auto &&_pp_token_to_pp_token =
    [](_pp_token &&tok) {
      return pp_token{tok.get_type(), tok.get_value(),
		      tok.get_file_range(),
		      std::move(tok.used_macros()),
		      std::move(tok.used_macro_undefs())};
    };

  // Basically, this is just a wrapper around _expand()
  // which removes or adds whitespace as needed:
  // - Whitespace at the end of lines gets removed.
  // - Multiple whitespace tokens in a sequence of whitespace
  //   and empty tokens get converted into empties.
  // - Empty lines are removed.
  // - Whitespace gets inserted inbetween certain kind of tokens
  //   in order to guarantee idempotency of preprocessing.
  //
  // A queue of lookahead tokens is maintained in ::_pending_tokens.
  //
  // The following patters are possible for _pending_tokens at each
  // invocation:
  // - a single whitespace + zero or more empties,
  // - zero or more empties + end of line or eof,
  // - one or more empties + whitespace or
  // - zero or more empties + (added) whitespace + something else.
  if (!_pending_tokens.empty() &&
      (_pending_tokens.front().is_empty() ||
       _pending_tokens.front().is_newline() ||
       _pending_tokens.front().is_eof() ||
       (_pending_tokens.size() > 1 &&
	_pending_tokens.front().is_ws() &&
	!_pending_tokens.back().is_empty()))) {
    auto tok = std::move(_pending_tokens.front());
    _pending_tokens.pop();
    if (tok.is_newline()) {
      assert(_pending_tokens.empty());
      if (!_line_empty) {
	_line_empty = true;
	return _pp_token_to_pp_token(std::move(tok));
      }
    } else {
      return _pp_token_to_pp_token(std::move(tok));
    }
  }

 read_next:
  auto next_tok = _expand(_root_expansion_state,
			std::bind(&preprocessor::_read_next_plain_token, this));
  next_tok.expansion_history().clear();

  if (!_pending_tokens.empty() && _pending_tokens.front().is_ws()) {
    if (next_tok.is_newline() || next_tok.is_eof()) {
      // Queued whitespace at end of line. Turn it into an empty.
      _pending_tokens.front().set_type_and_value(pp_token::type::empty, "");
      _pending_tokens.push(std::move(next_tok));
      next_tok = std::move(_pending_tokens.front());
      _pending_tokens.pop();
      return _pp_token_to_pp_token(std::move(next_tok));

    } else if (next_tok.is_empty()) {
      _pending_tokens.push(std::move(next_tok));
      goto read_next;

    } else if (next_tok.is_ws()) {
      // Sequence of multipe whitespace tokens, possibly intermixed
      // with empties. Turn the current whitespace token into an empty.
      next_tok.set_type_and_value(pp_token::type::empty, "");
      _pending_tokens.push(std::move(next_tok));
      goto read_next;
    }

    // Something which is not an EOF, newline, whitespace or empty
    // token. Emit the pending whitespace and queue the current
    // token.
    _line_empty = false;
    _pending_tokens.push(std::move(next_tok));
    next_tok = std::move(_pending_tokens.front());
    _pending_tokens.pop();
    return _pp_token_to_pp_token(std::move(next_tok));

  }

  assert(_pending_tokens.empty() ||
	 (_pending_tokens.size() == 1 && !_pending_tokens.front().is_ws() &&
	  !_pending_tokens.front().is_newline() &&
	  !_pending_tokens.front().is_empty() &&
	  !_pending_tokens.front().is_eof()));
  // There are certain token combinations where GNU cpp emits an extra
  // space in order to make preprocessing idempotent:
  // pp_number {-, +, --, ++, -=, +=, ., id, pp_number}
  // . pp_number
  // id {id, pp_number)
  // + {+, ++, =}, - {-, --, =, >}, ! =, * =, / =, % =, ^ =,
  // < {<, =, <=, <<=, :, %}, << =, > {>, =, >=, >>=}, >> =,
  // & {=, &}, | {=, |},
  // % {=, >, :},  %: %:, : >,
  // : :, . ., # # if any of these is coming from macro expansion
  //                or they're separated by any empties
  _pp_token prev_tok(pp_token::type::eof, std::string(), file_range());
  if (_pending_tokens.empty()) {
    if (next_tok.is_ws()) {
      _pending_tokens.push(std::move(next_tok));
      goto read_next;
    } else if (next_tok.is_newline()) {
      if (_line_empty)
	goto read_next;
      _line_empty = true;
      return _pp_token_to_pp_token(std::move(next_tok));
    } else if (next_tok.is_empty() || next_tok.is_eof()) {
      return _pp_token_to_pp_token(std::move(next_tok));
    } else {
      prev_tok = std::move(next_tok);
      next_tok = _expand(_root_expansion_state,
			std::bind(&preprocessor::_read_next_plain_token, this));
    }
  } else {
    assert(_pending_tokens.size() == 1);
    prev_tok = std::move(_pending_tokens.front());
    _pending_tokens.pop();
  }

  assert (!prev_tok.is_empty() && !prev_tok.is_newline() &&
	  !prev_tok.is_ws() && !prev_tok.is_eof());
  _line_empty = false;
  while (next_tok.is_empty()) {
    _pending_tokens.push(std::move(next_tok));
    next_tok = _expand(_root_expansion_state,
		       std::bind(&preprocessor::_read_next_plain_token, this));
  }

  if (next_tok.is_ws() || next_tok.is_newline() || next_tok.is_eof()) {
    _pending_tokens.push(std::move(next_tok));
    return _pp_token_to_pp_token(std::move(prev_tok));
  }

  if ((prev_tok.is_type_any_of<pp_token::type::pp_number>() &&
       (next_tok.is_punctuator("-") || next_tok.is_punctuator("+") ||
	next_tok.is_punctuator("--") || next_tok.is_punctuator("++") ||
	next_tok.is_punctuator("-=") || next_tok.is_punctuator("+=") ||
	next_tok.is_punctuator(".") || next_tok.is_id() ||
	next_tok.is_type_any_of<pp_token::type::pp_number>())) ||
      (prev_tok.is_punctuator(".") &&
       next_tok.is_type_any_of<pp_token::type::pp_number>()) ||
      (prev_tok.is_id() &&
       (next_tok.is_id() ||
	next_tok.is_type_any_of<pp_token::type::pp_number>())) ||
      (prev_tok.is_type_any_of<pp_token::type::punctuator>() &&
       next_tok.is_type_any_of<pp_token::type::punctuator>() &&
       ((prev_tok.is_punctuator("+") &&
	 (next_tok.is_punctuator("+") || next_tok.is_punctuator("++") ||
	  next_tok.is_punctuator("="))) ||
	(prev_tok.is_punctuator("-") &&
	 (next_tok.is_punctuator("-") || next_tok.is_punctuator("--") ||
	  next_tok.is_punctuator("=") || next_tok.is_punctuator(">"))) ||
	(prev_tok.is_punctuator("!") && next_tok.is_punctuator("=")) ||
	(prev_tok.is_punctuator("*") && next_tok.is_punctuator("=")) ||
	(prev_tok.is_punctuator("/") && next_tok.is_punctuator("=")) ||
	(prev_tok.is_punctuator("%") && next_tok.is_punctuator("=")) ||
	(prev_tok.is_punctuator("^") && next_tok.is_punctuator("=")) ||
	(prev_tok.is_punctuator("<") &&
	 (next_tok.is_punctuator("<") || next_tok.is_punctuator("=") ||
	  next_tok.is_punctuator("<=") || next_tok.is_punctuator("<<=") ||
	  next_tok.is_punctuator(":") || next_tok.is_punctuator("%"))) ||
	(prev_tok.is_punctuator("<<") && next_tok.is_punctuator("=")) ||
	(prev_tok.is_punctuator(">") &&
	 (next_tok.is_punctuator(">") || next_tok.is_punctuator("=") ||
	  next_tok.is_punctuator(">=") || next_tok.is_punctuator(">>="))) ||
	(prev_tok.is_punctuator(">>") && next_tok.is_punctuator("=")) ||
	(prev_tok.is_punctuator("&") &&
	 (next_tok.is_punctuator("=") || next_tok.is_punctuator("&"))) ||
	(prev_tok.is_punctuator("|") &&
	 (next_tok.is_punctuator("=") || next_tok.is_punctuator("|"))) ||
	(prev_tok.is_punctuator("%") &&
	 (next_tok.is_punctuator("=") || next_tok.is_punctuator(">") ||
	  next_tok.is_punctuator(":"))) ||
	(prev_tok.is_punctuator("%:") && next_tok.is_punctuator("%:")) ||
	(prev_tok.is_punctuator(":") && next_tok.is_punctuator(">")) ||
	(((prev_tok.is_punctuator(":") && next_tok.is_punctuator(":")) ||
	  (prev_tok.is_punctuator(".") && next_tok.is_punctuator(".")) ||
	  (prev_tok.is_punctuator("#") && next_tok.is_punctuator("#"))) &&
	 (!prev_tok.used_macros().empty() || !next_tok.used_macros().empty() ||
	  !_pending_tokens.empty()))))) {
    _pending_tokens.emplace(pp_token::type::ws, " ",
	file_range(next_tok.get_file_range().get_header_inclusion_node(),
		   next_tok.get_file_range().get_start_loc()));
  }

  _pending_tokens.push(std::move(next_tok));
  return _pp_token_to_pp_token(std::move(prev_tok));
}

template<typename T>
void preprocessor::_grab_remarks_from(T &from)
{
  _remarks += from.get_remarks();
  from.get_remarks().clear();
}

preprocessor::_pp_token preprocessor::_read_next_plain_token()
{
 again:
  // Has the final eof token been seen?
  if (_eof_tok.get_type() != pp_token::type::empty)
    return _eof_tok;
  assert(!_tokenizers.empty());
  try {
    auto raw_tok = _tokenizers.top().read_next_token();
    _grab_remarks_from(_tokenizers.top());

    if (raw_tok.is_eof()) {
      _handle_eof_from_tokenizer(std::move(raw_tok));
      _maybe_pp_directive = true;
      goto again;
    }

    _tracking->_append_token(raw_tok);

    if (raw_tok.is_newline()) {
      _maybe_pp_directive = true;
    } else if (_maybe_pp_directive && raw_tok.is_punctuator("#")) {
      _handle_pp_directive(std::move(raw_tok));
      goto again;
    } else if (!raw_tok.is_ws()) {
      _maybe_pp_directive = false;
    }

    if (_cond_incl_inactive())
      goto again;

    return _pp_token{raw_tok.get_type(), raw_tok.get_value(),
		     raw_tok.get_file_range()};
  } catch (const pp_except&) {
    _grab_remarks_from(_tokenizers.top());
    throw;
  }
}

void preprocessor::_handle_eof_from_tokenizer(raw_pp_token &&eof_tok)
{
  // If the topmost inclusion tree node is an (unfinished) conditional
  // inclusion, that's an error.
  if (_cur_incl_node_is_cond()) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "missing #endif at end of file",
			   eof_tok.get_file_range());
    _remarks.add(remark);
    throw pp_except(remark);
  }
  _tokenizers.pop();
  _inclusions.pop();

  if (_tokenizers.empty()) {
    if (++_cur_header_inclusion_root == _header_inclusion_roots.end()) {
      _eof_tok = _pp_token{eof_tok.get_type(), eof_tok.get_value(),
			   eof_tok.get_file_range()};
      return;
    }

    _tokenizers.emplace(**_cur_header_inclusion_root);
    _inclusions.emplace(std::ref(**_cur_header_inclusion_root));
  }
}

void preprocessor::_handle_pp_directive(raw_pp_token &&sharp_tok)
{
  raw_pp_tokens directive_toks{1U, std::move(sharp_tok)};
  bool endif_possible = true;
  bool is_endif = false;
  while (true) {
    assert(!_tokenizers.empty());
    auto tok = _tokenizers.top().read_next_token();
    _grab_remarks_from(_tokenizers.top());

    if (tok.is_eof()) {
      directive_toks.push_back(tok);
      _handle_eof_from_tokenizer(std::move(tok));
      break;
    }

    _tracking->_append_token(tok);

    if (tok.is_newline()) {
      directive_toks.push_back(tok);
      break;

    } else if (endif_possible && tok.get_type() == pp_token::type::id &&
	       tok.get_value() == "endif") {
      // Be careful not to read until the end of line or end of file
      // for #endif directives: _handle_eof_from_tokenizer() will
      // throw an exception if there's some conditional inclusion
      // pending.
      is_endif = true;
      endif_possible = false;

      if (!_cur_incl_node_is_cond()) {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "#endif without #if",
			       tok.get_file_range());
	_remarks.add(remark);
	throw pp_except(remark);
      }

      if (_cond_incl_nesting == _cond_incl_states.size())
	_pop_cond_incl(directive_toks.back().get_file_range().get_end_loc());
      --_cond_incl_nesting;

    } else if (!tok.is_ws()) {
      endif_possible = false;
      if (is_endif) {
	code_remark_raw remark(code_remark_raw::severity::warning,
			       "garbage after #endif",
			       tok.get_file_range());
	_remarks.add(remark);
      }
    }

    directive_toks.push_back(std::move(tok));
  }

  if (is_endif)
    return;

  auto it_tok = directive_toks.cbegin() + 1;
  assert(it_tok != directive_toks.cend());

  if (it_tok->is_ws())
    ++it_tok;

  if (it_tok->is_newline() || it_tok->is_eof()) {
    // Null directive
    return;
  }

  assert(!it_tok->is_ws());
  if (!it_tok->is_id()) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "identifier expected in preprocessor directive",
			   it_tok->get_file_range());
    _remarks.add(remark);
    throw pp_except(remark);
  }


  // First, process conditional inclusion directives: those
  // must be looked at even if within a non-taken branch of
  // another conditional inclusion.
  if (it_tok->get_value() == "if") {
    ++_cond_incl_nesting;
    if (_cond_incl_inactive())
      return;

    _cond_incl_states.emplace
      (directive_toks.begin()->get_file_range().get_start_loc());

    if (_eval_conditional_inclusion(std::move(directive_toks)))
      _enter_cond_incl();

  } else if (it_tok->get_value() == "ifdef") {
    ++_cond_incl_nesting;
    if (_cond_incl_inactive())
      return;

    ++it_tok;
    if (it_tok->is_ws())
      ++it_tok;

    if (!it_tok->is_id()) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "identifier expected after #ifdef",
			     it_tok->get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    _cond_incl_states.emplace
      (directive_toks.begin()->get_file_range().get_start_loc());

    auto it_m = _macros.find(it_tok->get_value());
    if (it_m != _macros.end()) {
      _cond_incl_states.top().um += it_m->second;
      _enter_cond_incl();
    } else {
      auto it_m_undef = _macro_undefs.find(it_tok->get_value());
      if (it_m_undef != _macro_undefs.end())
	_cond_incl_states.top().umu += it_m_undef->second;
    }

  } else if (it_tok->get_value() == "ifndef") {
    ++_cond_incl_nesting;
    if (_cond_incl_inactive())
      return;

    ++it_tok;
    if (it_tok->is_ws())
      ++it_tok;

    if (!it_tok->is_id()) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "identifier expected after #ifndef",
			     it_tok->get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    _cond_incl_states.emplace
      (directive_toks.begin()->get_file_range().get_start_loc());

    auto it_m = _macros.find(it_tok->get_value());
    if (it_m != _macros.end()) {
      _cond_incl_states.top().um += it_m->second;
    } else {
      auto it_m_undef = _macro_undefs.find(it_tok->get_value());
      if (it_m_undef != _macro_undefs.end())
	_cond_incl_states.top().umu += it_m_undef->second;
      _enter_cond_incl();
    }

  } else if (it_tok->get_value() == "elif") {
    if (!_cur_incl_node_is_cond()) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "#elif without #if",
			     it_tok->get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    if (_cond_incl_nesting > _cond_incl_states.size())
      return;

    if (!_cond_incl_states.top().incl_node) {
      if (_eval_conditional_inclusion(std::move(directive_toks)))
	_enter_cond_incl();
    } else {
      _cond_incl_states.top().branch_active = false;
    }

  }

  // Look at the other directives only if they're not within a
  // non-taken conditional inclusion branch.
  if (_cond_incl_inactive())
    return;

  if (it_tok->get_value() == "define") {
    ++it_tok;
    if (it_tok->is_ws())
      ++it_tok;

    if (!it_tok->is_id()) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "identifier expected after #define",
			     it_tok->get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    auto it_m_undef = _macro_undefs.find(it_tok->get_value());
    std::shared_ptr<const macro_undef> m_undef;
    if (it_m_undef != _macro_undefs.end()) {
      m_undef = std::move(it_m_undef->second);
      _macro_undefs.erase(it_m_undef);
    }

    std::shared_ptr<const macro> m =
      macro::parse_macro_definition(directive_toks.cbegin(),
				    directive_toks.cend(),
				    std::move(m_undef),
				    _remarks);

    auto it_existing = _macros.find(m->get_name());
    if (it_existing != _macros.end() && *it_existing->second != *m) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "macro redefined in an incompatible way",
			     m->get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    _macros.insert(std::make_pair(m->get_name(), std::move(m)));
  } else if (it_tok->get_value() == "undef") {
    ++it_tok;
    if (it_tok->is_ws())
      ++it_tok;

    if (!it_tok->is_id()) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "identifier expected after #undef",
			     it_tok->get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    auto it_tok_id = it_tok;
    ++it_tok;
    if (!it_tok->is_newline() && !it_tok->is_eof()) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "garbage after #undef",
			     it_tok->get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    auto it_m = _macros.find(it_tok_id->get_value());
    if (it_m != _macros.end()) {
      const file_range &range_start = directive_toks.begin()->get_file_range();
      const file_range &range_end = directive_toks.rbegin()->get_file_range();

      std::shared_ptr<const macro_undef> mu =
	macro_undef::create(std::move(it_m->second),
			    file_range(range_start, range_end));
      _macro_undefs.insert(std::make_pair(it_tok_id->get_value(),
					  std::move(mu)));
      _macros.erase(it_m);
    }

  } else if (it_tok->get_value() == "include") {
    _handle_include(std::move(directive_toks));

  }
}

preprocessor::_pp_token
preprocessor::_expand(_expansion_state &state,
		      const std::function<_pp_token()> &token_reader,
		      const bool from_cond_incl_cond)
{
  auto read_tok = [&]() {
    while (!state.macro_instances.empty()) {
      try {
	auto tok = state.macro_instances.back().read_next_token();
	_grab_remarks_from(state.macro_instances.back());
	if (!tok.is_eof()) {
	  return tok;
	} else {
	  state.macro_instances.pop_back();
	}
      } catch (const pp_except&) {
	_grab_remarks_from(state.macro_instances.back());
	throw;
      }
    }

    return token_reader();
  };

 read_next:
  _pp_token tok = (state.pending_tokens.empty()
		   ? read_tok()
		   : std::move(state.pending_tokens.front()));
  if (!state.pending_tokens.empty())
    state.pending_tokens.pop();

  if (tok.is_eof())
    return tok;

  // Deal with possible macro invocation
  if (tok.is_id()) {
    // Check for the predefined macros first.
    if (tok.get_value() == "__FILE__") {
      return _pp_token(pp_token::type::str,
		tok.get_file_range().get_header_inclusion_node().get_filename(),
		tok.get_file_range(), used_macros(),
		std::move(tok.used_macros()), tok.used_macro_undefs());
    } else if (tok.get_value() == "__LINE__") {
      auto line = tok.get_file_range().get_start_line();
      return _pp_token(pp_token::type::pp_number, std::to_string(line),
		      tok.get_file_range(), used_macros(),
		      std::move(tok.used_macros()), tok.used_macro_undefs());
    } else if (tok.get_value() == "__INCLUDE_LEVEL__") {
      return _pp_token(pp_token::type::pp_number,
		      std::to_string(_tokenizers.size() - 1),
		      tok.get_file_range(), used_macros(),
		      std::move(tok.used_macros()), tok.used_macro_undefs());
    } else if (tok.get_value() == "__COUNTER__") {
      return _pp_token(pp_token::type::pp_number, std::to_string(__counter__++),
		      tok.get_file_range(), used_macros(),
		      std::move(tok.used_macros()), tok.used_macro_undefs());
    }

    auto m = _macros.find(tok.get_value());
    if (m != _macros.end() && !tok.expansion_history().count(m->second)) {
      if (!m->second->is_func_like()) {
	state.macro_instances.push_back(
		_handle_object_macro_invocation(m->second, std::move(tok)));
	goto read_next;
      } else {
	_pp_token next_tok = read_tok();
	while (next_tok.is_ws() || next_tok.is_newline() ||
	       next_tok.is_empty()) {
	  state.pending_tokens.push(std::move(next_tok));
	  next_tok = read_tok();
	}

	// Not a macro invocation?
	if (!next_tok.is_punctuator("(")) {
	  state.pending_tokens.push(std::move(next_tok));
	  return tok;
	}

	used_macros base_um(std::move(tok.used_macros()));
	used_macro_undefs base_umu(std::move(tok.used_macro_undefs()));
	while (!state.pending_tokens.empty()) {
	  base_um += std::move(state.pending_tokens.front().used_macros());
	  base_umu +=
	    std::move(state.pending_tokens.front().used_macro_undefs());
	  state.pending_tokens.pop();
	}

	base_um += next_tok.used_macros();
	base_umu += next_tok.used_macro_undefs();
	state.macro_instances.push_back(_handle_func_macro_invocation(
						m->second, std::move(base_um),
						std::move(base_umu),
						tok.get_file_range(),
						read_tok));
	}
	goto read_next;
    } else if (m == _macros.end()) {
      auto it_m_undef = _macro_undefs.find(tok.get_value());
      if (it_m_undef != _macro_undefs.end())
	tok.used_macro_undefs() += it_m_undef->second;
    }

    if (from_cond_incl_cond && tok.get_value() == "defined") {
      file_range invocation_range = tok.get_file_range();
      used_macros um(std::move(tok.used_macros()));
      used_macro_undefs umu(std::move(tok.used_macro_undefs()));
      do {
	tok = read_tok();
	um += tok.used_macros();
	umu += tok.used_macro_undefs();
      } while (tok.is_ws() || tok.is_newline() || tok.is_empty());

      // No opening parenthesis?
      if (!tok.is_punctuator("(")) {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "operator \"defined\" without arguments",
			       invocation_range);
	_remarks.add(remark);
	throw pp_except(remark);
      }

      std::tuple<_pp_tokens, _pp_tokens, _pp_token> arg
	= _create_macro_arg(read_tok, false, false, um, umu);
      const char arg_delim = std::get<2>(arg).get_value()[0];
      if (arg_delim == ',') {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "too many arguments to \"defined\" operator",
			       invocation_range);
	_remarks.add(remark);
	throw pp_except(remark);
      }
      assert(arg_delim == ')');

      if (std::get<0>(arg).size() != 1 ||
	  std::get<0>(arg)[0].get_type() != pp_token::type::id) {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "invalid argument to \"defined\" operator",
			       invocation_range);
	_remarks.add(remark);
	throw pp_except(remark);
      }

      const std::string &id = std::get<0>(arg)[0].get_value();
      bool is_defined = false;
      auto it_m = _macros.find(id);
      if (it_m != _macros.end()) {
	um += it_m->second;
	is_defined = true;
      } else {
	auto it_m_undef = _macro_undefs.find(id);
	if (it_m_undef != _macro_undefs.end())
	  umu += it_m_undef->second;
      }

      return _pp_token(pp_token::type::pp_number,
		       is_defined ? "1" : "0",
		       file_range(invocation_range,
				  std::get<2>(arg).get_file_range()),
		       used_macros{}, std::move(um), std::move(umu));
    }
  }

  return tok;
}


preprocessor::_pp_token::_pp_token(const pp_token::type type,
				   const std::string &value,
				   const file_range &file_range,
				   const class used_macros &eh,
				   class used_macros &&um,
				   const class used_macro_undefs &umu)
  : _value(value), _file_range(file_range),
    _expansion_history(std::move(eh)), _used_macros(std::move(um)),
    _used_macro_undefs(umu), _type(type)
{}

preprocessor::_pp_token::_pp_token(const pp_token::type type,
				   const std::string &value,
				   const file_range &file_range)
  : _value(value), _file_range(file_range), _type(type)
{}

void preprocessor::_pp_token::set_type_and_value(const pp_token::type type,
						 const std::string &value)
{
  _type = type;
  _value = value;
}

std::string preprocessor::_pp_token::stringify() const
{
  return pp_token::stringify(_type, _value);
}

void preprocessor::_pp_token::concat(const _pp_token &tok,
				     code_remarks &remarks)
{
  assert(_used_macros.empty());
  assert(_used_macro_undefs.empty());
  assert(tok._used_macros.empty());
  assert(tok._used_macro_undefs.empty());

  assert(_type != pp_token::type::ws);
  assert(tok._type != pp_token::type::ws);
  assert(_type != pp_token::type::newline);
  assert(tok._type != pp_token::type::newline);
  assert(_type != pp_token::type::eof);
  assert(tok._type != pp_token::type::eof);
  assert(_type != pp_token::type::qstr);
  assert(tok._type != pp_token::type::qstr);
  assert(_type != pp_token::type::hstr);
  assert(tok._type != pp_token::type::hstr);

  _expansion_history += tok._expansion_history;
  if (_type == pp_token::type::empty) {
    _type = tok._type;
    _value = tok._value;
    return;
  } else if (tok._type == pp_token::type::empty) {
    return;
  }

  if (_type == pp_token::type::id && tok._type == pp_token::type::pp_number) {
    if (!std::all_of(tok._value.cbegin(), tok._value.cend(),
		     [](const char c) {
		       return (c == '_' || ('a' <= c && c <= 'z') ||
			       ('A' <= c && c <= 'Z') ||
			       ('0' <= c && c <= '9'));
		     })) {
      code_remark_raw remark
	(code_remark_raw::severity::fatal,
	 "can't concatenate " + _value + " and " + tok._value,
	 tok.get_file_range());
      remarks.add(remark);
      throw pp_except(remark);
    }

    _value += tok._value;
    return;
  } else if (_type == pp_token::type::pp_number &&
	     tok._type == pp_token::type::id) {
    _value += tok._value;
    return;
  } else if (_type == pp_token::type::pp_number &&
	     tok._type == pp_token::type::punctuator &&
     (tok._value == "-" || tok._value == "+")) {
    _value += tok._value;
    return;
  }

  if (_type != tok._type) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "can't concatenate tokens of different type",
			   tok.get_file_range());
    remarks.add(remark);
    throw pp_except(remark);
  } else if(_type == pp_token::type::chr || _type == pp_token::type::wchr) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "can't concatenate character constants",
			   tok.get_file_range());
    remarks.add(remark);
    throw pp_except(remark);
  } else if (_type == pp_token::type::non_ws_char) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "can't concatenate " + _value + " and " + tok._value,
			   tok.get_file_range());
    remarks.add(remark);
    throw pp_except(remark);
  }

  _value += tok._value;

  if (_type == pp_token::type::punctuator) {
    if (_value != "->" && _value != "++" && _value != "--" && _value != "<<" &&
	_value != ">>" && _value != "<=" && _value != ">=" && _value != "==" &&
	_value != "!=" && _value != "&&" && _value != "||" && _value != "*=" &&
	_value != "/=" && _value != "%=" && _value != "+=" && _value != "-=" &&
	_value != "<<=" && _value != ">>=" && _value != "&=" &&
	_value != "^=" && _value != "|=" && _value != "##" && _value != "<:" &&
	_value != ":>" && _value != "<%" && _value != "%>" && _value != "%:" &&
	_value != "%:%:") {
      code_remark_raw remark
	(code_remark_raw::severity::fatal,
	 "can't concatenate " + _value + " and " + tok._value,
	 tok.get_file_range());
      remarks.add(remark);
      throw pp_except(remark);
    }
    return;
  }
}


preprocessor::_macro_instance
preprocessor::_handle_object_macro_invocation(
				const std::shared_ptr<const macro> &macro,
				_pp_token &&id_tok)
{
  return _macro_instance(macro, std::move(id_tok.used_macros()),
			 std::move(id_tok.used_macro_undefs()),
			 std::vector<_pp_tokens>(), std::vector<_pp_tokens>(),
			 id_tok.get_file_range());
}

preprocessor::_macro_instance
preprocessor::_handle_func_macro_invocation(
	const std::shared_ptr<const macro> &macro,
	used_macros &&used_macros_base,
	used_macro_undefs &&used_macro_undefs_base,
	const file_range &id_tok_file_range,
	const std::function<_pp_token()> &token_reader)
{
  std::vector<_pp_tokens> args;
  std::vector<_pp_tokens> exp_args;
  file_range invocation_range;

  if (macro->non_va_arg_count() || macro->is_variadic()) {
    size_t i_arg = 0;
    const size_t n_args =  macro->non_va_arg_count() + macro->is_variadic();
    args.reserve(n_args);
    exp_args.reserve(n_args);
    while (i_arg < n_args) {
      std::tuple<_pp_tokens, _pp_tokens, _pp_token> cur_arg
	= _create_macro_arg(token_reader, macro->shall_expand_arg(i_arg),
			    i_arg == macro->non_va_arg_count(),
			    used_macros_base, used_macro_undefs_base);
      ++i_arg;
      args.push_back(std::move(std::get<0>(cur_arg)));
      exp_args.push_back(std::move(std::get<1>(cur_arg)));

      assert(std::get<2>(cur_arg).get_type() == pp_token::type::punctuator);
      assert(std::get<2>(cur_arg).get_value().size() == 1);
      const char arg_delim = std::get<2>(cur_arg).get_value()[0];
      if (i_arg < (!macro->is_variadic() ? n_args : n_args - 1)) {
	if (arg_delim != ',') {
	  assert(arg_delim == ')');
	  code_remark_raw remark(code_remark_raw::severity::fatal,
				 "too few parameters in macro invocation",
				 std::get<2>(cur_arg).get_file_range());
	  _remarks.add(remark);
	  throw pp_except(remark);
	}
      } else if (i_arg == n_args - 1) {
	// A variadic argument might simply have been left out.
	assert(macro->is_variadic());
	if (arg_delim == ')') {
	  // Add empty dummy argument.
	  args.emplace_back();
	  exp_args.emplace_back();
	  ++i_arg;
	}
      } else if (i_arg == n_args) {
	if (arg_delim != ')') {
	  assert(arg_delim == ',');
	  code_remark_raw remark(code_remark_raw::severity::fatal,
				 "too many parameters in macro invocation",
				 std::get<2>(cur_arg).get_file_range());
	  _remarks.add(remark);
	  throw pp_except(remark);
	}
      }

      if (i_arg == n_args) {
	assert(arg_delim == ')');
	invocation_range = file_range(id_tok_file_range,
				      std::get<2>(cur_arg).get_file_range());
      }
    }
  } else {
    // Slurp in the tokens between the () and make sure they're all empties.
    std::tuple<_pp_tokens, _pp_tokens, _pp_token> dummy_arg
      = _create_macro_arg(token_reader, false, false,
			  used_macros_base, used_macro_undefs_base);
    assert(std::get<2>(dummy_arg).get_type() == pp_token::type::punctuator);
    bool non_empty_found = false;
    for (auto tok : std::get<0>(dummy_arg)) {
      if (!tok.is_empty() && !tok.is_ws()) {
	non_empty_found = true;
	break;
      }
    }
    if (non_empty_found || std::get<2>(dummy_arg).get_value() != ")") {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "too many parameters in macro invocation",
			     std::get<2>(dummy_arg).get_file_range());
      _remarks.add(remark);
      throw pp_except(remark);
    }
    invocation_range = file_range(id_tok_file_range,
				  std::get<2>(dummy_arg).get_file_range());
  }

  return _macro_instance(macro, std::move(used_macros_base),
			 std::move(used_macro_undefs_base),
			 std::move(args), std::move(exp_args),
			 invocation_range);
}

std::tuple<preprocessor::_pp_tokens, preprocessor::_pp_tokens,
	   preprocessor::_pp_token>
preprocessor::_create_macro_arg(const std::function<_pp_token()> &token_reader,
				const bool expand, const bool variadic,
				used_macros &used_macros_base,
				used_macro_undefs &used_macro_undefs_base)
{
  _pp_tokens arg;
  _pp_tokens exp_arg;
  std::size_t lparens = 0;
  _pp_token end_delim(pp_token::type::eof, std::string(), file_range());

  _pp_token pending_tok(pp_token::type::eof, std::string(), file_range());
  bool pending_tok_valid = false;

  bool leading_ws = true; // Strip leading whitespace.
  auto read_arg_tok = [&]() {
    _pp_token tok =
      [&]() {
	if (!pending_tok_valid || pending_tok.is_ws()) {
	  while (true) {
	    _pp_token tok = token_reader();

	    used_macros_base += tok.used_macros();
	    tok.used_macros().clear();
	    used_macro_undefs_base += tok.used_macro_undefs();
	    tok.used_macro_undefs().clear();

	    if (tok.is_empty())
	      continue;

	    if (tok.is_eof()) {
	      code_remark_raw remark
		(code_remark_raw::severity::fatal,
		 "no closing right parenthesis in macro invocation",
		 tok.get_file_range());
	      _remarks.add(remark);
	      throw pp_except(remark);
	    }

	    // Turn all whitespace into single spaces.
	    if (tok.is_newline()) {
	      tok.set_type_and_value(pp_token::type::ws, " ");
	    } else if (tok.is_ws() && tok.get_value().size() > 1) {
	      tok.set_type_and_value(pp_token::type::ws, " ");
	    }

	    if (pending_tok_valid && tok.is_ws()) {
	      assert(pending_tok.is_ws());
	      // Skip multiple consecutive whitespace.
	      continue;

	    } else if (leading_ws && tok.is_ws()) {
	      // Drop leading whitespace.
	      assert(!pending_tok_valid);
	      continue;

	    } else if (tok.is_ws()) {
	      // Queue pending whitespace and examine next token.
	      assert(!pending_tok_valid);
	      pending_tok = std::move(tok);
	      pending_tok_valid = true;
	      continue;

	    } else if (!lparens &&
		       ((!variadic && tok.is_punctuator(",")) ||
			tok.is_punctuator(")"))) {
	      // End of macro argument. Skip queued whitespace.
	      pending_tok_valid = false;
	      end_delim = std::move(tok);
	      return _pp_token(pp_token::type::eof, std::string(),
			       end_delim.get_file_range());

	    } else if (pending_tok_valid) {
	      // Return queued whitespace and queue next non-whitespace
	      // token.
	      assert(pending_tok.is_ws());
	      std::swap(pending_tok, tok);
	      return tok;

	    } else {
	      // No queued whitespace and current token also isn't
	      // whitespace, return it.
	      leading_ws = false;
	      return tok;
	    }
	  }

	} else {
	  // Return queued non-whitespace token.
	  leading_ws = false;
	  _pp_token tok = std::move(pending_tok);
	  assert(!tok.is_ws() || tok.is_eof());
	  pending_tok_valid = false;
	  return tok;
	}
      }();

    if (tok.is_punctuator("(")) {
      ++lparens;
    } else if (tok.is_punctuator(")")) {
      --lparens;
    }

    if (!tok.is_eof())
      arg.push_back(tok);
    return tok;
  };

  if (!expand) {
    while (read_arg_tok().get_type() != pp_token::type::eof) {}
  } else {
    _expansion_state state;
    while(true) {
      auto tok = _expand(state, read_arg_tok);
      if (tok.is_eof())
	break;
      exp_arg.push_back(std::move(tok));
    }
  }

  return std::tuple<_pp_tokens, _pp_tokens, _pp_token>
		(std::move(arg), std::move(exp_arg), std::move(end_delim));
}

used_macros preprocessor::_collect_used_macros(const _pp_tokens &toks)
{
  used_macros result;

  for (auto tok : toks) {
    result += tok.used_macros();
  }

  return result;
}

used_macro_undefs
preprocessor::_collect_used_macro_undefs(const _pp_tokens &toks)
{
  used_macro_undefs result;

  for (auto tok : toks) {
    result += tok.used_macro_undefs();
  }

  return result;
}

void preprocessor::_handle_include(raw_pp_tokens &&directive_toks)
{
  auto it_raw_tok = directive_toks.cbegin();

  assert(it_raw_tok->is_punctuator("#"));
  ++it_raw_tok;

  if (it_raw_tok->is_ws())
    ++it_raw_tok;

  assert(it_raw_tok->is_id());
  assert(it_raw_tok->get_value() == "include");
  ++it_raw_tok;

  if (it_raw_tok->is_ws())
    ++it_raw_tok;

  assert(it_raw_tok != directive_toks.cend());
  std::string unresolved;
  bool is_qstr;
  used_macros um;
  used_macro_undefs umu;
  if (it_raw_tok->is_type_any_of<pp_token::type::qstr,
				 pp_token::type::hstr>()) {
    unresolved = it_raw_tok->get_value();
    is_qstr = it_raw_tok->get_type() == pp_token::type::qstr;
    ++it_raw_tok;
    if (it_raw_tok->is_ws())
      ++it_raw_tok;

    if (!it_raw_tok->is_newline() && !it_raw_tok->is_eof()) {
      file_range fr (it_raw_tok->get_file_range().get_header_inclusion_node(),
		     it_raw_tok->get_file_range().get_start_loc());
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "garbage after #include", fr);
      _remarks.add(remark);
      throw pp_except(remark);
    }
  } else {
    auto read_tok = [&]() {
      if (it_raw_tok->is_newline() || it_raw_tok->is_eof()) {
	return _pp_token(pp_token::type::eof, std::string(), file_range());
      }

     _pp_token tok{it_raw_tok->get_type(), it_raw_tok->get_value(),
		   it_raw_tok->get_file_range()};
      ++it_raw_tok;
      return tok;
    };

    _pp_tokens expanded;
    _expansion_state state;
    while (true) {
      auto tok = _expand(state, read_tok);
      if (tok.is_eof())
	break;
      expanded.push_back(std::move(tok));
    }

    um = _collect_used_macros(expanded);
    umu = _collect_used_macro_undefs(expanded);

    auto skip = [&expanded] (_pp_tokens::const_iterator it){
      while(it != expanded.cend() &&
	    it->is_type_any_of<pp_token::type::ws,
			       pp_token::type::empty>()) {
	assert(!it->is_newline());
	++it;
      }
      return it;
    };

    auto it_tok = expanded.cbegin();
    it_tok = skip(it_tok);

    if (it_tok == expanded.cend()) {
      auto it = directive_toks.cbegin();
      file_range fr (it->get_file_range().get_header_inclusion_node(),
		     it->get_file_range().get_start_loc());
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "macro expansion at #include evaluated to nothing",
			     fr);
      _remarks.add(remark);
      throw pp_except(remark);
    }

    if (it_tok->is_punctuator("<")) {
      is_qstr = false;

      ++it_tok;
      for (auto it_next = skip(it_tok + 1); it_next != expanded.cend();
	   it_tok = it_next, it_next = skip(it_next + 1)) {
	unresolved += it_tok->stringify();
      }

      assert(it_tok != expanded.cend());
      if (!it_tok->is_punctuator(">")) {
	auto it = directive_toks.cbegin();
	file_range fr (it->get_file_range().get_header_inclusion_node(),
		       it->get_file_range().get_start_loc());
	code_remark_raw remark
	  (code_remark_raw::severity::fatal,
	   "macro expansion at #include does not end with '>'",
	   fr);
	_remarks.add(remark);
	throw pp_except(remark);
      } else if (unresolved.empty()) {
	auto it = directive_toks.cbegin();
	file_range fr (it->get_file_range().get_header_inclusion_node(),
		       it->get_file_range().get_start_loc());
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "macro expansion at #include gives \"<>\"",
			       fr);
	_remarks.add(remark);
	throw pp_except(remark);
      }
    } else if (it_tok->get_type() == pp_token::type::str) {
      is_qstr = true;
      unresolved = it_tok->get_value();

      it_tok = skip(it_tok + 1);
      if (it_tok != expanded.cend()) {
	auto it = directive_toks.cbegin();
	file_range fr (it->get_file_range().get_header_inclusion_node(),
		       it->get_file_range().get_start_loc());
	code_remark_raw remark
	  (code_remark_raw::severity::fatal,
	   "macro expansion at #include yields garbage at end",
	   fr);
	_remarks.add(remark);
	throw pp_except(remark);
      }
    } else {
      auto it = directive_toks.cbegin();
      file_range fr (it->get_file_range().get_header_inclusion_node(),
		     it->get_file_range().get_start_loc());
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "macro expansion at #include doesn't conform",
			     fr);
      _remarks.add(remark);
      throw pp_except(remark);
    }
  }

  std::string resolved
    = (is_qstr ?
       _header_resolver.resolve(unresolved,
				(_tokenizers.top().get_header_inclusion_node()
				 .get_filename())) :
       _header_resolver.resolve(unresolved));

  if (resolved.empty()) {
    auto it = directive_toks.cbegin();
    file_range fr (it->get_file_range().get_header_inclusion_node(),
		   it->get_file_range().get_start_loc());
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "could not find header from #include",
			   fr);
    _remarks.add(remark);
    throw pp_except(remark);
  }

  file_range fr (directive_toks.cbegin()->get_file_range(),
		 (directive_toks.cend() - 1)->get_file_range());

  header_inclusion_child &new_header_inclusion_node
    = _inclusions.top().get().add_header_inclusion(resolved, std::move(fr),
						   std::move(um),
						   std::move(umu));
  _tokenizers.emplace(new_header_inclusion_node);
  _inclusions.emplace(std::ref(new_header_inclusion_node));
}

bool preprocessor::_cond_incl_inactive() const noexcept
{
  return !_cond_incl_states.empty() && !_cond_incl_states.top().branch_active;
}

bool preprocessor::_cur_incl_node_is_cond() const noexcept
{
  if (_cond_incl_states.empty())
    return false;

  // Is there some conditional inclusion pending from which neither any
  // of its branches has evaluated to true yet?
  if (!_cond_incl_states.top().incl_node)
    return true;

  return &_inclusions.top().get() == _cond_incl_states.top().incl_node;
}

void preprocessor::_enter_cond_incl()
{
  _cond_incl_state &cond_incl_state = _cond_incl_states.top();
  assert(!cond_incl_state.incl_node);

  conditional_inclusion_node &new_conditional_inclusion_node =
    (_inclusions.top().get().add_conditional_inclusion
     (cond_incl_state.start_loc, std::move(cond_incl_state.um),
      std::move(cond_incl_state.umu)));

  cond_incl_state.incl_node = &new_conditional_inclusion_node;
  cond_incl_state.branch_active = true;
  _inclusions.emplace(std::ref(new_conditional_inclusion_node));
}

void preprocessor::_pop_cond_incl(const file_range::loc_type end_loc)
{
  _cond_incl_state &cond_incl_state = _cond_incl_states.top();

  if (!cond_incl_state.incl_node) {
    // None of the branches had been taken. Insert the conditional inclusion
    // into the inclusion tree now.
    conditional_inclusion_node &node
      = (_inclusions.top().get().add_conditional_inclusion
	 (cond_incl_state.start_loc, std::move(cond_incl_state.um),
	  std::move(cond_incl_state.umu)));
    node.set_end_loc(end_loc);

  } else {
    cond_incl_state.incl_node->set_end_loc(end_loc);
    assert(cond_incl_state.incl_node == &_inclusions.top().get());
    _inclusions.pop();
  }

  _cond_incl_states.pop();
}

bool preprocessor::_eval_conditional_inclusion(raw_pp_tokens &&directive_toks)
{
  auto it = directive_toks.begin() + 1;
  if (it->is_ws())
    ++it;
  assert(it->get_type() == pp_token::type::id &&
	 (it->get_value() == "if" || it->get_value() == "elif"));
  ++it;

  file_range eof_file_range
    (directive_toks.back().get_file_range().get_inclusion_node(),
     directive_toks.back().get_file_range().get_end_loc());

  auto read_tok =
    [&]() {
      if (it == directive_toks.end())
	return _pp_token(pp_token::type::eof, std::string(), eof_file_range);

      _pp_token tok{it->get_type(), it->get_value(), it->get_file_range()};
      ++it;
      return tok;
    };

  _expansion_state state;
  auto exp_read_tok =
    [&]() -> pp_token {
      auto tok = _expand(state, read_tok, true);

      return pp_token{tok.get_type(), tok.get_value(), tok.get_file_range(),
		      std::move(tok.used_macros()), tok.used_macro_undefs()};
    };

  yy::pp_expr_parser_driver pd(exp_read_tok);
  try {
    pd.parse();
  } catch (const pp_except&) {
    _remarks += pd.get_remarks();
    pd.get_remarks().clear();
    throw;
  } catch (const parse_except&) {
    _remarks += pd.get_remarks();
    pd.get_remarks().clear();
    throw;
  }

  ast::ast_pp_expr a = pd.grab_result();
  bool result;
  try {
    result = a.evaluate(_arch);
  } catch (const semantic_except&) {
    _remarks += a.get_remarks();
    a.get_remarks().clear();
    throw;
  }
  _remarks += a.get_remarks();
  a.get_remarks().clear();

  return result;
}


preprocessor::_macro_instance::
_macro_instance(const std::shared_ptr<const macro> &macro,
		used_macros &&used_macros_base,
		used_macro_undefs &&used_macro_undefs_base,
		std::vector<_pp_tokens> &&args,
		std::vector<_pp_tokens> &&exp_args,
		const file_range &file_range)
  : _macro(macro),
    _used_macros_base(std::move(used_macros_base)),
    _used_macro_undefs_base(std::move(used_macro_undefs_base)),
    _file_range(file_range),
    _it_repl(_macro->get_repl().cbegin()), _cur_arg(nullptr),
    _in_concat(false), _anything_emitted(false)
{
  assert(args.size() == exp_args.size());

  auto arg_it = args.begin();
  auto exp_arg_it = exp_args.begin();
  size_t i = 0;
  for (auto arg_name : _macro->get_arg_names()) {
    assert(_macro->shall_expand_arg(i) || exp_arg_it->empty());
    ++i;
    _args.insert(std::make_pair(arg_name,
				std::make_pair(std::move(*arg_it++),
					       std::move(*exp_arg_it++))));
  }
  args.clear();
  exp_args.clear();
}

const preprocessor::_pp_tokens* preprocessor::_macro_instance::
_resolve_arg(const std::string &name, const bool expanded) const noexcept
{
  if (!_macro->is_func_like())
    return nullptr;

  auto arg = _args.find(name);
  if (arg == _args.end())
    return nullptr;

  const _pp_tokens *resolved
    = expanded ? &arg->second.second : &arg->second.first;
  return resolved;
}

used_macros preprocessor::_macro_instance::_tok_expansion_history_init() const
{
  used_macros eh;
  eh += _macro;
  return eh;
}

used_macros preprocessor::_macro_instance::_tok_used_macros_init() const
{
  // Anything relevant to the current macro call
  // being even recognized during re-evaluation:
  used_macros result = _used_macros_base;

  // Record the current macro itself
  result += _macro;

  return result;
}

used_macro_undefs preprocessor::_macro_instance::_tok_used_macro_undefs_init()
  const
{
  return _used_macro_undefs_base;
}


bool preprocessor::_macro_instance::
_is_stringification(raw_pp_tokens::const_iterator it) const noexcept
{
  assert(it != _macro->get_repl().end());

  if (!_macro->is_func_like() || !it->is_punctuator("#"))
    return false;

  assert((it + 1) != _macro->get_repl().end() && (it + 1)->is_id());

  return true;
}

raw_pp_tokens::const_iterator preprocessor::_macro_instance::
_skip_stringification_or_single(const raw_pp_tokens::const_iterator &it)
  const noexcept
{
  assert(it != _macro->get_repl().end());

  if (_macro->is_func_like() && it->is_punctuator("#")) {
    assert(it + 1 != _macro->get_repl().end());
    assert((it + 1)->is_id());
    return it + 2;
  }
  return it + 1;
}

bool preprocessor::_macro_instance::
_is_concat_op(const raw_pp_tokens::const_iterator &it) const noexcept
{
  return (it != _macro->get_repl().end() && it->is_punctuator("##"));
}

preprocessor::_pp_token preprocessor::_macro_instance::_handle_stringification()
{
  assert(_it_repl != _macro->get_repl().end());
  assert(_it_repl->is_punctuator("#"));
  assert(_it_repl + 1 != _macro->get_repl().end());
  assert((_it_repl + 1)->is_id());

  auto arg = _resolve_arg((_it_repl + 1)->get_value(), false);
  assert(arg);
  assert(_collect_used_macros(*arg).empty());
  assert(_collect_used_macro_undefs(*arg).empty());

  _it_repl += 2;


  // Concatenate the stringified argument tokens
  std::string s;
  for (auto &&it_tok = (*arg).begin(); it_tok != (*arg).end(); ++it_tok) {
    s += it_tok->stringify();
  }

  // And escape '\' and '"' characters
  for (auto &&it = s.begin(); it != s.end();) {
    if (*it == '"' || *it == '\\') {
      it = s.insert(it, '\\');
      it += 2;
    } else {
      ++it;
    }
  }

  return _pp_token(pp_token::type::str, std::move(s),
		   _file_range, used_macros(), _tok_used_macros_init(),
		   _tok_used_macro_undefs_init());
}

void preprocessor::_macro_instance::_add_concat_token(const _pp_token &tok)
{
  assert(!tok.is_ws() && !tok.is_newline() && !tok.is_eof());

  assert(tok.used_macros().empty());
  assert(tok.used_macro_undefs().empty());

  if (!_concat_token) {
    _concat_token.reset(new _pp_token(tok.get_type(), tok.get_value(),
				      _file_range,
				      _tok_expansion_history_init(),
				      used_macros(), used_macro_undefs()));
    _concat_token->expansion_history() += tok.expansion_history();
  } else {
    _concat_token->concat(tok, _remarks);
  }
}

void preprocessor::_macro_instance::
_add_concat_token(const raw_pp_token &raw_tok)
{
  assert(!raw_tok.is_ws() && !raw_tok.is_newline() && !raw_tok.is_eof());

  if (!_concat_token) {
    _concat_token.reset(new _pp_token(raw_tok.get_type(), raw_tok.get_value(),
				      raw_tok.get_file_range()));
  } else {
    _pp_token tok {raw_tok.get_type(), raw_tok.get_value(),
		   raw_tok.get_file_range()};
    _concat_token->concat(tok, _remarks);
  }
}

preprocessor::_pp_token preprocessor::_macro_instance::_yield_concat_token()
{
  assert(_concat_token);
  _anything_emitted = true;

  _pp_token tok = std::move(*_concat_token);
  assert(_concat_token->used_macros().empty());
  _concat_token->used_macros() = _tok_used_macros_init();
  assert(_concat_token->used_macro_undefs().empty());
  _concat_token->used_macro_undefs() = _tok_used_macro_undefs_init();
  _concat_token.reset();
  return tok;
}

preprocessor::_pp_token preprocessor::_macro_instance::read_next_token()
{
  // This is the core of macro expansion.
  //
  // The next replacement token to emit is determined dynamically
  // based on the current _macro_instance's state information and
  // returned.
  //
  // In case the macro expands to nothing, a dummy
  // pp_token::type::empty token is emitted in order to allow for
  // tracing of this macro invocation at later stages.
  //
  // Finally, after the list of replacement tokens has been exhausted,
  // a final pp_token::type::eof marker is returned.
  //
  // For the non-expanded argument values, which are used in
  // concatenations and stringifications, the associated _pp_tokens
  // sequence must not contain any
  // - empty token,
  // - multiple consecutive whitespace tokens or
  // - leading or trailing whitespace.
  // - For the expanded argument value, there must not be
  //   - any sequence of empties and whitespace tokens with more
  //     than one whitespace token in it,
  //   - any leading or trailing whitespace token that doesn't have
  //     any empty token next to it.
  //
  if (_concat_token) {
    // The previous run assembled a concatenated token, detected that
    // it was non-empty and thus had to emit a pending space. Now it's
    // time to emit that concat_token.
    assert(!_in_concat);
    assert(!_concat_token->is_empty());
    return _yield_concat_token();
  }

  // Continue processing the currently "active" parameter, if any.
  if (_cur_arg) {
  handle_arg:
    assert(_cur_arg);
    if (_cur_arg->empty()) {
      _cur_arg = nullptr;
      ++_it_repl;
      if (_is_concat_op(_it_repl)) {
	_in_concat = true;
	++_it_repl;
      } else if (_in_concat) {
	_in_concat = false;
	if (_concat_token)
	  return _yield_concat_token();
      }
    } else if (_cur_arg_it == _cur_arg->begin() && _in_concat) {
      // We're at the beginning of a parameter's replacement list and
      // that parameter has been preceeded by a ## concatenation
      // operator in the macro replacement list.
      assert(_cur_arg_it != _cur_arg->end());
      assert(!_cur_arg_it->is_ws());
      assert(!_cur_arg_it->is_empty());

      _add_concat_token(*_cur_arg_it++);

      assert(_cur_arg_it != _cur_arg->end() || !_cur_arg_it->is_empty());
      assert(_cur_arg_it + 1 != _cur_arg->end() || !_cur_arg_it->is_ws());

      // No more tokens left in current parameter's replacement list?
      if (_cur_arg_it == _cur_arg->cend()) {
	_cur_arg = nullptr;
	++_it_repl;
      }

      // If there are some more elements in the current parameter's
      // replacement list or the next token in macro replacement list
      // isn't a concatenation operator, then we're done with
      // concatenating and return the result.
      if (_cur_arg || !_is_concat_op(_it_repl)) {
	assert(_concat_token);
	_in_concat = false;
	return _yield_concat_token();
      }
      // The else case, i.e. the current argument's replacement list
      // has been exhausted and the parameter is followed by a ##
      // concatenation operator in the macro's replacement list, is
      // handled below.
      assert(_it_repl != _macro->get_repl().end());
      assert(_it_repl->is_punctuator("##"));
      ++_it_repl;
      assert(_it_repl != _macro->get_repl().end());

    } else if (_is_concat_op(_it_repl + 1) &&
	       _cur_arg_it + 1 == _cur_arg->end()) {
      // This is the last token in the current parameter's replacement
      // list and the next token is a ## operator. Prepare for the
      // concatenation.
      assert(!_cur_arg_it->is_empty());
      assert(!_cur_arg_it->is_ws());

      // The in_concat == true case would have been handled by the
      // branch above.
      assert(!_in_concat);
      _in_concat = true;

      _add_concat_token(*_cur_arg_it++);

      assert (_cur_arg_it == _cur_arg->end());
      _cur_arg = nullptr;
      _it_repl += 2;
    } else {
      // Normal parameter substitution
      assert(!_in_concat);
      assert(_cur_arg_it != _cur_arg->end());

      auto tok = _pp_token(_cur_arg_it->get_type(), _cur_arg_it->get_value(),
			   _file_range, _tok_expansion_history_init(),
			   _tok_used_macros_init(),
			   _tok_used_macro_undefs_init());
      tok.expansion_history() += _cur_arg_it->expansion_history();
      tok.used_macros() += _cur_arg_it->used_macros();
      tok.used_macro_undefs() += _cur_arg_it->used_macro_undefs();
      ++_cur_arg_it;

      if (_cur_arg_it == _cur_arg->cend()) {
	_cur_arg = nullptr;
	++_it_repl;
      }

      assert(!tok.is_ws() || _anything_emitted);
      _anything_emitted = true;
      return tok;
    }
  }

 from_repl_list:
  assert(_cur_arg == nullptr);

  assert(_it_repl == _macro->get_repl().end() ||
	 !_it_repl->is_punctuator("##"));

  // Process all but the last operands of a ## concatenation.
  while(_it_repl != _macro->get_repl().end() &&
	_is_concat_op(_skip_stringification_or_single(_it_repl))) {
    assert(!_it_repl->is_ws());
    if (_it_repl->is_id()) {
      _cur_arg = _resolve_arg(_it_repl->get_value(), false);
      if (_cur_arg != nullptr) {
	_cur_arg_it = _cur_arg->begin();
	goto handle_arg;
      }
      // Remaining case of non-parameter identifier is handled below.
    } else if (_macro->is_variadic() && _it_repl->is_punctuator(",") &&
	       (_it_repl + 2)->is_id() &&
	       (_it_repl + 2)->get_value() == _macro->get_arg_names().back()) {
      // Handle the GNU extension that a in a sequence of
      //  , ## __VA_ARGS__,
      // the comma gets removed if __VA_ARGS__ is empty.
      auto vaarg = _resolve_arg((_it_repl + 2)->get_value(), false);
      assert(vaarg);
      if (vaarg->empty()) {
	// __VA_ARGS__ is empty, skip the comma.
	_it_repl += 2;
	continue;
      } else {
	// __VA_ARGS__ is not empty. What to do next depends on whether
	// the comma has again been preceeded by a ## or not.
	// In either case, the comma is not concatenated to the __VA_ARGS__,
	// but each are treated separately.
	if (_concat_token) {
	  _add_concat_token(*_it_repl);
	  _it_repl += 2;
	  assert(_in_concat);
	  return _yield_concat_token();
	} else {
	  _pp_token tok{_it_repl->get_type(), _it_repl->get_value(),
			_it_repl->get_file_range()};
	  _it_repl += 2;
	  _in_concat = true;
	  return tok;
	}
      }
    }

    if (!_is_stringification(_it_repl)) {
      _add_concat_token(*_it_repl++);
    } else {
      // Note that GCC gives priority to # over ## and so do we.
      _add_concat_token(_handle_stringification());
    }
    assert(_it_repl->is_punctuator("##"));
    ++_it_repl;
    assert(!_it_repl->is_ws());
    _in_concat = true;
  }

  // At this point there's either no ## concatenation in progress
  // or the current token is the very last operand.
  if (_it_repl == _macro->get_repl().end()) {
    assert(!_in_concat);
    if (!_anything_emitted) {
      // Emit an empty token in order to report usage of this macro.
      _anything_emitted = true;
      return _pp_token(pp_token::type::empty, std::string(),
		       _file_range, _tok_expansion_history_init(),
		       _tok_used_macros_init(),
		       _tok_used_macro_undefs_init());
    }

    return _pp_token(pp_token::type::eof, std::string(), file_range());
  }

  if (_it_repl->is_id()) {
    _cur_arg = _resolve_arg(_it_repl->get_value(), !_in_concat);
    if (_cur_arg != nullptr) {
      _cur_arg_it = _cur_arg->begin();
      goto handle_arg;
    }
  }

  if (_in_concat) {
    assert(!_it_repl->is_ws());
    if (_is_stringification(_it_repl))
      _add_concat_token(_handle_stringification());
    else
      _add_concat_token(*_it_repl++);

    assert(_concat_token);
    _in_concat = false;
    return _yield_concat_token();
  }

  // All the cases below will emit something.
  _anything_emitted = true;

  if (_is_stringification(_it_repl)) {
    return _handle_stringification();
  }

  auto tok = _pp_token(_it_repl->get_type(), _it_repl->get_value(),
		       _file_range,
		       _tok_expansion_history_init(), _tok_used_macros_init(),
		       _tok_used_macro_undefs_init());
  ++_it_repl;

  return tok;
}


preprocessor::_cond_incl_state::
_cond_incl_state(const file_range::loc_type _start_loc)
  : start_loc(_start_loc), incl_node(nullptr), branch_active(false)
{}


preprocessor::_expansion_state::_expansion_state() = default;
