#include <puppet/runtime/definition_scanner.hpp>
#include <puppet/runtime/expression_evaluator.hpp>
#include <puppet/runtime/executor.hpp>
#include <puppet/compiler/node.hpp>
#include <puppet/ast/expression_def.hpp>
#include <puppet/cast.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;

namespace puppet { namespace runtime {

    /**
     * This utility type is responsible for scanning the AST for class, type, and node definitions.
     * Because classes can be declared before they are defined, scanning needs to take place
     * before AST evaluation.
     */
    struct scanning_visitor : boost::static_visitor<void>
    {
        scanning_visitor(runtime::catalog& catalog, shared_ptr<compiler::context> const& context) :
            _catalog(catalog),
            _context(context)
        {
            // Push a "top level" scope
            _scopes.push_back("::");
        }

        result_type operator()(ast::basic_expression const& expr)
        {
            // Basic expressions have no class scope
            class_scope scope(_scopes);

            boost::apply_visitor(*this, expr);
        }

        result_type operator()(ast::catalog_expression const& expr)
        {
            boost::apply_visitor(*this, expr);
        }

        result_type operator()(ast::control_flow_expression const& expr)
        {
            // Control flow expressions have no class scope
            class_scope scope(_scopes);

            boost::apply_visitor(*this, expr);
        }

        result_type operator()(ast::unary_expression const& expr)
        {
            boost::apply_visitor(*this, expr.operand());
        }

        result_type operator()(ast::postfix_expression const& expr)
        {
            boost::apply_visitor(*this, expr.primary());

            for (auto const& subexpression : expr.subexpressions()) {
                boost::apply_visitor(*this, subexpression);
            }
        }

        result_type operator()(ast::expression const& expr)
        {
            boost::apply_visitor(*this, expr.primary());

            for (auto const& binary : expr.binary()) {
                boost::apply_visitor(*this, binary.operand());
            }
        }

        result_type operator()(ast::case_expression const& expr)
        {
            operator()(expr.expression());

            for (auto const& proposition : expr.propositions()) {
                for (auto const& option : proposition.options()) {
                    operator()(option);
                }
                if (proposition.body()) {
                    for (auto const& expression : *proposition.body()) {
                        operator()(expression);
                    }
                }
            }
        }

        result_type operator()(ast::if_expression const& expr)
        {
            operator()(expr.conditional());
            if (expr.body()) {
                for (auto const& expression : *expr.body()) {
                    operator()(expression);
                }
            }

            if (expr.elsifs()) {
                for (auto const& elsif : *expr.elsifs()) {
                    operator()(elsif.conditional());
                    if (elsif.body()) {
                        for (auto const& expression : *elsif.body()) {
                            operator()(expression);
                        }
                    }
                }
            }

            if (expr.else_() && expr.else_()->body()) {
                for (auto const& expression : *expr.else_()->body()) {
                    operator()(expression);
                }
            }
        }

        result_type operator()(ast::unless_expression const& expr)
        {
            operator()(expr.conditional());
            if (expr.body()) {
                for (auto const& expression : *expr.body()) {
                    operator()(expression);
                }
            }

            if (expr.else_() && expr.else_()->body()) {
                for (auto const& expression : *expr.else_()->body()) {
                    operator()(expression);
                }
            }
        }

        result_type operator()(ast::function_call_expression const& expr)
        {
            if (expr.arguments()) {
                for (auto const& argument : *expr.arguments()) {
                    operator()(argument);
                }
            }
            if (expr.lambda()) {
                operator()(*expr.lambda());
            }
        }

        result_type operator()(ast::selector_expression const& expr)
        {
            for (auto const& case_ : expr.cases()) {
                operator()(case_.selector());
                operator()(case_.result());
            }
        }

        result_type operator()(ast::access_expression const& expr)
        {
            for (auto const& argument : expr.arguments()) {
                operator()(argument);
            }
        }

        result_type operator()(ast::method_call_expression const& expr)
        {
            if (expr.arguments()) {
                for (auto const& argument : *expr.arguments()) {
                    operator()(argument);
                }
            }
            if (expr.lambda()) {
                operator()(*expr.lambda());
            }
        }

        result_type operator()(ast::lambda const& lambda)
        {
            if (lambda.parameters()) {
                for (auto const& parameter : *lambda.parameters()) {
                    if (parameter.type()) {
                        boost::apply_visitor(*this, *parameter.type());
                    }
                    if (parameter.default_value()) {
                        operator()(*parameter.default_value());
                    }
                }
            }

            if (lambda.body()) {
                for (auto const& expression : *lambda.body()) {
                    operator()(expression);
                }
            }
        }

        result_type operator()(boost::blank const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::undef const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::defaulted const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::boolean const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::number const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::regex const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::variable const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::name const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::bare_word const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::type const&)
        {
            // No subexpressions to scan
        }

        result_type operator()(ast::string const&)
        {
            // TODO: implement an interpolation scanner?
        }

        result_type operator()(ast::array const& array)
        {
            if (array.elements()) {
                for (auto const& element : *array.elements()) {
                    operator()(element);
                }
            }
        }

        result_type operator()(ast::hash const& hash)
        {
            if (hash.elements()) {
                for (auto const& pair : *hash.elements()) {
                    operator()(pair.first);
                    operator()(pair.second);
                }
            }
        }

        result_type operator()(ast::resource_expression const& expr)
        {
            // Resource expressions have no class scope
            class_scope scope(_scopes);

            for (auto const& body : expr.bodies()) {
                operator()(body.title());
                if (body.attributes()) {
                    for (auto const& attribute : *body.attributes()) {
                        operator()(attribute.value());
                    }
                }
            }
        }

        result_type operator()(ast::resource_override_expression const& expr)
        {
            // Resource expressions have no class scope
            class_scope scope(_scopes);

            boost::apply_visitor(*this, expr.reference());

            if (expr.attributes()) {
                for (auto const& attribute : *expr.attributes()) {
                    operator()(attribute.value());
                }
            }
        }

        result_type operator()(ast::resource_defaults_expression const& expr)
        {
            // Resource expressions have no class scope
            class_scope scope(_scopes);

            if (expr.attributes()) {
                for (auto const& attribute : *expr.attributes()) {
                    operator()(attribute.value());
                }
            }
        }

        result_type operator()(ast::class_definition_expression const& expr)
        {
            // Validate the class name
            types::klass klass(validate_name(true, expr.name()));

            // Check to see if this class's parent matches existing definitions
            if (expr.parent()) {
                auto definitions = _catalog.find_class(klass);
                if (definitions) {
                    types::klass parent(expr.parent()->value());
                    for (auto const& definition : *definitions) {
                        auto existing = definition.parent();
                        if (!existing) {
                            continue;
                        }
                        if (parent == *existing) {
                            continue;
                        }
                        throw evaluation_exception(
                            (boost::format("class '%1%' cannot inherit from '%2%' because the class already inherits from '%3%' at %4%:%5%.") %
                             klass.title() %
                             expr.parent()->value() %
                             existing->title() %
                             *definition.path() %
                             definition.line()
                            ).str(),
                            _context,
                            expr.parent()->position());
                    }
                }
            }

            // Validate the class parameters
            if (expr.parameters()) {
                validate_parameters(true, *expr.parameters());
            }

            // Push back the class definition
            _catalog.define_class(klass, _context, expr);

            // Scan the parameters
            if (expr.parameters()) {
                // Parameters have no class scope
                class_scope scope(_scopes, {});

                for (auto const& parameter : *expr.parameters()) {
                    if (parameter.type()) {
                        boost::apply_visitor(*this, *parameter.type());
                    }
                    if (parameter.default_value()) {
                        operator()(*parameter.default_value());
                    }
                }
            }

            // Scan the body
            if (expr.body()) {
                // Set the class scope
                class_scope scope(_scopes, expr.name().value());

                for (auto const &expression : *expr.body()) {
                    operator()(expression);
                }
            }
        }

        result_type operator()(ast::defined_type_expression const& expr)
        {
            // Validate the defined type parameters
            if (expr.parameters()) {
                validate_parameters(false, *expr.parameters());
            }

            // Add the defined type
            _catalog.define_type(validate_name(false, expr.name()), _context, expr);

            // Defined types have no class scope
            class_scope scope(_scopes, {});

            // Scan the parameters
            if (expr.parameters()) {
                for (auto const& parameter : *expr.parameters()) {
                    if (parameter.type()) {
                        boost::apply_visitor(*this, *parameter.type());
                    }
                    if (parameter.default_value()) {
                        operator()(*parameter.default_value());
                    }
                }
            }

            // Scan the body
            if (expr.body()) {
                for (auto const& expression : *expr.body()) {
                    operator()(expression);
                }
            }
        }

        result_type operator()(ast::node_definition_expression const& expr)
        {
            if (!can_define()) {
                throw evaluation_exception("node definitions can only be defined at top-level or inside a class.", _context, expr.position());
            }

            // Define the node in the catalog
            _catalog.define_node(_context, expr);

            // Node definitions have no class scope
            class_scope scope(_scopes, {});

            // Scan the body
            if (expr.body()) {
                for (auto const &expression : *expr.body()) {
                    operator()(expression);
                }
            }
        }

        result_type operator()(ast::collection_expression const& expr)
        {
            // Collection expressions have no class scope
            class_scope scope(_scopes, {});

            // Scan the first query's value
            if (expr.first()) {
                operator()(expr.first()->value());
            }

            // Scan all the remaining query expression values
            for (auto const& binary : expr.remainder()) {
                operator()(binary.operand().value());
            }
        }

     private:
        bool can_define() const
        {
            return !_scopes.back().empty();
        }

        string qualify(std::string const& name)
        {
            if (_scopes.size() < 2) {
                return name;
            }

            string qualified;
            for (auto it = _scopes.begin() + 1; it != _scopes.end(); ++it) {
                if (!qualified.empty()) {
                    qualified += "::";
                }
                qualified += *it;
            }
            if (!qualified.empty()) {
                qualified += "::";
            }
            qualified += name;
            return qualified;
        }

        string validate_name(bool is_class, ast::name const& name)
        {
            if (!can_define()) {
                throw evaluation_exception((boost::format("%1% can only be defined at top-level or inside a class.") % (is_class ? "classes" : "defined types")).str(), _context, name.position());
            }

            if (name.value().empty()) {
                throw evaluation_exception((boost::format("a %1% cannot have an empty name.") % (is_class ? "class" : "defined type")).str(), _context, name.position());
            }

            // Ensure the name is valid
            if (boost::starts_with(name.value(), "::")) {
                throw evaluation_exception((boost::format("'%1%' is not a valid %2% name.") % name % (is_class ? "class" : "defined type")).str(), _context, name.position());
            }

            // Cannot define a class called "main" or "settings" because they are built-in objects
            auto qualified_name = qualify(name.value());
            if (qualified_name == "main" || qualified_name == "settings") {
                throw evaluation_exception((boost::format("'%1%' is the name of a built-in class and cannot be used.") % qualified_name).str(), _context, name.position());
            }

            // Check for conflicts between defined types and classes
            if (is_class) {
                auto type = _catalog.find_defined_type(qualified_name);
                if (type) {
                    throw evaluation_exception((boost::format("'%1%' was previously defined as a defined type at %2%:%3%.") % qualified_name % *type->path() % type->line()).str(), _context, name.position());
                }
            } else {
                auto definitions = _catalog.find_class(types::klass(qualified_name));
                if (definitions) {
                    auto& first = definitions->front();
                    throw evaluation_exception((boost::format("'%1%' was previously defined as a class at %2%:%3%.") % qualified_name % *first.path() % first.line()).str(), _context, name.position());
                }
            }
            return qualified_name;
        }

        void validate_parameters(bool is_class, vector<ast::parameter> const& parameters)
        {
            for (auto const& parameter : parameters) {
                auto const& name = parameter.variable().name();

                // Check for reserved names
                if (name == "title" || name == "name") {
                    throw evaluation_exception((boost::format("parameter $%1% is reserved and cannot be used.") % name).str(), _context, parameter.variable().position());
                }

                // Check for capture parameters
                if (parameter.captures()) {
                    throw evaluation_exception((boost::format("%1% parameter $%2% cannot \"captures rest\".") % (is_class ? "class" : "defined type") % name).str(), _context, parameter.variable().position());
                }

                // Check for metaparameter names
                if (resource::is_metaparameter(name)) {
                    throw evaluation_exception((boost::format("parameter $%1% is reserved for resource metaparameter '%1%'.") % name).str(), _context, parameter.variable().position());
                }
            }
        }

        struct class_scope
        {
            explicit class_scope(deque<string>& scopes, string name = {}) :
                _scopes(scopes)
            {
                scopes.push_back(std::move(name));
            }

            ~class_scope()
            {
                _scopes.pop_back();
            }

         private:
            deque<string>& _scopes;
        };

        runtime::catalog& _catalog;
        shared_ptr<compiler::context> const& _context;
        deque<string> _scopes;
    };

    definition_scanner::definition_scanner(runtime::catalog& catalog) :
        _catalog(catalog)
    {
    }

    void definition_scanner::scan(shared_ptr<compiler::context> const& context)
    {
        if (!context) {
            return;
        }

        auto& tree = context->tree();
        if (!tree.body()) {
            return;
        }

        scanning_visitor visitor(_catalog, context);
        for (auto const& expression : *tree.body()) {
            visitor(expression);
        }
    }

}}  // namespace puppet::runtime
