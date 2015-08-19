#include <puppet/runtime/functions/assert_type.hpp>
#include <puppet/cast.hpp>

using namespace std;
using namespace puppet::lexer;
using namespace puppet::runtime::values;

namespace puppet { namespace runtime { namespace functions {

    value assert_type::operator()(call_context& context) const
    {
        auto& evaluator = context.evaluator();

        // Check the argument count
        auto& arguments = context.arguments();
        if (arguments.size() != 2) {
            throw evaluator.create_exception(arguments.size() > 2 ? context.position(2) : context.position(), (boost::format("expected 2 arguments to '%1%' function but %2% were given.") % context.name() % arguments.size()).str());
        }

        // First argument should be a type (TODO: should accept a string that is a type name too)
        auto type = as<values::type>(arguments[0]);
        if (!type) {
            throw evaluator.create_exception(context.position(0), (boost::format("expected %1% for first argument but found %2%.") % types::type::name() % get_type(arguments[0])).str());
        }

        // If the value is an instance of the type, return it
        if (is_instance(arguments[1], *type)) {
            return rvalue_cast(arguments[1]);
        }

        // Otherwise, a lambda is required
        if (!context.lambda_given()) {
            throw evaluator.create_exception(context.position(1), (boost::format("type assertion failure: expected %1% but found %2%.") % *type % get_type(arguments[1])).str());
        }

        arguments[1] = get_type(arguments[1]);
        return context.yield(arguments);
    }

}}}  // namespace puppet::runtime::functions
