#include <stdexcept>
#include <set>
#include <utility>
#include <cassert>
#include <algorithm>
#include "macro.hh"
#include "pp_token.hh"
#include "pp_except.hh"

using namespace klp::ccp;

macro::instance::instance(const std::shared_ptr<const macro> &macro,
			  used_macros &&used_macros_base,
			  used_macro_undefs &&used_macro_undefs_base,
			  std::vector<pp_tokens> &&args,
			  std::vector<pp_tokens> &&exp_args,
			  const file_range &file_range)
  : _macro(macro),
    _used_macros_base(std::move(used_macros_base)),
    _used_macro_undefs_base(std::move(used_macro_undefs_base)),
    _file_range(file_range),
    _it_repl(_macro->_repl.cbegin()), _cur_arg(nullptr),
    _in_concat(false), _anything_emitted(false)
{
  assert(args.size() == exp_args.size());
  assert(args.size() == _macro->_do_expand_args.size());

  auto arg_it = args.begin();
  auto exp_arg_it = exp_args.begin();
  size_t i = 0;
  for (auto arg_name : _macro->_arg_names) {
    assert(_macro->shall_expand_arg(i) || exp_arg_it->empty());
    ++i;
    _args.insert(std::make_pair(arg_name,
				std::make_pair(std::move(*arg_it++),
					       std::move(*exp_arg_it++))));
  }
  args.clear();
  exp_args.clear();
}

const pp_tokens*
macro::instance::_resolve_arg(const std::string &name, const bool expanded)
  const noexcept
{
  if (!_macro->_func_like)
    return nullptr;

  auto arg = _args.find(name);
  if (arg == _args.end())
    return nullptr;

  const pp_tokens *resolved
    = expanded ? &arg->second.second : &arg->second.first;
  return resolved;
}

used_macros macro::instance::_tok_expansion_history_init() const
{
  used_macros eh;
  eh += _macro;
  return eh;
}

used_macros macro::instance::_tok_used_macros_init() const
{
  // Anything relevant to the current macro call
  // being even recognized during re-evaluation:
  used_macros result = _used_macros_base;

  // Record the current macro itself
  result += _macro;

  return result;
}

used_macro_undefs macro::instance::_tok_used_macro_undefs_init() const
{
  return _used_macro_undefs_base;
}

pp_token macro::instance::_handle_stringification()
{
  assert(_it_repl != _macro->_repl.end());
  assert(_it_repl->is_punctuator("#"));
  assert(_it_repl + 1 != _macro->_repl.end());
  assert((_it_repl + 1)->is_id());

  auto arg = _resolve_arg((_it_repl + 1)->get_value(), false);
  assert(arg);
  assert(arg->get_used_macros().empty());
  assert(arg->get_used_macro_undefs().empty());

  _it_repl += 2;
  return pp_token(pp_token::type::str, arg->stringify(true),
		  _file_range, used_macros(), _tok_used_macros_init(),
		  _tok_used_macro_undefs_init());
}

void macro::instance::_add_concat_token(const pp_token &tok)
{
  assert(!tok.is_ws() && !tok.is_newline() && !tok.is_eof());

  assert(tok.used_macros().empty());
  assert(tok.used_macro_undefs().empty());

  if (!_concat_token) {
    _concat_token.reset(new pp_token(tok.get_type(), tok.get_value(),
				     _file_range, _tok_expansion_history_init(),
				     used_macros(), used_macro_undefs()));
    _concat_token->expansion_history() += tok.expansion_history();
  } else {
    _concat_token->concat(tok, _remarks);
  }
}

void macro::instance::_add_concat_token(const raw_pp_token &raw_tok)
{
  assert(!raw_tok.is_ws() && !raw_tok.is_newline() && !raw_tok.is_eof());

  if (!_concat_token) {
    _concat_token.reset(new pp_token(raw_tok.get_type(), raw_tok.get_value(),
				     raw_tok.get_file_range()));
  } else {
    pp_token tok {raw_tok.get_type(), raw_tok.get_value(),
		  raw_tok.get_file_range()};
    _concat_token->concat(tok, _remarks);
  }
}

pp_token macro::instance::_yield_concat_token()
{
  assert(_concat_token);
  _anything_emitted = true;

  pp_token tok = std::move(*_concat_token);
  assert(_concat_token->used_macros().empty());
  _concat_token->used_macros() = _tok_used_macros_init();
  assert(_concat_token->used_macro_undefs().empty());
  _concat_token->used_macro_undefs() = _tok_used_macro_undefs_init();
  _concat_token.reset();
  return tok;
}

pp_token macro::instance::read_next_token()
{
  // This is the core of macro expansion.
  //
  // The next replacement token to emit is determined dynamically
  // based on the current macro::instance's state information and
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
  // concatenations and stringifications, the associated pp_tokens
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
      if (_macro->_is_concat_op(_it_repl)) {
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
      if (_cur_arg || !_macro->_is_concat_op(_it_repl)) {
	assert(_concat_token);
	_in_concat = false;
	return _yield_concat_token();
      }
      // The else case, i.e. the current argument's replacement list
      // has been exhausted and the parameter is followed by a ##
      // concatenation operator in the macro's replacement list, is
      // handled below.
      assert(_it_repl != _macro->_repl.end());
      assert(_it_repl->is_punctuator("##"));
      ++_it_repl;
      assert(_it_repl != _macro->_repl.end());

    } else if (_macro->_is_concat_op(_it_repl + 1) &&
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

      auto tok = pp_token(_cur_arg_it->get_type(), _cur_arg_it->get_value(),
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

  assert(_it_repl == _macro->_repl.end() || !_it_repl->is_punctuator("##"));

  // Process all but the last operands of a ## concatenation.
  while(_it_repl != _macro->_repl.end() &&
	_macro->_is_concat_op(
			_macro->_skip_stringification_or_single(_it_repl))) {
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
	       (_it_repl + 2)->get_value() == _macro->_arg_names.back()) {
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
	  pp_token tok{_it_repl->get_type(), _it_repl->get_value(),
		       _it_repl->get_file_range()};
	  _it_repl += 2;
	  _in_concat = true;
	  return tok;
	}
      }
    }

    if (!_macro->_is_stringification(_it_repl)) {
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
  if (_it_repl == _macro->_repl.end()) {
    assert(!_in_concat);
    if (!_anything_emitted) {
      // Emit an empty token in order to report usage of this macro.
      _anything_emitted = true;
      return pp_token(pp_token::type::empty, std::string(),
		      _file_range, _tok_expansion_history_init(),
		      _tok_used_macros_init(),
		      _tok_used_macro_undefs_init());
    }

    return pp_token(pp_token::type::eof, std::string(), file_range());
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
    if (_macro->_is_stringification(_it_repl))
      _add_concat_token(_handle_stringification());
    else
      _add_concat_token(*_it_repl++);

    assert(_concat_token);
    _in_concat = false;
    return _yield_concat_token();
  }

  // All the cases below will emit something.
  _anything_emitted = true;

  if (_macro->_is_stringification(_it_repl)) {
    return _handle_stringification();
  }

  auto tok = pp_token(_it_repl->get_type(), _it_repl->get_value(),
		      _file_range,
		      _tok_expansion_history_init(), _tok_used_macros_init(),
		      _tok_used_macro_undefs_init());
  ++_it_repl;

  return tok;
}


macro::macro(const std::string &name,
	     const bool func_like,
	     const bool variadic,
	     std::vector<std::string> &&arg_names,
	     raw_pp_tokens &&repl,
	     const file_range &file_range,
	     std::shared_ptr<const macro_undef> &&prev_macro_undef)
  : _name(name), _arg_names(std::move(arg_names)),
    _repl(std::move(repl)),
    _file_range(file_range),
    _prev_macro_undef(prev_macro_undef),
    _func_like(func_like),
    _variadic(variadic)
{
  // Due to the restriction that a ## concatenated token must again
  // yield a valid preprocessing token, macro evaluation can fail and
  // thus yield an error. Hence macro arguments must not get macro
  // expanded if not needed. An argument needs to get expanded if it
  // appears in the replacement list, ignoring operands of ## and
  // #. Determine those.
  std::set<std::string> do_expand;
  bool in_concat = false;
  for (auto it = _repl.cbegin(); it != _repl.cend();) {
    if (it->is_id() &&
	(std::find(_arg_names.cbegin(), _arg_names.cend(), it->get_value())
	 != _arg_names.cend())) {
      if (_is_concat_op(it + 1)) {
	in_concat = true;
	it += 2;
      } else if (in_concat) {
	in_concat = false;
	it = _next_non_ws_repl(it + 1);
      } else {
	do_expand.insert(it->get_value());
	it = _next_non_ws_repl(it + 1);
      }
    } else {
      it = _skip_stringification_or_single(it);
      if (_is_concat_op(it)) {
	in_concat = true;
	++it;
      } else {
	in_concat = false;
      }
    }
  }

  _do_expand_args.resize(_arg_names.size(), false);
  for (size_t i = 0; i < _arg_names.size(); ++i) {
    if (do_expand.count(_arg_names[i]))
      _do_expand_args[i] = true;
  }
}

std::shared_ptr<const macro>
macro::parse_macro_definition(const raw_pp_tokens::const_iterator begin,
			      raw_pp_tokens::const_iterator end,
			      std::shared_ptr<const macro_undef> &&macro_undef,
			      code_remarks &remarks)
{
  assert (begin != end);
  assert(begin->is_punctuator("#"));

  assert((end - 1)->is_newline() || (end - 1)->is_eof());
  // end is decremented by one such that we can dereference
  // it even for it == end from now on.
  --end;

  const file_range &range_start = begin->get_file_range();
  const file_range &range_end = end->get_file_range();

  auto skip_ws = [&end](const decltype(end) it) {
    if (it == end || !it->is_ws()) {
      return it;
    }
    // No need to loop here:
    // Macro definitions come in a single line and pp_tokenizer
    // won't emit more than one consecutive whitespace.
    assert(it + 1 == end || !(it + 1)->is_ws());
    return it + 1;
  };

  auto it = skip_ws(begin + 1);
  assert(it->is_id() && it->get_value() == "define");
  it = skip_ws(it + 1);

  if (!it->is_id()) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "no identifier following #define",
			   it->get_file_range());
    remarks.add(remark);
    throw pp_except(remark);
  }

  const std::string &name = it->get_value();
  ++it;

  bool func_like = false;
  bool variadic = false;
  std::vector<std::string> arg_names;
  std::set<std::string> unique_arg_names;
  if (it->is_punctuator("(")) {
    func_like = true;

    it = skip_ws(it + 1);
    while (it != end && !it->is_punctuator(")")) {
      if (variadic) {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "garbage in macro argument list after ...",
			       it->get_file_range());
	remarks.add(remark);
	throw pp_except(remark);
      }

      if (!arg_names.empty() && !it->is_punctuator(",")) {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "comma expected in macro argument list",
			       it->get_file_range());
	remarks.add(remark);
	throw pp_except(remark);
      } else if (it->is_punctuator(",")) {
	if (arg_names.empty()) {
	  code_remark_raw remark(code_remark_raw::severity::fatal,
				 "leading comma in macro argument list",
				 it->get_file_range());
	  remarks.add(remark);
	  throw pp_except(remark);
	}
	it = skip_ws(it + 1);
	if (it == end || it->is_punctuator(")")) {
	  code_remark_raw remark(code_remark_raw::severity::fatal,
				 "trailing comma in macro argument list",
				 it->get_file_range());
	  remarks.add(remark);
	  throw pp_except(remark);
	}
      }

      if (it->is_punctuator("...")) {
	variadic = true;
	arg_names.push_back("__VA_ARGS__");
	unique_arg_names.insert("__VA_ARGS__");
	it = skip_ws(it + 1);
      } else if (it->is_id()) {
	if (it->get_value() == "__VA_ARGS__") {
	  code_remark_raw remark
	    (code_remark_raw::severity::fatal,
	     "__VA_ARGS__ not allowed as a macro argument name",
	     it->get_file_range());
	  remarks.add(remark);
	  throw pp_except(remark);
	} else if (!unique_arg_names.insert(it->get_value()).second) {
	  code_remark_raw remark(code_remark_raw::severity::fatal,
				 "duplicate macro argument name",
				 it->get_file_range());
	  remarks.add(remark);
	  throw pp_except(remark);
	}

	arg_names.push_back(it->get_value());
	it = skip_ws(it + 1);
	if (it->is_punctuator("...")) {
	  variadic = true;
	  it = skip_ws(it + 1);
	}
      } else {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "garbage in macro argument list",
			       it->get_file_range());
	remarks.add(remark);
	throw pp_except(remark);
      }
    }
    if (it == end) {
      code_remark_raw remark(code_remark_raw::severity::fatal,
			     "macro argument list not closed",
			     end->get_file_range());
      remarks.add(remark);
      throw pp_except(remark);
    }

    assert(it->is_punctuator(")"));
    ++it;
  }

  if (it != end && !it->is_ws()) {
    code_remark_raw remark(code_remark_raw::severity::warning,
			   "no whitespace before macro replacement list",
			   it->get_file_range());
    remarks.add(remark);
  }

  it = skip_ws(it);

  return create(name, func_like, variadic, std::move(arg_names),
		_normalize_repl(it, end, func_like, unique_arg_names,
				remarks),
		file_range(range_start, range_end),
		std::move(macro_undef));
}

bool macro::operator==(const macro &rhs) const noexcept
{
  return (_name == rhs._name &&
	  _func_like == rhs._func_like &&
	  _arg_names == rhs._arg_names &&
	  _variadic == rhs._variadic &&
	  _repl == rhs._repl);
}

size_t macro::non_va_arg_count() const noexcept
{
  if (_variadic)
    return _do_expand_args.size() - 1;

  return _do_expand_args.size();
}

bool macro::shall_expand_arg(const size_t pos) const noexcept
{
  assert(pos <= _do_expand_args.size());
  return _do_expand_args[pos];
}

raw_pp_tokens macro::_normalize_repl(const raw_pp_tokens::const_iterator begin,
				     const raw_pp_tokens::const_iterator end,
				     const bool func_like,
				     const std::set<std::string> &arg_names,
				     code_remarks &remarks)
{
  if (begin == end)
    return raw_pp_tokens();

  assert(!begin->is_ws() && !(end-1)->is_ws());

  if (begin->is_punctuator("##")) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "## at beginning of macro replacement list",
			   begin->get_file_range());
    remarks.add(remark);
    throw pp_except(remark);
  } else if ((end - 1)->is_punctuator("##")) {
    code_remark_raw remark(code_remark_raw::severity::fatal,
			   "## at end of macro replacement list",
			   (end - 1)->get_file_range());
    remarks.add(remark);
    throw pp_except(remark);
  }

  // Thanks to pp_tokenizer, we're already free of multiple
  // consecutive whitespace tokens.
  // Remove any whitespace separating either ## or # and their
  // operands. Collapse multiple ## operators into one.
  auto concat_or_stringify_op_pred = [func_like](const raw_pp_token &tok) {
    return (tok.is_punctuator("##") ||
	    (func_like && tok.is_punctuator("#")));
  };

  raw_pp_tokens result;
  auto it_seq_begin = begin;
  while (it_seq_begin != end) {
    auto it_seq_end = std::find_if(it_seq_begin, end,
				   concat_or_stringify_op_pred);
    if (it_seq_end == end) {
      result.insert(result.end(), it_seq_begin, it_seq_end);
    } else if (it_seq_end->is_punctuator("##")) {
      if ((it_seq_end - 1)->is_ws()) {
	assert((it_seq_end - 1) != it_seq_begin);
	result.insert(result.end(), it_seq_begin, it_seq_end - 1);
      } else {
	result.insert(result.end(), it_seq_begin, it_seq_end);
      }
      result.push_back(*it_seq_end);

      while (it_seq_end != end &&
	     (it_seq_end->is_ws() || it_seq_end->is_punctuator("##"))) {
	++it_seq_end;
      }
    } else {
      assert(func_like && it_seq_end->is_punctuator("#"));
      auto it_arg = it_seq_end + 1;
      if (it_arg != end && it_arg->is_ws())
	++it_arg;
      assert(it_arg == end || !it_arg->is_ws());
      if (it_arg == end || !it_arg->is_id() ||
	  !arg_names.count(it_arg->get_value())) {
	code_remark_raw remark(code_remark_raw::severity::fatal,
			       "# in func-like macro not followed by parameter",
			       (end - 1)->get_file_range());
	remarks.add(remark);
	throw pp_except(remark);
      }

      result.insert(result.end(), it_seq_begin, it_seq_end + 1);
      result.push_back(*it_arg);
      it_seq_end = it_arg + 1;
    }
    it_seq_begin = it_seq_end;
  }

  return result;
}

raw_pp_tokens::const_iterator
macro::_next_non_ws_repl(const raw_pp_tokens::const_iterator it)
  const noexcept
{
  if (it == _repl.end() || !it->is_ws()) {
    return it;
  }
  // No need to loop here: we won't have more than one
  // consecutive whitespace in a macro replacement list
  assert(it + 1 == _repl.end() || !(it + 1)->is_ws());
  return it + 1;
}

bool macro::_is_stringification(raw_pp_tokens::const_iterator it)
  const noexcept
{
  assert(it != _repl.end());

  if (!_func_like || !it->is_punctuator("#"))
    return false;

  assert((it + 1) != _repl.end() && (it + 1)->is_id());

  return true;
}

raw_pp_tokens::const_iterator
macro::_skip_stringification_or_single(const raw_pp_tokens::const_iterator &it)
  const noexcept
{
  assert(it != _repl.end());

  if (_func_like && it->is_punctuator("#")) {
    assert(it + 1 != _repl.end());
    assert((it + 1)->is_id());
    return it + 2;
  }
  return it + 1;
}

bool macro::_is_concat_op(const raw_pp_tokens::const_iterator &it)
  const noexcept
{
  return (it != _repl.end() && it->is_punctuator("##"));
}
