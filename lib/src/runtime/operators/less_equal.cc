#include <puppet/runtime/operators/less_equal.hpp>
#include <puppet/runtime/expression_evaluator.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace puppet::lexer;
using namespace puppet::runtime::values;

namespace puppet { namespace runtime { namespace operators {

    struct less_equal_visitor : boost::static_visitor<value>
    {
        explicit less_equal_visitor(binary_context& context) :
            _context(context)
        {
        }

        result_type operator()(int64_t left, int64_t right) const
        {
            return left <= right;
        }

        result_type operator()(int64_t left, long double right) const
        {
            return operator()(static_cast<long double>(left), right);
        }

        result_type operator()(long double left, int64_t right) const
        {
            return operator()(left, static_cast<long double>(right));
        }

        result_type operator()(long double left, long double right) const
        {
            return left <= right;
        }

        result_type operator()(string const& left, string const& right) const
        {
            // TODO: revisit performance
            return boost::ilexicographical_compare(left, right) || boost::iequals(left, right);
        }

        result_type operator()(type const& left, type const& right) const
        {
            return left == right || is_specialization(right, left);
        }

        template <
            typename Right,
            typename = typename enable_if<!is_same<Right, int64_t>::value>::type,
            typename = typename enable_if<!is_same<Right, long double>::value>::type
        >
        result_type operator()(int64_t const&, Right const& right) const
        {
            throw _context.evaluator().create_exception(_context.right_position(), (boost::format("expected %1% for comparison but found %2%.") % types::numeric::name() % get_type(right)).str());
        }

        template <
            typename Right,
            typename = typename enable_if<!is_same<Right, int64_t>::value>::type,
            typename = typename enable_if<!is_same<Right, long double>::value>::type
        >
        result_type operator()(long double const&, Right const& right) const
        {
            throw _context.evaluator().create_exception(_context.right_position(), (boost::format("expected %1% for comparison but found %2%.") % types::numeric::name() % get_type(right)).str());
        }

        template <
            typename Right,
            typename = typename enable_if<!is_same<Right, string>::value>::type
        >
        result_type operator()(string const&, Right const& right) const
        {
            throw _context.evaluator().create_exception(_context.right_position(), (boost::format("expected %1% for comparison but found %2%.") % types::string::name() % get_type(right)).str());
        }

        template <
            typename Right,
            typename = typename enable_if<!is_same<Right, type>::value>::type
        >
        result_type operator()(type const&, Right const& right) const
        {
            throw _context.evaluator().create_exception(_context.right_position(), (boost::format("expected %1% for comparison but found %2%.") % types::type::name() % get_type(right)).str());
        }

        template <typename Left, typename Right>
        result_type operator()(Left const& left, Right const&) const
        {
            throw _context.evaluator().create_exception(_context.left_position(), (boost::format("expected %1%, %2%, or %3% for comparison but found %4%.") % types::numeric::name() % types::string::name() % types::type::name() % get_type(left)).str());
        }

     private:
        binary_context& _context;
    };

    value less_equal::operator()(binary_context& context) const
    {
        less_equal_visitor visitor(context);
        return boost::apply_visitor(visitor, dereference(context.left()), dereference(context.right()));
    }

}}}  // namespace puppet::runtime::operators
