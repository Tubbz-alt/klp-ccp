/*
 * Copyright (C) 2019  SUSE Software Solutions Germany GmbH
 *
 * This file is part of klp-ccp.
 *
 * klp-ccp is free software: you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * klp-ccp is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with klp-ccp. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PP_TOKENS_HH
#define PP_TOKENS_HH

#include <vector>
#include <string>
#include <algorithm>

namespace klp
{
  namespace ccp
  {
    class pp_token;

    class pp_tokens
    {
    private:
      typedef std::vector<pp_token> _pp_tokens_type;

    public:
      template<typename... Targs>
      pp_tokens(Targs&&... args)
	: _tokens(std::forward<Targs>(args)...)
      {}

      pp_tokens(const pp_tokens&) = delete;
      pp_tokens(pp_tokens&&) = default;

      typedef _pp_tokens_type::const_iterator const_iterator;
      typedef _pp_tokens_type::iterator iterator;
      typedef _pp_tokens_type::reverse_iterator reverse_iterator;
      typedef _pp_tokens_type::size_type size_type;
      typedef _pp_tokens_type::value_type value_type;

      bool empty() const noexcept
      { return _tokens.empty(); }

      const_iterator cbegin() const noexcept
      { return _tokens.cbegin(); }

      const_iterator begin() const noexcept
      { return _tokens.begin(); }

      iterator begin() noexcept
      { return _tokens.begin(); }

      reverse_iterator rbegin() noexcept
      { return _tokens.rbegin(); }

      const_iterator cend() const noexcept
      { return _tokens.cend(); }

      const_iterator end() const noexcept
      { return _tokens.end(); }

      iterator end() noexcept
      { return _tokens.end(); }

      reverse_iterator rend() noexcept
      { return _tokens.rend(); }

      size_type size() const noexcept
      { return _tokens.size(); }

      const value_type& front() const noexcept
      { return _tokens.front(); }

      const value_type& back() const noexcept
      { return _tokens.back(); }

      value_type& back() noexcept
      { return _tokens.back(); }

      template<typename... Targs>
      void push_back(Targs&&... args)
      {
	_tokens.push_back(std::forward<Targs>(args)...);
      }

      template<typename... Targs>
      void insert(Targs&&... args)
      {
	_tokens.insert(std::forward<Targs>(args)...);
      }

      template<typename... Targs>
      void erase(Targs&&... args)
      {
	_tokens.erase(std::forward<Targs>(args)...);
      }

      bool operator==(const pp_tokens &rhs) const noexcept
      {
	return _tokens == rhs._tokens;
      }

      const pp_token& operator[](const _pp_tokens_type::size_type i) const
      { return _tokens[i]; }

      std::string stringify(const bool as_string) const;

      void shrink(const size_type new_size);

    private:
      _pp_tokens_type _tokens;
    };
  }
}

#endif
