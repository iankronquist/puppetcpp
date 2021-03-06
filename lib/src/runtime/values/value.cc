#include <puppet/runtime/values/value.hpp>
#include <puppet/runtime/expression_evaluator.hpp>
#include <puppet/cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

using namespace std;

namespace puppet { namespace runtime { namespace values {

    struct value_visitor : boost::static_visitor<ostream&>
    {
        explicit value_visitor(ostream& os) :
            _os(os)
        {
        }

        result_type operator()(bool val) const
        {
            _os << (val ? "true" : "false");
            return _os;
        }

        template <typename T>
        result_type operator()(T const& t) const
        {
            _os << t;
            return _os;
        }

    private:
        ostream& _os;
    };

    ostream& operator<<(ostream& os, value const& val)
    {
        return boost::apply_visitor(value_visitor(os), val);
    }

    value mutate(value& v)
    {
        // Check for variable first and create a copy
        if (boost::get<variable>(&v)) {
            return dereference(v);
        }
        // Otherwise, just move the argument
        return rvalue_cast(v);
    }

    value const& dereference(value const& val)
    {
        auto result = &val;
        auto ptr = boost::get<variable>(result);
        while (ptr) {
            result = &ptr->value();
            ptr = boost::get<variable>(result);
        }
        return *result;
    }

    bool is_undef(value const& val)
    {
        return as<undef>(val);
    }

    bool is_default(value const& val)
    {
        return as<defaulted>(val);
    }

    bool is_true(value const& val)
    {
        auto ptr = as<bool>(val);
        return ptr && *ptr;
    }

    bool is_false(value const& val)
    {
        auto ptr = as<bool>(val);
        return ptr && !*ptr;
    }

    struct truthy_visitor : boost::static_visitor<bool>
    {
        result_type operator()(undef const&) const
        {
            return false;
        }

        result_type operator()(bool val) const
        {
            return val;
        }

        result_type operator()(variable const& var) const
        {
            return is_truthy(var.value());
        }

        template <typename T>
        result_type operator()(T const&) const
        {
            return true;
        }
    };

    bool is_truthy(value const& val)
    {
        return boost::apply_visitor(truthy_visitor(), val);
    }

    struct type_visitor : boost::static_visitor<values::type>
    {
        result_type operator()(undef const&) const
        {
            return types::undef();
        }

        result_type operator()(defaulted const&) const
        {
            return types::defaulted();
        }

        result_type operator()(int64_t) const
        {
            return types::integer();
        }

        result_type operator()(long double) const
        {
            return types::floating();
        }

        result_type operator()(bool) const
        {
            return types::boolean();
        }

        result_type operator()(string const&) const
        {
            return types::string();
        }

        result_type operator()(values::regex const&) const
        {
            return types::regexp();
        }

        result_type operator()(type const& t) const
        {
            return types::type(t);
        }

        result_type operator()(variable const& var) const
        {
            return boost::apply_visitor(*this, var.value());
        }

        result_type operator()(array const&) const
        {
            return types::array(types::any());
        }

        result_type operator()(hash const&) const
        {
            return types::hash(types::any(), types::any());
        }
    };

    values::type get_type(value const& val)
    {
        return boost::apply_visitor(type_visitor(), val);
    }

    struct is_instance_visitor : boost::static_visitor<bool>
    {
        explicit is_instance_visitor(value const& val) :
            _value(val)
        {
        }

        template <typename Type>
        result_type operator()(Type const& t) const
        {
            return t.is_instance(_value);
        }

     private:
        value const& _value;
    };

    bool is_instance(value const& val, type const& t)
    {
        return boost::apply_visitor(is_instance_visitor(dereference(val)), t);
    }

    struct is_specialization_visitor : boost::static_visitor<bool>
    {
        explicit is_specialization_visitor(type const& t) :
            _type(t)
        {
        }

        template <typename Type>
        result_type operator()(Type const& t) const
        {
            return t.is_specialization(_type);
        }

    private:
        type const& _type;
    };

    bool is_specialization(type const& first, type const& second)
    {
        return boost::apply_visitor(is_specialization_visitor(second), first);
    }

    array to_array(value& val, bool convert_hash)
    {
        // If already an array, return a copy
        if (as<values::array>(val)) {
            return mutate_as<values::array>(val);
        }

        array result;

        // Check for hash
        auto hash_ptr = as<values::hash>(val);
        if (convert_hash && hash_ptr) {
            // Turn the hash into an array of [K,V]
            for (auto& kvp : *hash_ptr) {
                array element;
                element.emplace_back(kvp.first);
                element.emplace_back(kvp.second);
                result.emplace_back(rvalue_cast(element));
            }
        } else if (!is_undef(val)) {
            // Otherwise, add the value as the only element
            result.emplace_back(val);
        }
        return result;
    }

    void join(ostream& os, array const& arr, std::string const& separator)
    {
        bool first = true;
        for (auto const& element : arr) {
            if (first) {
                first = false;
            } else {
                os << separator;
            }
            os << element;
        }
    }

    bool operator==(undef const&, undef const&)
    {
        return true;
    }

    bool operator==(defaulted const&, defaulted const&)
    {
        return true;
    }

    bool operator==(regex const& left, regex const& right)
    {
        return left.pattern() == right.pattern();
    }

    bool operator==(array const& left, array const& right)
    {
        if (left.size() != right.size()) {
            return false;
        }
        for (size_t i = 0; i < left.size(); ++i) {
            if (!equals(left[i], right[i])) {
                return false;
            }
        }
        return true;
    }

    bool operator==(hash const& left, hash const& right)
    {
        if (left.size() != right.size()) {
            return false;
        }
        for (auto const& element : left) {
            // Other hash must have the same key
            auto other = right.find(element.first);
            if (other == right.end()) {
                return false;
            }
            // Values must be equal
            if (!equals(element.second, other->second)) {
                return false;
            }
        }
        return true;
    }

    equality_visitor::result_type equality_visitor::operator()(string const& left, string const& right) const
    {
        return boost::algorithm::iequals(left, right);
    }

    bool equals(value const& left, value const& right)
    {
        return boost::apply_visitor(equality_visitor(), left, right);
    }

    void each_resource(values::value const& value, function<void(types::resource const&)> const& callback, function<void(string const&)> const& error)
    {
        // Check for string, type, or array
        if (auto str = as<string>(value)) {
            // Parse as resource
            auto resource = types::resource::parse(*str);
            if (!resource) {
                if (error) {
                    error((boost::format("expected a resource string but found \"%1%\".") % *str).str());
                }
                return;
            }
            callback(*resource);
            return;
        } else if (auto type = as<values::type>(value)) {
            // Check for a resource or klass type
            if (auto resource = boost::get<types::resource>(type)) {
                if (resource->fully_qualified()) {
                    callback(*resource);
                    return;
                }
            } else if (auto klass = boost::get<types::klass>(type)) {
                if (!klass->title().empty()) {
                    callback(types::resource("class", klass->title()));
                    return;
                }
            }
        } else if (auto array = as<values::array>(value)) {
            // For arrays, recurse on each element
            for (auto& element : *array) {
                each_resource(element, callback, error);
            }
            return;
        }

        if (error) {
            error((boost::format("expected %1%, %2%, or %3% for relationship operator but found %4%.") %
                   types::string::name() %
                   types::resource::name() %
                   types::array(types::variant({ values::type(types::string()), values::type(types::resource()) })) %
                   get_type(value)).str());
        }
    }

}}}  // namespace puppet::runtime::values
