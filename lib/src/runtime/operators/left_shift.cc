#include <puppet/runtime/operators/left_shift.hpp>
#include <puppet/runtime/expression_evaluator.hpp>
#include <puppet/cast.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace puppet::lexer;
using namespace puppet::runtime::values;

namespace puppet { namespace runtime { namespace operators {

    struct left_shift_visitor : boost::static_visitor<value>
    {
        explicit left_shift_visitor(binary_context& context) :
            _context(context)
        {
        }

        result_type operator()(int64_t left, int64_t right) const
        {
            // If right < 0. reverse direction
            // If left is negative, keep the sign bit
            if (right < 0 && left < 0) {
                return -(-left >> -right);
            }
            if (right < 0) {
                return left >> -right;
            }
            if (left < 0) {
                return -(-left << right);
            }
            return left << right;
        }

        template <typename Right>
        result_type operator()(values::array& left, Right& right) const
        {
            left.emplace_back(rvalue_cast(right));
            return rvalue_cast(left);
        }

        template <
            typename Right,
            typename = typename enable_if<!is_same<Right, int64_t>::value>::type
        >
        result_type operator()(int64_t const&, Right const& right) const
        {
            throw _context.evaluator().create_exception(_context.right_position(), (boost::format("expected %1% for bitwise left shift but found %2%.") % types::integer::name() % get_type(right)).str());
        }

        template <typename Left, typename Right>
        result_type operator()(Left const& left, Right const&) const
        {
            throw _context.evaluator().create_exception(_context.left_position(), (boost::format("expected %1% for bitwise left shift but found %2%.") % types::integer::name() % get_type(left)).str());
        }

     private:
        binary_context& _context;
    };

    value left_shift::operator()(binary_context& context) const
    {
        // Prepare the left and right for mutation
        auto left = mutate(context.left());
        auto right = mutate(context.right());

        left_shift_visitor visitor(context);
        return boost::apply_visitor(visitor, left, right);
    }

}}}  // namespace puppet::runtime::operators
