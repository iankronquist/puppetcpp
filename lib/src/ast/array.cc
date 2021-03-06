#include <puppet/ast/array.hpp>
#include <puppet/ast/expression_def.hpp>
#include <puppet/ast/utility.hpp>
#include <puppet/cast.hpp>

using namespace std;
using boost::optional;

namespace puppet { namespace ast {

    array::array(lexer::position position, optional<vector<expression>> elements) :
        _position(rvalue_cast(position)),
        _elements(rvalue_cast(elements))
    {
    }

    optional<vector<expression>> const&array::elements() const
    {
        return _elements;
    }

    lexer::position const& array::position() const
    {
        return _position;
    }

    ostream& operator<<(ostream& os, array const& array)
    {
        os << '[';
        pretty_print(os, array.elements(), ", ");
        os << ']';
        return os;
    }

}}  // namespace puppet::ast
