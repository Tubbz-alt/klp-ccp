#include <cassert>
#include <functional>
#include "preprocessor.hh"
#include "architecture.hh"
#include "pp_except.hh"
#include "parse_except.hh"
#include "semantic_except.hh"
#include "macro_undef.hh"
#include "path.hh"
#include "pp_expr_parser_driver.hh"

using namespace klp::ccp;

preprocessor::
preprocessor(pp_result::header_inclusion_roots &&header_inclusion_roots,
	     const header_resolver &header_resolver,
	     const architecture &arch)
  : _header_resolver(header_resolver), _arch(arch),
    _pp_result(new pp_result{std::move(header_inclusion_roots)}),
    _cur_header_inclusion_root(_pp_result->get_header_inclusion_roots()
			       .begin()),
    _cond_incl_nesting(0), _root_expansion_state(),
    _cur_macro_invocation(nullptr), __counter__(0),
    _eof_tok(pp_token::type::empty, "", raw_pp_tokens_range()),
    _maybe_pp_directive(true), _line_empty(true)
{
  assert(!_pp_result->get_header_inclusion_roots().empty());
  _cur_header_inclusion_root->_set_range_begin(0);
  _tokenizers.emplace(*_cur_header_inclusion_root);
  _inclusions.emplace(std::ref(*_cur_header_inclusion_root));
}

pp_token_index preprocessor::read_next_token()
{
  // Basically, this is just a wrapper around _expand()
  // which removes or adds whitespace as needed:
  // - Whitespace at the end of lines gets removed.
  // - Multiple whitespace tokens get collated into one.
  // - Empty lines are removed.
  // - Whitespace gets inserted inbetween certain kind of tokens
  //   in order to guarantee idempotency of preprocessing.
  //
  // A queue of pending tokens is maintained in ::_pending_tokens.
  //
  // The following patters are possible for _pending_tokens at each
  // invocation:
  // - a single whitespace token,
  // - a non-whitespace token from lookahead,
  // - (added) whitespace + some punctuator, pp_number or id from lookahead
  //
  assert(_pending_tokens.empty() ||
	 (_pending_tokens.size() == 1) ||
	 (_pending_tokens.size() == 2 && _pending_tokens.front().is_ws() &&
	  (_pending_tokens.back().get_type() == pp_token::type::punctuator ||
	   _pending_tokens.back().get_type() == pp_token::type::pp_number ||
	   _pending_tokens.back().get_type() == pp_token::type::id)));
  if (!_pending_tokens.empty() &&
      (_pending_tokens.size() == 2 ||
       !(_pending_tokens.front()
	 .is_type_any_of<pp_token::type::ws,
			 pp_token::type::punctuator,
			 pp_token::type::pp_number,
			 pp_token::type::id>()))) {
    auto tok = std::move(_pending_tokens.front());
    _pending_tokens.pop();
    assert(!_line_empty);
    if (tok.is_newline())
      _line_empty = true;
    return _emit_pp_token(*_pp_result, std::move(tok));
  }
  assert(_pending_tokens.empty() ||
	 (_pending_tokens.size() == 1 &&
	  (_pending_tokens.front().is_type_any_of<pp_token::type::ws,
						  pp_token::type::punctuator,
						  pp_token::type::pp_number,
						  pp_token::type::id>())));

 read_next:
  auto next_tok = _expand(_root_expansion_state,
			std::bind(&preprocessor::_read_next_plain_token, this));
  bool empty_seen = false;
  while (next_tok.is_empty()) {
    empty_seen = true;
    next_tok = _expand(_root_expansion_state,
		       std::bind(&preprocessor::_read_next_plain_token, this));
  }
  next_tok.expansion_history().clear();

  // First, handle pending whitespace.
  if (!_pending_tokens.empty() && _pending_tokens.front().is_ws()) {
    if (next_tok.is_newline()) {
      // Queued whitespace at end of line. Drop it.
      _pending_tokens.pop();
      assert(_pending_tokens.empty());
      if (!_line_empty) {
	_line_empty = true;
	return _emit_pp_token(*_pp_result, std::move(next_tok));
      }
      goto read_next;

    } else if (next_tok.is_eof()) {
      // Queued whitespace at end of file. Drop it.
      _pending_tokens.pop();
      assert(_pending_tokens.empty());
      return _emit_pp_token(*_pp_result, std::move(next_tok));

    }  else if (next_tok.is_ws()) {
      // Multiple consecutive whitespace tokens.
      goto read_next;
    }

    // Something which is not an EOF, newline, whitespace or empty
    // token. Emit the pending whitespace and queue the current
    // token.
    _line_empty = false;
    _pending_tokens.push(std::move(next_tok));
    next_tok = std::move(_pending_tokens.front());
    _pending_tokens.pop();
    return _emit_pp_token(*_pp_result, std::move(next_tok));
  }

  // At this point, a pending token, if any, is a punctuator,
  // pp_number or id which might have to get separated from the
  // subsequent token by extra whitespace.
  assert(_pending_tokens.empty() ||
	 (_pending_tokens.size() == 1 &&
	  (_pending_tokens.front().is_type_any_of<pp_token::type::punctuator,
						  pp_token::type::pp_number,
						  pp_token::type::id>())));
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
  if (next_tok.is_ws()) {
    if (!_pending_tokens.empty()) {
      std::swap(next_tok, _pending_tokens.front());
      return _emit_pp_token(*_pp_result, std::move(next_tok));
    } else {
      _pending_tokens.push(std::move(next_tok));
      goto read_next;
    }
  } else if (next_tok.is_newline()) {
    if (_line_empty) {
      assert(_pending_tokens.empty());
      goto read_next;
    } else {
      if (!_pending_tokens.empty())
	std::swap(next_tok, _pending_tokens.front());
      return _emit_pp_token(*_pp_result, std::move(next_tok));
    }
  } else if (!next_tok.is_type_any_of<pp_token::type::punctuator,
				      pp_token::type::pp_number,
				      pp_token::type::id>()) {
    _line_empty = false;
    if (!_pending_tokens.empty())
      std::swap(next_tok, _pending_tokens.front());
    return _emit_pp_token(*_pp_result, std::move(next_tok));
  } else if (_pending_tokens.empty()) {
    _line_empty = false;
    _pending_tokens.push(std::move(next_tok));
    goto read_next;
  }

  // There is a pending punctuator, pp_numer or id and the next token
  // is also of this type. Check whether extra whitespace needs to get
  // inserted.
  _line_empty = false;
  _pp_token prev_tok = std::move(_pending_tokens.front());
  _pending_tokens.pop();
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
	(empty_seen &&
	 ((prev_tok.is_punctuator(":") && next_tok.is_punctuator(":")) ||
	  (prev_tok.is_punctuator(".") && next_tok.is_punctuator(".")) ||
	  (prev_tok.is_punctuator("#") && next_tok.is_punctuator("#"))))))) {
    _pp_token extra_ws_tok{
		pp_token::type::ws, " ",
		raw_pp_tokens_range{
			next_tok.get_token_source().begin,
			next_tok.get_token_source().begin}};
    extra_ws_tok.set_macro_invocation(next_tok.get_macro_invocation());
    _pending_tokens.push(std::move(extra_ws_tok));
  }

  _pending_tokens.push(std::move(next_tok));
  return _emit_pp_token(*_pp_result, std::move(prev_tok));
}

template<typename T>
void preprocessor::_grab_remarks_from(T &from)
{
  _remarks += from.get_remarks();
  from.get_remarks().clear();
}

pp_token_index preprocessor::_emit_pp_token(pp_result &r, _pp_token &&tok)
{
  if (!tok.get_macro_invocation()) {
    r._append_token
      (pp_token{tok.get_type(), tok.get_value(),
		tok.get_token_source(),
		tok.get_used_macro_undef()});
  } else {
    r._append_token
      (pp_token{tok.get_type(), tok.get_value(),
		tok.get_macro_invocation()->get_source_range()});
  }
  return r._get_last_pp_index();
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

    _pp_result->_append_token(raw_tok);

    if (raw_tok.is_newline()) {
      _maybe_pp_directive = true;
    } else if (_maybe_pp_directive && raw_tok.is_punctuator("#")) {
      _handle_pp_directive();
      goto again;
    } else if (!raw_tok.is_ws()) {
      _maybe_pp_directive = false;
    }

    if (_cond_incl_inactive())
      goto again;

    return _pp_token{raw_tok.get_type(), raw_tok.get_value(),
		     raw_pp_tokens_range{_pp_result->_get_last_raw_index()}};
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
    _fixup_inclusion_node_ranges();
    const raw_pp_token_index eof_index = _pp_result->get_raw_tokens().size();
    const raw_pp_tokens_range eof_range{eof_index, eof_index};
    const auto it_src
      =  _pp_result->intersecting_sources_begin(eof_range);
    assert(it_src != _pp_result->intersecting_sources_end(eof_range));
    code_remark remark(code_remark::severity::fatal,
		       "missing #endif at end of file",
		       *it_src, eof_tok.get_range_in_file());
    _remarks.add(remark);
    throw pp_except(remark);
  }

  _tokenizers.pop();
  const raw_pp_token_index cur_raw_pos =
    (!_pp_result->get_raw_tokens().empty() ?
     _pp_result->_get_last_raw_index() + 1 :
     0);
  _inclusions.top().get()._set_range_end(cur_raw_pos);
  _inclusions.pop();

  if (_tokenizers.empty()) {
    if (++_cur_header_inclusion_root ==
	_pp_result->get_header_inclusion_roots().end()) {
      const raw_pp_token_index eof_index = _pp_result->get_raw_tokens().size();
      _eof_tok = _pp_token{eof_tok.get_type(), eof_tok.get_value(),
			   raw_pp_tokens_range{eof_index, eof_index}};
      return;
    }

    _cur_header_inclusion_root->_set_range_begin(cur_raw_pos);
    _tokenizers.emplace(*_cur_header_inclusion_root);
    _inclusions.emplace(std::ref(*_cur_header_inclusion_root));
  }
}

void preprocessor::_fixup_inclusion_node_ranges() noexcept
{
  // Fixup the all pending inclusion nodes' range information such
  // that those intersecting a given query range can be found.
  const raw_pp_token_index cur_raw_end = _pp_result->_raw_tokens.size();
  for (auto it_root = _cur_header_inclusion_root + 1;
       it_root != _pp_result->get_header_inclusion_roots().end();
       ++it_root) {
    it_root->_range.begin = cur_raw_end;
    it_root->_range.end = cur_raw_end;
  }

  for (pp_result::inclusion_node *n = &_inclusions.top().get(); n;
       n = n->_parent) {
    n->_range.end = cur_raw_end;
  }
}

const pp_result::header_inclusion_node& preprocessor::
_raw_tok_it_to_source(const raw_pp_tokens::const_iterator &it)
{
  const raw_pp_token_index index = it - _pp_result->get_raw_tokens().begin();
  return get_pending_token_source(index);
}

const pp_result::header_inclusion_node&
preprocessor::_raw_pp_tokens_range_to_source(const raw_pp_tokens_range &r)
{
  const pp_result::header_inclusion_node &s
    = get_pending_token_source(r.begin);
  assert(r.begin == r.end || &s == &get_pending_token_source(r.end - 1));
  return s;
}

range_in_file preprocessor::
_raw_pp_tokens_range_to_range_in_file(const raw_pp_tokens_range &r)
  const noexcept
{
    if (r.begin == r.end + 1) {
    // A single token, emit "point" range for its start.
    const raw_pp_token &tok = _pp_result->get_raw_tokens()[r.begin];
    const range_in_file &tok_rif = tok.get_range_in_file();
    return range_in_file{tok_rif.begin};
  } else if (r.begin == r.end) {
    // An empty range, used for EOFs. Emit a "point" range for the
    // preceeding token's end.
    assert(r.begin > 0);
    assert(r.begin <= _pp_result->get_raw_tokens().size());
    const raw_pp_token &tok = _pp_result->get_raw_tokens()[r.begin - 1];
    const range_in_file &tok_rif = tok.get_range_in_file();
    return range_in_file{tok_rif.end};
  } else {
    // A real range. It must come from a single header.
    assert(r.end <= _pp_result->get_raw_tokens().size());
    const raw_pp_token &first_tok = _pp_result->get_raw_tokens()[r.begin];
    const raw_pp_token &last_tok = _pp_result->get_raw_tokens()[r.end - 1];
    const range_in_file &first_tok_rif = first_tok.get_range_in_file();
    const range_in_file &last_tok_rif = last_tok.get_range_in_file();
    return range_in_file{first_tok_rif.begin, last_tok_rif.end};
  }
}

const pp_result::header_inclusion_node&
preprocessor::_pp_token_to_source(const _pp_token &tok)
{
  return _raw_pp_tokens_range_to_source(tok.get_token_source());
}

range_in_file preprocessor::_pp_token_to_range_in_file(const _pp_token &tok)
  const noexcept
{
  return _raw_pp_tokens_range_to_range_in_file(tok.get_token_source());
}

void preprocessor::_handle_pp_directive()
{
  const raw_pp_token_index raw_begin = _pp_result->_get_last_raw_index();
  raw_pp_token_index raw_end = raw_begin + 1;
  assert(_pp_result->get_raw_tokens()[raw_begin].is_punctuator("#"));
  bool endif_possible = true;
  bool is_endif = false;
  while (true) {
    assert(!_tokenizers.empty());
    auto tok = _tokenizers.top().read_next_token();
    _grab_remarks_from(_tokenizers.top());

    if (tok.is_eof()) {
      _handle_eof_from_tokenizer(std::move(tok));
      break;
    }

    _pp_result->_append_token(tok);
    raw_end = _pp_result->_get_last_raw_index() + 1;

    if (tok.is_newline()) {
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
	code_remark remark
	  (code_remark::severity::fatal,
	   "#endif without #if",
	   get_pending_token_source(_pp_result->_get_last_raw_index()),
	   tok.get_range_in_file());
	_remarks.add(remark);
	throw pp_except(remark);
      }

      if (_cond_incl_nesting == _cond_incl_states.size())
	_pop_cond_incl(raw_end);
      --_cond_incl_nesting;

    } else if (!tok.is_ws()) {
      endif_possible = false;
      if (is_endif) {
	code_remark remark
	  (code_remark::severity::warning,
	   "garbage after #endif",
	   get_pending_token_source(_pp_result->_get_last_raw_index()),
	   tok.get_range_in_file());
	_remarks.add(remark);
      }
    }
  }

  if (is_endif)
    return;

  auto it_tok = _pp_result->get_raw_tokens().begin() + raw_begin + 1;
  if (it_tok == _pp_result->get_raw_tokens().begin() + raw_end) {
    // Null directive terminated by EOF.
    return;
  }

  if (it_tok->is_ws())
    ++it_tok;

  if (it_tok->is_newline() ||
      it_tok == _pp_result->get_raw_tokens().begin() + raw_end) {
    // Null directive
    return;
  }

  assert(!it_tok->is_ws());
  if (!it_tok->is_id()) {
    code_remark remark(code_remark::severity::fatal,
		       "identifier expected in preprocessor directive",
		       _raw_tok_it_to_source(it_tok),
		       it_tok->get_range_in_file());
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

    _cond_incl_states.emplace(raw_begin);

    if (_eval_conditional_inclusion(raw_pp_tokens_range{raw_begin, raw_end}))
      _enter_cond_incl();

  } else if (it_tok->get_value() == "ifdef") {
    ++_cond_incl_nesting;
    if (_cond_incl_inactive())
      return;

    ++it_tok;
    if (it_tok->is_ws())
      ++it_tok;

    if (!it_tok->is_id()) {
      code_remark remark(code_remark::severity::fatal,
			 "identifier expected after #ifdef",
			 _raw_tok_it_to_source(it_tok),
			 it_tok->get_range_in_file());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    _cond_incl_states.emplace (raw_begin);

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
      code_remark remark(code_remark::severity::fatal,
			 "identifier expected after #ifndef",
			 _raw_tok_it_to_source(it_tok),
			 it_tok->get_range_in_file());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    _cond_incl_states.emplace(raw_begin);

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
      code_remark remark(code_remark::severity::fatal,
			 "#elif without #if",
			 _raw_tok_it_to_source(it_tok),
			 it_tok->get_range_in_file());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    if (_cond_incl_nesting > _cond_incl_states.size())
      return;

    if (!_cond_incl_states.top().incl_node) {
      if (_eval_conditional_inclusion(raw_pp_tokens_range{raw_begin, raw_end}))
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
      code_remark remark(code_remark::severity::fatal,
			 "identifier expected after #define",
			 _raw_tok_it_to_source(it_tok),
			 it_tok->get_range_in_file());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    auto it_m_undef = _macro_undefs.find(it_tok->get_value());
    const macro_undef *m_undef = nullptr;
    if (it_m_undef != _macro_undefs.end()) {
      m_undef = &it_m_undef->second.get();
      _macro_undefs.erase(it_m_undef);
    }

    const macro &m =
      _handle_macro_definition(raw_pp_tokens_range{raw_begin, raw_end},
			       m_undef);

    auto it_existing = _macros.find(m.get_name());
    if (it_existing != _macros.end() && it_existing->second.get() != m) {
      code_remark remark(code_remark::severity::fatal,
		"macro redefined in an incompatible way",
		_raw_pp_tokens_range_to_source(m.get_directive_range()),
		_raw_pp_tokens_range_to_range_in_file(m.get_directive_range()));
      _remarks.add(remark);
      throw pp_except(remark);
    }

    _macros.insert(std::make_pair(m.get_name(), std::ref(m)));
  } else if (it_tok->get_value() == "undef") {
    ++it_tok;
    if (it_tok->is_ws())
      ++it_tok;

    if (!it_tok->is_id()) {
      code_remark remark(code_remark::severity::fatal,
			 "identifier expected after #undef",
			 _raw_tok_it_to_source(it_tok),
			 it_tok->get_range_in_file());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    auto it_tok_id = it_tok;
    ++it_tok;
    if (!it_tok->is_newline() && !it_tok->is_eof()) {
      code_remark remark(code_remark::severity::fatal,
			 "garbage after #undef",
			 _raw_tok_it_to_source(it_tok),
			 it_tok->get_range_in_file());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    auto it_m = _macros.find(it_tok_id->get_value());
    if (it_m != _macros.end()) {
      const macro_undef &mu =
	_pp_result->_add_macro_undef(it_m->second,
				     raw_pp_tokens_range{raw_begin, raw_end});
      _macro_undefs.insert(std::make_pair(it_tok_id->get_value(),
					  std::ref(mu)));
      _macros.erase(it_m);
    }

  } else if (it_tok->get_value() == "include") {
    _handle_include(raw_pp_tokens_range{raw_begin, raw_end});

  }
}

preprocessor::_pp_token
preprocessor::_expand(_expansion_state &state,
		      const std::function<_pp_token()> &token_reader,
		      const bool from_cond_incl_cond)
{
  const bool state_is_root = (&state == &_root_expansion_state);
  bool raw_tokens_read = false;
  bool keep_cur_macro_invocation = false;

  auto read_tok = [&]() {
    while (!state.macro_instances.empty()) {
      try {
	auto tok = state.macro_instances.back().read_next_token();
	_grab_remarks_from(state.macro_instances.back());
	if (!tok.is_eof()) {
	  if (state_is_root)
	    tok.set_macro_invocation(_cur_macro_invocation);
	  return tok;
	} else {
	  assert(_cur_macro_invocation);
	  state.macro_instances.pop_back();
	  // During macro argument parsing, we can have state !=
	  // _root_expansion_state, but _root_expansion_state still
	  // being empty.
	  if (state_is_root &&
	      _root_expansion_state.macro_instances.empty() &&
	      !keep_cur_macro_invocation) {
	    _cur_macro_invocation = nullptr;
	  }
	}
      } catch (const pp_except&) {
	_grab_remarks_from(state.macro_instances.back());
	throw;
      }
    }

    raw_tokens_read = state_is_root;
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
      tok.set_type_and_value(pp_token::type::str,
			     (_pp_token_to_source(tok).get_filename()));
      return tok;
    } else if (tok.get_value() == "__LINE__") {
      auto line = (_pp_token_to_source(tok).offset_to_line_col
		   (_pp_token_to_range_in_file(tok).begin)).first;
      tok.set_type_and_value(pp_token::type::pp_number, std::to_string(line));
      return tok;
    } else if (tok.get_value() == "__INCLUDE_LEVEL__") {
      tok.set_type_and_value(pp_token::type::pp_number,
			     std::to_string(_tokenizers.size() - 1));
      return tok;
    } else if (tok.get_value() == "__COUNTER__") {
      tok.set_type_and_value(pp_token::type::pp_number,
			     std::to_string(__counter__++));
      return tok;
    }

    auto m = _macros.find(tok.get_value());
    if (m != _macros.end() && !tok.expansion_history().count(m->second)) {
      if (!m->second.get().is_func_like()) {
	const raw_pp_tokens_range &tok_range = tok.get_token_source();
	if (!_cur_macro_invocation) {
	  assert(_root_expansion_state.macro_instances.empty());
	  assert(state_is_root);
	  assert(tok_range.begin + 1 == tok_range.end);
	  _cur_macro_invocation =
	    &_pp_result->_add_macro_invocation(m->second, tok_range);
	} else {
	  _cur_macro_invocation->_used_macros += m->second;
	}
	state.macro_instances.push_back(
		_handle_object_macro_invocation(m->second, std::move(tok)));
	goto read_next;
      } else {
	// It can happen that the tail result of some previous macro
	// invocation combines with some subsequent raw input tokens
	// to form another function-style macro invocation.  In this
	// case, there should be only a single macro_invocation
	// instance covering the whole range => extend the current
	// one.
	pp_result::macro_invocation * const saved_cur_macro_invocation
	  = _cur_macro_invocation;
	raw_tokens_read = false;
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

	while (!state.pending_tokens.empty())
	  state.pending_tokens.pop();

	assert(raw_tokens_read || _cur_macro_invocation);
	_cur_macro_invocation = saved_cur_macro_invocation;

	if (!_cur_macro_invocation) {
	  assert(raw_tokens_read);
	  // We'll need an active _cur_macro_invocation for the macro
	  // argument expansion. Create one now and let
	  // _handle_func_macro_invocation() called below set its
	  // invocation range's end once it is known.
	  const raw_pp_tokens_range &tok_range = tok.get_token_source();
	  assert(tok_range.begin + 1 == tok_range.end);
	  _cur_macro_invocation =
	    &_pp_result->_add_macro_invocation(m->second, tok_range);
	} else {
	  _cur_macro_invocation->_used_macros += m->second;
	}

	keep_cur_macro_invocation = true;
	state.macro_instances.push_back(_handle_func_macro_invocation(
						m->second,
						tok.get_token_source().begin,
						read_tok,
						&raw_tokens_read));
	keep_cur_macro_invocation = false;
      }
      goto read_next;
    } else if (m == _macros.end()) {
      auto it_m_undef = _macro_undefs.find(tok.get_value());
      if (it_m_undef != _macro_undefs.end()) {
	pp_result::macro_invocation *mi = _cur_macro_invocation;
	if (!mi) {
	  // tok could still come from _root_expansion_state's pending
	  // tokens and thus, some macro expansion.
	  mi = tok.get_macro_invocation();
	}
	if (mi) {
	  mi->_used_macro_undefs += it_m_undef->second;
	} else {
	  tok.set_used_macro_undef(&it_m_undef->second.get());
	}
      }
    }

    if (from_cond_incl_cond && tok.get_value() == "defined") {
      assert(state_is_root);
      const _pp_token defined_tok = tok;
      const raw_pp_token_index invocation_begin = tok.get_token_source().begin;
      do {
	tok = read_tok();
      } while (tok.is_ws() || tok.is_newline() || tok.is_empty());

      // No opening parenthesis?
      if (!tok.is_punctuator("(")) {
	code_remark remark(code_remark::severity::fatal,
			   "operator \"defined\" without arguments",
			   _pp_token_to_source(tok),
			   _pp_token_to_range_in_file(tok));
	_remarks.add(remark);
	throw pp_except(remark);
      }

      std::tuple<_pp_tokens, _pp_tokens, _pp_token> arg
	= _create_macro_arg(read_tok, false, false);
      const _pp_token delim_tok = std::get<2>(arg);
      const char arg_delim = delim_tok.get_value()[0];
      if (arg_delim == ',') {
	code_remark remark(code_remark::severity::fatal,
			   "too many arguments to \"defined\" operator",
			   _pp_token_to_source(std::get<2>(arg)),
			   _pp_token_to_range_in_file(std::get<2>(arg)));
	_remarks.add(remark);
	throw pp_except(remark);
      }
      assert(arg_delim == ')');

      if (std::get<0>(arg).size() != 1 ||
	  std::get<0>(arg)[0].get_type() != pp_token::type::id) {
	code_remark remark(code_remark::severity::fatal,
			   "invalid argument to \"defined\" operator",
			   _pp_token_to_source(std::get<0>(arg)[0]),
			   _pp_token_to_range_in_file(std::get<0>(arg)[0]));
	_remarks.add(remark);
	throw pp_except(remark);
      }

      pp_result::macro_invocation *mi = defined_tok.get_macro_invocation();
      if (mi) {
	// Extend the macro expansion the "defined" token came from,
	// if necessary.
	if (!delim_tok.get_macro_invocation()) {
	  _pp_result->_extend_macro_invocation_range
	    (*mi, delim_tok.get_token_source().end);
	} else {
	  assert(delim_tok.get_macro_invocation() == mi);
	}
      }
      const std::string &id = std::get<0>(arg)[0].get_value();
      bool is_defined = false;
      const macro_undef *used_macro_undef = nullptr;
      auto it_m = _macros.find(id);
      if (it_m != _macros.end()) {
	if (!mi) {
	  // Add a macro invocation to record this macro usage against,
	  // if any.
	  mi = &_pp_result->_add_macro_invocation
		(it_m->second,
		 raw_pp_tokens_range{defined_tok.get_token_source().begin,
				     delim_tok.get_token_source().end});
	} else {
	  mi->_used_macros += it_m->second;
	}

	is_defined = true;
      } else {
	auto it_m_undef = _macro_undefs.find(id);
	if (it_m_undef != _macro_undefs.end()) {
	  if (mi) {
	    // Add this macro undef to the macro_invocation the
	    // "defined" token came from.
	    mi->_used_macro_undefs += it_m_undef->second;
	  } else {
	    used_macro_undef = &it_m_undef->second.get();
	  }
	}
      }

      const raw_pp_token_index invocation_end =
	delim_tok.get_token_source().end;
      if (mi) {
	_pp_token result_tok{pp_token::type::pp_number,
			     is_defined ? "1" : "0",
			     raw_pp_tokens_range{invocation_begin,
						 invocation_end}};
	result_tok.set_macro_invocation(mi);

      } else {
	return _pp_token(pp_token::type::pp_number,
			 is_defined ? "1" : "0",
			 raw_pp_tokens_range{invocation_begin, invocation_end},
			 used_macro_undef);
      }
    }
  }

  return tok;
}


preprocessor::_pp_token::
_pp_token(const pp_token::type type, const std::string &value,
	  const raw_pp_tokens_range &token_source,
	  const class used_macros &eh)
  : _value(value), _token_source(token_source), _macro_invocation(nullptr),
    _used_macro_undef(nullptr), _expansion_history(std::move(eh)),
    _type(type)
{}

preprocessor::_pp_token::
_pp_token(const pp_token::type type, const std::string &value,
	  const raw_pp_tokens_range &token_source,
	  const macro_undef *used_macro_undef)
  : _value(value), _token_source(token_source), _macro_invocation(nullptr),
    _used_macro_undef(used_macro_undef), _type(type)
{}

void preprocessor::_pp_token::set_type_and_value(const pp_token::type type,
						 const std::string &value)
{
  _type = type;
  _value = value;
}

void preprocessor::_pp_token::
set_macro_invocation(pp_result::macro_invocation *mi) noexcept
{
  assert(!_macro_invocation && !_used_macro_undef);
  _macro_invocation = mi;
}

pp_result::macro_invocation *
preprocessor::_pp_token::get_macro_invocation() const noexcept
{
  return _macro_invocation;
}

void preprocessor::_pp_token::
set_used_macro_undef(const macro_undef *used_macro_undef) noexcept
{
  assert(!_macro_invocation && !_used_macro_undef);
  _used_macro_undef = used_macro_undef;
}

const macro_undef* preprocessor::_pp_token::
get_used_macro_undef() const noexcept
{
  return _used_macro_undef;
}

std::string preprocessor::_pp_token::stringify() const
{
  return pp_token::stringify(_type, _value);
}

void preprocessor::_pp_token::concat(const _pp_token &tok,
				     preprocessor &p,
				     code_remarks &remarks)
{
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
      code_remark remark
	(code_remark::severity::fatal,
	 "can't concatenate " + _value + " and " + tok._value,
	 p._pp_token_to_source(tok), p._pp_token_to_range_in_file(tok));
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
    code_remark remark(code_remark::severity::fatal,
		       "can't concatenate tokens of different type",
		       p._pp_token_to_source(tok),
		       p._pp_token_to_range_in_file(tok));
    remarks.add(remark);
    throw pp_except(remark);
  } else if(_type == pp_token::type::chr || _type == pp_token::type::wchr) {
    code_remark remark(code_remark::severity::fatal,
		       "can't concatenate character constants",
		       p._pp_token_to_source(tok),
		       p._pp_token_to_range_in_file(tok));
    remarks.add(remark);
    throw pp_except(remark);
  } else if (_type == pp_token::type::non_ws_char) {
    code_remark remark(code_remark::severity::fatal,
		       "can't concatenate " + _value + " and " + tok._value,
		       p._pp_token_to_source(tok),
		       p._pp_token_to_range_in_file(tok));
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
      code_remark remark
	(code_remark::severity::fatal,
	 "can't concatenate " + _value + " and " + tok._value,
	 p._pp_token_to_source(tok), p._pp_token_to_range_in_file(tok));
      remarks.add(remark);
      throw pp_except(remark);
    }
    return;
  }
}


preprocessor::_macro_instance
preprocessor::_handle_object_macro_invocation(const macro &macro,
					      _pp_token &&id_tok)
{
  return _macro_instance(*this, macro,
			 std::vector<_pp_tokens>(), std::vector<_pp_tokens>(),
			 id_tok.get_token_source());
}

preprocessor::_macro_instance
preprocessor::_handle_func_macro_invocation(
	const macro &macro,
	const raw_pp_token_index invocation_begin,
	const std::function<_pp_token()> &token_reader,
	const bool *update_cur_macro_invocation_range)
{
  std::vector<_pp_tokens> args;
  std::vector<_pp_tokens> exp_args;

  raw_pp_token_index invocation_end;
  if (macro.non_va_arg_count() || macro.is_variadic()) {
    size_t i_arg = 0;
    const size_t n_args =  macro.non_va_arg_count() + macro.is_variadic();
    args.reserve(n_args);
    exp_args.reserve(n_args);
    while (i_arg < n_args) {
      std::tuple<_pp_tokens, _pp_tokens, _pp_token> cur_arg
	= _create_macro_arg(token_reader, macro.shall_expand_arg(i_arg),
			    i_arg == macro.non_va_arg_count());
      ++i_arg;
      args.push_back(std::move(std::get<0>(cur_arg)));
      exp_args.push_back(std::move(std::get<1>(cur_arg)));

      assert(std::get<2>(cur_arg).get_type() == pp_token::type::punctuator);
      assert(std::get<2>(cur_arg).get_value().size() == 1);
      const char arg_delim = std::get<2>(cur_arg).get_value()[0];
      if (i_arg < (!macro.is_variadic() ? n_args : n_args - 1)) {
	if (arg_delim != ',') {
	  assert(arg_delim == ')');
	  code_remark remark
	    (code_remark::severity::fatal,
	     "too few parameters in macro invocation",
	     _pp_token_to_source(std::get<2>(cur_arg)),
	     _pp_token_to_range_in_file(std::get<2>(cur_arg)));
	  _remarks.add(remark);
	  throw pp_except(remark);
	}
      } else if (i_arg == n_args - 1) {
	// A variadic argument might simply have been left out.
	assert(macro.is_variadic());
	if (arg_delim == ')') {
	  // Add empty dummy argument.
	  args.emplace_back();
	  exp_args.emplace_back();
	  ++i_arg;
	}
      } else if (i_arg == n_args) {
	if (arg_delim != ')') {
	  assert(arg_delim == ',');
	  code_remark remark
	    (code_remark::severity::fatal,
	     "too many parameters in macro invocation",
	     _pp_token_to_source(std::get<2>(cur_arg)),
	     _pp_token_to_range_in_file(std::get<2>(cur_arg)));
	  _remarks.add(remark);
	  throw pp_except(remark);
	}
      }

      if (i_arg == n_args) {
	assert(arg_delim == ')');
	const raw_pp_tokens_range &delim_range =
	  std::get<2>(cur_arg).get_token_source();
	invocation_end = delim_range.end;

	if (*update_cur_macro_invocation_range) {
	  assert(delim_range.begin + 1 == delim_range.end);
	  _pp_result->_extend_macro_invocation_range(*_cur_macro_invocation,
						     delim_range.end);
	}
      }
    }
  } else {
    // Slurp in the tokens between the () and make sure they're all empties.
    std::tuple<_pp_tokens, _pp_tokens, _pp_token> dummy_arg
      = _create_macro_arg(token_reader, false, false);
    assert(std::get<2>(dummy_arg).get_type() == pp_token::type::punctuator);
    bool non_empty_found = false;
    for (auto tok : std::get<0>(dummy_arg)) {
      if (!tok.is_empty() && !tok.is_ws()) {
	non_empty_found = true;
	break;
      }
    }
    if (non_empty_found || std::get<2>(dummy_arg).get_value() != ")") {
      code_remark remark
	(code_remark::severity::fatal,
	 "too many parameters in macro invocation",
	 _pp_token_to_source(std::get<2>(dummy_arg)),
	 _pp_token_to_range_in_file(std::get<2>(dummy_arg)));
      _remarks.add(remark);
      throw pp_except(remark);
    }

    const raw_pp_tokens_range &delim_range =
      std::get<2>(dummy_arg).get_token_source();
    invocation_end = delim_range.end;
    if (*update_cur_macro_invocation_range) {
      assert(delim_range.begin + 1 == delim_range.end);
      _pp_result->_extend_macro_invocation_range(*_cur_macro_invocation,
						 delim_range.end);
    }
  }

  return _macro_instance(*this, macro,
			 std::move(args), std::move(exp_args),
			 raw_pp_tokens_range{invocation_begin, invocation_end});
}

std::tuple<preprocessor::_pp_tokens, preprocessor::_pp_tokens,
	   preprocessor::_pp_token>
preprocessor::_create_macro_arg(const std::function<_pp_token()> &token_reader,
				const bool expand, const bool variadic)
{
  _pp_tokens arg;
  _pp_tokens exp_arg;
  std::size_t lparens = 0;
  _pp_token end_delim(pp_token::type::eof, std::string(), raw_pp_tokens_range());

  _pp_token pending_tok(pp_token::type::eof, std::string(), raw_pp_tokens_range());
  bool pending_tok_valid = false;

  bool leading_ws = true; // Strip leading whitespace.
  auto read_arg_tok = [&]() {
    _pp_token tok =
      [&]() {
	if (!pending_tok_valid || pending_tok.is_ws()) {
	  while (true) {
	    _pp_token tok = token_reader();

	    if (tok.is_empty())
	      continue;

	    if (tok.is_eof()) {
	      code_remark remark
		(code_remark::severity::fatal,
		 "no closing right parenthesis in macro invocation",
		 _pp_token_to_source(tok), _pp_token_to_range_in_file(tok));
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
			       end_delim.get_token_source());

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

std::pair<used_macros, used_macro_undefs>
preprocessor::_drop_pp_tokens_tail(const pp_tokens::size_type new_end)
{
  used_macros um;
  used_macro_undefs umu;

  // First collect the used_macros and used_macro_undefs from all
  // macro_invocations overlapping with the to be dropped tail.
  const pp_tokens::const_iterator tail_begin
    = _pp_result->get_pp_tokens().cbegin() + new_end;

  if (tail_begin == _pp_result->get_pp_tokens().end())
    return std::make_pair(um, umu);

  const auto tail_macro_invocations
    = (_pp_result->find_overlapping_macro_invocations
       (raw_pp_tokens_range{tail_begin->get_token_source().begin,
			    _pp_result->get_raw_tokens().size()}));
  for (auto it_mi = tail_macro_invocations.first;
       it_mi != tail_macro_invocations.second; ++it_mi) {
    um += it_mi->get_used_macros();
    umu += it_mi->get_used_macro_undefs();
  }

  // Collect used_macro_undefs from the pp_tokens themselves.
  for(auto it_tok = tail_begin; it_tok != _pp_result->get_pp_tokens().end();
      ++it_tok) {
    if (it_tok->get_used_macro_undef())
      umu += *it_tok->get_used_macro_undef();
  }

  // And finally drop the tail.
  _pp_result->_drop_pp_tokens_tail(new_end);

  return std::make_pair(std::move(um), std::move(umu));
}


used_macros preprocessor::_collect_used_macros(const pp_result &pp_result)
{
  used_macros result;

  for (auto &mi : pp_result._macro_invocations) {
    result += mi->_used_macros;
  }

  return result;
}

used_macro_undefs
preprocessor::_collect_used_macro_undefs(const pp_result &pp_result)
{
  used_macro_undefs result;

  for (auto &mi : pp_result._macro_invocations) {
    result += mi->_used_macro_undefs;
  }

  return result;
}

used_macro_undefs
preprocessor::_collect_used_macro_undefs(const pp_tokens &toks)
{
  used_macro_undefs result;

  for (auto tok : toks) {
    if (tok.get_used_macro_undef())
      result += *tok.get_used_macro_undef();
  }

  return result;
}

void preprocessor::_handle_include(const raw_pp_tokens_range &directive_range)
{
  const raw_pp_tokens::const_iterator raw_begin =
    _pp_result->get_raw_tokens().begin() + directive_range.begin;
  const raw_pp_tokens::const_iterator raw_end =
    _pp_result->get_raw_tokens().begin() + directive_range.end;
  auto it_raw_tok = raw_begin;

  assert(it_raw_tok->is_punctuator("#"));
  ++it_raw_tok;

  if (it_raw_tok->is_ws())
    ++it_raw_tok;

  assert(it_raw_tok->is_id());
  assert(it_raw_tok->get_value() == "include");
  ++it_raw_tok;

  if (it_raw_tok->is_ws())
    ++it_raw_tok;

  assert(it_raw_tok != raw_end);

  auto &&raw_it_to_range_in_file =
    [&](const raw_pp_tokens::const_iterator &it_raw) {
      // Shrink range to start location
      return range_in_file{it_raw->get_range_in_file().begin};
    };
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
      code_remark remark(code_remark::severity::fatal,
			 "garbage after #include",
			 _raw_tok_it_to_source(it_raw_tok),
			 raw_it_to_range_in_file(it_raw_tok));
      _remarks.add(remark);
      throw pp_except(remark);
    }
  } else {
    // Run a macro expansion. Temporarily append the expanded pp_token
    // instances to _pp_result and drop them afterwards again.
    auto read_tok = [&]() {
      if (it_raw_tok->is_newline() || it_raw_tok->is_eof()) {
	const raw_pp_token_index eof_index =
	  _pp_result->get_raw_tokens().size();
	return _pp_token(pp_token::type::eof, std::string(),
			 raw_pp_tokens_range{eof_index, eof_index});
      }

      const raw_pp_token_index tok_index =
	it_raw_tok - _pp_result->get_raw_tokens().cbegin();
      _pp_token tok{it_raw_tok->get_type(), it_raw_tok->get_value(),
		    raw_pp_tokens_range{tok_index}};
      ++it_raw_tok;
      return tok;
    };

    assert(_root_expansion_state.macro_instances.empty());
    assert(_root_expansion_state.pending_tokens.empty());
    auto exp_read_tok =
      [&]() -> pp_token_index {
	_pp_token tok = _expand(_root_expansion_state, read_tok, false);
	return _emit_pp_token(*_pp_result, std::move(tok));
      };

    const pp_tokens::size_type temp_tokens_tail_begin
      = _pp_result->get_pp_tokens().size();
    while (true) {
      auto tok = exp_read_tok();
      if (_pp_result->get_pp_tokens()[tok].is_eof())
	break;
    }

    auto skip = [&] (pp_tokens::const_iterator it) {
	while(it->is_type_any_of<pp_token::type::ws,
				 pp_token::type::empty>()) {
	assert(!it->is_newline());
	++it;
      }
      return it;
    };

    auto it_tok = _pp_result->get_pp_tokens().cbegin() + temp_tokens_tail_begin;
    it_tok = skip(it_tok);

    if (it_tok->is_eof()) {
      code_remark remark(code_remark::severity::fatal,
			 "macro expansion at #include evaluated to nothing",
			 _raw_tok_it_to_source(raw_begin),
			 raw_it_to_range_in_file(raw_begin));
      _remarks.add(remark);
      throw pp_except(remark);
    }

    if (it_tok->is_punctuator("<")) {
      is_qstr = false;

      ++it_tok;
      for (auto it_next = skip(it_tok + 1); !it_next->is_eof();
	   it_tok = it_next, it_next = skip(it_next + 1)) {
	unresolved += it_tok->stringify();
      }

      assert(!it_tok->is_eof());
      if (!it_tok->is_punctuator(">")) {
	code_remark remark
	  (code_remark::severity::fatal,
	   "macro expansion at #include does not end with '>'",
	   _raw_tok_it_to_source(raw_begin),
	   raw_it_to_range_in_file(raw_begin));
	_remarks.add(remark);
	throw pp_except(remark);
      } else if (unresolved.empty()) {
	code_remark remark(code_remark::severity::fatal,
			       "macro expansion at #include gives \"<>\"",
			       _raw_tok_it_to_source(raw_begin),
			       raw_it_to_range_in_file(raw_begin));
	_remarks.add(remark);
	throw pp_except(remark);
      }
    } else if (it_tok->get_type() == pp_token::type::str) {
      is_qstr = true;
      unresolved = it_tok->get_value();

      it_tok = skip(it_tok + 1);
      if (!it_tok->is_eof()) {
	code_remark remark
	  (code_remark::severity::fatal,
	   "macro expansion at #include yields garbage at end",
	   _raw_tok_it_to_source(raw_begin),
	   raw_it_to_range_in_file(raw_begin));
	_remarks.add(remark);
	throw pp_except(remark);
      }
    } else {
      code_remark remark(code_remark::severity::fatal,
			 "macro expansion at #include doesn't conform",
			 _raw_tok_it_to_source(raw_begin),
			 raw_it_to_range_in_file(raw_begin));
      _remarks.add(remark);
      throw pp_except(remark);
    }

    std::tie(um, umu) = _drop_pp_tokens_tail(temp_tokens_tail_begin);
  }

  std::string resolved
    = (is_qstr ?
       _header_resolver.resolve(unresolved,
				(_tokenizers.top().get_header_inclusion_node()
				 .get_filename())) :
       _header_resolver.resolve(unresolved));

  if (resolved.empty()) {
    code_remark remark(code_remark::severity::fatal,
		       "could not find header from #include",
		       _raw_tok_it_to_source(raw_begin),
		       raw_it_to_range_in_file(raw_begin));
    _remarks.add(remark);
    throw pp_except(remark);
  }

  pp_result::header_inclusion_child &new_header_inclusion_node
    = _inclusions.top().get()._add_header_inclusion(resolved,
						    directive_range,
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

  pp_result::conditional_inclusion_node &new_conditional_inclusion_node =
    (_inclusions.top().get()._add_conditional_inclusion
     (cond_incl_state.range_begin, std::move(cond_incl_state.um),
      std::move(cond_incl_state.umu)));

  cond_incl_state.incl_node = &new_conditional_inclusion_node;
  cond_incl_state.branch_active = true;
  _inclusions.emplace(std::ref(new_conditional_inclusion_node));
}

void preprocessor::_pop_cond_incl(const raw_pp_token_index range_end)
{
  _cond_incl_state &cond_incl_state = _cond_incl_states.top();

  if (!cond_incl_state.incl_node) {
    // None of the branches had been taken. Insert the conditional inclusion
    // into the inclusion tree now.
    pp_result::conditional_inclusion_node &node
      = (_inclusions.top().get()._add_conditional_inclusion
	 (cond_incl_state.range_begin, std::move(cond_incl_state.um),
	  std::move(cond_incl_state.umu)));
    node._set_range_end(range_end);

  } else {
    cond_incl_state.incl_node->_set_range_end(range_end);
    assert(cond_incl_state.incl_node == &_inclusions.top().get());
    _inclusions.pop();
  }

  _cond_incl_states.pop();
}

bool preprocessor::
_eval_conditional_inclusion(const raw_pp_tokens_range &directive_range)
{
  const raw_pp_tokens::const_iterator raw_begin =
    _pp_result->get_raw_tokens().begin() + directive_range.begin;
  auto it_raw_tok = raw_begin + 1;
  if (it_raw_tok->is_ws())
    ++it_raw_tok;
  assert(it_raw_tok->get_type() == pp_token::type::id &&
	 (it_raw_tok->get_value() == "if" ||
	  it_raw_tok->get_value() == "elif"));
  ++it_raw_tok;

  // Temporarily append the expanded pp_token instances from the
  // condition to _pp_result. They'll get purged again when we're done
  // with them.
  _fixup_inclusion_node_ranges();
  auto read_tok =
    [&]() {
      if (it_raw_tok->is_newline() || it_raw_tok->is_eof()) {
	const raw_pp_token_index eof_index =
	  _pp_result->get_raw_tokens().size();
	return _pp_token(pp_token::type::eof, std::string(),
			 raw_pp_tokens_range{eof_index, eof_index});
      }

      const raw_pp_token_index tok_index =
	it_raw_tok - _pp_result->get_raw_tokens().cbegin();
      _pp_token tok{it_raw_tok->get_type(), it_raw_tok->get_value(),
		    raw_pp_tokens_range{tok_index}};
      ++it_raw_tok;
      return tok;
    };

  assert(_root_expansion_state.macro_instances.empty());
  assert(_root_expansion_state.pending_tokens.empty());
  auto exp_read_tok =
    [&]() -> pp_token_index {
      _pp_token tok = _expand(_root_expansion_state, read_tok, true);
      return _emit_pp_token(*_pp_result, std::move(tok));
    };

  const pp_tokens::size_type temp_tokens_tail_begin
    = _pp_result->get_pp_tokens().size();
  yy::pp_expr_parser_driver pd(exp_read_tok, *_pp_result);
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

  auto um_umu = _drop_pp_tokens_tail(temp_tokens_tail_begin);
  _cond_incl_states.top().um += std::move(um_umu.first);
  _cond_incl_states.top().umu += std::move(um_umu.second);

  return result;
}

const macro& preprocessor::
_handle_macro_definition(const raw_pp_tokens_range &directive_range,
			 const macro_undef *macro_undef)
{
  const raw_pp_tokens::const_iterator begin =
    _pp_result->get_raw_tokens().begin() + directive_range.begin;
  raw_pp_tokens::const_iterator end =
    _pp_result->get_raw_tokens().begin() + directive_range.end;
  assert (begin != end);
  assert(begin->is_punctuator("#"));

  assert((end - 1)->is_newline() || (end - 1)->is_eof());
  // end is decremented by one such that we can dereference
  // it even for it == end from now on.
  --end;

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
    code_remark remark(code_remark::severity::fatal,
		       "no identifier following #define",
		       _raw_tok_it_to_source(it),
		       it->get_range_in_file());
    _remarks.add(remark);
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
	code_remark remark(code_remark::severity::fatal,
			   "garbage in macro argument list after ...",
			   _raw_tok_it_to_source(it),
			   it->get_range_in_file());
	_remarks.add(remark);
	throw pp_except(remark);
      }

      if (!arg_names.empty() && !it->is_punctuator(",")) {
	code_remark remark(code_remark::severity::fatal,
			   "comma expected in macro argument list",
			   _raw_tok_it_to_source(it),
			   it->get_range_in_file());
	_remarks.add(remark);
	throw pp_except(remark);
      } else if (it->is_punctuator(",")) {
	if (arg_names.empty()) {
	  code_remark remark(code_remark::severity::fatal,
			     "leading comma in macro argument list",
			     _raw_tok_it_to_source(it),
			     it->get_range_in_file());
	  _remarks.add(remark);
	  throw pp_except(remark);
	}
	it = skip_ws(it + 1);
	if (it == end || it->is_punctuator(")")) {
	  code_remark remark(code_remark::severity::fatal,
			     "trailing comma in macro argument list",
			     _raw_tok_it_to_source(it),
			     it->get_range_in_file());
	  _remarks.add(remark);
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
	  code_remark remark
	    (code_remark::severity::fatal,
	     "__VA_ARGS__ not allowed as a macro argument name",
	     _raw_tok_it_to_source(it), it->get_range_in_file());
	  _remarks.add(remark);
	  throw pp_except(remark);
	} else if (!unique_arg_names.insert(it->get_value()).second) {
	  code_remark remark(code_remark::severity::fatal,
			     "duplicate macro argument name",
			     _raw_tok_it_to_source(it),
			     it->get_range_in_file());
	  _remarks.add(remark);
	  throw pp_except(remark);
	}

	arg_names.push_back(it->get_value());
	it = skip_ws(it + 1);
	if (it->is_punctuator("...")) {
	  variadic = true;
	  it = skip_ws(it + 1);
	}
      } else {
	code_remark remark(code_remark::severity::fatal,
			   "garbage in macro argument list",
			   _raw_tok_it_to_source(it),
			   it->get_range_in_file());
	_remarks.add(remark);
	throw pp_except(remark);
      }
    }
    if (it == end) {
      code_remark remark(code_remark::severity::fatal,
			 "macro argument list not closed",
			 _raw_tok_it_to_source(it),
			 end->get_range_in_file());
      _remarks.add(remark);
      throw pp_except(remark);
    }

    assert(it->is_punctuator(")"));
    ++it;
  }

  if (it != end && !it->is_ws()) {
    code_remark remark(code_remark::severity::warning,
		       "no whitespace before macro replacement list",
		       _raw_tok_it_to_source(it),
		       it->get_range_in_file());
    _remarks.add(remark);
  }

  it = skip_ws(it);

  return (_pp_result->_add_macro
	  (name, func_like, variadic, std::move(arg_names),
	   _normalize_macro_repl(it, end, func_like, unique_arg_names),
	   directive_range, macro_undef));
}

raw_pp_tokens preprocessor::
_normalize_macro_repl(const raw_pp_tokens::const_iterator begin,
		      const raw_pp_tokens::const_iterator end,
		      const bool func_like,
		      const std::set<std::string> &arg_names)
{
  if (begin == end)
    return raw_pp_tokens();

  assert(!begin->is_ws() && !(end-1)->is_ws());

  if (begin->is_punctuator("##")) {
    code_remark remark(code_remark::severity::fatal,
		       "## at beginning of macro replacement list",
		       _raw_tok_it_to_source(begin),
		       begin->get_range_in_file());
    _remarks.add(remark);
    throw pp_except(remark);
  } else if ((end - 1)->is_punctuator("##")) {
    code_remark remark(code_remark::severity::fatal,
		       "## at end of macro replacement list",
		       _raw_tok_it_to_source(end - 1),
		       (end - 1)->get_range_in_file());
    _remarks.add(remark);
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
	code_remark remark(code_remark::severity::fatal,
			   "# in func-like macro not followed by parameter",
			   _raw_tok_it_to_source(end - 1),
			   (end - 1)->get_range_in_file());
	_remarks.add(remark);
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

preprocessor::_macro_instance::
_macro_instance(preprocessor &preprocessor,
		const macro &macro,
		std::vector<_pp_tokens> &&args,
		std::vector<_pp_tokens> &&exp_args,
		const raw_pp_tokens_range &invocation_range)
  : _preprocessor(preprocessor), _macro(macro),
    _invocation_range(invocation_range),
    _it_repl(_macro.get_repl().cbegin()), _cur_arg(nullptr),
    _emit_empty_tok(true), _last_tok_was_empty_or_ws(false)
{
  assert(args.size() == exp_args.size());

  auto arg_it = args.begin();
  auto exp_arg_it = exp_args.begin();
  size_t i = 0;
  for (auto arg_name : _macro.get_arg_names()) {
    assert(_macro.shall_expand_arg(i) || exp_arg_it->empty());
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
  if (!_macro.is_func_like())
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


bool preprocessor::_macro_instance::
_is_stringification(raw_pp_tokens::const_iterator it) const noexcept
{
  assert(it != _macro.get_repl().end());

  if (!_macro.is_func_like() || !it->is_punctuator("#"))
    return false;

  assert((it + 1) != _macro.get_repl().end() && (it + 1)->is_id());

  return true;
}

raw_pp_tokens::const_iterator preprocessor::_macro_instance::
_skip_stringification_or_single(const raw_pp_tokens::const_iterator &it)
  const noexcept
{
  assert(it != _macro.get_repl().end());

  if (_macro.is_func_like() && it->is_punctuator("#")) {
    assert(it + 1 != _macro.get_repl().end());
    assert((it + 1)->is_id());
    return it + 2;
  }
  return it + 1;
}

bool preprocessor::_macro_instance::
_is_concat_op(const raw_pp_tokens::const_iterator &it) const noexcept
{
  return (it != _macro.get_repl().end() && it->is_punctuator("##"));
}

preprocessor::_pp_token preprocessor::_macro_instance::_handle_stringification()
{
  assert(_it_repl != _macro.get_repl().end());
  assert(_it_repl->is_punctuator("#"));
  assert(_it_repl + 1 != _macro.get_repl().end());
  assert((_it_repl + 1)->is_id());

  auto arg = _resolve_arg((_it_repl + 1)->get_value(), false);
  assert(arg);

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
		   _invocation_range);
}

void preprocessor::_macro_instance::_add_concat_token(const _pp_token &tok)
{
  assert(!tok.is_ws() && !tok.is_newline() && !tok.is_eof());

  if (!_concat_token) {
    _concat_token.reset(new _pp_token(tok.get_type(), tok.get_value(),
				      _invocation_range,
				      _tok_expansion_history_init()));
    _concat_token->expansion_history() += tok.expansion_history();
  } else {
    _concat_token->concat(tok, _preprocessor, _remarks);
  }
}

void preprocessor::_macro_instance::
_add_concat_token(const raw_pp_token &raw_tok)
{
  assert(!raw_tok.is_ws() && !raw_tok.is_newline() && !raw_tok.is_eof());

  if (!_concat_token) {
    _concat_token.reset(new _pp_token(raw_tok.get_type(), raw_tok.get_value(),
				      _invocation_range));
  } else {
    _pp_token tok {raw_tok.get_type(), raw_tok.get_value(),
		   _invocation_range};
    _concat_token->concat(tok, _preprocessor, _remarks);
  }
}

preprocessor::_pp_token preprocessor::_macro_instance::_yield_concat_token()
{
  assert(_concat_token);
  _pp_token tok = std::move(*_concat_token);
  _concat_token.reset();
  assert(!tok.is_ws() && !tok.is_empty());
  _last_tok_was_empty_or_ws = false;
  return tok;
}

preprocessor::_pp_token preprocessor::_macro_instance::_yield_empty_token()
{
  assert(!_last_tok_was_empty_or_ws);
  _last_tok_was_empty_or_ws = true;
  _emit_empty_tok = false;
  return _pp_token{pp_token::type::empty, std::string{},
		   raw_pp_tokens_range{}};
}


preprocessor::_pp_token preprocessor::_macro_instance::read_next_token()
{
  // This is the core of macro expansion.
  //
  // The next replacement token to emit is determined dynamically
  // based on the current _macro_instance's state information and
  // returned.
  //
  // The macro expansion will insert empty dummy tokens at various
  // locations to allow the higher levels,
  // preprocessor::read_next_token() in particular, to separate
  // certain productions from surrounding context. These locations are
  // - before and after the whole macro expansion
  // - before and after a concatentation chain
  // - before and after the emission of an expanded macro parameter.
  // This is needed such that
  //    #define __concat(a,b) :a ## b:
  //    :__concat(::a,b::):
  // can evaluate to
  //    : ::ab:: :
  // and so on.
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
  //
  if (_emit_empty_tok) {
    _emit_empty_tok = false;
    if (!_last_tok_was_empty_or_ws)
      return _yield_empty_token();
  }

  // Continue processing the currently "active" parameter, if any.
  bool in_concat = false;
  if (_cur_arg) {
  handle_arg:
    assert(_cur_arg);
    if (_cur_arg->empty()) {
      _cur_arg = nullptr;
      ++_it_repl;
      if (_is_concat_op(_it_repl)) {
	in_concat = true;
	++_it_repl;
      } else if (in_concat) {
	// Last parameter in a sequence of concatenations.
	// Emit an empty token afterwards.
	in_concat = false;
	if (_concat_token) {
	  _emit_empty_tok = true;
	  return _yield_concat_token();
	}
      } else {
	// "Normal" parameter expansion, empty has already been emitted
	// before it started.
	assert(_last_tok_was_empty_or_ws);
      }
    } else if (_cur_arg_it == _cur_arg->begin() && in_concat) {
      // We're at the beginning of a parameter's replacement list and
      // that parameter has been preceeded by a ## concatenation
      // operator in the macro replacement list.
      assert(_cur_arg_it != _cur_arg->end());
      assert(!_cur_arg_it->is_ws());
      assert(!_cur_arg_it->is_empty());

      _add_concat_token(*_cur_arg_it++);

      assert(_cur_arg_it == _cur_arg->end() || !_cur_arg_it->is_empty());
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
	in_concat = false;
	if (!_cur_arg) {
	  // Last parameter in a sequence of concatenations.
	  // Emit an empty token afterwards.
	  _emit_empty_tok = true;
	}
	return _yield_concat_token();
      }
      // The else case, i.e. the current argument's replacement list
      // has been exhausted and the parameter is followed by a ##
      // concatenation operator in the macro's replacement list, is
      // handled below.
      assert(_it_repl != _macro.get_repl().end());
      assert(_it_repl->is_punctuator("##"));
      ++_it_repl;
      assert(_it_repl != _macro.get_repl().end());

    } else if (_is_concat_op(_it_repl + 1) &&
	       _cur_arg_it + 1 == _cur_arg->end()) {
      // This is the last token in the current parameter's replacement
      // list and the next token is a ## operator. Prepare for the
      // concatenation.
      assert(!_cur_arg_it->is_empty());
      assert(!_cur_arg_it->is_ws());

      // The in_concat == true case would have been handled by the
      // branch above.
      assert(!in_concat);
      in_concat = true;

      _add_concat_token(*_cur_arg_it++);

      assert (_cur_arg_it == _cur_arg->end());
      _cur_arg = nullptr;
      _it_repl += 2;
    } else {
      // Normal parameter substitution
      assert(!in_concat);
      assert(_cur_arg_it != _cur_arg->end());

      auto tok = _pp_token(_cur_arg_it->get_type(), _cur_arg_it->get_value(),
			   _invocation_range, _tok_expansion_history_init());
      tok.expansion_history() += _cur_arg_it->expansion_history();
      ++_cur_arg_it;
      _last_tok_was_empty_or_ws = (tok.is_empty() || tok.is_ws());
      if (_cur_arg_it == _cur_arg->cend()) {
	if (!_last_tok_was_empty_or_ws)
	  _emit_empty_tok = true;
	_cur_arg = nullptr;
	++_it_repl;
      }
      return tok;
    }
  }

 from_repl_list:
  assert(_cur_arg == nullptr);

  assert(_it_repl == _macro.get_repl().end() ||
	 !_it_repl->is_punctuator("##"));

  // Process all but the last operands of a ## concatenation.
  while(_it_repl != _macro.get_repl().end() &&
	_is_concat_op(_skip_stringification_or_single(_it_repl))) {
    assert(!_it_repl->is_ws());
    if (_it_repl->is_id()) {
      _cur_arg = _resolve_arg(_it_repl->get_value(), false);
      if (_cur_arg != nullptr) {
	_cur_arg_it = _cur_arg->begin();
	if (!_last_tok_was_empty_or_ws) {
	  // The start of a concatenation sequence.  Emit an empty
	  // token to separate the result from the tokens preceeding
	  // it.
	  return _yield_empty_token();
	}
	goto handle_arg;
      }
      // Remaining case of non-parameter identifier is handled below.
    } else if (_macro.is_variadic() && _it_repl->is_punctuator(",") &&
	       (_it_repl + 2)->is_id() &&
	       (_it_repl + 2)->get_value() == _macro.get_arg_names().back()) {
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
	if (in_concat) {
	  _add_concat_token(*_it_repl);
	  _it_repl += 2;
	  in_concat = false;
	  _last_tok_was_empty_or_ws = false;
	  return _yield_concat_token();
	} else {
	  _pp_token tok{_it_repl->get_type(), _it_repl->get_value(),
			_invocation_range};
	  _it_repl += 2;
	  _last_tok_was_empty_or_ws = false;
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
    in_concat = true;
  }

  // At this point there's either no ## concatenation in progress
  // or the current token is the very last operand.
  if (_it_repl == _macro.get_repl().end()) {
    assert(!in_concat);
    if (!_last_tok_was_empty_or_ws) {
      // Terminate the macro expansion with an empty in order to
      // separate it from subsequent tokens.
      return _yield_empty_token();
    }
    return _pp_token(pp_token::type::eof, std::string(),
		     raw_pp_tokens_range{_invocation_range.end,
					 _invocation_range.end});
  }

  if (_it_repl->is_id()) {
    _cur_arg = _resolve_arg(_it_repl->get_value(), !in_concat);
    if (_cur_arg != nullptr) {
      _cur_arg_it = _cur_arg->begin();
      if (!in_concat && !_last_tok_was_empty_or_ws) {
	// Start of a "normal" parameter substitution. Emit an empty
	// token in order to separate it from the preceeding one.
	return _yield_empty_token();
      }
      goto handle_arg;
    }
  }

  if (in_concat) {
    assert(!_it_repl->is_ws());
    if (_is_stringification(_it_repl))
      _add_concat_token(_handle_stringification());
    else
      _add_concat_token(*_it_repl++);

    assert(_concat_token);
    in_concat = false;
    _last_tok_was_empty_or_ws = false;
    return _yield_concat_token();
  }

  if (_is_stringification(_it_repl)) {
    _last_tok_was_empty_or_ws = false;
    return _handle_stringification();
  }

  auto tok = _pp_token(_it_repl->get_type(), _it_repl->get_value(),
		       _invocation_range,
		       _tok_expansion_history_init());
  ++_it_repl;

  _last_tok_was_empty_or_ws = (tok.is_empty() || tok.is_ws());
  return tok;
}

const pp_result::header_inclusion_node&
preprocessor::get_pending_token_source(const raw_pp_token_index tok)
{
  _fixup_inclusion_node_ranges();
  return _pp_result->get_raw_token_source(tok);
}

preprocessor::_cond_incl_state::
_cond_incl_state(const pp_token_index _range_begin)
  : range_begin(_range_begin), incl_node(nullptr), branch_active(false)
{}


preprocessor::_expansion_state::_expansion_state() = default;
