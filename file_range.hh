#ifndef FILE_RANGE_HH
#define FILE_RANGE_HH

#include <string>
#include <ios>

namespace suse
{
namespace cp
  {
    class inclusion_node;
    class header_inclusion_node;

    class file_range
    {
    public:
      typedef std::streamoff loc_type;

      file_range() = default;
      file_range(const inclusion_node &inclusion_node,
		 const loc_type &start_loc,
		 const loc_type &end_loc);
      file_range(const inclusion_node &inclusion_node,
		 const loc_type &loc);

      file_range(const file_range &start,
		 const file_range &end);

      bool operator==(const file_range &rhs) const noexcept;

      const inclusion_node& get_inclusion_node() const noexcept
      { return *_inclusion_node; }

      const header_inclusion_node& get_header_inclusion_node() const noexcept;

      loc_type get_start_loc() const noexcept
      { return _start_loc; }

      loc_type get_end_loc() const noexcept
      { return _end_loc; }

      loc_type get_start_line() const noexcept;

      friend std::ostream& operator<<(std::ostream &o, const file_range &r);

    private:
      const inclusion_node *_inclusion_node;
      loc_type _start_loc;
      loc_type _end_loc;
    };

    std::ostream& operator<<(std::ostream &o, const file_range &r);
  }
}

#endif
