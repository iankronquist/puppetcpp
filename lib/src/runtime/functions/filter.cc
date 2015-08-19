#include <puppet/runtime/functions/filter.hpp>
#include <puppet/runtime/expression_evaluator.hpp>
#include <puppet/cast.hpp>

using namespace std;
using namespace puppet::lexer;
using namespace puppet::runtime::values;

namespace puppet { namespace runtime { namespace functions {

    struct filter_visitor : boost::static_visitor<value>
    {
        explicit filter_visitor(call_context& context) :
            _context(context)
        {
        }

        result_type operator()(string const& argument)
        {
            values::array result;

            values::array arguments;
            arguments.reserve(2);
            for (size_t i = 0; i < argument.size(); ++i) {
                arguments.clear();

                string value(1, argument[i]);
                if (_context.lambda_parameter_count() == 1) {
                    arguments.push_back(value);
                } else {
                    arguments.push_back(static_cast<int64_t>(i));
                    arguments.push_back(value);
                }
                if (is_true(_context.yield(arguments))) {
                    result.emplace_back(rvalue_cast(value));
                }
            }
            return result;
        }

        result_type operator()(int64_t argument)
        {
            if (argument <= 0) {
                return values::array();
            }
            return enumerate(types::integer(0, argument));
        }

        result_type operator()(values::array& argument)
        {
            values::array result;

            values::array arguments;
            arguments.reserve(2);
            for (size_t i = 0; i < argument.size(); ++i) {
                arguments.clear();
                if (_context.lambda_parameter_count() == 1) {
                    arguments.push_back(argument[i]);
                } else {
                    arguments.push_back(static_cast<int64_t>(i));
                    arguments.push_back(argument[i]);
                }
                if (is_true(_context.yield(arguments))) {
                    result.emplace_back(rvalue_cast(argument[i]));
                }
            }
            return result;
        }

        result_type operator()(values::hash& argument)
        {
            values::hash result;

            values::array arguments;
            arguments.reserve(2);
            for (auto& kvp : argument) {
                arguments.clear();
                if (_context.lambda_parameter_count() == 1) {
                    arguments.emplace_back(values::array { kvp.first, kvp.second });
                } else {
                    arguments.push_back(kvp.first);
                    arguments.push_back(kvp.second);
                }
                if (is_true(_context.yield(arguments))) {
                    result.emplace(make_pair(kvp.first, rvalue_cast(kvp.second)));
                }
            }
            return result;
        }

        result_type operator()(type& argument)
        {
            return boost::apply_visitor(*this, argument);
        }

        result_type operator()(types::integer& argument)
        {
            if (!argument.enumerable()) {
                throw _context.evaluator().create_exception(_context.position(0), (boost::format("%1% is not enumerable.") % argument).str());
            }
            return enumerate(argument);
        }

        template <typename T>
        result_type operator()(T const& argument)
        {
            throw _context.evaluator().create_exception(_context.position(0), (boost::format("expected enumerable type for first argument but found %1%.") % get_type(argument)).str());
        }

     private:
        result_type enumerate(types::integer const& range)
        {
            values::array result;

            values::array arguments;
            arguments.reserve(2);

            range.each([&](int64_t index, int64_t value) {
                arguments.clear();
                if (_context.lambda_parameter_count() == 1) {
                    arguments.push_back(value);
                } else {
                    arguments.push_back(index);
                    arguments.push_back(value);
                }
                if (is_true(_context.yield(arguments))) {
                    result.emplace_back(value);
                }
                return true;
            });
            return result;
        }

        call_context& _context;
    };

    value filter::operator()(call_context& context) const
    {
        auto& evaluator = context.evaluator();

        // Check the argument count
        auto& arguments = context.arguments();
        if (arguments.size() != 1) {
            throw evaluator.create_exception(arguments.size() > 1 ? context.position(1) : context.position(), (boost::format("expected 1 argument to '%1%' function but %2% were given.") % context.name() % arguments.size()).str());
        }

        // Check the lambda
        if (!context.lambda_given()) {
            throw evaluator.create_exception(context.position(), "expected a lambda to 'filter' function but one was not given.");
        }
        auto count = context.lambda_parameter_count();
        if (count == 0 || count > 2) {
            throw evaluator.create_exception(context.lambda_position(), (boost::format("expected 1 or 2 lambda parameters but %1% were given.") % count).str());
        }

        value argument = mutate(arguments[0]);

        filter_visitor visitor(context);
        return boost::apply_visitor(visitor, argument);
    }

}}}  // namespace puppet::runtime::functions
