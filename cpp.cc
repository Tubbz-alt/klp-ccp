#include <iostream>
#include "preprocessor.hh"
#include "pp_except.hh"
#include "parse_except.hh"
#include "semantic_except.hh"
#include "arch_x86_64_gcc.hh"

using namespace klp::ccp;

int main(int argc, char* argv[])
{
  if (argc != 2) {
    std::cerr << "error: usage: " << argv[0] << " <file>" << std::endl;
    return 1;
  }

  header_resolver hr;
  arch_x86_64_gcc arch{"4.8.5"};
  preprocessor p{hr, arch};
  arch.parse_command_line(0, nullptr, hr, p,
			  [](const std::string&) {
			    assert(0);
			    __builtin_unreachable();
			  });
  p.add_root_source(argv[1], false);
  p.set_base_file(argv[1]);

  const pp_tokens &tokens = p.get_result().get_pp_tokens();
  while(true) {
    try {
      auto &tok = tokens[p.read_next_token()];
      if (!p.get_remarks().empty()) {
	std::cerr << p.get_remarks();
	p.get_remarks().clear();
      }

      if (tok.is_eof())
	break;

      std::cout << tok.stringify();

    } catch (const pp_except&) {
	std::cerr << p.get_remarks();
	return 1;
    } catch (const parse_except&) {
      std::cerr << p.get_remarks();
      return 2;
    } catch (const semantic_except&) {
      std::cerr << p.get_remarks();
      return 3;
    }
  }

  return 0;
}
