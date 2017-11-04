#include <cassert>
#include "ast.hh"

using namespace suse::cp::ast;

template<typename T>
static T* mv_p(T* &&p)
{
	T *_p = nullptr;
	std::swap(p, _p);
	return _p;
}

#ifdef DEBUG_PARSER
std::ostream&
suse::cp::ast::operator<<(std::ostream &o, const pp_tokens_range &r)
{
  o << r.begin << '-' << r.end;
  return o;
}
#endif

_ast_entity::_ast_entity(const pp_tokens_range &tr) noexcept
  : _parent(nullptr), _tokens_range(tr)
{}

_ast_entity::_ast_entity(const _ast_entity &ae) noexcept
  : _parent(nullptr), _tokens_range(ae._tokens_range)
{}

_ast_entity::~_ast_entity() noexcept = default;

void _ast_entity::_extend_tokens_range(const pp_tokens_range &tr) noexcept
{
  assert(_tokens_range.end <= tr.begin);
  _tokens_range.end = tr.end;
}


expr_list::expr_list(expr* &&e)
  : ast_entity(e->get_tokens_range())
{
  auto _e = std::ref(*mv_p(std::move(e)));
  try {
    _exprs.push_back(_e);
  } catch(...) {
    delete &_e.get();
    throw;
  }
  _e.get()._set_parent(*this);
}

expr_list::expr_list(expr_list &&el)
  : ast_entity(el), _exprs(std::move(el._exprs))
{
  for (auto e : _exprs)
    e.get()._reset_parent(*this, el);
}

expr_list::~expr_list()
{
  for (auto e : _exprs)
    delete &e.get();
}

void expr_list::extend(expr* &&e)
{
  auto _e = std::ref(*mv_p(std::move(e)));
  try {
    _exprs.push_back(_e);
  } catch(...) {
    delete &_e.get();
    throw;
  }
  _e.get()._set_parent(*this);
  _extend_tokens_range(_e.get().get_tokens_range());
}

template<typename list_type>
static pp_tokens_range tr_from_aes(const list_type &aes)
{
  assert(aes.size());
  return pp_tokens_range{
    (*aes.begin())->get_tokens_range().begin,
    (*(aes.end() - 1))->get_tokens_range().end
  };
}

expr::expr(const std::initializer_list<expr*> &exprs) noexcept
  : ast_entity(tr_from_aes(exprs))
{
  for (auto e : exprs)
    e->_set_parent(*this);
}
expr::expr(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

expr::~expr() noexcept = default;


expr_comma::expr_comma(expr* &&l, expr* &&r) noexcept
  : expr{l, r}, _left(*mv_p(std::move(l))), _right(*mv_p(std::move(r)))
{}

expr_comma::~expr_comma() noexcept
{
  delete &_left;
  delete &_right;
}


expr_assignment::expr_assignment(const assign_op op, expr* &&lhs, expr* &&rhs)
  noexcept
  : expr{lhs, rhs}, _op(op),
    _lhs(*mv_p(std::move(lhs))), _rhs(*mv_p(std::move(rhs)))
{}

expr_assignment::~expr_assignment() noexcept
{
  delete &_lhs;
  delete &_rhs;
}


expr_conditional::expr_conditional(expr* &&cond,
				   expr* &&expr_true, expr* &&expr_false)
  noexcept
  : expr{cond, expr_true, expr_false},
    _cond(*mv_p(std::move(cond))), _expr_true(mv_p(std::move(expr_true))),
    _expr_false(*mv_p(std::move(expr_false)))
{}

expr_conditional::expr_conditional(expr* &&cond,  expr* &&expr_false) noexcept
  : expr{cond, expr_false},
    _cond(*mv_p(std::move(cond))), _expr_true(nullptr),
    _expr_false(*mv_p(std::move(expr_false)))
{}

expr_conditional::~expr_conditional() noexcept
{
  delete &_cond;
  delete _expr_true;
  delete &_expr_false;
}


expr_binop::expr_binop(const binary_op op, expr* &&l, expr* &&r) noexcept
  : expr{l, r}, _left(*mv_p(std::move(l))), _right(*mv_p(std::move(r)))
{}

expr_binop::~expr_binop() noexcept
{
  delete &_left;
  delete &_right;
}


expr_cast::expr_cast(const pp_tokens_range &tr, type_name* &&tn, expr* &&e)
  noexcept
  : expr(tr), _tn(*mv_p(std::move(tn))), _e(*mv_p(std::move(e)))
{
  _tn._set_parent(*this);
  _e._set_parent(static_cast<expr&>(*this));
}

expr_cast::~expr_cast() noexcept
{
  delete &_tn;
  delete &_e;
}


expr_label_addr::expr_label_addr(const pp_tokens_range &tr,
				 const pp_token_index label_tok) noexcept
  : expr(tr), _label_tok(label_tok)
{}

expr_label_addr::~expr_label_addr() noexcept = default;


expr_unop_pre::expr_unop_pre(const pp_tokens_range &tr,
			     const unary_op_pre op, expr* &&e) noexcept
  : expr(tr), _op(op), _e(*mv_p(std::move(e)))
{
  _e._set_parent(static_cast<expr&>(*this));
}

expr_unop_pre::~expr_unop_pre() noexcept
{
  delete &_e;
}


expr_sizeof_expr::expr_sizeof_expr(const pp_tokens_range &tr, expr* &&e)
  noexcept
  : expr(tr), _e(*mv_p(std::move(e)))
{
  _e._set_parent(static_cast<expr&>(*this));
}

expr_sizeof_expr::~expr_sizeof_expr() noexcept
{
  delete &_e;
}


expr_sizeof_type_name::expr_sizeof_type_name(const pp_tokens_range &tr,
					     type_name* &&tn) noexcept
  : expr(tr), _tn(*mv_p(std::move(tn)))
{
  _tn._set_parent(*this);
}

expr_sizeof_type_name::~expr_sizeof_type_name() noexcept
{
  delete &_tn;
}


expr_array_subscript::expr_array_subscript(const pp_tokens_range &tr,
					   expr* &&base, expr* &&index)
  noexcept
  : expr(tr), _base(*mv_p(std::move(base))), _index(*mv_p(std::move(index)))
{
  _base._set_parent(static_cast<expr&>(*this));
  _index._set_parent(static_cast<expr&>(*this));
}

expr_array_subscript::~expr_array_subscript() noexcept
{
  delete &_base;
  delete &_index;
}


expr_func_invocation::expr_func_invocation(const pp_tokens_range &tr,
		     expr* &&func, expr_list* &&args) noexcept
  : expr(tr), _func(*mv_p(std::move(func))), _args(mv_p(std::move(args)))
{
  _func._set_parent(static_cast<expr&>(*this));
  _args->_set_parent(*this);
}

expr_func_invocation::expr_func_invocation(const pp_tokens_range &tr,
					   expr* &&func) noexcept
  : expr(tr), _func(*mv_p(std::move(func))), _args(nullptr)
{
  _func._set_parent(static_cast<expr&>(*this));
}

expr_func_invocation::~expr_func_invocation() noexcept
{
  delete &_func;
  delete _args;
}


expr_member_deref::expr_member_deref(const pp_tokens_range &tr,
				     const member_deref_type deref_type,
				     expr* &&base,
				     const pp_token_index member_tok)
  noexcept
  : expr(tr), _deref_type(deref_type), _base(*mv_p(std::move(base))),
    _member_tok(member_tok)
{
  _base._set_parent(static_cast<expr&>(*this));
}

expr_member_deref::~expr_member_deref() noexcept
{
  delete &_base;
}


expr_unop_post::expr_unop_post(const pp_tokens_range &tr,
			       const unary_op_post op, expr* &&e) noexcept
  : expr(tr), _op(op), _e(*mv_p(std::move(e)))
{
  _e._set_parent(static_cast<expr&>(*this));
}

expr_unop_post::~expr_unop_post() noexcept
{
  delete &_e;
}


expr_compound_literal::expr_compound_literal(const pp_tokens_range &tr,
					     type_name* &&tn,
					     initializer_list* &&il) noexcept
  : expr(tr), _tn(*mv_p(std::move(tn))), _il(*mv_p(std::move(il)))
{
  _tn._set_parent(*this);
  _il._set_parent(*this);
}

expr_compound_literal::~expr_compound_literal() noexcept
{
  delete &_tn;
  delete &_il;
}


expr_statement::expr_statement(const pp_tokens_range &tr, stmt* &&s) noexcept
  : expr(tr), _s(*mv_p(std::move(s)))
{
  _s._set_parent(*this);
}

expr_statement::~expr_statement() noexcept
{
  delete &_s;
}


expr_id::expr_id(const pp_token_index id_tok) noexcept
  : expr(pp_tokens_range{id_tok, id_tok + 1}), _id_tok(id_tok)
{}

expr_id::~expr_id() noexcept = default;


expr_constant::expr_constant(const pp_token_index const_tok) noexcept
  : expr(pp_tokens_range{const_tok, const_tok + 1}), _const_tok(const_tok)
{}

expr_constant::~expr_constant() noexcept = default;


string_literal::string_literal(const pp_token_index s)
  : ast_entity(pp_tokens_range{s, s+1}), _strings(1, s)
{}

string_literal::~string_literal() noexcept = default;

void string_literal::extend(const pp_token_index s)
{
  _strings.push_back(s);
  _extend_tokens_range(pp_tokens_range{s, s+1});
}


expr_string_literal::expr_string_literal(string_literal* &&sl) noexcept
  : expr(sl->get_tokens_range()), _sl(*mv_p(std::move(sl)))
{
  _sl._set_parent(*this);
}

expr_string_literal::~expr_string_literal() noexcept
{
  delete &_sl;
}


expr_parenthesized::expr_parenthesized(const pp_tokens_range &tr,
					      expr* &&e) noexcept
  : expr(tr), _e(*mv_p(std::move(e)))
{
  _e._set_parent(static_cast<expr&>(*this));
}

expr_parenthesized::~expr_parenthesized() noexcept
{
  delete &_e;
}


attribute::attribute(const pp_token_index name_tok) noexcept
  : ast_entity(pp_tokens_range{name_tok, name_tok + 1}),
    _name_tok(name_tok), _params(nullptr)
{}

attribute::attribute(const pp_tokens_range &tr, const pp_token_index name_tok,
		     expr_list* &&params) noexcept
  : ast_entity(tr), _name_tok(name_tok), _params(mv_p(std::move(params)))
{
  _params->_set_parent(*this);
}

attribute::~attribute() noexcept
{
  delete _params;
}


attribute_list::attribute_list(const pp_tokens_range &tr, attribute* &&a)
  : ast_entity(tr)
{
  attribute * const _a = mv_p(std::move(a));
  try {
    _attributes.push_back(_a);
  } catch(...) {
    delete _a;
    throw;
  }
  if (_a)
    _a->_set_parent(*this);
}

attribute_list::~attribute_list() noexcept
{
  for (auto a : _attributes)
    delete a;
}

void attribute_list::extend(const pp_tokens_range &tr, attribute* &&a)
{
  attribute * const _a = mv_p(std::move(a));
  try {
    _attributes.push_back(_a);
  } catch(...) {
    delete _a;
    throw;
  }
  if (_a)
    _a->_set_parent(*this);
  _extend_tokens_range(tr);
}


attribute_specifier::attribute_specifier(const pp_tokens_range &tr,
					 attribute_list* &&al) noexcept
  : ast_entity(tr), _al(*mv_p(std::move(al)))
{
  _al._set_parent(*this);
}

attribute_specifier::~attribute_specifier() noexcept = default;


attribute_specifier_list::attribute_specifier_list(attribute_specifier* &&as)
  : ast_entity(as->get_tokens_range())
{
  auto _as = std::ref(*mv_p(std::move(as)));
  try {
    _ass.push_back(_as);
  } catch (...) {
    delete &_as.get();
    throw;
  }
  _as.get()._set_parent(*this);
}

attribute_specifier_list::~attribute_specifier_list() noexcept
{
  for (auto as : _ass)
    delete &as.get();
}

void attribute_specifier_list::extend(attribute_specifier* &&as)
{
  auto _as = std::ref(*mv_p(std::move(as)));
  try {
    _ass.push_back(_as);
  } catch (...) {
    delete &_as.get();
    throw;
  }
  _extend_tokens_range(_as.get().get_tokens_range());
  _as.get()._set_parent(*this);
}


pointer::pointer(const pp_tokens_range &tr, type_qualifier_list* &&tql)
  : ast_entity(tr)
{
  type_qualifier_list * const _tql = mv_p(std::move(tql));
  try {
    _tqls.push_back(_tql);
  } catch (...) {
    delete _tql;
    throw;
  }
  if (_tql)
    _tql->_set_parent(*this);
}

pointer::~pointer() noexcept
{
  for (auto tql : _tqls)
    delete tql;
}

void pointer::extend(const pp_tokens_range &tr, type_qualifier_list* &&tql)
{
  type_qualifier_list * const _tql = mv_p(std::move(tql));
  try {
    _tqls.push_back(_tql);
  } catch (...) {
    delete _tql;
    throw;
  }
  _extend_tokens_range(tr);
  if (_tql)
    _tql->_set_parent(*this);
}


direct_abstract_declarator::
direct_abstract_declarator(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

direct_abstract_declarator::~direct_abstract_declarator() noexcept = default;


direct_abstract_declarator_parenthesized::
direct_abstract_declarator_parenthesized(const pp_tokens_range &tr,
					 attribute_specifier_list* &&asl,
					 abstract_declarator* &&ad) noexcept
  : direct_abstract_declarator(tr), _asl(mv_p(std::move(asl))),
    _ad(*mv_p(std::move(ad)))
{
  if (_asl)
    _asl->_set_parent(*this);
  _ad._set_parent(*this);
}

direct_abstract_declarator_parenthesized::
~direct_abstract_declarator_parenthesized() noexcept
{
  delete _asl;
  delete &_ad;
}


direct_abstract_declarator_array::
direct_abstract_declarator_array(const pp_tokens_range &tr,
				 direct_abstract_declarator* &&dad,
				 type_qualifier_list* &&tql,
				 expr* &&size,
				 const bool is_static) noexcept
  : direct_abstract_declarator(tr), _dad(mv_p(std::move(dad))),
    _tql(mv_p(std::move(tql))), _size(mv_p(std::move(size))),
    _static(is_static), _vla_unspec_size(false)
{
  assert(!_static || _size);
  if (_dad)
    _dad->_set_parent(static_cast<direct_abstract_declarator&>(*this));
  if (_tql)
    _tql->_set_parent(*this);
  if (_size)
    _size->_set_parent(*this);
}

direct_abstract_declarator_array::
direct_abstract_declarator_array(const pp_tokens_range &tr,
				 direct_abstract_declarator* &&dad,
				 const vla_unspec_size_tag&) noexcept
  : direct_abstract_declarator(tr), _tql(nullptr), _size(nullptr),
    _static(false), _vla_unspec_size(true)
{
  if (_dad)
    _dad->_set_parent(static_cast<direct_abstract_declarator&>(*this));
}

direct_abstract_declarator_array::
~direct_abstract_declarator_array() noexcept
{
  delete _dad;
  delete _tql;
  delete _size;
}


direct_abstract_declarator_func::
direct_abstract_declarator_func(const pp_tokens_range &tr,
			       direct_abstract_declarator* &&dad,
			       parameter_declaration_list* &&ptl) noexcept
  : direct_abstract_declarator(tr),
    _dad(mv_p(std::move(dad))), _ptl(mv_p(std::move(ptl)))
{
  if (_dad)
    _dad->_set_parent(static_cast<direct_abstract_declarator&>(*this));
  if (_ptl)
    _ptl->_set_parent(*this);
}

direct_abstract_declarator_func::~direct_abstract_declarator_func() noexcept
{
  delete _dad;
  delete _ptl;
}


abstract_declarator::abstract_declarator(const pp_tokens_range &tr,
					 pointer* &&pt,
					 direct_abstract_declarator* &&dad)
noexcept
  : ast_entity(tr), _pt(mv_p(std::move(pt))), _dad(mv_p(std::move(dad)))
{
  if (_pt)
    _pt->_set_parent(*this);
  if (_dad)
    _dad->_set_parent(*this);
}

abstract_declarator::~abstract_declarator() noexcept
{
  delete _pt;
  delete _dad;
}


type_name::type_name(const pp_tokens_range &tr, specifier_qualifier_list* &&sql,
		     abstract_declarator* &&ad) noexcept
  : ast_entity(tr), _sql(*mv_p(std::move(sql))), _ad(mv_p(std::move(ad)))
{
  _sql._set_parent(*this);
  if (_ad)
    _ad->_set_parent(*this);
}

type_name::~type_name() noexcept
{
  delete &_sql;
  delete _ad;
}


direct_declarator::direct_declarator(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

direct_declarator::~direct_declarator() noexcept = default;


direct_declarator_id::direct_declarator_id(const pp_token_index id_tok) noexcept
  : direct_declarator(pp_tokens_range{id_tok, id_tok + 1}), _id_tok(id_tok)
{}

direct_declarator_id::~direct_declarator_id() noexcept = default;

pp_token_index direct_declarator_id::get_id_tok() const noexcept
{
  return _id_tok;
}


direct_declarator_parenthesized::
direct_declarator_parenthesized(const pp_tokens_range &tr,
				declarator* &&d,
				attribute_specifier_list* &&asl)
noexcept
  : direct_declarator(tr), _d(*mv_p(std::move(d))),
    _asl(mv_p(std::move(asl)))
{
  _d._set_parent(*this);
  if (_asl)
    _asl->_set_parent(*this);
}

direct_declarator_parenthesized::~direct_declarator_parenthesized() noexcept
{
  delete &_d;
  delete _asl;
}

pp_token_index direct_declarator_parenthesized::get_id_tok() const noexcept
{
  return _d.get_id_tok();
}


direct_declarator_array::direct_declarator_array(const pp_tokens_range &tr,
				direct_declarator* &&dd,
				type_qualifier_list* &&tql,
				expr* &&size,
				const bool is_static) noexcept
  : direct_declarator(tr), _dd(*mv_p(std::move(dd))),
    _tql(mv_p(std::move(tql))), _size(mv_p(std::move(size))),
    _static(is_static), _vla_unspec_size(false)
{
  assert(!_static || _size);
  _dd._set_parent(static_cast<direct_declarator&>(*this));
  if (_tql)
    _tql->_set_parent(*this);
  if (_size)
    _size->_set_parent(*this);
}

direct_declarator_array::direct_declarator_array(const pp_tokens_range &tr,
				direct_declarator* &&dd,
				type_qualifier_list* &&tql,
				const vla_unspec_size_tag&) noexcept
  : direct_declarator(tr), _dd(*mv_p(std::move(dd))),
    _tql(mv_p(std::move(tql))), _size(nullptr),
    _static(false), _vla_unspec_size(true)
{
  _dd._set_parent(static_cast<direct_declarator&>(*this));
  if (_tql)
    _tql->_set_parent(*this);
}

direct_declarator_array::~direct_declarator_array() noexcept
{
  delete &_dd;
  delete _tql;
  delete _size;
}

pp_token_index direct_declarator_array::get_id_tok() const noexcept
{
  return _dd.get_id_tok();
}


direct_declarator_func::direct_declarator_func(const pp_tokens_range &tr,
					     direct_declarator* &&dd,
					     parameter_declaration_list* &&ptl)
noexcept
  : direct_declarator(tr), _dd(*mv_p(std::move(dd))),
    _ptl(mv_p(std::move(ptl))), _il(nullptr)
{
  _dd._set_parent(static_cast<direct_declarator&>(*this));
  assert(_ptl);
  _ptl->_set_parent(*this);
}

direct_declarator_func::direct_declarator_func(const pp_tokens_range &tr,
					       direct_declarator* &&dd,
					       identifier_list* &&il)
noexcept
  : direct_declarator(tr), _dd(*mv_p(std::move(dd))),
    _ptl(nullptr), _il(mv_p(std::move(il)))
{
  _dd._set_parent(static_cast<direct_declarator&>(*this));
  if (_il)
  _il->_set_parent(*this);
}

direct_declarator_func::~direct_declarator_func() noexcept
{
  delete &_dd;
  delete _ptl;
  delete _il;
}

pp_token_index direct_declarator_func::get_id_tok() const noexcept
{
  return _dd.get_id_tok();
}


declarator::declarator(const pp_tokens_range &tr, pointer* &&pt,
		       direct_declarator* &&dd) noexcept
  : ast_entity(tr), _pt(mv_p(std::move(pt))),
    _dd(*mv_p(std::move(dd)))
{
  _dd._set_parent(*this);
  if (_pt)
    _pt->_set_parent(*this);
}

declarator::~declarator() noexcept
{
  delete _pt;
  delete &_dd;
}

pp_token_index declarator::get_id_tok() const noexcept
{
  return _dd.get_id_tok();
}


storage_class_specifier::
storage_class_specifier(const pp_tokens_range &tr,
			const storage_class sc) noexcept
  : ast_entity(tr), _sc(sc)
{}

storage_class_specifier::~storage_class_specifier() noexcept = default;



type_qualifier::type_qualifier(const pp_tokens_range &tr,
			       const type_qualifier_type &tqt) noexcept
  : ast_entity(tr), _tqt(tqt)
{}

type_qualifier::~type_qualifier() noexcept = default;


type_qualifier_list::type_qualifier_list(type_qualifier* &&tq)
  : ast_entity(tq->get_tokens_range())
{
  auto _tq = std::ref(*mv_p(std::move(tq)));
  try {
    _tqs.push_back(_tq);
  } catch(...) {
    delete &_tq.get();
    throw;
  }
  _tq.get()._set_parent(*this);
}

type_qualifier_list::type_qualifier_list(attribute_specifier_list* &&asl)
  : ast_entity(asl->get_tokens_range())
{
  auto _asl = std::ref(*mv_p(std::move(asl)));
  try {
    _asls.push_back(_asl);
  } catch(...) {
    delete &_asl.get();
    throw;
  }
  _asl.get()._set_parent(*this);
}


type_qualifier_list::~type_qualifier_list() noexcept
{
  for (auto tq : _tqs)
    delete &tq.get();

  for (auto asl : _asls)
    delete &asl.get();
}

void type_qualifier_list::extend(type_qualifier* &&tq)
{
  auto _tq = std::ref(*mv_p(std::move(tq)));
  try {
    _tqs.push_back(_tq);
  } catch(...) {
    delete &_tq.get();
    throw;
  }
  _extend_tokens_range(_tq.get().get_tokens_range());
  _tq.get()._set_parent(*this);
}

void type_qualifier_list::extend(attribute_specifier_list* &&asl)
{
  auto _asl = std::ref(*mv_p(std::move(asl)));
  try {
    _asls.push_back(_asl);
  } catch(...) {
    delete &_asl.get();
    throw;
  }
  _extend_tokens_range(_asl.get().get_tokens_range());
  _asl.get()._set_parent(*this);
}


type_specifier::type_specifier(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

type_specifier::~type_specifier() noexcept = default;


type_specifier_pod::type_specifier_pod(const pp_tokens_range &tr,
				       const pod_spec &pod_spec) noexcept
  : type_specifier(tr), _pod_spec(pod_spec)
{}

type_specifier_pod::~type_specifier_pod() noexcept = default;


type_specifier_tdid::type_specifier_tdid(const pp_token_index tdid_tok) noexcept
  : type_specifier(pp_tokens_range{tdid_tok, tdid_tok + 1}),
    _tdid_tok(tdid_tok)
{}

type_specifier_tdid::~type_specifier_tdid() noexcept = default;


struct_declaration::struct_declaration(const pp_tokens_range &tr,
				       specifier_qualifier_list* &&sql)
  noexcept
  : ast_entity(tr), _sql(mv_p(std::move(sql)))
{
  if (_sql)
    _sql->_set_parent(*this);
}

struct_declaration::~struct_declaration() noexcept
{
  delete _sql;
}


struct_declarator::struct_declarator(const pp_tokens_range &tr,
				     declarator* &&d, expr* &&width,
				     attribute_specifier_list* &&asl_after)
  noexcept
  : ast_entity(tr), _d(*mv_p(std::move(d))),
    _width(mv_p(std::move(width))),
    _asl_before(nullptr), _asl_after(mv_p(std::move(asl_after)))
{
  _d._set_parent(*this);
  if (_width)
    _width->_set_parent(*this);
  if (_asl_after)
    _asl_after->_set_parent(*this);
}

struct_declarator::~struct_declarator() noexcept
{
  delete &_d;
  delete _width;
  delete _asl_before;
  delete _asl_after;
}

void struct_declarator::set_asl_before(attribute_specifier_list* &&asl_before)
  noexcept
{
  assert(!_asl_before);
  _asl_before = mv_p(std::move(asl_before));
  if (_asl_before)
    _asl_before->_set_parent(*this);
}


struct_declarator_list::struct_declarator_list(struct_declarator* &&sd)
  : ast_entity(sd->get_tokens_range())
{
  auto _sd = std::ref(*mv_p(std::move(sd)));
  try {
    _sds.push_back(_sd);
  } catch (...) {
    delete &_sd.get();
    throw;
  }
  _sd.get()._set_parent(*this);
}

struct_declarator_list::~struct_declarator_list() noexcept
{
  for (auto sd : _sds)
    delete &sd.get();
}

void struct_declarator_list::extend(struct_declarator* &&sd)
{
  auto _sd = std::ref(*mv_p(std::move(sd)));
  try {
    _sds.push_back(_sd);
  } catch (...) {
    delete &_sd.get();
    throw;
  }
  _extend_tokens_range(_sd.get().get_tokens_range());
  _sd.get()._set_parent(*this);
}


struct_declaration_c99::struct_declaration_c99(const pp_tokens_range &tr,
					       specifier_qualifier_list* &&sql,
					       struct_declarator_list * &&sdl)
  noexcept
  : struct_declaration(tr, std::move(sql)), _sdl(*mv_p(std::move(sdl)))
{
  _sdl._set_parent(*this);
}

struct_declaration_c99::~struct_declaration_c99() noexcept
{
  delete &_sdl;
}


unnamed_struct_or_union::
unnamed_struct_or_union(const pp_tokens_range &tr,
			const struct_or_union sou,
			struct_declaration_list* &&sdl,
			attribute_specifier_list* &&asl_before,
			attribute_specifier_list* &&asl_after) noexcept
  : ast_entity(tr), _sou(sou), _sdl(mv_p(std::move(sdl))),
    _asl_before(mv_p(std::move(asl_before))),
    _asl_after(mv_p(std::move(asl_after)))
{
  if (_sdl)
    _sdl->_set_parent(*this);
  if (_asl_before)
    _asl_before->_set_parent(*this);
  if (_asl_after)
    _asl_after->_set_parent(*this);
}

unnamed_struct_or_union::~unnamed_struct_or_union() noexcept
{
  delete _sdl;
  delete _asl_before;
  delete _asl_after;
}


struct_declaration_unnamed_sou::
struct_declaration_unnamed_sou(const pp_tokens_range &tr,
			       specifier_qualifier_list* &&sql,
			       unnamed_struct_or_union* &&unnamed_sou)
  noexcept
  : struct_declaration(tr, std::move(sql)),
    _unnamed_sou(*mv_p(std::move(unnamed_sou)))
{
  _unnamed_sou._set_parent(*this);
}

struct_declaration_unnamed_sou::~struct_declaration_unnamed_sou() noexcept
{
  delete &_unnamed_sou;
}

struct_declaration_list::struct_declaration_list(struct_declaration* &&sd)
  : ast_entity(sd->get_tokens_range())
{
  auto _sd = std::ref(*mv_p(std::move(sd)));
  try {
    _sds.push_back(_sd);
  } catch (...) {
    delete &_sd.get();
    throw;
  }
  _sd.get()._set_parent(*this);
}

struct_declaration_list::~struct_declaration_list() noexcept
{
  for (auto sd : _sds)
    delete &sd.get();
}

void struct_declaration_list::extend(struct_declaration* &&sd)
{
  auto _sd = std::ref(*mv_p(std::move(sd)));
  try {
    _sds.push_back(_sd);
  } catch (...) {
    delete &_sd.get();
    throw;
  }
  _sd.get()._set_parent(*this);
}


struct_or_union_def::struct_or_union_def(const pp_tokens_range &tr,
					 const struct_or_union sou,
					 const pp_token_index id_tok,
					 struct_declaration_list* &&sdl,
					 attribute_specifier_list* &&asl_before,
					 attribute_specifier_list* &&asl_after)
  noexcept
  : struct_or_union_def(tr, sou, id_tok, std::move(sdl),
			std::move(asl_before), std::move(asl_after), true)
{}

struct_or_union_def::struct_or_union_def(const pp_tokens_range &tr,
					 const struct_or_union sou,
					 struct_declaration_list* &&sdl,
					 attribute_specifier_list* &&asl_before,
					 attribute_specifier_list* &&asl_after)
  noexcept
  : struct_or_union_def(tr, sou, 0, std::move(sdl),
			std::move(asl_before), std::move(asl_after), false)
{}

struct_or_union_def::struct_or_union_def(const pp_tokens_range &tr,
					 const struct_or_union sou,
					 const pp_token_index id_tok,
					 struct_declaration_list* &&sdl,
					 attribute_specifier_list* &&asl_before,
					 attribute_specifier_list* &&asl_after,
					 const bool id_tok_valid) noexcept

  : type_specifier(tr), _sou(sou),
    _sdl(mv_p(std::move(sdl))), _asl_before(mv_p(std::move(asl_before))),
    _asl_after(mv_p(std::move(asl_after))), _id_tok_valid(id_tok_valid)
{
  if (_sdl)
    _sdl->_set_parent(*this);
  if (_asl_before)
    _asl_before->_set_parent(*this);
  if (_asl_after)
    _asl_after->_set_parent(*this);
}

struct_or_union_def::~struct_or_union_def() noexcept
{
  delete _sdl;
  delete _asl_before;
  delete _asl_after;
}


struct_or_union_ref::struct_or_union_ref(const pp_tokens_range &tr,
					 const struct_or_union sou,
					 const pp_token_index id_tok,
					 attribute_specifier_list* &&asl)
  noexcept
  : type_specifier(tr), _sou(sou), _id_tok(id_tok),
    _asl(mv_p(std::move(asl)))
{
  if (_asl)
    _asl->_set_parent(*this);
}

struct_or_union_ref::~struct_or_union_ref() noexcept
{
  delete _asl;
}


enumerator::enumerator(const pp_tokens_range &tr, const pp_token_index id_tok,
		       expr* &&value) noexcept
  : ast_entity(tr), _id_tok(id_tok), _value(mv_p(std::move(value)))
{
  if (_value)
    _value->_set_parent(*this);
}

enumerator::~enumerator() noexcept
{
  delete _value;
}


enumerator_list::enumerator_list(enumerator* &&e)
  : ast_entity(e->get_tokens_range())
{
  auto _e = std::ref(*mv_p(std::move(e)));
  try {
    _es.push_back(_e);
  } catch(...) {
    delete &_e.get();
    throw;
  }
  _e.get()._set_parent(*this);
}

enumerator_list::~enumerator_list() noexcept
{
  for (auto e : _es)
    delete &e.get();
}

void enumerator_list::extend(enumerator* &&e)
{
  auto _e = std::ref(*mv_p(std::move(e)));
  try {
    _es.push_back(_e);
  } catch(...) {
    delete &_e.get();
    throw;
  }
  _extend_tokens_range(_e.get().get_tokens_range());
  _e.get()._set_parent(*this);
}


enum_def::enum_def(const pp_tokens_range &tr,
		   const pp_token_index id_tok,
		   enumerator_list* &&el,
		   attribute_specifier_list* &&asl_before,
		   attribute_specifier_list* &&asl_after) noexcept
  : enum_def(tr, id_tok, std::move(el), std::move(asl_before),
	     std::move(asl_after), true)
{}

enum_def::enum_def(const pp_tokens_range &tr,
		   enumerator_list* &&el,
		   attribute_specifier_list* &&asl_before,
		   attribute_specifier_list* &&asl_after) noexcept
  : enum_def(tr, 0, std::move(el), std::move(asl_before),
	     std::move(asl_after), false)
{}

enum_def::enum_def(const pp_tokens_range &tr,
		   const pp_token_index id_tok,
		   enumerator_list* &&el,
		   attribute_specifier_list* &&asl_before,
		   attribute_specifier_list* &&asl_after,
		   const bool id_tok_valid) noexcept
  : type_specifier(tr), _id_tok(id_tok),
    _el(*mv_p(std::move(el))), _asl_before(mv_p(std::move(asl_before))),
    _asl_after(mv_p(std::move(asl_after))), _id_tok_valid(id_tok_valid)
{
  _el._set_parent(*this);
  if (_asl_before)
    _asl_before->_set_parent(*this);
  if (_asl_after)
    _asl_after->_set_parent(*this);
}

enum_def::~enum_def() noexcept
{
  delete &_el;
  delete _asl_before;
  delete _asl_after;
}


enum_ref::enum_ref(const pp_tokens_range &tr,
		   const pp_token_index id_tok,
		   attribute_specifier_list* &&asl) noexcept
  : type_specifier(tr), _id_tok(id_tok), _asl(mv_p(std::move(asl)))
{
  if (_asl)
    _asl->_set_parent(*this);
}

enum_ref::~enum_ref() noexcept
{
  delete _asl;
}


typeof_expr::typeof_expr(const pp_tokens_range &tr,
			 expr* &&e) noexcept
  : type_specifier(tr), _e(*mv_p(std::move(e)))
{
  _e._set_parent(*this);
}

typeof_expr::~typeof_expr() noexcept
{
  delete &_e;
}


typeof_type_name::typeof_type_name(const pp_tokens_range &tr,
				   type_name* &&tn) noexcept
  : type_specifier(tr), _tn(*mv_p(std::move(tn)))
{
  _tn._set_parent(*this);
}

typeof_type_name::~typeof_type_name() noexcept
{
  delete &_tn;
}


function_specifier::function_specifier(const pp_token_index spec_tok) noexcept
  : ast_entity(pp_tokens_range{spec_tok, spec_tok + 1}),
    _spec_tok(spec_tok)
{}

function_specifier::~function_specifier() noexcept = default;


specifier_qualifier_list::specifier_qualifier_list(type_specifier* &&ts)
  : ast_entity(ts->get_tokens_range())
{
  auto _ts = std::ref(*mv_p(std::move(ts)));
  try {
    _tss.push_back(_ts);
  } catch(...) {
    delete &_ts.get();
    throw;
  }
  _ts.get()._set_parent(*this);
}

specifier_qualifier_list::specifier_qualifier_list(type_qualifier* &&tq)
  : ast_entity(tq->get_tokens_range())
{
  auto _tq = std::ref(*mv_p(std::move(tq)));
  try {
    _tqs.push_back(_tq);
  } catch(...) {
    delete &_tq.get();
    throw;
  }
  _tq.get()._set_parent(*this);
}

specifier_qualifier_list::
specifier_qualifier_list(attribute_specifier_list* &&asl)
  : ast_entity(asl->get_tokens_range())
{
  auto _asl = std::ref(*mv_p(std::move(asl)));
  try {
    _asls.push_back(_asl);
  } catch(...) {
    delete &_asl.get();
    throw;
  }
  _asl.get()._set_parent(*this);
}

specifier_qualifier_list::specifier_qualifier_list(const pp_tokens_range &tr)
  noexcept
  : ast_entity(tr)
{}

specifier_qualifier_list::~specifier_qualifier_list() noexcept
{
  for (auto ts: _tss)
    delete &ts.get();

  for (auto tq : _tqs)
    delete &tq.get();

  for (auto asl : _asls)
    delete &asl.get();
}

void specifier_qualifier_list::extend(type_specifier* &&ts)
{
  auto _ts = std::ref(*mv_p(std::move(ts)));
  try {
    _tss.push_back(_ts);
  } catch(...) {
    delete &_ts.get();
    throw;
  }
  _extend_tokens_range(_ts.get().get_tokens_range());
  _ts.get()._set_parent(*this);
}

void specifier_qualifier_list::extend(type_qualifier* &&tq)
{
  auto _tq = std::ref(*mv_p(std::move(tq)));
  try {
    _tqs.push_back(_tq);
  } catch(...) {
    delete &_tq.get();
    throw;
  }
  _extend_tokens_range(_tq.get().get_tokens_range());
  _tq.get()._set_parent(*this);
}

void specifier_qualifier_list::extend(attribute_specifier_list* &&asl)
{
  auto _asl = std::ref(*mv_p(std::move(asl)));
  try {
    _asls.push_back(_asl);
  } catch(...) {
    delete &_asl.get();
    throw;
  }
  _extend_tokens_range(_asl.get().get_tokens_range());
  _asl.get()._set_parent(*this);
}


void specifier_qualifier_list::extend(specifier_qualifier_list* &&sql)
{
  if (!sql)
    return;

  auto _sql = mv_p(std::move(sql));
  try {
    if (!_sql->_tss.empty())
      _tss.reserve(_tss.size() + _sql->_tss.size());

    if (!_sql->_tqs.empty())
      _tqs.reserve(_tqs.size() + _sql->_tqs.size());

    if (!_sql->_asls.empty())
      _asls.reserve(_asls.size() + _sql->_asls.size());
  } catch (...) {
    delete _sql;
    throw;
  }

  for (auto &ts : _sql->_tss) {
    _tss.push_back(ts);
    ts.get()._reset_parent(*this, *_sql);
  }
  _sql->_tss.clear();

  for (auto &tq : _sql->_tqs) {
    _tqs.push_back(tq);
    tq.get()._reset_parent(*this, *_sql);
  }
  _sql->_tqs.clear();

  for (auto &asl : _sql->_asls) {
    _asls.push_back(asl);
    asl.get()._reset_parent(*this, *_sql);
  }
  _sql->_asls.clear();

  _extend_tokens_range(_sql->get_tokens_range());
  delete _sql;
}


declaration_specifiers::declaration_specifiers(storage_class_specifier* &&scs)
  : specifier_qualifier_list(scs->get_tokens_range())
{
  auto _scs = std::ref(*mv_p(std::move(scs)));
  try {
    _scss.push_back(_scs);
  } catch(...) {
    delete &_scs.get();
    throw;
  }
  _scs.get()._set_parent(*this);
}

declaration_specifiers::declaration_specifiers(function_specifier* &&fs)
  : specifier_qualifier_list(fs->get_tokens_range())
{
  auto _fs = std::ref(*mv_p(std::move(fs)));
  try {
    _fss.push_back(_fs);
  } catch(...) {
    delete &_fs.get();
    throw;
  }
  _fs.get()._set_parent(*this);
}

declaration_specifiers::declaration_specifiers(type_specifier* &&ts)
  : specifier_qualifier_list(std::move(ts))
{}

declaration_specifiers::declaration_specifiers(type_qualifier* &&tq)
  : specifier_qualifier_list(std::move(tq))
{}

declaration_specifiers::declaration_specifiers(attribute_specifier_list* &&asl)
  : specifier_qualifier_list(std::move(asl))
{}

declaration_specifiers::~declaration_specifiers() noexcept
{
  for (auto scs: _scss)
    delete &scs.get();

  for (auto fs : _fss)
    delete &fs.get();
}

void declaration_specifiers::extend(storage_class_specifier* &&scs)
{
  auto _scs = std::ref(*mv_p(std::move(scs)));
  try {
    _scss.push_back(_scs);
  } catch(...) {
    delete &_scs.get();
    throw;
  }
  _extend_tokens_range(_scs.get().get_tokens_range());
  _scs.get()._set_parent(*this);
}

void declaration_specifiers::extend(function_specifier* &&fs)
{
  auto _fs = std::ref(*mv_p(std::move(fs)));
  try {
    _fss.push_back(_fs);
  } catch(...) {
    delete &_fs.get();
    throw;
  }
  _extend_tokens_range(_fs.get().get_tokens_range());
  _fs.get()._set_parent(*this);
}

void declaration_specifiers::extend(type_specifier* &&ts)
{
  specifier_qualifier_list::extend(std::move(ts));
}

void declaration_specifiers::extend(type_qualifier* &&tq)
{
  specifier_qualifier_list::extend(std::move(tq));
}

void declaration_specifiers::extend(attribute_specifier_list* &&asl)
{
  specifier_qualifier_list::extend(std::move(asl));
}

void declaration_specifiers::extend(declaration_specifiers* &&ds)
{
  if (!ds)
    return;

  auto _ds = mv_p(std::move(ds));
  try {
    if (!_ds->_scss.empty())
      _scss.reserve(_scss.size() + _ds->_scss.size());

    if (!_ds->_fss.empty())
      _fss.reserve(_fss.size() + _ds->_fss.size());
  } catch (...) {
    delete _ds;
    throw;
  }

  for (auto &scs : _ds->_scss) {
    _scss.push_back(scs);
    scs.get()._reset_parent(*this, *_ds);
  }
  _ds->_scss.clear();

  for (auto &fs : _ds->_fss) {
    _fss.push_back(fs);
    fs.get()._reset_parent(*this, *_ds);
  }
  _ds->_fss.clear();


  specifier_qualifier_list::extend(std::move(_ds));
}


initializer::initializer(const pp_tokens_range &tr) noexcept
  : ast_entity(tr), _d(nullptr)
{}

initializer::~initializer() noexcept
{
  delete _d;
}

void initializer::set_designation(designation* &&d) noexcept
{
  assert(!_d);
  _d = mv_p(std::move(d));
  if (_d)
    _d->_set_parent(*this);
}


initializer_expr::initializer_expr(expr* &&e) noexcept
  : initializer(e->get_tokens_range()), _e(*mv_p(std::move(e)))
{
  _e._set_parent(*this);
}

initializer_expr::~initializer_expr() noexcept
{
  delete &_e;
}


initializer_init_list::initializer_init_list(const pp_tokens_range &tr,
					     initializer_list* &&il) noexcept
  : initializer(tr), _il(*mv_p(std::move(il)))
{
  _il._set_parent(*this);
}

initializer_init_list::~initializer_init_list() noexcept
{
  delete &_il;
}


designator::designator(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

designator::~designator() noexcept = default;


designator_member::designator_member(const pp_tokens_range &tr,
				     const pp_token_index member_tok) noexcept
  : designator(tr), _member_tok(member_tok)
{}

designator_member::~designator_member() noexcept = default;


designator_array::designator_array(const pp_tokens_range &tr,
				   expr* &&index) noexcept
  : designator(tr), _index(*mv_p(std::move(index)))
{
  _index._set_parent(*this);
}

designator_array::~designator_array() noexcept
{
  delete &_index;
}


designator_list::designator_list(designator* &&d)
  : ast_entity(d->get_tokens_range())
{
  auto _d = std::ref(*mv_p(std::move(d)));
  try {
    _ds.push_back(_d);
  } catch(...) {
    delete &_d.get();
    throw;
  }
  _d.get()._set_parent(*this);
}

designator_list::~designator_list() noexcept
{
  for (auto d : _ds)
    delete &d.get();
}

void designator_list::extend(designator* &&d)
{
  auto _d = std::ref(*mv_p(std::move(d)));
  try {
    _ds.push_back(_d);
  } catch(...) {
    delete &_d.get();
    throw;
  }
  _extend_tokens_range(_d.get().get_tokens_range());
  _d.get()._set_parent(*this);
}


designation::designation(const pp_tokens_range &tr,
			 designator_list* &&dl) noexcept
  : ast_entity(tr), _dl(*mv_p(std::move(dl)))
{
  _dl._set_parent(*this);
}

designation::~designation() noexcept
{
  delete &_dl;
}


initializer_list::initializer_list(initializer* &&i)
  : ast_entity(i->get_tokens_range())
{
  auto _i = std::ref(*mv_p(std::move(i)));
  try {
    _is.push_back(_i);
  } catch(...) {
    delete &_i.get();
    throw;
  }
  _i.get()._set_parent(*this);
}

initializer_list::~initializer_list() noexcept
{
  for (auto i : _is)
    delete &i.get();
}

void initializer_list::extend(initializer* &&i)
{
  auto _i = std::ref(*mv_p(std::move(i)));
  try {
    _is.push_back(_i);
  } catch(...) {
    delete &_i.get();
    throw;
  }
  _extend_tokens_range(_i.get().get_tokens_range());
  _i.get()._set_parent(*this);
}


asm_label::asm_label(const pp_tokens_range &tr,
		     const pp_token_index label_tok) noexcept
  : ast_entity(tr), _label_tok(label_tok)
{}

asm_label::~asm_label() noexcept = default;


init_declarator::init_declarator(const pp_tokens_range &tr,
				 declarator* &&d, initializer* &&i,
				 asm_label* &&al,
				 attribute_specifier_list* &&asl_middle,
				 attribute_specifier_list* &&asl_after) noexcept
  : ast_entity(tr), _d(*mv_p(std::move(d))), _i(mv_p(std::move(i))),
    _al(mv_p(std::move(al))), _asl_before(nullptr),
    _asl_middle(mv_p(std::move(asl_middle))),
    _asl_after(mv_p(std::move(asl_after)))
{
  _d._set_parent(*this);
  if (_i)
    _i->_set_parent(*this);
  if (_al)
    _al->_set_parent(*this);
  if (_asl_after)
    _asl_after->_set_parent(*this);
}

init_declarator::~init_declarator() noexcept
{
  delete &_d;
  delete _i;
  delete _al;
  delete _asl_before;
  delete _asl_middle;
  delete _asl_after;
}

void init_declarator::set_asl_before(attribute_specifier_list* &&asl_before)
  noexcept
{
  assert(!_asl_before);
  _asl_before = mv_p(std::move(asl_before));
  if (_asl_before)
    _asl_before->_set_parent(*this);
}


init_declarator_list::init_declarator_list(init_declarator* &&id)
  : ast_entity(id->get_tokens_range())
{
  auto _id = std::ref(*mv_p(std::move(id)));
  try {
    _ids.push_back(_id);
  } catch(...) {
    delete &_id.get();
    throw;
  }
  _id.get()._set_parent(*this);
}

init_declarator_list::~init_declarator_list() noexcept
{
  for (auto id : _ids)
    delete &id.get();
}

void init_declarator_list::extend(init_declarator* &&id)
{
  auto _id = std::ref(*mv_p(std::move(id)));
  try {
    _ids.push_back(_id);
  } catch(...) {
    delete &_id.get();
    throw;
  }
  _extend_tokens_range(_id.get().get_tokens_range());
  _id.get()._set_parent(*this);
}


declaration::declaration(const pp_tokens_range &tr,
			 declaration_specifiers* &&ds,
			 init_declarator_list* &&idl) noexcept
  : ast_entity(tr), _ds(*mv_p(std::move(ds))), _idl(mv_p(std::move(idl)))
{
  _ds._set_parent(*this);
  if (_idl)
    _idl->_set_parent(*this);
}

declaration::~declaration() noexcept
{
  delete &_ds;
  delete _idl;
}


parameter_declaration::
parameter_declaration(const pp_tokens_range &tr,
		      declaration_specifiers* &&ds) noexcept
  : ast_entity(tr), _ds(*mv_p(std::move(ds)))
{
  _ds._set_parent(*this);
}

parameter_declaration::~parameter_declaration() noexcept
{
  delete &_ds;
}


parameter_declaration_declarator::
parameter_declaration_declarator(const pp_tokens_range &tr,
				 declaration_specifiers* &&ds,
				 declarator* &&d) noexcept
  : parameter_declaration(tr, std::move(ds)),
    _d(*mv_p(std::move(d)))
{
  _d._set_parent(*this);
}

parameter_declaration_declarator::~parameter_declaration_declarator() noexcept
{
  delete &_d;
}


parameter_declaration_abstract::
parameter_declaration_abstract(const pp_tokens_range &tr,
			       declaration_specifiers* &&ds,
			       abstract_declarator* &&ad)
  noexcept
  : parameter_declaration(tr, std::move(ds)),
    _ad(mv_p(std::move(ad)))
{
  if (_ad)
    _ad->_set_parent(*this);
}

parameter_declaration_abstract::~parameter_declaration_abstract() noexcept
{
  delete _ad;
}


parameter_declaration_list::
parameter_declaration_list(parameter_declaration* &&pd)
  : ast_entity(pd->get_tokens_range()), _variadic(false)
{
  auto _pd = std::ref(*mv_p(std::move(pd)));
  try {
    _pds.push_back(_pd);
  } catch(...) {
    delete &_pd.get();
    throw;
  }
  _pd.get()._set_parent(*this);
}

parameter_declaration_list::~parameter_declaration_list() noexcept
{
  for (auto pd : _pds)
    delete &pd.get();
}

void parameter_declaration_list::extend(parameter_declaration* &&pd)
{
  auto _pd = std::ref(*mv_p(std::move(pd)));
  try {
    _pds.push_back(_pd);
  } catch(...) {
    delete &_pd.get();
    throw;
  }
  _extend_tokens_range(_pd.get().get_tokens_range());
  _pd.get()._set_parent(*this);
}

void
parameter_declaration_list::set_variadic(const pp_tokens_range &triple_dot_tr)
  noexcept
{
  _variadic = true;
  _extend_tokens_range(triple_dot_tr);
}


identifier_list::identifier_list(const pp_token_index id_tok)
  : ast_entity(pp_tokens_range{id_tok, id_tok + 1}),
    _ids(1, id_tok)
{}

identifier_list::~identifier_list() noexcept = default;

void identifier_list::extend(const pp_token_index id_tok)
{
  _ids.push_back(id_tok);
  _extend_tokens_range(pp_tokens_range{id_tok, id_tok + 1});
}


declaration_list::declaration_list(declaration* &&d)
  : ast_entity(d->get_tokens_range())
{
  auto _d = std::ref(*mv_p(std::move(d)));
  try {
    _ds.push_back(_d);
  } catch(...) {
    delete &_d.get();
    throw;
  }
  _d.get()._set_parent(*this);
}

declaration_list::~declaration_list() noexcept
{
  for (auto d : _ds)
    delete &d.get();
}

void declaration_list::extend(declaration* &&d)
{
  auto _d = std::ref(*mv_p(std::move(d)));
  try {
    _ds.push_back(_d);
  } catch(...) {
    delete &_d.get();
    throw;
  }
  _extend_tokens_range(_d.get().get_tokens_range());
  _d.get()._set_parent(*this);
}


stmt::stmt(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

stmt::~stmt() noexcept = default;


stmt_labeled::stmt_labeled(const pp_tokens_range &tr,
			   const pp_token_index label_tok,
			   stmt* &&s, attribute_specifier_list* &&asl) noexcept
  : stmt(tr), _label_tok(label_tok), _s(*mv_p(std::move(s))),
    _asl(mv_p(std::move(asl)))
{
  _s._set_parent(static_cast<stmt&>(*this));
  if(_asl)
    _asl->_set_parent(*this);
}

stmt_labeled::~stmt_labeled() noexcept
{
  delete &_s;
  delete _asl;
}


stmt_case::stmt_case(const pp_tokens_range &tr,
		     expr* &&e, stmt* &&s) noexcept
  : stmt(tr), _e(*mv_p(std::move(e))), _s(*mv_p(std::move(s)))
{
  _e._set_parent(*this);
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_case::~stmt_case() noexcept
{
  delete &_e;
  delete &_s;
}


stmt_case_range::stmt_case_range(const pp_tokens_range &tr,
				 expr* &&e_low, expr* &&e_high, stmt* &&s)
  noexcept
  : stmt(tr), _e_low(*mv_p(std::move(e_low))),
    _e_high(*mv_p(std::move(e_high))), _s(*mv_p(std::move(s)))
{
  _e_low._set_parent(*this);
  _e_high._set_parent(*this);
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_case_range::~stmt_case_range() noexcept
{
  delete &_e_low;
  delete &_e_high;
  delete &_s;
}


stmt_default::stmt_default(const pp_tokens_range &tr, stmt* &&s) noexcept
  : stmt(tr), _s(*mv_p(std::move(s)))
{
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_default::~stmt_default() noexcept
{
  delete &_s;
}


local_label_declaration::local_label_declaration(const pp_tokens_range &tr,
						 identifier_list* &&idl)
  noexcept
  : ast_entity(tr), _idl(*mv_p(std::move(idl)))
{
  _idl._set_parent(*this);
}

local_label_declaration::~local_label_declaration() noexcept
{
  delete &_idl;
}


local_label_declaration_list::
local_label_declaration_list(local_label_declaration* &&lld)
  : ast_entity(lld->get_tokens_range())
{
  auto _lld = std::ref(*mv_p(std::move(lld)));
  try {
    _llds.push_back(_lld);
  } catch(...) {
    delete &_lld.get();
    throw;
  }
  _lld.get()._set_parent(*this);
}

local_label_declaration_list::~local_label_declaration_list() noexcept
{
  for (auto lld : _llds)
    delete &lld.get();
}

void local_label_declaration_list::extend(local_label_declaration* &&lld)
{
  auto _lld = std::ref(*mv_p(std::move(lld)));
  try {
    _llds.push_back(_lld);
  } catch(...) {
    delete &_lld.get();
    throw;
  }
  _extend_tokens_range(_lld.get().get_tokens_range());
  _lld.get()._set_parent(*this);
}


block_item::block_item(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

block_item::~block_item() noexcept = default;


block_item_decl::block_item_decl(declaration* &&d) noexcept
  : block_item(d->get_tokens_range()), _d(*mv_p(std::move(d)))
{
  _d._set_parent(*this);
}

block_item_decl::~block_item_decl() noexcept
{
  delete &_d;
}


block_item_stmt::block_item_stmt(stmt* &&s) noexcept
  : block_item(s->get_tokens_range()), _s(*mv_p(std::move(s)))
{
  _s._set_parent(*this);
}


block_item_stmt::~block_item_stmt() noexcept
{
  delete &_s;
}


block_item_function_definition::
block_item_function_definition(function_definition* &&fd) noexcept
  : block_item(fd->get_tokens_range()), _fd(*mv_p(std::move(fd)))
{
  _fd._set_parent(*this);
}


block_item_function_definition::~block_item_function_definition() noexcept
{
  delete &_fd;
}


block_item_list::block_item_list(block_item* &&bi)
  : ast_entity(bi->get_tokens_range())
{
  auto _bi = std::ref(*mv_p(std::move(bi)));
  try {
    _bis.push_back(_bi);
  } catch(...) {
    delete &_bi.get();
    throw;
  }
  _bi.get()._set_parent(*this);
}

block_item_list::~block_item_list() noexcept
{
  for (auto bi : _bis)
    delete &bi.get();
}

void block_item_list::extend(block_item* &&bi)
{
  auto _bi = std::ref(*mv_p(std::move(bi)));
  try {
    _bis.push_back(_bi);
  } catch(...) {
    delete &_bi.get();
    throw;
  }
  _extend_tokens_range(_bi.get().get_tokens_range());
  _bi.get()._set_parent(*this);
}


stmt_compound::stmt_compound(const pp_tokens_range &tr,
			     local_label_declaration_list* &&lldl,
			     block_item_list* &&bil) noexcept
  : stmt(tr), _lldl(mv_p(std::move(lldl))), _bil(mv_p(std::move(bil)))
{
  if (_lldl)
    _lldl->_set_parent(*this);
  if (_bil)
    _bil->_set_parent(*this);
}

stmt_compound::~stmt_compound() noexcept
{
  delete _lldl;
  delete _bil;
}


stmt_expr::stmt_expr(const pp_tokens_range &tr, expr* &&e) noexcept
  : stmt(tr), _e(mv_p(std::move(e)))
{
  if (_e)
    _e->_set_parent(*this);
}

stmt_expr::~stmt_expr() noexcept
{
  delete _e;
}


stmt_if::stmt_if(const pp_tokens_range &tr,
		 expr* &&cond, stmt* &&s_true, stmt* &&s_false)
  noexcept
  : stmt(tr), _cond(*mv_p(std::move(cond))),
    _s_true(*mv_p(std::move(s_true))), _s_false(mv_p(std::move(s_false)))
{
  _cond._set_parent(*this);
  _s_true._set_parent(static_cast<stmt&>(*this));
  if (_s_false)
    _s_false->_set_parent(static_cast<stmt&>(*this));
}

stmt_if::~stmt_if() noexcept
{
  delete &_cond;
  delete &_s_true;
  delete _s_false;
}


stmt_switch::stmt_switch(const pp_tokens_range &tr, expr* &&e, stmt* &&s)
  noexcept
  : stmt(tr), _e(*mv_p(std::move(e))), _s(*mv_p(std::move(s)))
{
  _e._set_parent(*this);
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_switch::~stmt_switch() noexcept
{
  delete &_e;
  delete &_s;
}


stmt_while::stmt_while(const pp_tokens_range &tr, expr* &&e, stmt* &&s)
  noexcept
  : stmt(tr), _e(*mv_p(std::move(e))), _s(*mv_p(std::move(s)))
{
  _e._set_parent(*this);
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_while::~stmt_while() noexcept
{
  delete &_e;
  delete &_s;
}


stmt_do::stmt_do(const pp_tokens_range &tr, expr* &&e, stmt* &&s)
  noexcept
  : stmt(tr), _e(*mv_p(std::move(e))), _s(*mv_p(std::move(s)))
{
  _e._set_parent(*this);
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_do::~stmt_do() noexcept
{
  delete &_e;
  delete &_s;
}


stmt_for_init_expr::stmt_for_init_expr(const pp_tokens_range &tr,
				       expr* &&init, expr* &&cond, expr* &&next,
				       stmt* &&s) noexcept
  : stmt(tr), _init(mv_p(std::move(init))), _cond(mv_p(std::move(cond))),
    _next(mv_p(std::move(next))), _s(*mv_p(std::move(s)))
{
  if (_init)
    _init->_set_parent(*this);
  if (_cond)
    _cond->_set_parent(*this);
  if (_next)
    _next->_set_parent(*this);
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_for_init_expr::~stmt_for_init_expr() noexcept
{
  delete _init;
  delete _cond;
  delete _next;
  delete &_s;
}


stmt_for_init_decl::stmt_for_init_decl(const pp_tokens_range &tr,
				       declaration* &&d,
				       expr* &&cond, expr* &&next,
				       stmt* &&s) noexcept
  : stmt(tr), _d(*mv_p(std::move(d))), _cond(mv_p(std::move(cond))),
    _next(mv_p(std::move(next))), _s(*mv_p(std::move(s)))
{
  _d._set_parent(*this);
  if (_cond)
    _cond->_set_parent(*this);
  if (_next)
    _next->_set_parent(*this);
  _s._set_parent(static_cast<stmt&>(*this));
}

stmt_for_init_decl::~stmt_for_init_decl() noexcept
{
  delete &_d;
  delete _cond;
  delete _next;
  delete &_s;
}


stmt_goto::stmt_goto(const pp_tokens_range &tr, expr* &&e) noexcept
  : stmt(tr), _e(*mv_p(std::move(e)))
{
  _e._set_parent(*this);
}

stmt_goto::~stmt_goto() noexcept
{
  delete &_e;
}


stmt_continue::stmt_continue(const pp_tokens_range &tr) noexcept
  : stmt(tr)
{}

stmt_continue::~stmt_continue() noexcept = default;


stmt_break::stmt_break(const pp_tokens_range &tr) noexcept
  : stmt(tr)
{}

stmt_break::~stmt_break() noexcept = default;


stmt_return::stmt_return(const pp_tokens_range &tr, expr* &&e) noexcept
  : stmt(tr), _e(mv_p(std::move(e)))
{
  if (_e)
    _e->_set_parent(*this);
}

stmt_return::~stmt_return() noexcept
{
  delete _e;
}


asm_qualifier_list::asm_qualifier_list(const pp_tokens_range &tr,
				       const asm_qualifier aq)
  : ast_entity(tr), _aqs(1, aq)
{}

asm_qualifier_list::~asm_qualifier_list() noexcept = default;

void asm_qualifier_list::extend(const pp_tokens_range &tr,
				const asm_qualifier aq)
{
  _aqs.push_back(aq);
  _extend_tokens_range(tr);
}


asm_operand_name::asm_operand_name(const pp_tokens_range &tr,
				   const pp_token_index id_tok) noexcept
  : ast_entity(tr), _id_tok(id_tok)
{}

asm_operand_name::~asm_operand_name() noexcept = default;


asm_operand::asm_operand(const pp_tokens_range &tr, asm_operand_name* &&aon,
			 const pp_token_index constraint_tok, expr* &&e)
  noexcept
  : ast_entity(tr), _aon(mv_p(std::move(aon))),
    _constraint_tok(constraint_tok), _e(*mv_p(std::move(e)))
{
  if (_aon)
    _aon->_set_parent(*this);
  _e._set_parent(*this);
}

asm_operand::~asm_operand() noexcept
{
  delete _aon;
  delete &_e;
}


asm_operand_list::asm_operand_list(asm_operand* &&ao)
  : ast_entity(ao->get_tokens_range())
{
  auto _ao = std::ref(*mv_p(std::move(ao)));
  try {
    _aos.push_back(_ao);
  } catch(...) {
    delete &_ao.get();
    throw;
  }
  _ao.get()._set_parent(*this);
}

asm_operand_list::~asm_operand_list() noexcept
{
  for (auto ao : _aos)
    delete &ao.get();
}

void asm_operand_list::extend(asm_operand* &&ao)
{
  auto _ao = std::ref(*mv_p(std::move(ao)));
  try {
    _aos.push_back(_ao);
  } catch(...) {
    delete &_ao.get();
    throw;
  }
  _extend_tokens_range(_ao.get().get_tokens_range());
  _ao.get()._set_parent(*this);
}


asm_clobber_list::asm_clobber_list(const pp_token_index clobber_tok)
  : ast_entity(pp_tokens_range{clobber_tok, clobber_tok + 1}),
    _clobber_toks(1, clobber_tok)
{}

asm_clobber_list::~asm_clobber_list() noexcept = default;

void asm_clobber_list::extend(const pp_token_index clobber_tok)
{
  _clobber_toks.push_back(clobber_tok);
  _extend_tokens_range(pp_tokens_range{clobber_tok, clobber_tok + 1});
}


asm_jump_to_label_list::asm_jump_to_label_list(const pp_token_index label_tok)
  : ast_entity(pp_tokens_range{label_tok, label_tok + 1}),
    _label_toks(1, label_tok)
{}

asm_jump_to_label_list::~asm_jump_to_label_list() noexcept = default;

void asm_jump_to_label_list::extend(const pp_token_index label_tok)
{
  _label_toks.push_back(label_tok);
  _extend_tokens_range(pp_tokens_range{label_tok, label_tok + 1});
}


stmt_asm::stmt_asm(const pp_tokens_range &tr,
		   asm_qualifier_list* &&aql, string_literal* &&asm_s,
		   asm_operand_list* &&output_aol,
		   asm_operand_list* &&input_aol,
		   asm_clobber_list* &&acl, asm_jump_to_label_list* &&ajtll)
  noexcept
  : stmt(tr), _aql(mv_p(std::move(aql))), _asm_s(*mv_p(std::move(asm_s))),
    _output_aol(mv_p(std::move(output_aol))),
    _input_aol(mv_p(std::move(input_aol))), _acl(mv_p(std::move(acl))),
    _ajtll(mv_p(std::move(ajtll)))
{
  if (_aql)
    _aql->_set_parent(*this);
  _asm_s._set_parent(*this);
  if (_output_aol)
    _output_aol->_set_parent(*this);
  if (_input_aol)
    _input_aol->_set_parent(*this);
  if (_acl)
    _acl->_set_parent(*this);
  if (_ajtll)
    _ajtll->_set_parent(*this);
}

stmt_asm::~stmt_asm() noexcept
{
  delete _aql;
  delete &_asm_s;
  delete _output_aol;
  delete _input_aol;
  delete _acl;
  delete _ajtll;
}


function_definition::function_definition(const pp_tokens_range &tr,
					 declaration_specifiers* &&ds,
					 declarator* &&d,
					 attribute_specifier_list* &&asl,
					 declaration_list* &&dl,
					 stmt_compound* &&sc) noexcept
  : ast_entity(tr), _ds(*mv_p(std::move(ds))), _d(*mv_p(std::move(d))),
    _asl(mv_p(std::move(asl))), _dl(mv_p(std::move(dl))),
    _sc(*mv_p(std::move(sc)))
{
  _ds._set_parent(*this);
  _d._set_parent(*this);
  if (_asl)
    _asl->_set_parent(*this);
  if (_dl)
    _dl->_set_parent(*this);
  _sc._set_parent(*this);
}

function_definition::~function_definition() noexcept
{
  delete &_ds;
  delete &_d;
  delete _asl;
  delete _dl;
  delete &_sc;
}


external_declaration::external_declaration(const pp_tokens_range &tr) noexcept
  : ast_entity(tr)
{}

external_declaration::~external_declaration() noexcept = default;


external_declaration_decl::external_declaration_decl(declaration* &&d) noexcept
  : external_declaration(d->get_tokens_range()), _d(*mv_p(std::move(d)))
{
  _d._set_parent(*this);
}

external_declaration_decl::~external_declaration_decl() noexcept
{
  delete &_d;
}


external_declaration_func::
external_declaration_func(function_definition* &&fd) noexcept
  : external_declaration(fd->get_tokens_range()), _fd(*mv_p(std::move(fd)))
{
  _fd._set_parent(*this);
}

external_declaration_func::~external_declaration_func() noexcept
{
  delete &_fd;
}


translation_unit::translation_unit(external_declaration* &&ed)
  : ast_entity(ed->get_tokens_range())
{
  auto _ed = std::ref(*mv_p(std::move(ed)));
  try {
    _eds.push_back(_ed);
  } catch(...) {
    delete &_ed.get();
    throw;
  }
  _ed.get()._set_parent(*this);
}

translation_unit::~translation_unit() noexcept
{
  for (auto ed : _eds)
    delete &ed.get();
}

void translation_unit::extend(external_declaration* &&ed)
{
  auto _ed = std::ref(*mv_p(std::move(ed)));
  try {
    _eds.push_back(_ed);
  } catch(...) {
    delete &_ed.get();
    throw;
  }
  _extend_tokens_range(_ed.get().get_tokens_range());
  _ed.get()._set_parent(*this);
}

void translation_unit::extend_tokens_range(const pp_tokens_range &tr) noexcept
{
  _extend_tokens_range(tr);
}
