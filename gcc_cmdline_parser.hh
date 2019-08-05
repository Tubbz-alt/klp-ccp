#ifndef GCC_CMDLINE_PARSER_HH
#define GCC_CMDLINE_PARSER_HH

#include <utility>
#include <vector>
#include <functional>

namespace klp
{
  namespace ccp
  {
    class gcc_cmdline_parser
    {
    public:
      struct option
      {
	enum component
	{
	  comp_none	= 0,
	  comp_common	= 0x1,
	  comp_driver	= 0x2,
	  comp_target	= 0x4,
	  comp_c	= 0x8 ,
	  comp_cxx	= 0x10,
	  comp_objc	= 0x20,
	  comp_objcxx	= 0x40,
	};

	enum argument
	{
	  arg_none			= 0,
	  arg_joined			= 0x1,
	  arg_joined_or_missing	= 0x2,
	  arg_separate			= 0x4,
	};

	struct alias_type
	{
	  const char *name;
	  const char *pos_arg;
	  const char *neg_arg;
	};

	const char *name;
	unsigned int comp;
	unsigned int arg;
	bool reject_negative;
	alias_type alias;
      };

      void register_table(const option * const table);

      void operator()(const int argc, const char *argv[],
		      const std::function<void(const char *name,
					       const char *val,
					       bool negative)> &callback) const;

    private:
      typedef std::pair<const option*, std::size_t> _table_type;

      static const option* _find_option(const char *s, const _table_type &table)
	noexcept;

      const option* _find_option(const char *s) const noexcept;

      std::vector<_table_type> _tables;
    };
  }
}

#endif