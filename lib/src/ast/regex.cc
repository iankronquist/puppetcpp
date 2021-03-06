#include <puppet/ast/regex.hpp>

using namespace std;
using namespace puppet::lexer;

namespace puppet { namespace ast {

    regex::regex()
    {
    }

    string const& regex::value() const
    {
        return _value;
    }

    lexer::position const& regex::position() const
    {
        return _position;
    }

    ostream& operator<<(ostream& os, regex const& regex)
    {
        os << "/" << regex.value() << "/";
        return os;
    }

}}  // namespace puppet::ast
